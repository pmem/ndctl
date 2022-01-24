// SPDX-License-Identifier: GPL-2.0
/* Copyright (C) 2020-2021 Intel Corporation. All rights reserved. */
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <util/json.h>
#include <json-c/json.h>
#include <cxl/libcxl.h>
#include <util/parse-options.h>

#include "filter.h"

static struct cxl_filter_params param;

static int num_list_flags(void)
{
	return param.memdevs;
}

int cmd_list(int argc, const char **argv, struct cxl_ctx *ctx)
{
	const struct option options[] = {
		OPT_STRING('m', "memdev", &param.memdev_filter, "memory device name",
			   "filter by CXL memory device name"),
		OPT_STRING('s', "serial", &param.serial_filter, "memory device serial",
			   "filter by CXL memory device serial number"),
		OPT_BOOLEAN('M', "memdevs", &param.memdevs,
			    "include CXL memory device info"),
		OPT_BOOLEAN('i', "idle", &param.idle, "include disabled devices"),
		OPT_BOOLEAN('u', "human", &param.human,
				"use human friendly number formats "),
		OPT_BOOLEAN('H', "health", &param.health,
				"include memory device health information "),
		OPT_END(),
	};
	const char * const u[] = {
		"cxl list [<options>]",
		NULL
	};
	int i;

	argc = parse_options(argc, argv, options, u, 0);
	for (i = 0; i < argc; i++)
		error("unknown parameter \"%s\"\n", argv[i]);

	if (argc)
		usage_with_options(u, options);

	if (num_list_flags() == 0) {
		if (param.memdev_filter || param.serial_filter)
			param.memdevs = true;
		else {
			/*
			 * TODO: We likely want to list regions by default if
			 * nothing was explicitly asked for. But until we have
			 * region support, print this error asking for devices
			 * explicitly.  Once region support is added, this TODO
			 * can be removed.
			 */
			error("please specify entities to list, e.g. using -m/-M\n");
			usage_with_options(u, options);
		}
	}

	log_init(&param.ctx, "cxl list", "CXL_LIST_LOG");

	return cxl_filter_walk(ctx, &param);
}
