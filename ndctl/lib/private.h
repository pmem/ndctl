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
#ifndef _LIBNDCTL_PRIVATE_H_
#define _LIBNDCTL_PRIVATE_H_

#include <errno.h>
#include <stdbool.h>
#include <syslog.h>
#include <string.h>
#include <libudev.h>
#include <libkmod.h>
#include <util/log.h>
#include <uuid/uuid.h>
#include <ccan/list/list.h>
#include <ccan/array_size/array_size.h>
#ifdef HAVE_NDCTL_H
#include <linux/ndctl.h>
#else
#include <ndctl.h>
#endif
#include <ndctl/libndctl.h>
#include <ccan/endian/endian.h>
#include <ccan/short_types/short_types.h>
#include "hpe1.h"
#include "msft.h"

/**
 * struct ndctl_dimm - memory device as identified by NFIT
 * @module: kernel module (libnvdimm)
 * @handle: NFIT-handle value
 * @major: /dev/nmemX major character device number
 * @minor: /dev/nmemX minor character device number
 * @phys_id: SMBIOS physical id
 * @vendor_id: hardware component vendor
 * @device_id: hardware device id
 * @revision_id: hardware revision id
 * @node: system node-id
 * @socket: socket-id in the node
 * @imc: memory-controller-id in the socket
 * @channel: channel-id in the memory-controller
 * @dimm: dimm-id in the channel
 * @formats: number of support interfaces
 * @format: array of format interface code numbers
 */
struct ndctl_dimm {
	struct kmod_module *module;
	struct ndctl_bus *bus;
	struct ndctl_smart_ops *smart_ops;
	unsigned int handle, major, minor, serial;
	unsigned short phys_id;
	unsigned short vendor_id;
	unsigned short device_id;
	unsigned short revision_id;
	unsigned short subsystem_vendor_id;
	unsigned short subsystem_device_id;
	unsigned short subsystem_revision_id;
	unsigned short manufacturing_date;
	unsigned char manufacturing_location;
	unsigned long dsm_family;
	unsigned long dsm_mask;
	char *unique_id;
	char *dimm_path;
	char *dimm_buf;
	int health_eventfd;
	int buf_len;
	int id;
	union dimm_flags {
		unsigned long flags;
		struct {
			unsigned int f_map:1;
			unsigned int f_arm:1;
			unsigned int f_save:1;
			unsigned int f_flush:1;
			unsigned int f_smart:1;
			unsigned int f_restore:1;
			unsigned int f_notify:1;
		};
	} flags;
	struct list_node list;
	int formats;
	int format[0];
};

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

/**
 * struct ndctl_ctx - library user context to find "nd" instances
 *
 * Instantiate with ndctl_new(), which takes an initial reference.  Free
 * the context by dropping the reference count to zero with
 * ndctrl_unref(), or take additional references with ndctl_ref()
 * @timeout: default library timeout in milliseconds
 */
struct ndctl_ctx {
	/* log_ctx must be first member for ndctl_set_log_fn compat */
	struct log_ctx ctx;
	int refcount;
	int regions_init;
	void *userdata;
	struct list_head busses;
	int busses_init;
	struct udev *udev;
	struct udev_queue *udev_queue;
	struct kmod_ctx *kmod_ctx;
	struct daxctl_ctx *daxctl_ctx;
	unsigned long timeout;
	void *private_data;
};

/**
 * struct ndctl_cmd - device-specific-method (_DSM ioctl) container
 * @dimm: set if the command is relative to a dimm, NULL otherwise
 * @bus: set if the command is relative to a bus (like ARS), NULL otherwise
 * @refcount: reference for passing command buffer around
 * @type: cmd number
 * @size: total size of the ndctl_cmd allocation
 * @status: negative if failed, 0 if success, > 0 if never submitted
 * @firmware_status: NFIT command output status code
 * @iter: iterator for multi-xfer commands
 * @source: source cmd of an inherited iter.total_buf
 *
 * For dynamically sized commands like 'get_config', 'set_config', or
 * 'vendor', @size encompasses the entire buffer for the command input
 * and response output data.
 *
 * A command may only specify one of @source, or @iter.total_buf, not both.
 */
