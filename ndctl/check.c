// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2015-2020 Intel Corporation. All rights reserved.
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <setjmp.h>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <ndctl.h>
#include <limits.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <util/log.h>
#include <uuid/uuid.h>
#include <sys/types.h>
#include <util/json.h>
#include <util/size.h>
#include <util/util.h>
#include <util/bitmap.h>
#include <util/fletcher.h>
#include <ndctl/libndctl.h>
#include <ndctl/namespace.h>
#include <ccan/endian/endian.h>
#include <ccan/minmax/minmax.h>
#include <ccan/array_size/array_size.h>
#include <ccan/short_types/short_types.h>

struct check_opts {
	bool verbose;
	bool force;
	bool repair;
	bool logfix;
};

struct btt_chk {
	char *path;
	int fd;
	uuid_t parent_uuid;
	unsigned long long rawsize;
	unsigned long long nlba;
	int start_off;
	int num_arenas;
	long sys_page_size;
	struct arena_info *arena;
	struct check_opts *opts;
	struct log_ctx ctx;
};

struct arena_info {
	struct arena_map map;
	u64 size;	/* Total bytes for this arena */
	u64 external_lba_start;
	u32 internal_nlba;
	u32 internal_lbasize;
	u32 external_nlba;
	u32 external_lbasize;
	u32 nfree;
	u16 version_major;
	u16 version_minor;
	u64 nextoff;
	u64 infooff;
	u64 dataoff;
	u64 mapoff;
	u64 logoff;
	u64 info2off;
	u32 flags;
	int num;
	struct btt_chk *bttc;
	int log_index[2];
};

static sigjmp_buf sj_env;

static void sigbus_hdl(int sig, siginfo_t *siginfo, void *ptr)
{
	siglongjmp(sj_env, 1);
}

static int repair_msg(struct btt_chk *bttc)
{
	info(bttc, "  Run with --repair to make the changes\n");
	return 0;
}

/**
 * btt_read_info - read an info block from a given offset
 * @bttc:	the main btt_chk structure for this btt
 * @btt_sb:	struct btt_sb where the info block will be copied into
 * @offset:	offset in the raw namespace to read the info block from
 *
 * This will also use 'pread' to read the info block, and not mmap+loads
 * as this is used before the mappings are set up.
 */
static int btt_read_info(struct btt_chk *bttc, struct btt_sb *btt_sb, u64 off)
{
	ssize_t size;

	size = pread(bttc->fd, btt_sb, sizeof(*btt_sb), off);
	if (size < 0) {
		err(bttc, "unable to read first info block: %s\n",
			strerror(errno));
		return -errno;
	}
	if (size != sizeof(*btt_sb)) {
		err(bttc, "short read of first info block: %ld\n", size);
		return -ENXIO;
	}

	return 0;
}

/**
 * btt_write_info - write an info block to the given offset
 * @bttc:	the main btt_chk structure for this btt
 * @btt_sb:	struct btt_sb where the info block will be copied from
 * @offset:	offset in the raw namespace to write the info block to
 *
 * This will also use 'pwrite' to write the info block, and not mmap+stores
 * as this is used before the mappings are set up.
 */
static int btt_write_info(struct btt_chk *bttc, struct btt_sb *btt_sb, u64 off)
{
	ssize_t size;
	int rc;

	if (!bttc->opts->repair) {
		err(bttc, "BTT info block at offset %#lx needs to be restored\n",
			off);
		repair_msg(bttc);
		return -EIO;
	}
	info(bttc, "Restoring BTT info block at offset %#lx\n", off);

	size = pwrite(bttc->fd, btt_sb, sizeof(*btt_sb), off);
	if (size < 0) {
		err(bttc, "unable to write the info block: %s\n",
			strerror(errno));
		return -errno;
	}
	if (size != sizeof(*btt_sb)) {
		err(bttc, "short write of the info block: %ld\n", size);
		return -ENXIO;
	}

	rc = fsync(bttc->fd);
	if (rc < 0)
		return -errno;
	return 0;
}

/**
 * btt_copy_to_info2 - restore the backup info block using the main one
 * @a:		the arena_info handle for this arena
 *
 * Called when a corrupted backup info block is detected. Copies the
 * main info block over to the backup location. This is done using
 * mmap + stores, and thus needs a msync.
 */
static int btt_copy_to_info2(struct arena_info *a)
{
	void *ms_align;
	size_t ms_size;

	if (!a->bttc->opts->repair) {
		err(a->bttc, "Arena %d: BTT info2 needs to be restored\n",
			a->num);
		return repair_msg(a->bttc);
	}
	printf("Arena %d: Restoring BTT info2\n", a->num);
	memcpy(a->map.info2, a->map.info, BTT_INFO_SIZE);

	ms_align = (void *)rounddown((u64)a->map.info2, a->bttc->sys_page_size);
	ms_size = max(BTT_INFO_SIZE, a->bttc->sys_page_size);
	if (msync(ms_align, ms_size, MS_SYNC) < 0)
		return -errno;

	return 0;
}

/*
 * btt_map_lookup - given a pre-map Arena Block Address, return the post-map ABA
 * @a:		the arena_info handle for this arena
 * @lba:	the logical block address for which we are performing the lookup
 *
 * This will correctly account for map entries in the 'initial state'
 */
