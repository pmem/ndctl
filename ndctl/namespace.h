/* SPDX-License-Identifier: LGPL-2.1 */
/* Copyright (C) 2014-2020, Intel Corporation. All rights reserved. */
#ifndef __NDCTL_NAMESPACE_H__
#define __NDCTL_NAMESPACE_H__
#include <sys/types.h>
#include <util/util.h>
#include <util/size.h>
#include <util/fletcher.h>
#include <ccan/endian/endian.h>
#include <ccan/short_types/short_types.h>

enum {
	NSINDEX_SIG_LEN = 16,
	NSINDEX_ALIGN = 256,
	NSINDEX_SEQ_MASK = 0x3,
	NSLABEL_UUID_LEN = 16,
	NSLABEL_NAMESPACE_MIN_SIZE = SZ_16M,
	NSLABEL_NAME_LEN = 64,
};

/**
 * struct namespace_index - label set superblock
 * @sig: NAMESPACE_INDEX\0
 * @flags: placeholder
 * @seq: sequence number for this index
 * @myoff: offset of this index in label area
 * @mysize: size of this index struct
 * @otheroff: offset of other index
 * @labeloff: offset of first label slot
 * @nslot: total number of label slots
 * @major: label area major version
 * @minor: label area minor version
 * @checksum: fletcher64 of all fields
 * @free[0]: bitmap, nlabel bits
 *
 * The size of free[] is rounded up so the total struct size is a
 * multiple of NSINDEX_ALIGN bytes.  Any bits this allocates beyond
 * nlabel bits must be zero.
 */
struct namespace_index {
	char sig[NSINDEX_SIG_LEN];
	u8 flags[3];
	u8 labelsize;
	le32 seq;
	le64 myoff;
	le64 mysize;
	le64 otheroff;
	le64 labeloff;
	le32 nslot;
	le16 major;
	le16 minor;
	le64 checksum;
	char free[0];
};

/**
 * struct namespace_label - namespace superblock
 * @uuid: UUID per RFC 4122
 * @name: optional name (NULL-terminated)
 * @flags: see NSLABEL_FLAG_*
 * @nlabel: num labels to describe this ns
 * @position: labels position in set
 * @isetcookie: interleave set cookie
 * @lbasize: LBA size in bytes or 0 for pmem
 * @dpa: DPA of NVM range on this DIMM
 * @rawsize: size of namespace
 * @slot: slot of this label in label area
 */
struct namespace_label {
	char uuid[NSLABEL_UUID_LEN];
	char name[NSLABEL_NAME_LEN];
	le32 flags;
	le16 nlabel;
	le16 position;
	le64 isetcookie;
	le64 lbasize;
	le64 dpa;
	le64 rawsize;
	le32 slot;
	/*
	 * Accessing fields past this point should be gated by a
	 * namespace_label_has() check.
	 */
	u8 align;
	u8 reserved[3];
	char type_guid[NSLABEL_UUID_LEN];
	char abstraction_guid[NSLABEL_UUID_LEN];
	u8 reserved2[88];
	le64 checksum;
};

#define BTT_SIG_LEN 16
#define BTT_SIG "BTT_ARENA_INFO\0"
#define MAP_TRIM_SHIFT 31
#define MAP_ERR_SHIFT 30
#define MAP_LBA_MASK (~((1 << MAP_TRIM_SHIFT) | (1 << MAP_ERR_SHIFT)))
#define MAP_ENT_NORMAL 0xC0000000
#define ARENA_MIN_SIZE (1UL << 24)	/* 16 MB */
#define ARENA_MAX_SIZE (1ULL << 39)	/* 512 GB */
#define BTT_INFO_SIZE 4096
#define IB_FLAG_ERROR_MASK 0x00000001
#define LOG_GRP_SIZE sizeof(struct log_group)
#define LOG_ENT_SIZE sizeof(struct log_entry)

#define BTT_NUM_OFFSETS 2
#define BTT1_START_OFFSET 4096
#define BTT2_START_OFFSET 0

struct log_entry {
	le32 lba;
	le32 old_map;
	le32 new_map;
	le32 seq;
};

