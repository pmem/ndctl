/* SPDX-License-Identifier: LGPL-2.1 */
/* Copyright (C) 2017 Hewlett Packard Enterprise Development LP */
/* Copyright (C) 2017-2020 Intel Corporation. All rights reserved. */
#ifndef __LIBNDCTL_NFIT_H__
#define __LIBNDCTL_NFIT_H__

#include <linux/types.h>
#include <ndctl/ndctl.h>

/*
 * libndctl-nfit.h : definitions for NFIT related commands/functions.
 */

/* nfit command numbers which are called via ND_CMD_CALL */
enum {
	NFIT_CMD_TRANSLATE_SPA = 5,
	NFIT_CMD_ARS_INJECT_SET = 7,
	NFIT_CMD_ARS_INJECT_CLEAR = 8,
	NFIT_CMD_ARS_INJECT_GET = 9,
};

/* error number of Translate SPA by firmware  */
#define ND_TRANSLATE_SPA_STATUS_INVALID_SPA  2

/* status definitions for error injection */
#define ND_ARS_ERR_INJ_STATUS_NOT_SUPP 1
#define ND_ARS_ERR_INJ_STATUS_INVALID_PARAM 2

enum err_inj_options {
	ND_ARS_ERR_INJ_OPT_NOTIFY = 0,
};

/*
 * The following structures are command packages which are
 * defined by ACPI 6.2 (or later).
 */

/* For Translate SPA */
struct nd_cmd_translate_spa {
	__u64 spa;
	__u32 status;
	__u8  flags;
	__u8  _reserved[3];
	__u64 translate_length;
	__u32 num_nvdimms;
	struct nd_nvdimm_device {
		__u32 nfit_device_handle;
		__u32 _reserved;
		__u64 dpa;
	} __attribute__((packed)) devices[0];

} __attribute__((packed));

/* For ARS Error Inject */
struct nd_cmd_ars_err_inj {
	__u64 err_inj_spa_range_base;
	__u64 err_inj_spa_range_length;
	__u8  err_inj_options;
	__u32 status;
} __attribute__((packed));

/* For ARS Error Inject Clear */
struct nd_cmd_ars_err_inj_clr {
	__u64 err_inj_clr_spa_range_base;
	__u64 err_inj_clr_spa_range_length;
	__u32 status;
} __attribute__((packed));

/* For ARS Error Inject Status Query */
struct nd_cmd_ars_err_inj_stat {
	__u32 status;
	__u32 inj_err_rec_count;
	struct nd_error_stat_query_record {
		__u64 err_inj_stat_spa_range_base;
		__u64 err_inj_stat_spa_range_length;
	} __attribute__((packed)) record[0];
} __attribute__((packed));

struct nd_cmd_bus {
	struct nd_cmd_pkg gen;
	union {
		struct nd_cmd_ars_err_inj_stat err_inj_stat;
		struct nd_cmd_ars_err_inj_clr err_inj_clr;
		struct nd_cmd_ars_err_inj err_inj;
		struct nd_cmd_translate_spa xlat_spa;
	};
};

int ndctl_bus_is_nfit_cmd_supported(struct ndctl_bus *bus, int cmd);

#endif /* __LIBNDCTL_NFIT_H__ */
