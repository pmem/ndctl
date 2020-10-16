/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2018-2020 Intel Corporation. All rights reserved. */

#ifndef _FIRMWARE_UPDATE_H_
#define _FIRMWARE_UPDATE_H_

#define ND_CMD_STATUS_SUCCESS	0
#define ND_CMD_STATUS_NOTSUPP	1
#define	ND_CMD_STATUS_NOTEXIST	2
#define ND_CMD_STATUS_INVALPARM	3
#define ND_CMD_STATUS_HWERR	4
#define ND_CMD_STATUS_RETRY	5
#define ND_CMD_STATUS_UNKNOWN	6
#define ND_CMD_STATUS_EXTEND	7
#define ND_CMD_STATUS_NORES	8
#define ND_CMD_STATUS_NOTREADY	9

/* extended status through ND_CMD_STATUS_EXTEND */
#define ND_CMD_STATUS_START_BUSY	0x10000
#define ND_CMD_STATUS_SEND_CTXINVAL	0x10000
#define ND_CMD_STATUS_FIN_CTXINVAL	0x10000
#define ND_CMD_STATUS_FIN_DONE		0x20000
#define ND_CMD_STATUS_FIN_BAD		0x30000
#define ND_CMD_STATUS_FIN_ABORTED	0x40000
#define ND_CMD_STATUS_FQ_CTXINVAL	0x10000
#define ND_CMD_STATUS_FQ_BUSY		0x20000
#define ND_CMD_STATUS_FQ_BAD		0x30000
#define ND_CMD_STATUS_FQ_ORDER		0x40000

struct fw_info {
	uint32_t store_size;
	uint32_t update_size;
	uint32_t query_interval;
	uint32_t max_query;
	uint64_t run_version;
	uint32_t context;
};

struct update_context {
	size_t fw_size;
	struct fw_info dimm_fw;
	struct ndctl_cmd *start;
	struct json_object *jdimms;
};

#endif
