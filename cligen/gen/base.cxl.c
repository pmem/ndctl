// SPDX-License-Identifier: GPL-2.0
/* Copyright (C) 2020-2021 Intel Corporation. All rights reserved. */
/* Copyright (C) 2005 Andreas Ericsson. All rights reserved. */

/* originally copied from perf and git */

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <ccan/array_size/array_size.h>
#include <ccan/endian/endian.h>
#include <ccan/short_types/short_types.h>
#include <cxl/libcxl.h>
#include <util/parse-options.h>
#include <ccan/array_size/array_size.h>

#include <util/strbuf.h>
#include <util/util.h>
#include <util/main.h>
#include <cxl/builtin.h>

const char cxl_usage_string[] = "cxl [--version] [--help] COMMAND [ARGS]";
const char cxl_more_info_string[] =
	"See 'cxl help COMMAND' for more information on a specific command.\n"
	" cxl --list-cmds to see all available commands";

static int cmd_version(int argc, const char **argv, struct cxl_ctx *ctx)
{
	printf("%s\n", VERSION);
	return 0;
}

static int cmd_help(int argc, const char **argv, struct cxl_ctx *ctx)
{
	const char * const builtin_help_subcommands[] = {
		"list",
		NULL,
	};
	struct option builtin_help_options[] = {
		OPT_END(),
	};
	const char *builtin_help_usage[] = {
		"cxl help [command]",
		NULL
	};

	argc = parse_options_subcommand(argc, argv, builtin_help_options,
			builtin_help_subcommands, builtin_help_usage, 0);

	if (!argv[0]) {
		printf("\n usage: %s\n\n", cxl_usage_string);
		printf("\n %s\n\n", cxl_more_info_string);
		return 0;
	}

	return help_show_man_page(argv[0], "cxl", "CXL_MAN_VIEWER");
}

static struct cmd_struct commands[] = {
	{ "version", .c_fn = cmd_version },
	{ "list", .c_fn = cmd_list },
	{ "help", .c_fn = cmd_help },
	{ "zero-labels", .c_fn = cmd_zero_labels },
	{ "read-labels", .c_fn = cmd_read_labels },
	{ "write-labels", .c_fn = cmd_write_labels },
	{ "id-cmd", .c_fn = cmd_identify },
	{ "get-supported-logs", .c_fn = cmd_get_supported_logs },
	{ "get-cel-log", .c_fn = cmd_get_cel_log },
	{ "get-event-interrupt-policy", .c_fn = cmd_get_event_interrupt_policy },
	{ "set-event-interrupt-policy", .c_fn = cmd_set_event_interrupt_policy },
	{ "get-timestamp", .c_fn = cmd_get_timestamp },
	{ "set-timestamp", .c_fn = cmd_set_timestamp },
	{ "get-alert-config", .c_fn = cmd_get_alert_config },
	{ "set-alert-config", .c_fn = cmd_set_alert_config },
	{ "get-health-info", .c_fn = cmd_get_health_info },
	{ "get-event-records", .c_fn = cmd_get_event_records },
	{ "get-ld-info", .c_fn = cmd_get_ld_info },
	{ "clear-event-records", .c_fn = cmd_clear_event_records },
	{ "ddr-info", .c_fn = cmd_ddr_info },
	/* insert here */
};

int main(int argc, const char **argv)
{
	struct cxl_ctx *ctx;
	int rc;

	/* Look for flags.. */
	argv++;
	argc--;
	main_handle_options(&argv, &argc, cxl_usage_string, commands,
			ARRAY_SIZE(commands));

	if (argc > 0) {
		if (!prefixcmp(argv[0], "--"))
			argv[0] += 2;
	} else {
		/* The user didn't specify a command; give them help */
		printf("\n usage: %s\n\n", cxl_usage_string);
		printf("\n %s\n\n", cxl_more_info_string);
		goto out;
	}

	rc = cxl_new(&ctx);
	if (rc)
		goto out;
	main_handle_internal_command(argc, argv, ctx, commands,
			ARRAY_SIZE(commands), PROG_CXL);
	cxl_unref(ctx);
	fprintf(stderr, "Unknown command: '%s'\n", argv[0]);
out:
	return 1;
}
