// SPDX-License-Identifier: LGPL-2.1
// Copyright (C) 2014-2020, Intel Corporation. All rights reserved.
#include <stdlib.h>
#include <limits.h>
#include <util/list.h>
#include <util/size.h>
#include <ndctl/libndctl.h>
#include <ccan/list/list.h>
#include <ndctl/libndctl-nfit.h>
#include <ccan/short_types/short_types.h>
#include "private.h"

NDCTL_EXPORT int ndctl_bus_has_error_injection(struct ndctl_bus *bus)
{
	/* Currently, only nfit buses have error injection */
	if (!bus || !ndctl_bus_has_nfit(bus))
		return 0;

	if (ndctl_bus_is_nfit_cmd_supported(bus, NFIT_CMD_ARS_INJECT_SET) &&
		ndctl_bus_is_nfit_cmd_supported(bus, NFIT_CMD_ARS_INJECT_GET) &&
		ndctl_bus_is_nfit_cmd_supported(bus, NFIT_CMD_ARS_INJECT_CLEAR))
		return 1;

	return 0;
}

static int ndctl_namespace_get_injection_bounds(
		struct ndctl_namespace *ndns, unsigned long long *ns_offset,
		unsigned long long *ns_size)
{
	struct ndctl_pfn *pfn = ndctl_namespace_get_pfn(ndns);
	struct ndctl_dax *dax = ndctl_namespace_get_dax(ndns);
	struct ndctl_btt *btt = ndctl_namespace_get_btt(ndns);

	if (!ns_offset || !ns_size)
		return -ENXIO;

	if (pfn) {
		*ns_offset = ndctl_pfn_get_resource(pfn);
		*ns_size = ndctl_pfn_get_size(pfn);
		return 0;
	}

	if (dax) {
		*ns_offset = ndctl_dax_get_resource(dax);
		*ns_size = ndctl_dax_get_size(dax);
		return 0;
	}

	if (btt)
		return -EOPNOTSUPP;

	/* raw */
	*ns_offset = ndctl_namespace_get_resource(ndns);
	*ns_size = ndctl_namespace_get_size(ndns);
	return 0;
}

static int block_to_spa_offset(struct ndctl_namespace *ndns,
		unsigned long long block, unsigned long long count,
		u64 *offset, u64 *length)
{
	struct ndctl_ctx *ctx = ndctl_namespace_get_ctx(ndns);
	unsigned long long ns_offset, ns_size;
	int rc;

	rc = ndctl_namespace_get_injection_bounds(ndns, &ns_offset, &ns_size);
	if (rc)
		return rc;
	*offset = ns_offset + block * 512;
	*length = count * 512;

	/* check bounds */
	if (*offset + *length > ns_offset + ns_size) {
		dbg(ctx, "Error: block %#llx, count %#llx are out of bounds\n",
			block, count);
		return -EINVAL;
	}
	return 0;
}

static int translate_status(u32 status)
{
	switch (status) {
	case ND_ARS_ERR_INJ_STATUS_NOT_SUPP:
		return -EOPNOTSUPP;
	case ND_ARS_ERR_INJ_STATUS_INVALID_PARAM:
		return -EINVAL;
	}
	return 0;
}

static int ndctl_namespace_get_clear_unit(struct ndctl_namespace *ndns)
{
	struct ndctl_bus *bus = ndctl_namespace_get_bus(ndns);
	struct ndctl_ctx *ctx = ndctl_bus_get_ctx(bus);
	unsigned long long ns_offset, ns_size;
	unsigned int clear_unit;
	struct ndctl_cmd *cmd;
	int rc;

	rc = ndctl_namespace_get_injection_bounds(ndns, &ns_offset,
		&ns_size);
	if (rc)
		return rc;
	cmd = ndctl_bus_cmd_new_ars_cap(bus, ns_offset, ns_size);
	if (!cmd) {
		err(ctx, "%s: failed to create cmd\n",
			ndctl_namespace_get_devname(ndns));
		return -ENOTTY;
	}
	rc = ndctl_cmd_submit(cmd);
	if (rc < 0) {
		dbg(ctx, "Error submitting ars_cap: %d\n", rc);
		goto out;
	}
	clear_unit = ndctl_cmd_ars_cap_get_clear_unit(cmd);
	if (clear_unit == 0) {
		dbg(ctx, "Got an invalid clear_err_unit from ars_cap\n");
		rc = -EINVAL;
		goto out;
	}
	rc = clear_unit;
out:
	ndctl_cmd_unref(cmd);
	return rc;
}

