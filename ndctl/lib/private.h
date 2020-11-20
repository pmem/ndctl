/* SPDX-License-Identifier: LGPL-2.1 */
/* Copyright (C) 2014-2020, Intel Corporation. All rights reserved. */
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

#include <ndctl.h>
#include <ndctl/libndctl.h>
#include <ccan/endian/endian.h>
#include <ccan/short_types/short_types.h>
#include "intel.h"
#include "hpe1.h"
#include "msft.h"
#include "hyperv.h"
#include "papr.h"
#include "libndctl-nfit.h"

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
	struct ndctl_dimm_ops *ops;
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
	unsigned long cmd_family;
	unsigned long cmd_mask;
	unsigned long nfit_dsm_mask;
	long long dirty_shutdown;
	enum ndctl_fwa_state fwa_state;
	enum ndctl_fwa_result fwa_result;
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

enum dsm_support {
	DIMM_DSM_UNSUPPORTED, /* don't attempt command */
	DIMM_DSM_SUPPORTED, /* good to go */
	DIMM_DSM_UNKNOWN, /* try ND_CMD_CALL on older kernels */
};

static inline enum dsm_support test_dimm_dsm(struct ndctl_dimm *dimm, int fn)
{
	if (dimm->nfit_dsm_mask == ULONG_MAX) {
		return DIMM_DSM_UNKNOWN;
	} else if (dimm->nfit_dsm_mask & (1 << fn))
		return DIMM_DSM_SUPPORTED;
	return DIMM_DSM_UNSUPPORTED;
}

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
	int has_of_node;
	char *bus_path;
	char *bus_buf;
	size_t buf_len;
	char *wait_probe_path;
	char *scrub_path;
	unsigned long cmd_mask;
	unsigned long nfit_dsm_mask;
	enum ndctl_fwa_state fwa_state;
	enum ndctl_fwa_method fwa_method;
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

