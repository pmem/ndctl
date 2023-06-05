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
int cmd_reserve_dpa(int argc, const char **argv, struct cxl_ctx *ctx);
int cmd_free_dpa(int argc, const char **argv, struct cxl_ctx *ctx);
int cmd_update_fw(int argc, const char **argv, struct cxl_ctx *ctx);
int cmd_disable_port(int argc, const char **argv, struct cxl_ctx *ctx);
int cmd_enable_port(int argc, const char **argv, struct cxl_ctx *ctx);
int cmd_set_partition(int argc, const char **argv, struct cxl_ctx *ctx);
int cmd_disable_bus(int argc, const char **argv, struct cxl_ctx *ctx);
int cmd_create_region(int argc, const char **argv, struct cxl_ctx *ctx);
int cmd_enable_region(int argc, const char **argv, struct cxl_ctx *ctx);
int cmd_disable_region(int argc, const char **argv, struct cxl_ctx *ctx);
int cmd_destroy_region(int argc, const char **argv, struct cxl_ctx *ctx);
#ifdef ENABLE_LIBTRACEFS
int cmd_monitor(int argc, const char **argv, struct cxl_ctx *ctx);
#else
static inline int cmd_monitor(int argc, const char **argv, struct cxl_ctx *ctx)
{
	fprintf(stderr,
		"cxl monitor: unavailable, rebuild with '-Dlibtracefs=enabled'\n");
	return EXIT_FAILURE;
}
#endif
#endif /* _CXL_BUILTIN_H_ */