static int ndctl_namespace_inject_one_error(struct ndctl_namespace *ndns,
		unsigned long long block, unsigned int flags)
{
	struct ndctl_bus *bus = ndctl_namespace_get_bus(ndns);
	struct ndctl_ctx *ctx = ndctl_bus_get_ctx(bus);
	struct nd_cmd_ars_err_inj *err_inj;
	struct nd_cmd_pkg *pkg;
	struct ndctl_cmd *cmd;
	u64 offset, length;
	int rc, clear_unit;

	rc = block_to_spa_offset(ndns, block, 1, &offset, &length);
	if (rc)
		return rc;

	clear_unit = ndctl_namespace_get_clear_unit(ndns);
	if (clear_unit < 0)
		return clear_unit;

	if (!(flags & (1 << NDCTL_NS_INJECT_SATURATE))) {
		/* clamp injection length per block to the clear_unit */
		if (length > (unsigned int)clear_unit)
			length = clear_unit;
	}

	cmd = ndctl_bus_cmd_new_err_inj(bus);
	if (!cmd)
		return -ENOMEM;

	pkg = (struct nd_cmd_pkg *)&cmd->cmd_buf[0];
	err_inj = (struct nd_cmd_ars_err_inj *)&pkg->nd_payload[0];
	err_inj->err_inj_spa_range_base = offset;
	err_inj->err_inj_spa_range_length = length;
	if (flags & (1 << NDCTL_NS_INJECT_NOTIFY))
		err_inj->err_inj_options |=
			(1 << ND_ARS_ERR_INJ_OPT_NOTIFY);

	rc = ndctl_cmd_submit(cmd);
	if (rc < 0) {
		dbg(ctx, "Error submitting command: %d\n", rc);
		goto out;
	}
	rc = translate_status(err_inj->status);
 out:
	ndctl_cmd_unref(cmd);
	return rc;
}

NDCTL_EXPORT int ndctl_namespace_inject_error2(struct ndctl_namespace *ndns,
		unsigned long long block, unsigned long long count,
		unsigned int flags)
{
	struct ndctl_bus *bus = ndctl_namespace_get_bus(ndns);
	struct ndctl_ctx *ctx = ndctl_bus_get_ctx(bus);
	unsigned long long i;
	int rc = -EINVAL;

	if (!ndctl_bus_has_error_injection(bus))
		return -EOPNOTSUPP;
	if (!ndctl_bus_has_nfit(bus))
		return -EOPNOTSUPP;

	for (i = 0; i < count; i++) {
		rc = ndctl_namespace_inject_one_error(ndns, block + i, flags);
		if (rc) {
			err(ctx, "Injection failed at block %llx\n",
				block + i);
			return rc;
		}
	}
	return rc;
}

NDCTL_EXPORT int ndctl_namespace_inject_error(struct ndctl_namespace *ndns,
		unsigned long long block, unsigned long long count, bool notify)
{
	return ndctl_namespace_inject_error2(ndns, block, count,
		notify ? (1 << NDCTL_NS_INJECT_NOTIFY) : 0);
}

static int ndctl_namespace_uninject_one_error(struct ndctl_namespace *ndns,
		unsigned long long block, unsigned int flags)
{
	struct ndctl_bus *bus = ndctl_namespace_get_bus(ndns);
	struct ndctl_ctx *ctx = ndctl_bus_get_ctx(bus);
	struct nd_cmd_ars_err_inj_clr *err_inj_clr;
	struct nd_cmd_pkg *pkg;
	struct ndctl_cmd *cmd;
	u64 offset, length;
	int rc, clear_unit;

	rc = block_to_spa_offset(ndns, block, 1, &offset, &length);
	if (rc)
		return rc;

	clear_unit = ndctl_namespace_get_clear_unit(ndns);
	if (clear_unit < 0)
		return clear_unit;

	if (!(flags & (1 << NDCTL_NS_INJECT_SATURATE))) {
		/* clamp injection length per block to the clear_unit */
		if (length > (unsigned int)clear_unit)
			length = clear_unit;
	}

	cmd = ndctl_bus_cmd_new_err_inj_clr(bus);
	if (!cmd)
		return -ENOMEM;

	pkg = (struct nd_cmd_pkg *)&cmd->cmd_buf[0];
	err_inj_clr =
		(struct nd_cmd_ars_err_inj_clr *)&pkg->nd_payload[0];
	err_inj_clr->err_inj_clr_spa_range_base = offset;
	err_inj_clr->err_inj_clr_spa_range_length = length;

	rc = ndctl_cmd_submit(cmd);
	if (rc < 0) {
		dbg(ctx, "Error submitting command: %d\n", rc);
		goto out;
	}
	rc = translate_status(err_inj_clr->status);
 out:
	ndctl_cmd_unref(cmd);
	return rc;
}

