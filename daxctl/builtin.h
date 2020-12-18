/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2015-2020 Intel Corporation. All rights reserved. */
#ifndef _DAXCTL_BUILTIN_H_
#define _DAXCTL_BUILTIN_H_

struct daxctl_ctx;
int cmd_list(int argc, const char **argv, struct daxctl_ctx *ctx);
int cmd_migrate(int argc, const char **argv, struct daxctl_ctx *ctx);
int cmd_create_device(int argc, const char **argv, struct daxctl_ctx *ctx);
int cmd_destroy_device(int argc, const char **argv, struct daxctl_ctx *ctx);
int cmd_reconfig_device(int argc, const char **argv, struct daxctl_ctx *ctx);
int cmd_disable_device(int argc, const char **argv, struct daxctl_ctx *ctx);
int cmd_enable_device(int argc, const char **argv, struct daxctl_ctx *ctx);
int cmd_online_memory(int argc, const char **argv, struct daxctl_ctx *ctx);
int cmd_offline_memory(int argc, const char **argv, struct daxctl_ctx *ctx);
int cmd_split_acpi(int argc, const char **argv, struct daxctl_ctx *ctx);
#endif /* _DAXCTL_BUILTIN_H_ */
