/*
 * Copyright(c) 2015-2017 Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include "action.h"
#include <util/filter.h>
#include <util/parse-options.h>
#include <ndctl/libndctl.h>

static struct {
	const char *bus;
	const char *type;
} param;

static const struct option region_options[] = {
	OPT_STRING('b', "bus", &param.bus, "bus-id",
			"<region> must be on a bus with an id/provider of <bus-id>"),
	OPT_STRING('t', "type", &param.type, "region-type",
			"<region> must be of the specified type"),
	OPT_END(),
};

static const char *parse_region_options(int argc, const char **argv,
		char *xable_usage)
{
	const char * const u[] = {
		xable_usage,
		NULL
	};
	int i;

        argc = parse_options(argc, argv, region_options, u, 0);

	if (argc == 0)
		error("specify a region to delete, or \"all\"\n");
	for (i = 1; i < argc; i++)
		error("unknown extra parameter \"%s\"\n", argv[i]);
	if (argc == 0 || argc > 1) {
		usage_with_options(u, region_options);
		return NULL; /* we won't return from usage_with_options() */
	}

	if (param.type) {
		if (strcmp(param.type, "pmem") == 0)
			/* pass */;
		else if (strcmp(param.type, "blk") == 0)
			/* pass */;
		else {
			error("unknown region type '%s', should be 'pmem' or 'blk'\n",
					param.type);
			usage_with_options(u, region_options);
			return NULL;
		}
	}

	return argv[0];
}

static int region_action(struct ndctl_region *region, enum device_action mode)
{
	struct ndctl_namespace *ndns;
	int rc = 0;

	switch (mode) {
	case ACTION_ENABLE:
		rc = ndctl_region_enable(region);
		break;
	case ACTION_DISABLE:
		ndctl_namespace_foreach(region, ndns) {
			rc = ndctl_namespace_disable_safe(ndns);
			if (rc)
				return rc;
		}
		rc = ndctl_region_disable_invalidate(region);
		break;
	default:
		break;
	}

	return 0;
}

static int do_xable_region(const char *region_arg, enum device_action mode,
		struct ndctl_ctx *ctx)
{
	int rc = -ENXIO, success = 0;
	struct ndctl_region *region;
	struct ndctl_bus *bus;

	if (!region_arg)
		goto out;

        ndctl_bus_foreach(ctx, bus) {
		if (!util_bus_filter(bus, param.bus))
			continue;

		ndctl_region_foreach(bus, region) {
			const char *type = ndctl_region_get_type_name(region);

			if (param.type && strcmp(param.type, type) != 0)
				continue;
			if (!util_region_filter(region, region_arg))
				continue;
			if (region_action(region, mode) == 0)
				success++;
		}
	}

	rc = success;
 out:
	param.bus = NULL;
	return rc;
}

int cmd_disable_region(int argc, const char **argv, void *ctx)
{
	char *xable_usage = "ndctl disable-region <region> [<options>]";
	const char *region = parse_region_options(argc, argv, xable_usage);
	int disabled = do_xable_region(region, ACTION_DISABLE, ctx);

	if (disabled < 0) {
		fprintf(stderr, "error disabling regions: %s\n",
				strerror(-disabled));
		return disabled;
	} else if (disabled == 0) {
		fprintf(stderr, "disabled 0 regions\n");
		return 0;
	} else {
		fprintf(stderr, "disabled %d region%s\n", disabled,
				disabled > 1 ? "s" : "");
		return 0;
	}
}

int cmd_enable_region(int argc, const char **argv, void *ctx)
{
	char *xable_usage = "ndctl enable-region <region> [<options>]";
	const char *region = parse_region_options(argc, argv, xable_usage);
	int enabled = do_xable_region(region, ACTION_ENABLE, ctx);

	if (enabled < 0) {
		fprintf(stderr, "error enabling regions: %s\n",
				strerror(-enabled));
		return enabled;
	} else if (enabled == 0) {
		fprintf(stderr, "enabled 0 regions\n");
		return 0;
	} else {
		fprintf(stderr, "enabled %d region%s\n", enabled,
				enabled > 1 ? "s" : "");
		return 0;
	}
}