static u32 btt_map_lookup(struct arena_info *a, u32 lba)
{
	u32 raw_mapping;

	raw_mapping = le32_to_cpu(a->map.map[lba]);
	if (raw_mapping & MAP_ENT_NORMAL)
		return raw_mapping & MAP_LBA_MASK;
	else
		return lba;
}

static int btt_map_write(struct arena_info *a, u32 lba, u32 mapping)
{
	void *ms_align;

	if (!a->bttc->opts->repair) {
		err(a->bttc,
			"Arena %d: map[%#x] needs to be updated to %#x\n",
			a->num, lba, mapping);
		return repair_msg(a->bttc);
	}
	info(a->bttc, "Arena %d: Updating map[%#x] to %#x\n", a->num,
		lba, mapping);

	/*
	 * We want to set neither of the Z or E flags, and in the actual
	 * layout, this means setting the bit positions of both to '1' to
	 * indicate a 'normal' map entry
	 */
	mapping |= MAP_ENT_NORMAL;
	a->map.map[lba] = cpu_to_le32(mapping);

	ms_align = (void *)rounddown((u64)&a->map.map[lba],
		a->bttc->sys_page_size);
	if (msync(ms_align, a->bttc->sys_page_size, MS_SYNC) < 0)
		return -errno;

	return 0;
}

static void btt_log_group_read(struct arena_info *a, u32 lane,
			struct log_group *log)
{
	memcpy(log, &a->map.log[lane], LOG_GRP_SIZE);
}

static void btt_log_group_write(struct arena_info *a, u32 lane,
			struct log_group *log)
{
	memcpy(&a->map.log[lane], log, LOG_GRP_SIZE);
}

static u32 log_seq(struct log_group *log, int log_idx)
{
	return le32_to_cpu(log->ent[log_idx].seq);
}

/*
 * This function accepts two log entries, and uses the sequence number to
 * find the 'older' entry. The return value indicates which of the two was
 * the 'old' entry
 */
static int btt_log_get_old(struct arena_info *a, struct log_group *log)
{
	int idx0 = a->log_index[0];
	int idx1 = a->log_index[1];
	int old;

	if (log_seq(log, idx0) == 0) {
		log->ent[idx0].seq = cpu_to_le32(1);
		return 0;
	}

	if (log_seq(log, idx0) < log_seq(log, idx1)) {
		if ((log_seq(log, idx1) - log_seq(log, idx0)) == 1)
			old = 0;
		else
			old = 1;
	} else {
		if ((log_seq(log, idx0) - log_seq(log, idx1)) == 1)
			old = 1;
		else
			old = 0;
	}

	return old;
}

static int btt_log_read(struct arena_info *a, u32 lane, struct log_entry *ent)
{
	int new_ent;
	struct log_group log;

	if (ent == NULL)
		return -EINVAL;
	btt_log_group_read(a, lane, &log);
	new_ent = 1 - btt_log_get_old(a, &log);
	memcpy(ent, &log.ent[a->log_index[new_ent]], LOG_ENT_SIZE);
	return 0;
}

/*
 * Never pass a mmapped buffer to this as it will attempt to write to
 * the buffer, and we want writes to only happened in a controlled fashion.
 * In the non --repair case, even if such a buffer is passed, the write will
 * result in a fault due to the readonly mmap flags.
 */
static int btt_info_verify(struct btt_chk *bttc, struct btt_sb *btt_sb)
{
	if (memcmp(btt_sb->signature, BTT_SIG, BTT_SIG_LEN) != 0)
		return -ENXIO;

	if (!uuid_is_null(btt_sb->parent_uuid))
		if (uuid_compare(bttc->parent_uuid, btt_sb->parent_uuid) != 0)
			return -ENXIO;

	if (!verify_infoblock_checksum((union info_block *) btt_sb))
		return -ENXIO;

	return 0;
}

static int btt_info_read_verify(struct btt_chk *bttc, struct btt_sb *btt_sb,
	u64 off)
{
	int rc;

	rc = btt_read_info(bttc, btt_sb, off);
	if (rc)
		return rc;
	rc = btt_info_verify(bttc, btt_sb);
	if (rc)
		return rc;
	return 0;
}

enum btt_errcodes {
	BTT_OK = 0,
	BTT_LOG_EQL_SEQ = 0x100,
	BTT_LOG_OOB_SEQ,
	BTT_LOG_OOB_LBA,
	BTT_LOG_OOB_OLD,
	BTT_LOG_OOB_NEW,
	BTT_LOG_MAP_ERR,
	BTT_MAP_OOB,
	BTT_BITMAP_ERROR,
	BTT_LOGFIX_ERR,
};

