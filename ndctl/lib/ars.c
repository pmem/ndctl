/*
 * Copyright (c) 2014-2016, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU Lesser General Public License,
 * version 2.1, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
 */
#include <stdlib.h>
#include <ndctl/libndctl.h>
#include "private.h"

NDCTL_EXPORT struct ndctl_cmd *ndctl_bus_cmd_new_ars_cap(struct ndctl_bus *bus,
		unsigned long long address, unsigned long long len)
{
	struct ndctl_ctx *ctx = ndctl_bus_get_ctx(bus);
	struct ndctl_cmd *cmd;
	size_t size;

	if (!ndctl_bus_is_cmd_supported(bus, ND_CMD_ARS_CAP)) {
		dbg(ctx, "unsupported cmd\n");
		return NULL;
	}

	size = sizeof(*cmd) + sizeof(struct nd_cmd_ars_cap);
	cmd = calloc(1, size);
	if (!cmd)
		return NULL;

	cmd->bus = bus;
	ndctl_cmd_ref(cmd);
	cmd->type = ND_CMD_ARS_CAP;
	cmd->size = size;
	cmd->status = 1;
	cmd->firmware_status = &cmd->ars_cap->status;
	cmd->ars_cap->address = address;
	cmd->ars_cap->length = len;

	return cmd;
}

static bool is_power_of_2(unsigned int v)
{
	return v && ((v & (v - 1)) == 0);
}

static bool validate_clear_error(struct ndctl_cmd *ars_cap)
{
	if (!is_power_of_2(ars_cap->ars_cap->clear_err_unit))
		return false;
	return true;
}

static bool __validate_ars_cap(struct ndctl_cmd *ars_cap)
{
	if (ars_cap->type != ND_CMD_ARS_CAP || ars_cap->status != 0)
		return false;
	if ((*ars_cap->firmware_status & ARS_STATUS_MASK) != 0)
		return false;
	return validate_clear_error(ars_cap);
}

#define validate_ars_cap(ctx, ars_cap) \
({ \
	bool __valid = __validate_ars_cap(ars_cap); \
	if (!__valid) \
		dbg(ctx, "expected sucessfully completed ars_cap command\n"); \
	__valid; \
})

NDCTL_EXPORT struct ndctl_cmd *ndctl_bus_cmd_new_ars_start(struct ndctl_cmd *ars_cap,
		int type)
{
	struct ndctl_bus *bus = ars_cap->bus;
	struct ndctl_ctx *ctx = ndctl_bus_get_ctx(bus);
	struct ndctl_cmd *cmd;
	size_t size;

	if (!ndctl_bus_is_cmd_supported(bus, ND_CMD_ARS_START)) {
		dbg(ctx, "unsupported cmd\n");
		return NULL;
	}

	if (!validate_ars_cap(ctx, ars_cap))
		return NULL;

	if (!(*ars_cap->firmware_status >> ARS_EXT_STATUS_SHIFT & type)) {
		dbg(ctx, "ars_cap does not show requested type as supported\n");
		return NULL;
	}

	size = sizeof(*cmd) + sizeof(struct nd_cmd_ars_start);
	cmd = calloc(1, size);
	if (!cmd)
		return NULL;

	cmd->bus = bus;
	ndctl_cmd_ref(cmd);
	cmd->type = ND_CMD_ARS_START;
	cmd->size = size;
	cmd->status = 1;
	cmd->firmware_status = &cmd->ars_start->status;
	cmd->ars_start->address = ars_cap->ars_cap->address;
	cmd->ars_start->length = ars_cap->ars_cap->length;
	cmd->ars_start->type = type;

	return cmd;
}