NDCTL_EXPORT int ndctl_namespace_uninject_error2(struct ndctl_namespace *ndns,
		unsigned long long block, unsigned long long count,
		unsigned int flags)
{
	struct ndctl_bus *bus = ndctl_namespace_get_bus(ndns);
	struct ndctl_ctx *ctx = ndctl_bus_get_ctx(bus);
	unsigned long long i;
	int rc = -EINVAL;

	if (!ndctl_bus_has_error_injection(bus))
		return -EOPNOTSUPP;
	if (!ndctl_bus_has_nfit(bus))
		return -EOPNOTSUPP;

	for (i = 0; i < count; i++) {
		rc = ndctl_namespace_uninject_one_error(ndns, block + i,
			flags);
		if (rc) {
			err(ctx, "Un-injection failed at block %llx\n",
				block + i);
			return rc;
		}
	}
	return rc;
}

NDCTL_EXPORT int ndctl_namespace_uninject_error(struct ndctl_namespace *ndns,
		unsigned long long block, unsigned long long count)
{
	return ndctl_namespace_uninject_error2(ndns, block, count, 0);
}

static int bb_add_record(struct list_head *h, u64 block, u64 count)
{
	struct ndctl_bb *bb, *bb_iter, *bb_next, *bb_prev;
	int merged = 0;

	bb = calloc(1, sizeof(*bb));
	if (bb == NULL)
		return -ENOMEM;
	bb->block = block;
	bb->count = count;

	if (list_empty(h)) {
		list_add(h, &bb->list);
		return 0;
	}

	/* add 'bb' to the list such that it remains sorted */
	list_for_each(h, bb_iter, list) {
		/* Find insertion point */
		bb_prev = list_prev(h, bb_iter, list);
		bb_next = list_next(h, bb_iter, list);

		if (bb_prev == NULL) {
			/* bb_iter is the first entry */
			if (bb->block < bb_iter->block) {
				list_add(h, &bb->list);
				bb = NULL;
				break;
			}
		}
		if (bb_next == NULL) {
			/*
			 * bb_iter is the last entry. If we've reached here,
			 * the only option is to add to the tail as the case
			 * for "tail - 1" should have been covered by the
			 * following checks for the previous iteration.
			 */
			list_add_tail(h, &bb->list);
			bb = NULL;
			break;
		}
		/* Add to the left of bb_iter */
		if (bb->block <= bb_iter->block) {
			if (bb_prev && (bb_prev->block <= bb->block)) {
				list_add_after(h, &bb_prev->list, &bb->list);
				bb = NULL;
				break;
			}
		}
		/* Add to the right of bb_iter */
		if (bb_iter->block <= bb->block) {
			if (bb_next && (bb->block <= bb_next->block)) {
				list_add_after(h, &bb_iter->list, &bb->list);
				bb = NULL;
				break;
			}
		}
	}

	/* ensure bb has actually been consumed (set to NULL earlier) */
	if (bb != NULL) {
		free(bb);
		return -ENXIO;
	}

	/* second pass over the list looking for mergeable entries */
	list_for_each(h, bb_iter, list) {
		u64 cur_end, next_end, cur_start, next_start;

		/*
		 * test for merges in a loop here because one addition can
		 * potentially have a cascading merge effect on multiple
		 * remaining entries
		 */
		do {
			/* reset the merged flag */
			merged = 0;

			bb_next = list_next(h, bb_iter, list);
			if (bb_next == NULL)
				break;

			cur_start = bb_iter->block;
			next_start = bb_next->block;
			cur_end = bb_iter->block + bb_iter->count - 1;
			next_end = bb_next->block + bb_next->count - 1;

			if (cur_end >= next_start) {
				/* overlapping records that can be merged */
				if (next_end > cur_end) {
					/* next extends cur */
					bb_iter->count =
						next_end - cur_start + 1;
				} else {
					/* next is contained in cur */
					;
				}
				/* next is now redundant */
				list_del_from(h, &bb_next->list);
				free(bb_next);
				merged = 1;
				continue;
			}
			if (next_start == cur_end + 1) {
				/* adjoining records that can be merged */
				bb_iter->count = next_end - cur_start + 1;
				list_del_from(h, &bb_next->list);
				free(bb_next);
				merged = 1;
				continue;
			}
		} while (merged);
	}

	return 0;
}

