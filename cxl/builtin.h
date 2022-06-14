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
int cmd_disable_memdev(int argc, const char **argv, struct cxl_ctx *ctx);
int cmd_enable_memdev(int argc, const char **argv, struct cxl_ctx *ctx);
int cmd_disable_port(int argc, const char **argv, struct cxl_ctx *ctx);
int cmd_enable_port(int argc, const char **argv, struct cxl_ctx *ctx);
int cmd_set_partition(int argc, const char **argv, struct cxl_ctx *ctx);
int cmd_disable_bus(int argc, const char **argv, struct cxl_ctx *ctx);
#endif /* _CXL_BUILTIN_H_ */