NDCTL_EXPORT struct ndctl_cmd *ndctl_bus_cmd_new_ars_status(struct ndctl_cmd *ars_cap)
{
	struct nd_cmd_ars_cap *ars_cap_cmd = ars_cap->ars_cap;
	struct ndctl_bus *bus = ars_cap->bus;
	struct ndctl_ctx *ctx = ndctl_bus_get_ctx(bus);
	struct ndctl_cmd *cmd;
	size_t size;

	if (!ndctl_bus_is_cmd_supported(bus, ND_CMD_ARS_STATUS)) {
		dbg(ctx, "unsupported cmd\n");
		return NULL;
	}

	if (!validate_ars_cap(ctx, ars_cap))
		return NULL;

	if (ars_cap_cmd->max_ars_out == 0) {
		dbg(ctx, "invalid ars_cap\n");
		return NULL;
	}

	size = sizeof(*cmd) + ars_cap_cmd->max_ars_out;
	cmd = calloc(1, size);
	if (!cmd)
		return NULL;

	cmd->bus = bus;
	ndctl_cmd_ref(cmd);
	cmd->type = ND_CMD_ARS_STATUS;
	cmd->size = size;
	cmd->status = 1;
	cmd->firmware_status = &cmd->ars_status->status;
	cmd->ars_status->out_length = ars_cap_cmd->max_ars_out;

	return cmd;
}

NDCTL_EXPORT unsigned int ndctl_cmd_ars_cap_get_size(struct ndctl_cmd *ars_cap)
{
	struct ndctl_ctx *ctx = ndctl_bus_get_ctx(cmd_to_bus(ars_cap));

	if (ars_cap->type == ND_CMD_ARS_CAP && ars_cap->status == 0) {
		dbg(ctx, "max_ars_out: %d\n",
			ars_cap->ars_cap->max_ars_out);
		return ars_cap->ars_cap->max_ars_out;
	}

	dbg(ctx, "invalid ars_cap\n");
	return 0;
}

NDCTL_EXPORT int ndctl_cmd_ars_cap_get_range(struct ndctl_cmd *ars_cap,
	struct ndctl_range *range)
{
	struct ndctl_ctx *ctx = ndctl_bus_get_ctx(cmd_to_bus(ars_cap));

	if (range && ars_cap->type == ND_CMD_ARS_CAP && ars_cap->status == 0) {
		struct nd_cmd_ars_cap *cap = ars_cap->ars_cap;

		dbg(ctx, "range: %llx - %llx\n", cap->address, cap->length);
		range->address = cap->address;
		range->length = cap->length;
		return 0;
	}

	dbg(ctx, "invalid ars_cap\n");
	return -EINVAL;
}

NDCTL_EXPORT unsigned int ndctl_cmd_ars_cap_get_clear_unit(
		struct ndctl_cmd *ars_cap)
{
	struct ndctl_ctx *ctx = ndctl_bus_get_ctx(cmd_to_bus(ars_cap));

	if (ars_cap->type == ND_CMD_ARS_CAP && ars_cap->status == 0) {
		dbg(ctx, "clear_err_unit: %d\n",
			ars_cap->ars_cap->clear_err_unit);
		return ars_cap->ars_cap->clear_err_unit;
	}

	dbg(ctx, "invalid ars_cap\n");
	return 0;
}

static bool __validate_ars_stat(struct ndctl_cmd *ars_stat)
{
	/*
	 * A positive status indicates an underrun, but that is fine since
	 * the firmware is not expected to completely fill the max_ars_out
	 * sized buffer.
	 */
	if (ars_stat->type != ND_CMD_ARS_STATUS || ars_stat->status < 0)
		return false;
	if ((ndctl_cmd_get_firmware_status(ars_stat) & ARS_STATUS_MASK) != 0)
		return false;
	return true;
}

#define validate_ars_stat(ctx, ars_stat) \
({ \
	bool __valid = __validate_ars_stat(ars_stat); \
	if (!__valid) \
		dbg(ctx, "expected sucessfully completed ars_stat command\n"); \
	__valid; \
})