static void btt_xlat_status(struct arena_info *a, int errcode)
{
	switch(errcode) {
	case BTT_OK:
		break;
	case BTT_LOG_EQL_SEQ:
		err(a->bttc,
			"arena %d: found a pair of log entries with the same sequence number\n",
			a->num);
		break;
	case BTT_LOG_OOB_SEQ:
		err(a->bttc,
			"arena %d: found a log entry with an out of bounds sequence number\n",
			a->num);
		break;
	case BTT_LOG_OOB_LBA:
		err(a->bttc,
			"arena %d: found a log entry with an out of bounds LBA\n",
			a->num);
		break;
	case BTT_LOG_OOB_OLD:
		err(a->bttc,
			"arena %d: found a log entry with an out of bounds 'old' mapping\n",
			a->num);
		break;
	case BTT_LOG_OOB_NEW:
		err(a->bttc,
			"arena %d: found a log entry with an out of bounds 'new' mapping\n",
			a->num);
		break;
	case BTT_LOG_MAP_ERR:
		info(a->bttc,
			"arena %d: found a log entry that does not match with a map entry\n",
			a->num);
		break;
	case BTT_MAP_OOB:
		err(a->bttc,
			"arena %d: found a map entry that is out of bounds\n",
			a->num);
		break;
	case BTT_BITMAP_ERROR:
		err(a->bttc,
			"arena %d: bitmap error: internal blocks are incorrectly referenced\n",
			a->num);
		break;
	case BTT_LOGFIX_ERR:
		err(a->bttc,
			"arena %d: rewrite-log error: log may be in an unknown/unrecoverable state\n",
			a->num);
		break;
	default:
		err(a->bttc, "arena %d: unknown error: %d\n",
			a->num, errcode);
	}
}

/* Check that log entries are self consistent */
static int btt_check_log_entries(struct arena_info *a)
{
	int idx0 = a->log_index[0];
	int idx1 = a->log_index[1];
	unsigned int i;
	int rc = 0;

	/*
	 * First, check both 'slots' for sequence numbers being distinct
	 * and in bounds
	 */
	for (i = 0; i < a->nfree; i++) {
		struct log_group *log = &a->map.log[i];

		if (log_seq(log, idx0) == log_seq(log, idx1))
			return BTT_LOG_EQL_SEQ;
		if (log_seq(log, idx0) > 3 || log_seq(log, idx1) > 3)
			return BTT_LOG_OOB_SEQ;
	}
	/*
	 * Next, check only the 'new' slot in each lane for the remaining
	 * fields being in bounds
	 */
	for (i = 0; i < a->nfree; i++) {
		struct log_entry ent;

		rc = btt_log_read(a, i, &ent);
		if (rc)
			return rc;

		if (ent.lba >= a->external_nlba)
			return BTT_LOG_OOB_LBA;
		if (ent.old_map >= a->internal_nlba)
			return BTT_LOG_OOB_OLD;
		if (ent.new_map >= a->internal_nlba)
			return BTT_LOG_OOB_NEW;
	}
	return rc;
}

/* Check that map entries are self consistent */
static int btt_check_map_entries(struct arena_info *a)
{
	unsigned int i;
	u32 mapping;

	for (i = 0; i < a->external_nlba; i++) {
		mapping = btt_map_lookup(a, i);
		if (mapping >= a->internal_nlba)
			return BTT_MAP_OOB;
	}
	return 0;
}

/* Check that each flog entry has the correct corresponding map entry */
static int btt_check_log_map(struct arena_info *a)
{
	unsigned int i;
	u32 mapping;
	int rc = 0, rc_saved = 0;

	for (i = 0; i < a->nfree; i++) {
		struct log_entry ent;

		rc = btt_log_read(a, i, &ent);
		if (rc)
			return rc;
		mapping = btt_map_lookup(a, ent.lba);

		/*
		 * Case where the flog was written, but map couldn't be
		 * updated. The kernel should also be able to detect and
		 * fix this condition.
		 */
		if (ent.new_map != mapping && ent.old_map == mapping) {
			info(a->bttc,
				"arena %d: log[%d].new_map (%#x) doesn't match map[%#x] (%#x)\n",
				a->num, i, ent.new_map, ent.lba, mapping);
			rc = btt_map_write(a, ent.lba, ent.new_map);
			if (rc)
				rc_saved = rc;
		}
	}
	return rc_saved ? BTT_LOG_MAP_ERR : 0;
}

static int btt_check_info2(struct arena_info *a)
{
	/*
	 * Repair info2 if needed. The main info-block can be trusted
	 * as it has been verified during arena discovery
	 */
	if(memcmp(a->map.info2, a->map.info, BTT_INFO_SIZE))
		return btt_copy_to_info2(a);
	return 0;
}

/*
 * This will create a bitmap where each bit corresponds to an internal
 * 'block'. Between the BTT map and flog (representing 'free' blocks),
 * every single internal block must be represented exactly once. This
 * check will detect cases where either one or more blocks are never
 * referenced, or if a block is referenced more than once.
 */
static int btt_check_bitmap(struct arena_info *a)
{
	unsigned long *bm;
	u32 i, btt_mapping;
	int rc = BTT_BITMAP_ERROR;

	bm = bitmap_alloc(a->internal_nlba);
	if (bm == NULL)
		return -ENOMEM;

	/* map 'external_nlba' number of map entries */
	for (i = 0; i < a->external_nlba; i++) {
		btt_mapping = btt_map_lookup(a, i);
		if (test_bit(btt_mapping, bm)) {
			info(a->bttc,
				"arena %d: internal block %#x is referenced by two map entries\n",
				a->num, btt_mapping);
			goto out;
		}
		bitmap_set(bm, btt_mapping, 1);
	}

	/* map 'nfree' number of flog entries */
	for (i = 0; i < a->nfree; i++) {
		struct log_entry ent;

		rc = btt_log_read(a, i, &ent);
		if (rc)
			goto out;
		if (test_bit(ent.old_map, bm)) {
			info(a->bttc,
				"arena %d: internal block %#x is referenced by two map/log entries\n",
				a->num, ent.old_map);
			rc = BTT_BITMAP_ERROR;
			goto out;
		}
		bitmap_set(bm, ent.old_map, 1);
	}

	/* check that the bitmap is full */
	if (!bitmap_full(bm, a->internal_nlba))
		rc = BTT_BITMAP_ERROR;
 out:
	free(bm);
	return rc;
}

