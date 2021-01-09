/* SPDX-License-Identifier: LGPL-2.1 */
/* Copyright (C) 2020-2021, Intel Corporation. All rights reserved. */
#ifndef _LIBCXL_PRIVATE_H_
#define _LIBCXL_PRIVATE_H_

#include <libkmod.h>
#include <cxl/cxl_mem.h>
#include <ccan/endian/endian.h>
#include <ccan/short_types/short_types.h>

#define CXL_EXPORT __attribute__ ((visibility("default")))

struct cxl_memdev {
	int id, major, minor;
	void *dev_buf;
	size_t buf_len;
	char *dev_path;
	char *firmware_version;
	struct cxl_ctx *ctx;
	struct list_node list;
	unsigned long long pmem_size;
	unsigned long long ram_size;
	int payload_max;
	struct kmod_module *module;
};

enum cxl_cmd_query_status {
	CXL_CMD_QUERY_NOT_RUN = 0,
	CXL_CMD_QUERY_OK,
	CXL_CMD_QUERY_UNSUPPORTED,
};

/**
 * struct cxl_cmd - CXL memdev command
 * @memdev: the memory device to which the command is being sent
 * @query_cmd: structure for the Linux 'Query commands' ioctl
 * @send_cmd: structure for the Linux 'Send command' ioctl
 * @input_payload: buffer for input payload managed by libcxl
 * @output_payload: buffer for output payload managed by libcxl
 * @refcount: reference for passing command buffer around
 * @query_status: status from query_commands
 * @query_idx: index of 'this' command in the query_commands array
 * @status: command return status from the device
 */
struct cxl_cmd {
	struct cxl_memdev *memdev;
	struct cxl_mem_query_commands *query_cmd;
	struct cxl_send_command *send_cmd;
	void *input_payload;
	void *output_payload;
	int refcount;
	int query_status;
	int query_idx;
	int status;
};

#define CXL_CMD_IDENTIFY_FW_REV_LENGTH 0x10

struct cxl_cmd_identify {
	char fw_revision[CXL_CMD_IDENTIFY_FW_REV_LENGTH];
	le64 total_capacity;
	le64 volatile_capacity;
	le64 persistent_capacity;
	le64 partition_align;
	le16 info_event_log_size;
	le16 warning_event_log_size;
	le16 failure_event_log_size;
	le16 fatal_event_log_size;
	le32 lsa_size;
	u8 poison_list_max_mer[3];
	le16 inject_poison_limit;
	u8 poison_caps;
	u8 qos_telemetry_caps;
} __attribute__((packed));

static inline int check_kmod(struct kmod_ctx *kmod_ctx)
{
	return kmod_ctx ? 0 : -ENXIO;
}

#endif /* _LIBCXL_PRIVATE_H_ */