NDCTL_EXPORT int ndctl_cmd_ars_in_progress(struct ndctl_cmd *cmd)
{
	struct ndctl_ctx *ctx = ndctl_bus_get_ctx(cmd_to_bus(cmd));

	if (!validate_ars_stat(ctx, cmd))
		return 0;

	if (ndctl_cmd_get_firmware_status(cmd) == 1 << 16) {
		/*
		 * If in-progress, invalidate the ndctl_cmd, so
		 * that if we're called again without a fresh
		 * ars_status command, we fail.
		 */
		cmd->status = 1;
		return 1;
	}
	return 0;
}

NDCTL_EXPORT unsigned int ndctl_cmd_ars_num_records(struct ndctl_cmd *ars_stat)
{
	struct ndctl_ctx *ctx = ndctl_bus_get_ctx(cmd_to_bus(ars_stat));

	if (!validate_ars_stat(ctx, ars_stat))
		return 0;

	return ars_stat->ars_status->num_records;
}

NDCTL_EXPORT unsigned long long ndctl_cmd_ars_get_record_addr(
		struct ndctl_cmd *ars_stat, unsigned int rec_index)
{
	struct ndctl_ctx *ctx = ndctl_bus_get_ctx(cmd_to_bus(ars_stat));

	if (!validate_ars_stat(ctx, ars_stat))
		return 0;

	if (rec_index >= ars_stat->ars_status->num_records) {
		dbg(ctx, "invalid record index\n");
		return 0;
	}

	return ars_stat->ars_status->records[rec_index].err_address;
}

NDCTL_EXPORT unsigned long long ndctl_cmd_ars_get_record_len(
		struct ndctl_cmd *ars_stat, unsigned int rec_index)
{
	struct ndctl_ctx *ctx = ndctl_bus_get_ctx(cmd_to_bus(ars_stat));

	if (!validate_ars_stat(ctx, ars_stat))
		return 0;

	if (rec_index >= ars_stat->ars_status->num_records) {
		dbg(ctx, "invalid record index\n");
		return 0;
	}

	return ars_stat->ars_status->records[rec_index].length;
}

NDCTL_EXPORT struct ndctl_cmd *ndctl_bus_cmd_new_clear_error(
		unsigned long long address, unsigned long long len,
		struct ndctl_cmd *ars_cap)
{
	size_t size;
	unsigned int mask;
	struct nd_cmd_ars_cap *cap;
	struct ndctl_cmd *clear_err;
	struct ndctl_bus *bus = ars_cap->bus;
	struct ndctl_ctx *ctx = ndctl_bus_get_ctx(bus);

	if (!ndctl_bus_is_cmd_supported(bus, ND_CMD_ARS_STATUS)) {
		dbg(ctx, "unsupported cmd\n");
		return NULL;
	}

	if (!validate_ars_cap(ctx, ars_cap))
		return NULL;

	cap = ars_cap->ars_cap;
	if (address < cap->address || address > (cap->address + cap->length)
			|| address + len > (cap->address + cap->length)) {
		dbg(ctx, "request out of range (relative to ars_cap)\n");
		return NULL;
	}

	mask = cap->clear_err_unit - 1;
	if ((address | len) & mask) {
		dbg(ctx, "request unaligned\n");
		return NULL;
	}

	size = sizeof(*clear_err) + sizeof(struct nd_cmd_clear_error);
	clear_err = calloc(1, size);
	if (!clear_err)
		return NULL;

	ndctl_cmd_ref(clear_err);
	clear_err->bus = bus;
	clear_err->type = ND_CMD_CLEAR_ERROR;
	clear_err->size = size;
	clear_err->status = 1;
	clear_err->firmware_status = &clear_err->clear_err->status;
	clear_err->clear_err->address = address;
	clear_err->clear_err->length = len;

	return clear_err;
}

NDCTL_EXPORT unsigned long long ndctl_cmd_clear_error_get_cleared(
		struct ndctl_cmd *clear_err)
{
	struct ndctl_ctx *ctx = ndctl_bus_get_ctx(cmd_to_bus(clear_err));

	if (clear_err->type == ND_CMD_CLEAR_ERROR && clear_err->status == 0)
		return clear_err->clear_err->cleared;

	dbg(ctx, "invalid clear_err\n");
	return 0;
}