static int btt_rewrite_log(struct arena_info *a)
{
	struct log_group log;
	int rc;
	u32 i;

	info(a->bttc, "arena %d: rewriting log\n", a->num);
	/*
	 * To rewrite the log, we implicitly use the 'new' padding scheme of
	 * (0, 1) but resetting the log to a completely initial state (i.e.
	 * slot-0 contains a made-up entry containing the 'free' block from
	 * the existing current log entry, and a sequence number of '1'. All
	 * other slots are zeroed.
	 *
	 * This way of rewriting the log is the most flexible as it can be
	 * (ab)used to convert a new padding format back to the old one.
	 * Since it only recreates slot-0, which is common between both
	 * existing formats, an older kernel will simply initialize the free
	 * list using those slot-0 entries, and run with it as though slot-2
	 * is the other valid slot.
	 */
	memset(&log, 0, LOG_GRP_SIZE);
	for (i = 0; i < a->nfree; i++) {
		struct log_entry ent;

		rc = btt_log_read(a, i, &ent);
		if (rc)
			return BTT_LOGFIX_ERR;

		log.ent[0].lba = ent.lba;
		log.ent[0].old_map = ent.old_map;
		log.ent[0].new_map = ent.new_map;
		log.ent[0].seq = 1;
		btt_log_group_write(a, i, &log);
	}
	return 0;
}

static int btt_check_arenas(struct btt_chk *bttc)
{
	struct arena_info *a = NULL;
	int i, rc;

	for(i = 0; i < bttc->num_arenas; i++) {
		info(bttc, "checking arena %d\n", i);
		a = &bttc->arena[i];
		rc = btt_check_log_entries(a);
		if (rc)
			break;
		rc = btt_check_map_entries(a);
		if (rc)
			break;
		rc = btt_check_log_map(a);
		if (rc)
			break;
		rc = btt_check_info2(a);
		if (rc)
			break;
		/*
		 * bitmap test has to be after check_log_map so that any
		 * pending log updates have been performed. Otherwise the
		 * bitmap test may result in a false positive
		 */
		rc = btt_check_bitmap(a);
		if (rc)
			break;

		if (bttc->opts->logfix) {
			rc = btt_rewrite_log(a);
			if (rc)
				break;
		}
	}

	if (a && rc != BTT_OK) {
		btt_xlat_status(a, rc);
		return -ENXIO;
	}
	return 0;
}

/*
 * This copies over information from the info block to the arena_info struct.
 * The main difference is that all the offsets (infooff, mapoff etc) were
 * relative to the arena in the info block, but in arena_info, we use
 * arena_off to make these offsets absolute, i.e. relative to the start of
 * the raw namespace.
 */
static int btt_parse_meta(struct arena_info *arena, struct btt_sb *btt_sb,
				u64 arena_off)
{
	arena->internal_nlba = le32_to_cpu(btt_sb->internal_nlba);
	arena->internal_lbasize = le32_to_cpu(btt_sb->internal_lbasize);
	arena->external_nlba = le32_to_cpu(btt_sb->external_nlba);
	arena->external_lbasize = le32_to_cpu(btt_sb->external_lbasize);
	arena->nfree = le32_to_cpu(btt_sb->nfree);

	if (arena->internal_nlba - arena->external_nlba != arena->nfree)
		return -ENXIO;
	if (arena->internal_lbasize != arena->external_lbasize)
		return -ENXIO;

	arena->version_major = le16_to_cpu(btt_sb->version_major);
	arena->version_minor = le16_to_cpu(btt_sb->version_minor);

	arena->nextoff = (btt_sb->nextoff == 0) ? 0 : (arena_off +
			le64_to_cpu(btt_sb->nextoff));
	arena->infooff = arena_off;
	arena->dataoff = arena_off + le64_to_cpu(btt_sb->dataoff);
	arena->mapoff = arena_off + le64_to_cpu(btt_sb->mapoff);
	arena->logoff = arena_off + le64_to_cpu(btt_sb->logoff);
	arena->info2off = arena_off + le64_to_cpu(btt_sb->info2off);

	arena->size = (le64_to_cpu(btt_sb->nextoff) > 0)
		? (le64_to_cpu(btt_sb->nextoff))
		: (arena->info2off - arena->infooff + BTT_INFO_SIZE);

	arena->flags = le32_to_cpu(btt_sb->flags);
	if (btt_sb->flags & IB_FLAG_ERROR_MASK) {
		err(arena->bttc, "Info block error flag is set, aborting\n");
		return -ENXIO;
	}
	return 0;
}

static bool ent_is_padding(struct log_entry *ent)
{
	return (ent->lba == 0) && (ent->old_map == 0) && (ent->new_map == 0)
		&& (ent->seq == 0);
}

/*
 * Detecting valid log indices: We read a log group, and iterate over its
 * four slots. We expect that a padding slot will be all-zeroes, and use this
 * to detect a padding slot vs. an actual entry.
 *
 * If a log_group is in the initial state, i.e. hasn't been used since the
 * creation of this BTT layout, it will have three of the four slots with
 * zeroes. We skip over these log_groups for the detection of log_index. If
 * all log_groups are in the initial state (i.e. the BTT has never been
 * written to), it is safe to assume the 'new format' of log entries in slots
 * (0, 1).
 */
