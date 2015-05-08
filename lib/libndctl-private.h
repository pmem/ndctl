/*
 * libndctl: helper library for the nd (nvdimm, nfit-defined, persistent
 *           memory, ...) sub-system.
 *
 * Copyright (c) 2014, Intel Corporation.
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
#ifndef _LIBNDCTL_PRIVATE_H_
#define _LIBNDCTL_PRIVATE_H_

#include <stdbool.h>
#include <syslog.h>
#include <libudev.h>
#include <libkmod.h>
#include <uuid/uuid.h>
#ifdef HAVE_NDCTL_H
#include <linux/ndctl.h>
#else
#include <ndctl.h>
#endif
#include <ndctl/libndctl.h>
#include <ccan/endian/endian.h>
#include <ccan/short_types/short_types.h>

#define SZ_16M 0x01000000

enum {
	NSINDEX_SIG_LEN = 16,
	NSINDEX_ALIGN = 256,
	NSLABEL_UUID_LEN = 16,
	NSLABEL_NAMESPACE_MIN_SIZE = SZ_16M,
	NSLABEL_NAME_LEN = 64,
	NSLABEL_FLAG_ROLABEL = 0x1,  /* read-only label */
	NSLABEL_FLAG_LOCAL = 0x2,    /* DIMM-local namespace */
	NSLABEL_FLAG_BTT = 0x4,      /* namespace contains a BTT */
	NSLABEL_FLAG_UPDATING = 0x8, /* label being updated */
	BTT_ALIGN = 4096,            /* all btt structures */
	BTTINFO_SIG_LEN = 16,
	BTTINFO_UUID_LEN = 16,
	BTTINFO_FLAG_ERROR = 0x1,    /* error state (read-only) */
	BTTINFO_MAJOR_VERSION = 1,
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
        u8 sig[NSINDEX_SIG_LEN];
        le32 flags;
        le32 seq;
        le64 myoff;
        le64 mysize;
        le64 otheroff;
        le64 labeloff;
        le32 nslot;
        le16 major;
        le16 minor;
        le64 checksum;
        u8 free[0];
};

static inline size_t sizeof_namespace_index(void)
{
	size_t size = sizeof(struct namespace_index);

	size += NSINDEX_ALIGN;
	size &= ~(NSINDEX_ALIGN - 1);
	return size;
}

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
 * @unused: must be zero
 */
struct namespace_label {
	u8 uuid[NSLABEL_UUID_LEN];
	u8 name[NSLABEL_NAME_LEN];
	le32 flags;
	le16 nlabel;
	le16 position;
	le64 isetcookie;
	le64 lbasize;
	le64 dpa;
	le64 rawsize;
	le32 slot;
	le32 unused;
};

static inline void __attribute__((always_inline, format(printf, 2, 3)))
ndctl_log_null(struct ndctl_ctx *ctx, const char *format, ...) {}

#define ndctl_log_cond(ctx, prio, arg...) \
do { \
	if (ndctl_get_log_priority(ctx) >= prio) \
		ndctl_log(ctx, prio, __FILE__, __LINE__, __FUNCTION__, ## arg); \
} while (0)

#ifdef ENABLE_LOGGING
#  ifdef ENABLE_DEBUG
#    define dbg(ctx, arg...) ndctl_log_cond(ctx, LOG_DEBUG, ## arg)
#  else
#    define dbg(ctx, arg...) ndctl_log_null(ctx, ## arg)
#  endif
#  define info(ctx, arg...) ndctl_log_cond(ctx, LOG_INFO, ## arg)
#  define err(ctx, arg...) ndctl_log_cond(ctx, LOG_ERR, ## arg)
#else
#  define dbg(ctx, arg...) ndctl_log_null(ctx, ## arg)
#  define info(ctx, arg...) ndctl_log_null(ctx, ## arg)
#  define err(ctx, arg...) ndctl_log_null(ctx, ## arg)
#endif

#ifndef HAVE_SECURE_GETENV
#  ifdef HAVE___SECURE_GETENV
#    define secure_getenv __secure_getenv
#  else
#    error neither secure_getenv nor __secure_getenv is available
#  endif
#endif

#define NDCTL_EXPORT __attribute__ ((visibility("default")))

void ndctl_log(struct ndctl_ctx *ctx,
		int priority, const char *file, int line, const char *fn,
		const char *format, ...)
	__attribute__((format(printf, 6, 7)));

static inline const char *devpath_to_devname(const char *devpath)
{
	return strrchr(devpath, '/') + 1;
}

static inline int check_udev(struct udev *udev)
{
	return udev ? 0 : -ENXIO;
}

static inline int check_kmod(struct kmod_ctx *kmod_ctx)
{
	return kmod_ctx ? 0 : -ENXIO;
}

static int ndctl_bind(struct ndctl_ctx *ctx, struct kmod_module *module,
		const char *devname);
static int ndctl_unbind(struct ndctl_ctx *ctx, const char *devpath);
static struct kmod_module *to_module(struct ndctl_ctx *ctx, const char *alias);

#endif /* _LIBNDCTL_PRIVATE_H_ */