static int injection_status_to_bb(struct ndctl_namespace *ndns,
		struct nd_cmd_ars_err_inj_stat *stat, u64 ns_spa, u64 ns_size)
{
	unsigned int i;
	int rc = 0;

	for (i = 0; i < stat->inj_err_rec_count; i++) {
		u64 ns_off, rec_off, rec_len;
		u64 block, count, start_pad;

		rec_off = stat->record[i].err_inj_stat_spa_range_base;
		rec_len = stat->record[i].err_inj_stat_spa_range_length;
		/* discard ranges outside the provided namespace */
		if (rec_off < ns_spa)
			continue;
		if (rec_off >= ns_spa + ns_size)
			continue;

		/* translate spa offset to namespace offset */
		ns_off = rec_off - ns_spa;

		block = ALIGN_DOWN(ns_off, 512)/512;
		start_pad = ns_off - (block * 512);
		count = ALIGN(start_pad + rec_len, 512)/512;
		rc = bb_add_record(&ndns->injected_bb, block, count);
		if (rc)
			break;
	}
	return rc;
}

NDCTL_EXPORT int ndctl_namespace_injection_status(struct ndctl_namespace *ndns)
{
	struct ndctl_bus *bus = ndctl_namespace_get_bus(ndns);
	struct ndctl_ctx *ctx = ndctl_bus_get_ctx(bus);
	struct nd_cmd_ars_err_inj_stat *err_inj_stat;
	unsigned long long ns_offset, ns_size;
	int rc = -EOPNOTSUPP, buf_size;
	struct ndctl_cmd *cmd = NULL;
	struct nd_cmd_pkg *pkg;

	if (!ndctl_bus_has_error_injection(bus))
		return -EOPNOTSUPP;

	if (ndctl_bus_has_nfit(bus)) {
		rc = ndctl_namespace_get_injection_bounds(ndns, &ns_offset,
			&ns_size);
		if (rc)
			return rc;

		cmd = ndctl_bus_cmd_new_ars_cap(bus, ns_offset, ns_size);
		if (!cmd) {
			err(ctx, "%s: failed to create cmd\n",
				ndctl_namespace_get_devname(ndns));
			return -ENOTTY;
		}
		rc = ndctl_cmd_submit(cmd);
		if (rc < 0) {
			dbg(ctx, "Error submitting ars_cap: %d\n", rc);
			goto out;
		}
		buf_size = ndctl_cmd_ars_cap_get_size(cmd);
		if (buf_size == 0) {
			dbg(ctx, "Got an invalid max_ars_out from ars_cap\n");
			rc = -EINVAL;
			goto out;
		}
		ndctl_cmd_unref(cmd);

		cmd = ndctl_bus_cmd_new_err_inj_stat(bus, buf_size);
		if (!cmd)
			return -ENOMEM;

		pkg = (struct nd_cmd_pkg *)&cmd->cmd_buf[0];
		err_inj_stat =
			(struct nd_cmd_ars_err_inj_stat *)&pkg->nd_payload[0];

		rc = ndctl_cmd_submit(cmd);
		if (rc < 0) {
			dbg(ctx, "Error submitting command: %d\n", rc);
			goto out;
		}
		rc = injection_status_to_bb(ndns, err_inj_stat,
			ns_offset, ns_size);
		if (rc) {
			dbg(ctx, "Error converting status to badblocks: %d\n",
				rc);
			goto out;
		}
	}

 out:
	ndctl_cmd_unref(cmd);
	return rc;
}

NDCTL_EXPORT struct ndctl_bb *ndctl_namespace_injection_get_first_bb(
		struct ndctl_namespace *ndns)
{
	return list_top(&ndns->injected_bb, struct ndctl_bb, list);
}

NDCTL_EXPORT struct ndctl_bb *ndctl_namespace_injection_get_next_bb(
		struct ndctl_namespace *ndns, struct ndctl_bb *bb)
{
	return list_next(&ndns->injected_bb, bb, list);
}

NDCTL_EXPORT unsigned long long ndctl_bb_get_block(struct ndctl_bb *bb)
{
	if (bb)
		return bb->block;
	errno = EINVAL;
	return ULLONG_MAX;
}

NDCTL_EXPORT unsigned long long ndctl_bb_get_count(struct ndctl_bb *bb)
{
	if (bb)
		return bb->count;
	errno = EINVAL;
	return ULLONG_MAX;
}
