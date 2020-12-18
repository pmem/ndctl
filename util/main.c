// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2015-2020 Intel Corporation. All rights reserved.
// Copyright (C) 2006 Linus Torvalds. All rights reserved.

/* originally copied from perf and git */

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <util/strbuf.h>
#include <util/util.h>
#include <util/main.h>

int main_handle_options(const char ***argv, int *argc, const char *usage_msg,
		struct cmd_struct *cmds, int num_cmds)
{
	int handled = 0;

	while (*argc > 0) {
		const char *cmd = (*argv)[0];
		if (cmd[0] != '-')
			break;

		if (!strcmp(cmd, "--version") || !strcmp(cmd, "--help"))
			break;

		/*
		 * Shortcut for '-h' and '-v' options to invoke help
		 * and version command.
		 */
		if (!strcmp(cmd, "-h")) {
			(*argv)[0] = "--help";
			break;
		}

		if (!strcmp(cmd, "-v")) {
			(*argv)[0] = "--version";
			break;
		}

		if (!strcmp(cmd, "--list-cmds")) {
			int i;

			for (i = 0; i < num_cmds; i++) {
				struct cmd_struct *p = cmds+i;

				/* filter out commands from auto-complete */
				if (strcmp(p->cmd, "create-nfit") == 0)
					continue;
				if (strcmp(p->cmd, "test") == 0)
					continue;
				if (strcmp(p->cmd, "bat") == 0)
					continue;
				printf("%s\n", p->cmd);
			}
			exit(0);
		} else {
			fprintf(stderr, "Unknown option: %s\n", cmd);
			usage(usage_msg);
		}

		(*argv)++;
		(*argc)--;
		handled++;
	}
	return handled;
}

static int run_builtin(struct cmd_struct *p, int argc, const char **argv,
		void *ctx, enum program prog)
{
	int status;
	struct stat st;

	if (prog == PROG_NDCTL)
		status = p->n_fn(argc, argv, ctx);
	else
		status = p->d_fn(argc, argv, ctx);

	if (status)
		return status & 0xff;

	/* Somebody closed stdout? */
	if (fstat(fileno(stdout), &st))
		return 0;
	/* Ignore write errors for pipes and sockets.. */
	if (S_ISFIFO(st.st_mode) || S_ISSOCK(st.st_mode))
		return 0;

	status = 1;
	/* Check for ENOSPC and EIO errors.. */
	if (fflush(stdout)) {
		fprintf(stderr, "write failure on standard output: %s", strerror(errno));
		goto out;
	}
	if (ferror(stdout)) {
		fprintf(stderr, "unknown write failure on standard output");
		goto out;
	}
	if (fclose(stdout)) {
		fprintf(stderr, "close failed on standard output: %s", strerror(errno));
		goto out;
	}
	status = 0;
out:
	return status;
}

void main_handle_internal_command(int argc, const char **argv, void *ctx,
		struct cmd_struct *cmds, int num_cmds, enum program prog)
{
	const char *cmd = argv[0];
	int i;

	/* Turn "<binary> cmd --help" into "<binary> help cmd" */
	if (argc > 1 && !strcmp(argv[1], "--help")) {
		argv[1] = argv[0];
		argv[0] = cmd = "help";
	}

	for (i = 0; i < num_cmds; i++) {
		struct cmd_struct *p = cmds+i;
		if (strcmp(p->cmd, cmd))
			continue;
		exit(run_builtin(p, argc, argv, ctx, prog));
	}
}
