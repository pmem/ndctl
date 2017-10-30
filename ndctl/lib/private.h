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

struct nvdimm_data {
	struct ndctl_cmd *cmd_read;
	void *data;
	unsigned long config_size;
	size_t nslabel_size;
	int ns_current, ns_next;
};

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
	struct nvdimm_data ndd;
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
	int locked;
	int aliased;
	struct list_node list;
	int formats;
	int format[0];
};

void region_flag_refresh(struct ndctl_region *region);

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
 * struct ndctl_bus - a nfit table instance
 * @major: control character device major number
 * @minor: control character device minor number
 * @revision: NFIT table revision number
 * @provider: identifier for the source of the NFIT table
 *
 * The expectation is one NFIT/nd bus per system provided by platform
 * firmware (for example @provider == "ACPI.NFIT").  However, the
 * nfit_test module provides multiple test busses with provider names of
 * the format "nfit_test.N"
 */
struct ndctl_bus {
	struct ndctl_ctx *ctx;
	unsigned int id, major, minor, revision;
	char *provider;
	struct list_head dimms;
	struct list_head regions;
	struct list_node list;
	int dimms_init;
	int regions_init;
	int has_nfit;
	char *bus_path;
	char *bus_buf;
	size_t buf_len;
	char *wait_probe_path;
	char *scrub_path;
	unsigned long dsm_mask;
	unsigned long nfit_dsm_mask;
};

/**
 * struct ndctl_lbasize - lbasize info for btt and blk-namespace devices
 * @select: currently selected sector_size
 * @supported: possible sector_size options
 * @num: number of entries in @supported
 */
struct ndctl_lbasize {
	int select;
	unsigned int *supported;
	int num;
};

/**
 * struct ndctl_namespace - device claimed by the nd_blk or nd_pmem driver
 * @module: kernel module
 * @type: integer nd-bus device-type
 * @type_name: 'namespace_io', 'namespace_pmem', or 'namespace_block'
 * @namespace_path: devpath for namespace device
 * @bdev: associated block_device of a namespace
 * @size: unsigned
 * @numa_node: numa node attribute
 *
 * A 'namespace' is the resulting device after region-aliasing and
 * label-parsing is resolved.
 */
struct ndctl_namespace {
	struct kmod_module *module;
	struct ndctl_region *region;
	struct list_node list;
	char *ndns_path;
	char *ndns_buf;
	char *bdev;
	int type, id, buf_len, raw_mode;
	int generation;
	unsigned long long resource, size;
	enum ndctl_namespace_mode enforce_mode;
	char *alt_name;
	uuid_t uuid;
	struct ndctl_lbasize lbasize;
	int numa_node;
	struct list_head injected_bb;
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

struct ndctl_bb {
	u64 block;
	u64 count;
	struct list_node list;
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
	unsigned int (*smart_get_shutdown_count)(struct ndctl_cmd *);
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

int ndctl_bus_nfit_translate_spa(struct ndctl_bus *bus, unsigned long long addr,
		unsigned int *handle, unsigned long long *dpa);
struct ndctl_cmd *ndctl_bus_cmd_new_err_inj(struct ndctl_bus *bus);
struct ndctl_cmd *ndctl_bus_cmd_new_err_inj_clr(struct ndctl_bus *bus);
struct ndctl_cmd *ndctl_bus_cmd_new_err_inj_stat(struct ndctl_bus *bus,
	u32 buf_size);

#endif /* _LIBNDCTL_PRIVATE_H_ */