static int log_set_indices(struct arena_info *arena)
{
	bool idx_set = false, initial_state = true;
	int log_index[2] = {-1, -1};
	struct log_group log;
	int j, next_idx = 0;
	u32 pad_count = 0;
	u32 i;

	for (i = 0; i < arena->nfree; i++) {
		btt_log_group_read(arena, i, &log);

		for (j = 0; j < 4; j++) {
			if (!idx_set) {
				if (ent_is_padding(&log.ent[j])) {
					pad_count++;
					continue;
				} else {
					/* Skip if index has been recorded */
					if ((next_idx == 1) &&
						(j == log_index[0]))
						continue;
					/* valid entry, record index */
					log_index[next_idx] = j;
					next_idx++;
				}
				if (next_idx == 2) {
					/* two valid entries found */
					idx_set = true;
				} else if (next_idx > 2) {
					/* too many valid indices */
					return -ENXIO;
				}
			} else {
				/*
				 * once the indices have been set, just verify
				 * that all subsequent log groups are either in
				 * their initial state or follow the same
				 * indices.
				 */
				if (j == log_index[0]) {
					/* entry must be 'valid' */
					if (ent_is_padding(&log.ent[j]))
						return -ENXIO;
				} else if (j == log_index[1]) {
					;
					/*
					 * log_index[1] can be padding if the
					 * lane never got used and it is still
					 * in the initial state (three 'padding'
					 * entries)
					 */
				} else {
					/* entry must be invalid (padding) */
					if (!ent_is_padding(&log.ent[j]))
						return -ENXIO;
				}
			}
		}
		/*
		 * If any of the log_groups have more than one valid,
		 * non-padding entry, then the we are no longer in the
		 * initial_state
		 */
		if (pad_count < 3)
			initial_state = false;
		pad_count = 0;
	}

	if (!initial_state && !idx_set)
		return -ENXIO;

	/*
	 * If all the entries in the log were in the initial state,
	 * assume new padding scheme
	 */
	if (initial_state)
		log_index[1] = 1;

	/*
	 * Only allow the known permutations of log/padding indices,
	 * i.e. (0, 1), and (0, 2)
	 */
	if ((log_index[0] == 0) && ((log_index[1] == 1) || (log_index[1] == 2)))
		; /* known index possibilities */
	else {
		err(arena->bttc, "Found an unknown padding scheme\n");
		return -ENXIO;
	}

	arena->log_index[0] = log_index[0];
	arena->log_index[1] = log_index[1];
	info(arena->bttc, "arena[%d]: log_index_0 = %d\n",
		arena->num, log_index[0]);
	info(arena->bttc, "arena[%d]: log_index_1 = %d\n",
		arena->num, log_index[1]);
	return 0;
}

static int btt_discover_arenas(struct btt_chk *bttc)
{
	int ret = 0;
	struct arena_info *arena;
	struct btt_sb *btt_sb;
	size_t remaining = bttc->rawsize;
	size_t cur_off = bttc->start_off;
	u64 cur_nlba = 0;
	int  i = 0;

	btt_sb = calloc(1, sizeof(*btt_sb));
	if (!btt_sb)
		return -ENOMEM;

	while (remaining) {
		/* Alloc memory for arena */
		arena = realloc(bttc->arena, (i + 1) * sizeof(*arena));
		if (!arena) {
			ret = -ENOMEM;
			goto out;
		} else {
			bttc->arena = arena;
			arena = &bttc->arena[i];
			/* zero the new memory */
			memset(arena, 0, sizeof(*arena));
		}

		arena->infooff = cur_off;
		ret = btt_read_info(bttc, btt_sb, cur_off);
		if (ret)
			goto out;

		if (btt_info_verify(bttc, btt_sb) != 0) {
			u64 offset;

			/* Try to find the backup info block */
			if (remaining <= ARENA_MAX_SIZE)
				offset = rounddown(bttc->rawsize, SZ_4K) -
					BTT_INFO_SIZE;
			else
				offset = cur_off + ARENA_MAX_SIZE -
					BTT_INFO_SIZE;

			info(bttc,
				"Arena %d: Attempting recover info-block using info2\n", i);
			ret = btt_read_info(bttc, btt_sb, offset);
			if (ret) {
				err(bttc, "Unable to read backup info block (offset %#lx)\n",
					offset);
				goto out;
			}
			ret = btt_info_verify(bttc, btt_sb);
			if (ret) {
				err(bttc, "Backup info block (offset %#lx) verification failed\n",
					offset);
				goto out;
			}
			ret = btt_write_info(bttc, btt_sb, cur_off);
			if (ret) {
				err(bttc, "Restoration of the info block failed: %s (%d)\n",
					strerror(abs(ret)), ret);
				goto out;
			}
		}

		arena->num = i;
		arena->bttc = bttc;
		arena->external_lba_start = cur_nlba;
		ret = btt_parse_meta(arena, btt_sb, cur_off);
		if (ret) {
			err(bttc, "Problem parsing arena[%d] metadata\n", i);
			goto out;
		}
		remaining -= arena->size;
		cur_off += arena->size;
		cur_nlba += arena->external_nlba;
		i++;

		if (arena->nextoff == 0)
			break;
	}
	bttc->num_arenas = i;
	bttc->nlba = cur_nlba;
	info(bttc, "found %d BTT arena%s\n", bttc->num_arenas,
		(bttc->num_arenas > 1) ? "s" : "");
	free(btt_sb);
	return ret;

 out:
	free(bttc->arena);
	free(btt_sb);
	return ret;
}

