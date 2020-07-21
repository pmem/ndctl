/*
 * Copyright (C) 2016-2017 Dell, Inc.
 * Copyright (C) 2016 Hewlett Packard Enterprise Development LP
 * Copyright (c) 2014-2015, Intel Corporation.
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
#ifndef __NDCTL_MSFT_H__
#define __NDCTL_MSFT_H__

enum {
	NDN_MSFT_CMD_QUERY = 0,

	/* non-root commands */
	NDN_MSFT_CMD_SMART = 11,
};

/* NDN_MSFT_CMD_SMART */
#define NDN_MSFT_SMART_HEALTH_VALID	ND_SMART_HEALTH_VALID
#define NDN_MSFT_SMART_TEMP_VALID	ND_SMART_TEMP_VALID
#define NDN_MSFT_SMART_USED_VALID	ND_SMART_USED_VALID

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

#endif /* __NDCTL_MSFT_H__ */
