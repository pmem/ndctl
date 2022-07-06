/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2020-2021 Intel Corporation. All rights reserved. */
#ifndef _CXL_BUILTIN_H_
#define _CXL_BUILTIN_H_

struct cxl_ctx;
int cmd_list(int argc, const char **argv, struct cxl_ctx *ctx);
int cmd_write_labels(int argc, const char **argv, struct cxl_ctx *ctx);
int cmd_read_labels(int argc, const char **argv, struct cxl_ctx *ctx);
int cmd_zero_labels(int argc, const char **argv, struct cxl_ctx *ctx);
int cmd_init_labels(int argc, const char **argv, struct cxl_ctx *ctx);
int cmd_check_labels(int argc, const char **argv, struct cxl_ctx *ctx);
int cmd_identify(int argc, const char **argv, struct cxl_ctx *ctx);
int cmd_get_supported_logs(int argc, const char **argv, struct cxl_ctx *ctx);
int cmd_get_cel_log(int argc, const char **argv, struct cxl_ctx *ctx);
int cmd_get_event_interrupt_policy(int argc, const char **argv, struct cxl_ctx *ctx);
int cmd_set_event_interrupt_policy(int argc, const char **argv, struct cxl_ctx *ctx);
int cmd_get_timestamp(int argc, const char **argv, struct cxl_ctx *ctx);
int cmd_set_timestamp(int argc, const char **argv, struct cxl_ctx *ctx);
int cmd_get_alert_config(int argc, const char **argv, struct cxl_ctx *ctx);
int cmd_set_alert_config(int argc, const char **argv, struct cxl_ctx *ctx);
int cmd_get_health_info(int argc, const char **argv, struct cxl_ctx *ctx);
int cmd_get_event_records(int argc, const char **argv, struct cxl_ctx *ctx);
int cmd_get_ld_info(int argc, const char **argv, struct cxl_ctx *ctx);
int cmd_clear_event_records(int argc, const char **argv, struct cxl_ctx *ctx);
int cmd_ddr_info(int argc, const char **argv, struct cxl_ctx *ctx);
/* insert here */
#endif /* _CXL_BUILTIN_H_ */
