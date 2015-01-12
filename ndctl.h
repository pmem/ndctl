/*
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
#ifndef __NDCTL_H__
#define __NDCTL_H__

#include <linux/types.h>

struct nfit_cmd_smart {
	__u32 status;
	__u8 data[8];
} __attribute__((packed));

struct nfit_cmd_get_config_size {
	__u32 status;
	__u32 config_size;
	__u32 optimal_io_size;
} __attribute__((packed));

struct nfit_cmd_get_config_data_hdr {
	__u32 in_offset;
	__u32 in_length;
	__u32 status;
	__u8 out_buf[0];
} __attribute__((packed));

struct nfit_cmd_set_config_hdr {
	__u32 in_offset;
	__u32 in_length;
	__u8 in_buf[0];
} __attribute__((packed));

struct nfit_cmd_vendor_hdr {
	__u32 in_length;
	__u8 in_buf[0];
} __attribute__((packed));

struct nfit_cmd_vendor_tail {
	__u32 status;
	__u32 out_length;
	__u8 out_buf[0];
} __attribute__((packed));

struct nfit_cmd_ars_cap {
	__u64 address;
	__u64 length;
	__u32 status;
} __attribute__((packed));

struct nfit_cmd_ars_start {
	__u64 address;
	__u64 length;
	__u16 type;
	__u32 status;
} __attribute__((packed));

struct nfit_cmd_ars_query {
	__u32 status;
	__u16 out_length;
	__u64 address;
	__u64 length;
	__u16 type;
	__u32 num_records;
	struct nfit_ars_record {
		__u32 nfit_handle;
		__u32 flags;
		__u64 err_address;
		__u64 mask;
	} __attribute__((packed)) records[0];
} __attribute__((packed));

struct nfit_cmd_arm {
	__u32 status;
} __attribute__((packed));

struct nfit_cmd_smart_threshold {
	__u32 status;
	__u8 data[8];
} __attribute__((packed));

enum {
	NFIT_CMD_IMPLEMENTED = 0,
	NFIT_CMD_SMART = 1,
	NFIT_CMD_GET_CONFIG_SIZE = 2,
	NFIT_CMD_GET_CONFIG_DATA = 3,
	NFIT_CMD_SET_CONFIG_DATA = 4,
	NFIT_CMD_VENDOR = 5,
	NFIT_CMD_ARS_CAP = 6,
	NFIT_CMD_ARS_START = 7,
	NFIT_CMD_ARS_QUERY = 8,
	NFIT_CMD_ARM = 9,
	NFIT_CMD_SMART_THRESHOLD = 10,
};

static __inline__ const char *nfit_cmd_name(int cmd)
{
	static const char *names[] = {
		[NFIT_CMD_SMART] = "smart",
		[NFIT_CMD_GET_CONFIG_SIZE] = "get_size",
		[NFIT_CMD_GET_CONFIG_DATA] = "get_data",
		[NFIT_CMD_SET_CONFIG_DATA] = "set_data",
		[NFIT_CMD_VENDOR] = "vendor",
		[NFIT_CMD_ARS_CAP] = "ars_cap",
		[NFIT_CMD_ARS_START] = "ars_start",
		[NFIT_CMD_ARS_QUERY] = "ars_query",
		[NFIT_CMD_ARM] = "arm",
		[NFIT_CMD_SMART_THRESHOLD] = "smart_t",
	};

	if (cmd >= NFIT_CMD_SMART && cmd <= NFIT_CMD_SMART_THRESHOLD)
		return names[cmd];
	return "unknown";
}

#define ND_IOCTL 'N'

#define NFIT_IOCTL_SMART		_IOWR(ND_IOCTL, NFIT_CMD_SMART,\
					struct nfit_cmd_smart)

#define NFIT_IOCTL_GET_CONFIG_SIZE	_IOWR(ND_IOCTL, NFIT_CMD_GET_CONFIG_SIZE,\
					struct nfit_cmd_get_config_size)

#define NFIT_IOCTL_GET_CONFIG_DATA	_IOWR(ND_IOCTL, NFIT_CMD_GET_CONFIG_DATA,\
					struct nfit_cmd_get_config_data_hdr)

#define NFIT_IOCTL_SET_CONFIG_DATA	_IOWR(ND_IOCTL, NFIT_CMD_SET_CONFIG_DATA,\
					struct nfit_cmd_set_config_hdr)

#define NFIT_IOCTL_VENDOR		_IOWR(ND_IOCTL, NFIT_CMD_VENDOR,\
					struct nfit_cmd_vendor_hdr)

#define NFIT_IOCTL_ARS_CAP		_IOWR(ND_IOCTL, NFIT_CMD_ARS_CAP,\
					struct nfit_cmd_ars_cap)

#define NFIT_IOCTL_ARS_START		_IOWR(ND_IOCTL, NFIT_CMD_ARS_START,\
					struct nfit_cmd_ars_start)

#define NFIT_IOCTL_ARS_QUERY		_IOWR(ND_IOCTL, NFIT_CMD_ARS_QUERY,\
					struct nfit_cmd_ars_query)

#define NFIT_IOCTL_ARM			_IOWR(ND_IOCTL, NFIT_CMD_ARM,\
					struct nfit_cmd_arm)

#define NFIT_IOCTL_SMART_THRESHOLD	_IOWR(ND_IOCTL, NFIT_CMD_SMART_THRESHOLD,\
					struct nfit_cmd_smart_threshold)


#define ND_DEVICE_DIMM 1            /* nd_dimm: container for "config data" */
#define ND_DEVICE_REGION_PMEM 2     /* nd_region: (parent of pmem namespaces) */
#define ND_DEVICE_REGION_BLOCK 3    /* nd_region: (parent of block namespaces) */
#define ND_DEVICE_NAMESPACE_IO 4    /* legacy persistent memory */
#define ND_DEVICE_NAMESPACE_PMEM 5  /* persistent memory namespace (may alias) */
#define ND_DEVICE_NAMESPACE_BLOCK 6 /* block-data-window namespace (may alias) */
#define ND_DEVICE_BTT 7		    /* block-translation table device */

enum nd_driver_flags {
	ND_DRIVER_DIMM            = 1 << ND_DEVICE_DIMM,
	ND_DRIVER_REGION_PMEM     = 1 << ND_DEVICE_REGION_PMEM,
	ND_DRIVER_REGION_BLOCK    = 1 << ND_DEVICE_REGION_BLOCK,
	ND_DRIVER_NAMESPACE_IO    = 1 << ND_DEVICE_NAMESPACE_IO,
	ND_DRIVER_NAMESPACE_PMEM  = 1 << ND_DEVICE_NAMESPACE_PMEM,
	ND_DRIVER_NAMESPACE_BLOCK = 1 << ND_DEVICE_NAMESPACE_BLOCK,
	ND_DRIVER_BTT		  = 1 << ND_DEVICE_BTT,
};

enum {
	ND_MIN_NAMESPACE_SIZE = 0x00400000,
};
#endif /* __NDCTL_H__ */
