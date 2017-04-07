/*
 * Copyright (c) 2016, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#ifndef _CHECK_H
#define _CHECK_H

#include <util/log.h>
#include <ccan/endian/endian.h>
#include <ccan/short_types/short_types.h>

#define BTT_SIG_LEN 16
#define BTT_SIG "BTT_ARENA_INFO\0"
#define MAP_TRIM_SHIFT 31
#define MAP_ERR_SHIFT 30
#define MAP_LBA_MASK (~((1 << MAP_TRIM_SHIFT) | (1 << MAP_ERR_SHIFT)))
#define MAP_ENT_NORMAL 0xC0000000
#define ARENA_MIN_SIZE (1UL << 24)	/* 16 MB */
#define ARENA_MAX_SIZE (1ULL << 39)	/* 512 GB */
#define BTT_INFO_SIZE 4096
#define BTT_START_OFFSET 4096
#define IB_FLAG_ERROR_MASK 0x00000001

struct log_entry {
	le32 lba;
	le32 old_map;
	le32 new_map;
	le32 seq;
	le64 padding[2];
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
	struct log_entry *log;
	size_t log_len;
	struct btt_sb *info2;
	size_t info2_len;
};

struct check_opts {
	bool verbose;
	bool force;
	bool repair;
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
};

int namespace_check(struct ndctl_namespace *ndns, struct check_opts *opts);

#endif
