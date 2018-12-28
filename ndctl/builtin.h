/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright(c) 2015-2018 Intel Corporation. All rights reserved. */
#ifndef _NDCTL_BUILTIN_H_
#define _NDCTL_BUILTIN_H_
int cmd_create_nfit(int argc, const char **argv, void *ctx);
int cmd_enable_namespace(int argc, const char **argv, void *ctx);
int cmd_create_namespace(int argc, const char **argv, void *ctx);
int cmd_destroy_namespace(int argc, const char **argv, void *ctx);
int cmd_disable_namespace(int argc, const char **argv, void *ctx);
int cmd_check_namespace(int argc, const char **argv, void *ctx);
int cmd_enable_region(int argc, const char **argv, void *ctx);
int cmd_disable_region(int argc, const char **argv, void *ctx);
int cmd_enable_dimm(int argc, const char **argv, void *ctx);
int cmd_disable_dimm(int argc, const char **argv, void *ctx);
int cmd_zero_labels(int argc, const char **argv, void *ctx);
int cmd_read_labels(int argc, const char **argv, void *ctx);
int cmd_write_labels(int argc, const char **argv, void *ctx);
int cmd_init_labels(int argc, const char **argv, void *ctx);
int cmd_check_labels(int argc, const char **argv, void *ctx);
int cmd_inject_error(int argc, const char **argv, void *ctx);
int cmd_wait_scrub(int argc, const char **argv, void *ctx);
int cmd_start_scrub(int argc, const char **argv, void *ctx);
int cmd_list(int argc, const char **argv, void *ctx);
int cmd_monitor(int argc, const char **argv, void *ctx);
#ifdef ENABLE_TEST
int cmd_test(int argc, const char **argv, void *ctx);
#endif
#ifdef ENABLE_DESTRUCTIVE
int cmd_bat(int argc, const char **argv, void *ctx);
#endif
int cmd_update_firmware(int argc, const char **argv, void *ctx);
int cmd_inject_smart(int argc, const char **argv, void *ctx);
#endif /* _NDCTL_BUILTIN_H_ */