enum {
	READ, WRITE,
};
struct ndctl_cmd {
	struct ndctl_dimm *dimm;
	struct ndctl_bus *bus;
	int refcount;
	int type;
	int size;
	int status;
	u32 *firmware_status;
	struct ndctl_cmd_iter {
		u32 *offset;
		u32 *xfer; /* pointer to xfer length in cmd */
		u8 *data; /* pointer to the data buffer location in cmd */
		u32 max_xfer;
		char *total_buf;
		u32 total_xfer;
		int dir;
	} iter;
	struct ndctl_cmd *source;
	union {
#ifdef HAVE_NDCTL_ARS
		struct nd_cmd_ars_cap ars_cap[0];
		struct nd_cmd_ars_start ars_start[0];
		struct nd_cmd_ars_status ars_status[0];
#endif
#ifdef HAVE_NDCTL_CLEAR_ERROR
		struct nd_cmd_clear_error clear_err[0];
#endif
		struct ndn_pkg_hpe1 hpe1[0];
		struct ndn_pkg_msft msft[0];
		struct nd_cmd_smart smart[0];
		struct nd_cmd_smart_threshold smart_t[0];
		struct nd_cmd_get_config_size get_size[0];
		struct nd_cmd_get_config_data_hdr get_data[0];
		struct nd_cmd_set_config_hdr set_data[0];
		struct nd_cmd_vendor_hdr vendor[0];
		char cmd_buf[0];
	};
};

struct ndctl_smart_ops {
	struct ndctl_cmd *(*new_smart)(struct ndctl_dimm *);
	unsigned int (*smart_get_flags)(struct ndctl_cmd *);
	unsigned int (*smart_get_health)(struct ndctl_cmd *);
	unsigned int (*smart_get_temperature)(struct ndctl_cmd *);
	unsigned int (*smart_get_spares)(struct ndctl_cmd *);
	unsigned int (*smart_get_alarm_flags)(struct ndctl_cmd *);
	unsigned int (*smart_get_life_used)(struct ndctl_cmd *);
	unsigned int (*smart_get_shutdown_state)(struct ndctl_cmd *);
	unsigned int (*smart_get_vendor_size)(struct ndctl_cmd *);
	unsigned char *(*smart_get_vendor_data)(struct ndctl_cmd *);
	struct ndctl_cmd *(*new_smart_threshold)(struct ndctl_dimm *);
	unsigned int (*smart_threshold_get_alarm_control)(struct ndctl_cmd *);
	unsigned int (*smart_threshold_get_temperature)(struct ndctl_cmd *);
	unsigned int (*smart_threshold_get_spares)(struct ndctl_cmd *);
};

#if HAS_SMART == 1
struct ndctl_smart_ops * const intel_smart_ops;
struct ndctl_smart_ops * const hpe1_smart_ops;
struct ndctl_smart_ops * const msft_smart_ops;
#else
static struct ndctl_smart_ops * const intel_smart_ops = NULL;
static struct ndctl_smart_ops * const hpe1_smart_ops = NULL;
static struct ndctl_smart_ops * const msft_smart_ops = NULL;
#endif

/* internal library helpers for conditionally defined command numbers */
#ifdef HAVE_NDCTL_ARS
static const int nd_cmd_ars_status = ND_CMD_ARS_STATUS;
static const int nd_cmd_ars_cap = ND_CMD_ARS_CAP;
#else
static const int nd_cmd_ars_status;
static const int nd_cmd_ars_cap;
#endif

#ifdef HAVE_NDCTL_CLEAR_ERROR
static const int nd_cmd_clear_error = ND_CMD_CLEAR_ERROR;
#else
static const int nd_cmd_clear_error;
#endif

static inline struct ndctl_bus *cmd_to_bus(struct ndctl_cmd *cmd)
{
	if (cmd->dimm)
		return ndctl_dimm_get_bus(cmd->dimm);
	return cmd->bus;
}

#define NDCTL_EXPORT __attribute__ ((visibility("default")))

static inline int check_udev(struct udev *udev)
{
	return udev ? 0 : -ENXIO;
}

static inline int check_kmod(struct kmod_ctx *kmod_ctx)
{
	return kmod_ctx ? 0 : -ENXIO;
}

#endif /* _LIBNDCTL_PRIVATE_H_ */
