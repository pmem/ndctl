// SPDX-License-Identifier: GPL-2.0
/* Copyright (C) 2020-2022 Intel Corporation. All rights reserved. */
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
static bool debug;

static const struct option options[] = {
	OPT_STRING('m', "memdev", &param.memdev_filter, "memory device name(s)",
		   "filter by CXL memory device name(s)"),
	OPT_STRING('s', "serial", &param.serial_filter,
		   "memory device serial(s)",
		   "filter by CXL memory device serial number(s)"),
	OPT_BOOLEAN('M', "memdevs", &param.memdevs,
		    "include CXL memory device info"),
	OPT_STRING('b', "bus", &param.bus_filter, "bus device name",
		   "filter by CXL bus device name(s)"),
	OPT_BOOLEAN('B', "buses", &param.buses, "include CXL bus info"),
	OPT_STRING('p', "port", &param.port_filter, "port device name",
		   "filter by CXL port device name(s)"),
	OPT_BOOLEAN('P', "ports", &param.ports, "include CXL port info"),
	OPT_BOOLEAN('S', "single", &param.single,
		    "skip listing descendant objects"),
	OPT_STRING('e', "endpoint", &param.endpoint_filter,
		   "endpoint device name",
		   "filter by CXL endpoint device name(s)"),
	OPT_BOOLEAN('E', "endpoints", &param.endpoints,
		    "include CXL endpoint info"),
	OPT_STRING('d', "decoder", &param.decoder_filter, "decoder device name",
		   "filter by CXL decoder device name(s) / class"),
	OPT_BOOLEAN('D', "decoders", &param.decoders,
		    "include CXL decoder info"),
	OPT_BOOLEAN('T', "targets", &param.targets,
		    "include CXL target data with decoders, ports, or regions"),
	OPT_STRING('r', "region", &param.region_filter, "region name",
		   "filter by CXL region name(s)"),
	OPT_BOOLEAN('R', "regions", &param.regions, "include CXL regions"),
	OPT_BOOLEAN('X', "dax", &param.dax, "include CXL DAX region enumeration"),
	OPT_BOOLEAN('i', "idle", &param.idle, "include disabled devices"),
	OPT_BOOLEAN('u', "human", &param.human,
		    "use human friendly number formats"),
	OPT_BOOLEAN('H', "health", &param.health,
		    "include memory device health information"),
	OPT_BOOLEAN('I', "partition", &param.partition,
		    "include memory device partition information"),
	OPT_BOOLEAN('F', "firmware", &param.fw,
		    "include memory device firmware information"),
	OPT_BOOLEAN('A', "alert-config", &param.alert_config,
		    "include alert configuration information"),
	OPT_BOOLEAN('L', "media-errors", &param.media_errors,
		    "include media-error information "),
	OPT_INCR('v', "verbose", &param.verbose, "increase output detail"),
#ifdef ENABLE_DEBUG
	OPT_BOOLEAN(0, "debug", &debug, "debug list walk"),
#endif
	OPT_END(),
};

static int num_list_flags(void)
{
	return !!param.memdevs + !!param.buses + !!param.ports +
	       !!param.endpoints + !!param.decoders + !!param.regions;
}

int cmd_list(int argc, const char **argv, struct cxl_ctx *ctx)
{
	const char * const u[] = {
		"cxl list [<options>]",
		NULL
	};
	struct json_object *jtopology;
	int i;

	argc = parse_options(argc, argv, options, u, 0);
	for (i = 0; i < argc; i++)
		error("unknown parameter \"%s\"\n", argv[i]);

	if (argc)
		usage_with_options(u, options);

	if (param.single && !param.port_filter) {
		error("-S/--single expects a port filter: -p/--port=\n");
		usage_with_options(u, options);
	}

	if (num_list_flags() == 0) {
		if (param.memdev_filter || param.serial_filter)
			param.memdevs = true;
		if (param.bus_filter)
			param.buses = true;
		if (param.port_filter)
			param.ports = true;
		if (param.endpoint_filter)
			param.endpoints = true;
		if (param.decoder_filter)
			param.decoders = true;
		param.single = true;
		if (param.region_filter)
			param.regions = true;
	}

	/* List regions and memdevs by default */
	if (num_list_flags() == 0) {
		param.regions = true;
		param.memdevs = true;
	}

	switch(param.verbose){
	default:
	case 3:
		param.health = true;
		param.partition = true;
		param.fw = true;
		param.alert_config = true;
		param.dax = true;
		param.media_errors = true;
		/* fallthrough */
	case 2:
		param.idle = true;
		/* fallthrough */
	case 1:
		param.buses = true;
		param.ports = true;
		param.endpoints = true;
		param.decoders = true;
		param.targets = true;
		param.regions = true;
		/*fallthrough*/
	case 0:
		break;
	}

	log_init(&param.ctx, "cxl list", "CXL_LIST_LOG");
	if (debug) {
		cxl_set_log_priority(ctx, LOG_DEBUG);
		param.ctx.log_priority = LOG_DEBUG;
	}

	if (cxl_filter_has(param.port_filter, "root") && param.ports)
		param.buses = true;

	if (cxl_filter_has(param.port_filter, "endpoint") && param.ports)
		param.endpoints = true;

	dbg(&param, "walk topology\n");
	jtopology = cxl_filter_walk(ctx, &param);
	if (!jtopology)
		return -ENOMEM;
	util_display_json_array(stdout, jtopology, cxl_filter_to_flags(&param));
	return 0;
}
