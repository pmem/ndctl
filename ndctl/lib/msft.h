/* SPDX-License-Identifier: LGPL-2.1 */
/* Copyright (C) 2016-2017 Dell, Inc. */
/* Copyright (C) 2016 Hewlett Packard Enterprise Development LP */
/* Copyright (C) 2014-2020, Intel Corporation. */
#ifndef __NDCTL_MSFT_H__
#define __NDCTL_MSFT_H__

enum {
	NDN_MSFT_CMD_QUERY = 0,

	/* non-root commands */
	NDN_MSFT_CMD_SMART = 11,
};

/*
 * This is actually function 11 data,
 * This is the closest I can find to match smart
 * Microsoft _DSM does not have smart function
 */
struct ndn_msft_smart_data {
	__u16	health;
	__u16	temp;
	__u8	err_thresh_stat;
	__u8	warn_thresh_stat;
	__u8	nvm_lifetime;
	__u8	count_dram_uncorr_err;
	__u8	count_dram_corr_err;
} __attribute__((packed));

struct ndn_msft_smart {
	__u32	status;
	union {
		__u8 buf[9];
		struct ndn_msft_smart_data data[1];
	};
} __attribute__((packed));

union ndn_msft_cmd {
	__u32			query;
	struct ndn_msft_smart	smart;
} __attribute__((packed));

struct ndn_pkg_msft {
	struct nd_cmd_pkg	gen;
	union ndn_msft_cmd	u;
} __attribute__((packed));

#define NDN_MSFT_STATUS_MASK		0xffff
#define NDN_MSFT_STATUS_SUCCESS	0
#define NDN_MSFT_STATUS_NOTSUPP	1
#define NDN_MSFT_STATUS_INVALPARM	2
#define NDN_MSFT_STATUS_I2CERR		3

#endif /* __NDCTL_MSFT_H__ */