/*
 * Wrap call to mmap(2) to work with btt device offsets that are not aligned
 * to system page boundary. It works by rounding down the requested offset
 * to sys_page_size when calling mmap(2) and then returning a fixed-up pointer
 * to the correct offset in the mmaped region.
 */
static void *btt_mmap(struct btt_chk *bttc, size_t length, off_t offset)
{
	off_t page_offset;
	int prot_flags;
	uint8_t *addr;

	if (!bttc->opts->repair)
		prot_flags = PROT_READ;
	else
		prot_flags = PROT_READ|PROT_WRITE;

	/* Calculate the page_offset from the system page boundary */
	page_offset = offset - rounddown(offset, bttc->sys_page_size);

	/* Update the offset and length with the page_offset calculated above */
	offset -= page_offset;
	length += page_offset;

	addr = mmap(NULL, length, prot_flags, MAP_SHARED, bttc->fd, offset);

	/* If needed fixup the return pointer to correct offset requested */
	if (addr != MAP_FAILED)
		addr += page_offset;

	dbg(bttc, "addr = %p, length = %#lx, offset = %#lx, page_offset = %#lx\n",
	    (void *) addr, length, offset, page_offset);

	return addr == MAP_FAILED ? NULL : addr;
}

static void btt_unmap(struct btt_chk *bttc, void *ptr, size_t length)
{
	off_t page_offset;
	uintptr_t addr = (uintptr_t) ptr;

	/* Calculate the page_offset from system page boundary */
	page_offset = addr - rounddown(addr, bttc->sys_page_size);

	addr -= page_offset;
	length += page_offset;

	munmap((void *) addr, length);
	dbg(bttc, "addr = %p, length = %#lx, page_offset = %#lx\n",
	    (void *) addr, length, page_offset);
}

static int btt_create_mappings(struct btt_chk *bttc)
{
	struct arena_info *a;
	int i;

	for (i = 0; i < bttc->num_arenas; i++) {
		a = &bttc->arena[i];
		a->map.info_len = BTT_INFO_SIZE;
		a->map.info = btt_mmap(bttc, a->map.info_len, a->infooff);
		if (!a->map.info) {
			err(bttc, "mmap arena[%d].info [sz = %#lx, off = %#lx] failed: %s\n",
				i, a->map.info_len, a->infooff, strerror(errno));
			return -errno;
		}

		a->map.data_len = a->mapoff - a->dataoff;
		a->map.data = btt_mmap(bttc, a->map.data_len, a->dataoff);
		if (!a->map.data) {
			err(bttc, "mmap arena[%d].data [sz = %#lx, off = %#lx] failed: %s\n",
				i, a->map.data_len, a->dataoff, strerror(errno));
			return -errno;
		}

		a->map.map_len = a->logoff - a->mapoff;
		a->map.map = btt_mmap(bttc, a->map.map_len, a->mapoff);
		if (!a->map.map) {
			err(bttc, "mmap arena[%d].map [sz = %#lx, off = %#lx] failed: %s\n",
				i, a->map.map_len, a->mapoff, strerror(errno));
			return -errno;
		}

		a->map.log_len = a->info2off - a->logoff;
		a->map.log = btt_mmap(bttc, a->map.log_len, a->logoff);
		if (!a->map.log) {
			err(bttc, "mmap arena[%d].log [sz = %#lx, off = %#lx] failed: %s\n",
				i, a->map.log_len, a->logoff, strerror(errno));
			return -errno;
		}

		a->map.info2_len = BTT_INFO_SIZE;
		a->map.info2 = btt_mmap(bttc, a->map.info2_len, a->info2off);
		if (!a->map.info2) {
			err(bttc, "mmap arena[%d].info2 [sz = %#lx, off = %#lx] failed: %s\n",
				i, a->map.info2_len, a->info2off, strerror(errno));
			return -errno;
		}
	}

	return 0;
}

static void btt_remove_mappings(struct btt_chk *bttc)
{
	struct arena_info *a;
	int i;

	for (i = 0; i < bttc->num_arenas; i++) {
		a = &bttc->arena[i];
		if (a->map.info)
			btt_unmap(bttc, a->map.info, a->map.info_len);
		if (a->map.data)
			btt_unmap(bttc, a->map.data, a->map.data_len);
		if (a->map.map)
			btt_unmap(bttc, a->map.map, a->map.map_len);
		if (a->map.log)
			btt_unmap(bttc, a->map.log, a->map.log_len);
		if (a->map.info2)
			btt_unmap(bttc, a->map.info2, a->map.info2_len);
	}
}

static int btt_sb_get_expected_offset(struct btt_sb *btt_sb)
{
	u16 version_major, version_minor;

	version_major = le16_to_cpu(btt_sb->version_major);
	version_minor = le16_to_cpu(btt_sb->version_minor);

	if (version_major == 1 && version_minor == 1)
		return BTT1_START_OFFSET;
	else if (version_major == 2 && version_minor == 0)
		return BTT2_START_OFFSET;
	else
		return -ENXIO;
}

