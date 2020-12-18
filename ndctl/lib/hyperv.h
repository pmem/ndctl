/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2019, Microsoft Corporation. All rights reserved. */

#ifndef __NDCTL_HYPERV_H__
#define __NDCTL_HYPERV_H__

/* See http://www.uefi.org/RFIC_LIST ("Virtual NVDIMM 0x1901") */

enum {
	ND_HYPERV_CMD_QUERY_SUPPORTED_FUNCTIONS	= 0,
	ND_HYPERV_CMD_GET_HEALTH_INFO		= 1,
	ND_HYPERV_CMD_GET_SHUTDOWN_INFO		= 2,
};

/* Get Health Information (Function Index 1) */
struct nd_hyperv_health_info {
	__u32	status;
	__u32	health;
} __attribute__((packed));

/* Get Unsafe Shutdown Count (Function Index 2) */
struct nd_hyperv_shutdown_info {
	 __u32   status;
	 __u32   count;
} __attribute__((packed));

union nd_hyperv_cmd {
	__u32				status;
	struct nd_hyperv_health_info	health_info;
	struct nd_hyperv_shutdown_info	shutdown_info;
} __attribute__((packed));

struct nd_pkg_hyperv {
	struct nd_cmd_pkg	gen;
	union  nd_hyperv_cmd	u;
} __attribute__((packed));

#endif /* __NDCTL_HYPERV_H__ */