struct badblocks_iter {
	struct badblock bb;
	FILE *file;
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
 * @target_node: target node were this region to be onlined
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
	struct badblocks_iter bb_iter;
	char *alt_name;
	uuid_t uuid;
	struct ndctl_lbasize lbasize;
	int numa_node, target_node;
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
 * @get_firmware_status: per command firmware status field retrieval
 * @iter: iterator for multi-xfer commands
 * @source: source cmd of an inherited iter.total_buf
 *
 * For dynamically sized commands like 'get_config', 'set_config', or
 * 'vendor', @size encompasses the entire buffer for the command input
 * and response output data.
 *
 * A command may only specify one of @source, or @iter.total_buf, not both.
 */
struct ndctl_cmd {
	struct ndctl_dimm *dimm;
	struct ndctl_bus *bus;
	int refcount;
	int type;
	int size;
	int status;
	u32 (*get_firmware_status)(struct ndctl_cmd *cmd);
	u32 (*get_xfer)(struct ndctl_cmd *cmd);
	u32 (*get_offset)(struct ndctl_cmd *cmd);
	void (*set_xfer)(struct ndctl_cmd *cmd, u32 xfer);
	void (*set_offset)(struct ndctl_cmd *cmd, u32 offset);
	struct ndctl_cmd_iter {
		u32 init_offset;
		u8 *data; /* pointer to the data buffer location in cmd */
		u32 max_xfer;
		char *total_buf;
		u32 total_xfer;
		int dir;
	} iter;
	struct ndctl_cmd *source;
	union {
		struct nd_cmd_ars_cap ars_cap[0];
		struct nd_cmd_ars_start ars_start[0];
		struct nd_cmd_ars_status ars_status[0];
		struct nd_cmd_clear_error clear_err[0];
		struct nd_cmd_pkg pkg[0];
		struct nd_cmd_bus cmd_bus[0];
		struct ndn_pkg_hpe1 hpe1[0];
		struct ndn_pkg_msft msft[0];
		struct nd_pkg_hyperv hyperv[0];
		struct nd_pkg_intel intel[0];
		struct nd_pkg_papr papr[0];
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

/* ars_status flags */
#define ND_ARS_STAT_FLAG_OVERFLOW (1 << 0)

struct ndctl_dimm_ops {
	const char *(*cmd_desc)(int);
	bool (*cmd_is_supported)(struct ndctl_dimm *, int);
	struct ndctl_cmd *(*new_smart)(struct ndctl_dimm *);
	unsigned int (*smart_get_flags)(struct ndctl_cmd *);
	unsigned int (*smart_get_health)(struct ndctl_cmd *);
	unsigned int (*smart_get_media_temperature)(struct ndctl_cmd *);
	unsigned int (*smart_get_ctrl_temperature)(struct ndctl_cmd *);
	unsigned int (*smart_get_spares)(struct ndctl_cmd *);
	unsigned int (*smart_get_alarm_flags)(struct ndctl_cmd *);
	unsigned int (*smart_get_life_used)(struct ndctl_cmd *);
	unsigned int (*smart_get_shutdown_state)(struct ndctl_cmd *);
	unsigned int (*smart_get_shutdown_count)(struct ndctl_cmd *);
	unsigned int (*smart_get_vendor_size)(struct ndctl_cmd *);
	unsigned char *(*smart_get_vendor_data)(struct ndctl_cmd *);
	struct ndctl_cmd *(*new_smart_threshold)(struct ndctl_dimm *);
	unsigned int (*smart_threshold_get_alarm_control)(struct ndctl_cmd *);
	unsigned int (*smart_threshold_get_media_temperature)(struct ndctl_cmd *);
	unsigned int (*smart_threshold_get_ctrl_temperature)(struct ndctl_cmd *);
	unsigned int (*smart_threshold_get_spares)(struct ndctl_cmd *);
	struct ndctl_cmd *(*new_smart_set_threshold)(struct ndctl_cmd *);
	unsigned int (*smart_threshold_get_supported_alarms)(struct ndctl_cmd *);
	int (*smart_threshold_set_alarm_control)(struct ndctl_cmd *, unsigned int);
	int (*smart_threshold_set_media_temperature)(struct ndctl_cmd *, unsigned int);
	int (*smart_threshold_set_ctrl_temperature)(struct ndctl_cmd *, unsigned int);
	int (*smart_threshold_set_spares)(struct ndctl_cmd *, unsigned int);
	struct ndctl_cmd *(*new_smart_inject)(struct ndctl_dimm *);
	int (*smart_inject_media_temperature)(struct ndctl_cmd *, bool, unsigned int);
	int (*smart_inject_ctrl_temperature)(struct ndctl_cmd *, bool, unsigned int);
	int (*smart_inject_spares)(struct ndctl_cmd *, bool, unsigned int);
	int (*smart_inject_fatal)(struct ndctl_cmd *, bool);
	int (*smart_inject_unsafe_shutdown)(struct ndctl_cmd *, bool);
	int (*smart_inject_supported)(struct ndctl_dimm *);
	struct ndctl_cmd *(*new_fw_get_info)(struct ndctl_dimm *);
	unsigned int (*fw_info_get_storage_size)(struct ndctl_cmd *);
	unsigned int (*fw_info_get_max_send_len)(struct ndctl_cmd *);
	unsigned int (*fw_info_get_query_interval)(struct ndctl_cmd *);
	unsigned int (*fw_info_get_max_query_time)(struct ndctl_cmd *);
	unsigned long long (*fw_info_get_run_version)(struct ndctl_cmd *);
	unsigned long long (*fw_info_get_updated_version)(struct ndctl_cmd *);
	struct ndctl_cmd *(*new_fw_start_update)(struct ndctl_dimm *);
	unsigned int (*fw_start_get_context)(struct ndctl_cmd *);
	struct ndctl_cmd *(*new_fw_send)(struct ndctl_cmd *,
			unsigned int, unsigned int, void *);
	struct ndctl_cmd *(*new_fw_finish)(struct ndctl_cmd *);
	struct ndctl_cmd *(*new_fw_abort)(struct ndctl_cmd *);
	struct ndctl_cmd *(*new_fw_finish_query)(struct ndctl_cmd *);
	unsigned long long (*fw_fquery_get_fw_rev)(struct ndctl_cmd *);
	enum ND_FW_STATUS (*fw_xlat_firmware_status)(struct ndctl_cmd *);
	struct ndctl_cmd *(*new_ack_shutdown_count)(struct ndctl_dimm *);
	int (*fw_update_supported)(struct ndctl_dimm *);
	int (*xlat_firmware_status)(struct ndctl_cmd *);
	u32 (*get_firmware_status)(struct ndctl_cmd *);
};

extern struct ndctl_dimm_ops * const intel_dimm_ops;
extern struct ndctl_dimm_ops * const hpe1_dimm_ops;
extern struct ndctl_dimm_ops * const msft_dimm_ops;
extern struct ndctl_dimm_ops * const hyperv_dimm_ops;
extern struct ndctl_dimm_ops * const papr_dimm_ops;

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