static int __btt_recover_first_sb(struct btt_chk *bttc, int off)
{
	int rc, est_arenas = 0;
	u64 offset, remaining;
	struct btt_sb *btt_sb;

	/* Estimate the number of arenas */
	remaining = bttc->rawsize - off;
	while (remaining) {
		if (remaining < ARENA_MIN_SIZE && est_arenas == 0)
			return -EINVAL;
		if (remaining > ARENA_MAX_SIZE) {
			/* full-size arena */
			remaining -= ARENA_MAX_SIZE;
			est_arenas++;
			continue;
		}
		if (remaining < ARENA_MIN_SIZE) {
			/* 'remaining' was too small for another arena */
			break;
		} else {
			/* last, short arena */
			remaining = 0;
			est_arenas++;
			break;
		}
	}
	info(bttc, "estimated arenas: %d, remaining bytes: %#lx\n",
		est_arenas, remaining);

	btt_sb = malloc(2 * sizeof(*btt_sb));
	if (btt_sb == NULL)
		return -ENOMEM;
	/* Read the original first info block into btt_sb[0] */
	rc = btt_read_info(bttc, &btt_sb[0], off);
	if (rc)
		goto out;

	/* Attepmt 1: try recovery from expected end of the first arena */
	if (est_arenas == 1)
		offset = rounddown(bttc->rawsize - remaining, SZ_4K) -
			BTT_INFO_SIZE;
	else
		offset = ARENA_MAX_SIZE - BTT_INFO_SIZE + off;

	info(bttc, "Attempting recover info-block from end-of-arena offset %#lx\n",
		offset);
	rc = btt_info_read_verify(bttc, &btt_sb[1], offset);
	if (rc == 0) {
		int expected_offset = btt_sb_get_expected_offset(&btt_sb[1]);

		/*
		 * The fact that the btt_sb is self-consistent doesn't tell us
		 * what BTT version it was, if restoring from the end of the
		 * arena. (i.e. a consistent sb may be found for any valid
		 * start offset). Use the version information in the sb to
		 * determine what the expected start offset is.
		 */
		if ((expected_offset < 0) || (expected_offset != off)) {
			rc = -ENXIO;
			goto out;
		}
		rc = btt_write_info(bttc, &btt_sb[1], off);
		goto out;
	}

	/*
	 * Attempt 2: From the very end of 'rawsize', try to copy the fields
	 * that are constant in every arena (only valid when multiple arenas
	 * are present)
	 */
	if (est_arenas > 1) {
		offset = rounddown(bttc->rawsize - remaining, SZ_4K) -
			BTT_INFO_SIZE;
		info(bttc, "Attempting to recover info-block from end offset %#lx\n",
			offset);
		rc = btt_info_read_verify(bttc, &btt_sb[1], offset);
		if (rc)
			goto out;
		/* copy over the arena0 specific fields from btt_sb[0] */
		btt_sb[1].flags = btt_sb[0].flags;
		btt_sb[1].external_nlba = btt_sb[0].external_nlba;
		btt_sb[1].internal_nlba = btt_sb[0].internal_nlba;
		btt_sb[1].nextoff = btt_sb[0].nextoff;
		btt_sb[1].dataoff = btt_sb[0].dataoff;
		btt_sb[1].mapoff = btt_sb[0].mapoff;
		btt_sb[1].logoff = btt_sb[0].logoff;
		btt_sb[1].info2off = btt_sb[0].info2off;
		btt_sb[1].checksum = btt_sb[0].checksum;
		rc = btt_info_verify(bttc, &btt_sb[1]);
		if (rc == 0) {
			rc = btt_write_info(bttc, &btt_sb[1], off);
			goto out;
		}
	}

	/*
	 * Attempt 3: use info2off as-is, and check if we find a valid info
	 * block at that location.
	 */
	offset = le32_to_cpu(btt_sb[0].info2off);
	if (offset > min(bttc->rawsize - BTT_INFO_SIZE,
			ARENA_MAX_SIZE - BTT_INFO_SIZE + off)) {
		rc = -ENXIO;
		goto out;
	}
	if (offset) {
		info(bttc, "Attempting to recover info-block from info2 offset %#lx\n",
			offset);
		rc = btt_info_read_verify(bttc, &btt_sb[1],
			offset + off);
		if (rc == 0) {
			rc = btt_write_info(bttc, &btt_sb[1], off);
			goto out;
		}
	} else
		rc = -ENXIO;
 out:
	free(btt_sb);
	return rc;
}

static int btt_recover_first_sb(struct btt_chk *bttc)
{
	int offsets[BTT_NUM_OFFSETS] = {
		BTT1_START_OFFSET,
		BTT2_START_OFFSET,
	};
	int i, rc;

	for (i = 0; i < BTT_NUM_OFFSETS; i++) {
		rc = __btt_recover_first_sb(bttc, offsets[i]);
		if (rc == 0) {
			bttc->start_off = offsets[i];
			return rc;
		}
	}

	return rc;
}

