/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2015-2020 Intel Corporation. All rights reserved. */
/* Copyright (C) 2006 Linus Torvalds. All rights reserved. */

/* originally copied from perf and git */

#ifndef __MAIN_H__
#define __MAIN_H__

enum program {
	PROG_NDCTL,
	PROG_DAXCTL,
};

struct ndctl_ctx;
struct daxctl_ctx;

struct cmd_struct {
	const char *cmd;
	union {
		int (*n_fn)(int, const char **, struct ndctl_ctx *ctx);
		int (*d_fn)(int, const char **, struct daxctl_ctx *ctx);
	};
};

int main_handle_options(const char ***argv, int *argc, const char *usage_msg,
		struct cmd_struct *cmds, int num_cmds);
void main_handle_internal_command(int argc, const char **argv, void *ctx,
		struct cmd_struct *cmds, int num_cmds, enum program prog);
int help_show_man_page(const char *cmd, const char *util_name,
		const char *viewer);
#endif /* __MAIN_H__ */
