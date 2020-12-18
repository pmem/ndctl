// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2015-2020 Intel Corporation. All rights reserved.
// Copyright (C) 2005 Andreas Ericsson. All rights reserved.

/* originally copied from perf and git */

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <daxctl/libdaxctl.h>
#include <util/parse-options.h>
#include <ccan/array_size/array_size.h>

#include <util/strbuf.h>
#include <util/util.h>
#include <util/main.h>
#include <daxctl/builtin.h>

const char daxctl_usage_string[] = "daxctl [--version] [--help] COMMAND [ARGS]";
const char daxctl_more_info_string[] =
	"See 'daxctl help COMMAND' for more information on a specific command.\n"
	" daxctl --list-cmds to see all available commands";

static int cmd_version(int argc, const char **argv, struct daxctl_ctx *ctx)
{
	printf("%s\n", VERSION);
	return 0;
}

static int cmd_help(int argc, const char **argv, struct daxctl_ctx *ctx)
{
	const char * const builtin_help_subcommands[] = {
		"list", NULL,
	};
	struct option builtin_help_options[] = {
		OPT_END(),
	};
	const char *builtin_help_usage[] = {
		"daxctl help [command]",
		NULL
	};

	argc = parse_options_subcommand(argc, argv, builtin_help_options,
			builtin_help_subcommands, builtin_help_usage, 0);

	if (!argv[0]) {
		printf("\n usage: %s\n\n", daxctl_usage_string);
		printf("\n %s\n\n", daxctl_more_info_string);
		return 0;
	}

	return help_show_man_page(argv[0], "daxctl", "DAXCTL_MAN_VIEWER");
}

static struct cmd_struct commands[] = {
	{ "version", .d_fn = cmd_version },
	{ "list", .d_fn = cmd_list },
	{ "help", .d_fn = cmd_help },
	{ "split-acpi", .d_fn = cmd_split_acpi, },
	{ "migrate-device-model", .d_fn = cmd_migrate },
	{ "create-device", .d_fn = cmd_create_device },
	{ "destroy-device", .d_fn = cmd_destroy_device },
	{ "reconfigure-device", .d_fn = cmd_reconfig_device },
	{ "online-memory", .d_fn = cmd_online_memory },
	{ "offline-memory", .d_fn = cmd_offline_memory },
	{ "disable-device", .d_fn = cmd_disable_device },
	{ "enable-device", .d_fn = cmd_enable_device },
};

int main(int argc, const char **argv)
{
	struct daxctl_ctx *ctx;
	int rc;

	/* Look for flags.. */
	argv++;
	argc--;
	main_handle_options(&argv, &argc, daxctl_usage_string, commands,
			ARRAY_SIZE(commands));

	if (argc > 0) {
		if (!prefixcmp(argv[0], "--"))
			argv[0] += 2;
	} else {
		/* The user didn't specify a command; give them help */
		printf("\n usage: %s\n\n", daxctl_usage_string);
		printf("\n %s\n\n", daxctl_more_info_string);
		goto out;
	}

	rc = daxctl_new(&ctx);
	if (rc)
		goto out;
	main_handle_internal_command(argc, argv, ctx, commands,
			ARRAY_SIZE(commands), PROG_DAXCTL);
	daxctl_unref(ctx);
	fprintf(stderr, "Unknown command: '%s'\n", argv[0]);
out:
	return 1;
}