int namespace_check(struct ndctl_namespace *ndns, bool verbose, bool force,
		bool repair, bool logfix)
{
	const char *devname = ndctl_namespace_get_devname(ndns);
	struct check_opts __opts = {
		.verbose = verbose,
		.force = force,
		.repair = repair,
		.logfix = logfix,
	}, *opts = &__opts;
	int raw_mode, rc, disabled_flag = 0, open_flags;
	struct btt_sb *btt_sb;
	struct btt_chk *bttc;
	struct sigaction act;
	char path[50];
	int i;

	bttc = calloc(1, sizeof(*bttc));
	if (bttc == NULL)
		return -ENOMEM;

	log_init(&bttc->ctx, devname, "NDCTL_CHECK_NAMESPACE");
	if (opts->verbose)
		bttc->ctx.log_priority = LOG_DEBUG;

	memset(&act, 0, sizeof(act));
	act.sa_sigaction = sigbus_hdl;
	act.sa_flags = SA_SIGINFO;

	if (sigaction(SIGBUS, &act, 0)) {
		err(bttc, "Unable to set sigaction\n");
		rc = -errno;
		goto out_bttc;
	}

	if (opts->logfix) {
		if (!opts->repair) {
			err(bttc, "--rewrite-log also requires --repair\n");
			rc = -EINVAL;
			goto out_bttc;
		}
		info(bttc,
			"WARNING: interruption may cause unrecoverable metadata corruption\n");
	}

	bttc->opts = opts;
	bttc->sys_page_size = sysconf(_SC_PAGESIZE);
	bttc->rawsize = ndctl_namespace_get_size(ndns);
	ndctl_namespace_get_uuid(ndns, bttc->parent_uuid);

	info(bttc, "checking %s\n", devname);
	if (ndctl_namespace_is_active(ndns)) {
		if (opts->force) {
			rc = ndctl_namespace_disable_safe(ndns);
			if (rc)
				goto out_bttc;
			disabled_flag = 1;
		} else {
			err(bttc, "%s: check aborted, namespace online\n",
				devname);
			rc = -EBUSY;
			goto out_bttc;
		}
	}

	/* In typical usage, the current raw_mode should be false. */
	raw_mode = ndctl_namespace_get_raw_mode(ndns);

	/*
	 * Putting the namespace into raw mode will allow us to access
	 * the btt metadata.
	 */
	rc = ndctl_namespace_set_raw_mode(ndns, 1);
	if (rc < 0) {
		err(bttc, "%s: failed to set the raw mode flag: %s (%d)\n",
			devname, strerror(abs(rc)), rc);
		goto out_ns;
	}
	/*
	 * Now enable the namespace.  This will result in a pmem device
	 * node showing up in /dev that is in raw mode.
	 */
	rc = ndctl_namespace_enable(ndns);
	if (rc != 0) {
		err(bttc, "%s: failed to enable in raw mode: %s (%d)\n",
			devname, strerror(abs(rc)), rc);
		goto out_ns;
	}

	sprintf(path, "/dev/%s", ndctl_namespace_get_block_device(ndns));
	bttc->path = path;

	btt_sb = malloc(sizeof(*btt_sb));
	if (btt_sb == NULL) {
		rc = -ENOMEM;
		goto out_ns;
	}

	if (!bttc->opts->repair)
		open_flags = O_RDONLY|O_EXCL;
	else
		open_flags = O_RDWR|O_EXCL;

	bttc->fd = open(bttc->path, open_flags);
	if (bttc->fd < 0) {
		err(bttc, "unable to open %s: %s\n",
			bttc->path, strerror(errno));
		rc = -errno;
		goto out_sb;
	}

	/*
	 * This is where we jump to if we receive a SIGBUS, prior to doing any
	 * mmaped reads, and can safely abort
	 */
	if (sigsetjmp(sj_env, 1)) {
		err(bttc, "Received a SIGBUS\n");
		err(bttc,
			"Metadata corruption found, recovery is not possible\n");
		rc = -EFAULT;
		goto out_close;
	}

	/* Try reading a BTT1 info block first */
	rc = btt_info_read_verify(bttc, btt_sb, BTT1_START_OFFSET);
	if (rc == 0)
		bttc->start_off = BTT1_START_OFFSET;
	if (rc) {
		/* Try reading a BTT2 info block */
		rc = btt_info_read_verify(bttc, btt_sb, BTT2_START_OFFSET);
		if (rc == 0)
			bttc->start_off = BTT2_START_OFFSET;
		if (rc) {
			rc = btt_recover_first_sb(bttc);
			if (rc) {
				err(bttc, "Unable to recover any BTT info blocks\n");
				err(bttc,
					"This may not be a sector mode namespace\n");
				goto out_close;
			}
			/*
			 * btt_recover_first_sb will have set bttc->start_off
			 * based on the version it found
			 */
			rc = btt_info_read_verify(bttc, btt_sb, bttc->start_off);
			if (rc)
				goto out_close;
		}
	}

	rc = btt_discover_arenas(bttc);
	if (rc)
		goto out_close;

	rc = btt_create_mappings(bttc);
	if (rc)
		goto out_close;

	for (i = 0; i < bttc->num_arenas; i++) {
		rc = log_set_indices(&bttc->arena[i]);
		if (rc) {
			err(bttc,
				"Unable to deduce log/padding indices\n");
			goto out_close;
		}
	}

	rc = btt_check_arenas(bttc);

	btt_remove_mappings(bttc);
 out_close:
	close(bttc->fd);
 out_sb:
	free(btt_sb);
 out_ns:
	ndctl_namespace_set_raw_mode(ndns, raw_mode);
	ndctl_namespace_disable_invalidate(ndns);
	if (disabled_flag)
		if(ndctl_namespace_enable(ndns) < 0)
			err(bttc, "%s: failed to re-enable namespace\n",
				devname);
 out_bttc:
	free(bttc);
	return rc;
}