/*
 * A log group represents one log 'lane', and consists of four log entries.
 * Two of the four entries are valid entries, and the remaining two are
 * padding. Due to an old bug in the padding location, we need to perform a
 * test to determine the padding scheme being used, and use that scheme
 * thereafter.
 *
 * In kernels prior to 4.15, 'log group' would have actual log entries at
 * indices (0, 2) and padding at indices (1, 3), where as the correct/updated
 * format has log entries at indices (0, 1) and padding at indices (2, 3).
 *
 * Old (pre 4.15) format:
 * +-----------------+-----------------+
 * |      ent[0]     |      ent[1]     |
 * |       16B       |       16B       |
 * | lba/old/new/seq |       pad       |
 * +-----------------------------------+
 * |      ent[2]     |      ent[3]     |
 * |       16B       |       16B       |
 * | lba/old/new/seq |       pad       |
 * +-----------------+-----------------+
 *
 * New format:
 * +-----------------+-----------------+
 * |      ent[0]     |      ent[1]     |
 * |       16B       |       16B       |
 * | lba/old/new/seq | lba/old/new/seq |
 * +-----------------------------------+
 * |      ent[2]     |      ent[3]     |
 * |       16B       |       16B       |
 * |       pad       |       pad       |
 * +-----------------+-----------------+
 *
 * We detect during start-up which format is in use, and set
 * arena->log_index[(0, 1)] with the detected format.
 */

struct log_group {
	struct log_entry ent[4];
};

struct btt_sb {
	u8 signature[BTT_SIG_LEN];
	u8 uuid[16];
	u8 parent_uuid[16];
	le32 flags;
	le16 version_major;
	le16 version_minor;
	le32 external_lbasize;
	le32 external_nlba;
	le32 internal_lbasize;
	le32 internal_nlba;
	le32 nfree;
	le32 infosize;
	le64 nextoff;
	le64 dataoff;
	le64 mapoff;
	le64 logoff;
	le64 info2off;
	u8 padding[3968];
	le64 checksum;
};

struct free_entry {
	u32 block;
	u8 sub;
	u8 seq;
};

struct arena_map {
	struct btt_sb *info;
	size_t info_len;
	void *data;
	size_t data_len;
	u32 *map;
	size_t map_len;
	struct log_group *log;
	size_t log_len;
	struct btt_sb *info2;
	size_t info2_len;
};

#define PFN_SIG_LEN 16
#define PFN_SIG "NVDIMM_PFN_INFO\0"
#define DAX_SIG "NVDIMM_DAX_INFO\0"

enum pfn_mode {
	PFN_MODE_NONE,
	PFN_MODE_RAM,
	PFN_MODE_PMEM,
};

struct pfn_sb {
	u8 signature[PFN_SIG_LEN];
	u8 uuid[16];
	u8 parent_uuid[16];
	le32 flags;
	le16 version_major;
	le16 version_minor;
	le64 dataoff; /* relative to namespace_base + start_pad */
	le64 npfns;
	le32 mode;
	/* minor-version-1 additions for section alignment */
	le32 start_pad;
	le32 end_trunc;
	/* minor-version-2 record the base alignment of the mapping */
	le32 align;
	/* minor-version-3 guarantee the padding and flags are zero */
	/* minor-version-4 record the page size and struct page size */
	le32 page_size;
	le16 page_struct_size;
	u8 padding[3994];
	le64 checksum;
};

union info_block {
	struct pfn_sb pfn_sb;
	struct btt_sb btt_sb;
};

static inline bool verify_infoblock_checksum(union info_block *sb)
{
	uint64_t sum;
	le64 sum_save;

	BUILD_BUG_ON(sizeof(union info_block) != SZ_4K);

	/* all infoblocks share the btt_sb layout for checksum */
	sum_save = sb->btt_sb.checksum;
	sb->btt_sb.checksum = 0;
	sum = fletcher64(&sb->btt_sb, sizeof(*sb), 1);
	if (sum != sum_save)
		return false;
	/* restore the checksum in the buffer */
	sb->btt_sb.checksum = sum_save;

	return true;
}


#endif /* __NDCTL_NAMESPACE_H__ */
