// SPDX-License-Identifier: GPL-2.0
/* Copyright (C) 2020-2022 Intel Corporation. All rights reserved. */
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <util/log.h>
#include <cxl/libcxl.h>
#include <util/parse-options.h>
#include <ccan/minmax/minmax.h>
#include <ccan/array_size/array_size.h>

#include "filter.h"

static struct parameters {
	bool debug;
	bool force;
} param;

static struct log_ctx bl;

#define BASE_OPTIONS()                                                 \
OPT_BOOLEAN(0, "debug", &param.debug, "turn on debug")

#define DISABLE_OPTIONS()                                              \
OPT_BOOLEAN('f', "force", &param.force,                                \
	    "DANGEROUS: override active memdev safety checks")

static const struct option disable_options[] = {
	BASE_OPTIONS(),
	DISABLE_OPTIONS(),
	OPT_END(),
};

static int action_disable(struct cxl_bus *bus)
{
	const char *devname = cxl_bus_get_devname(bus);
	struct cxl_ctx *ctx = cxl_bus_get_ctx(bus);
	struct cxl_memdev *memdev;
	int active_memdevs = 0;

	cxl_memdev_foreach(ctx, memdev)
		if (bus == cxl_memdev_get_bus(memdev))
			active_memdevs++;

	if (active_memdevs && !param.force) {
		/*
		 * TODO: actually detect rather than assume active just
		 * because the memdev is enabled
		 */
		log_err(&bl,
			"%s hosts %d memdev%s which %s part of an active region\n",
			devname, active_memdevs, active_memdevs > 1 ? "s" : "",
			active_memdevs > 1 ? "are" : "is");
		log_err(&bl,
			"See 'cxl list -M -b %s' to see impacted device%s\n",
			devname, active_memdevs > 1 ? "s" : "");
		return -EBUSY;
	}

	return cxl_bus_disable_invalidate(bus);
}

static struct cxl_bus *find_cxl_bus(struct cxl_ctx *ctx, const char *ident)
{
	struct cxl_bus *bus;

	cxl_bus_foreach(ctx, bus)
		if (util_cxl_bus_filter(bus, ident))
			return bus;
	return NULL;
}

static int bus_action(int argc, const char **argv, struct cxl_ctx *ctx,
		      int (*action)(struct cxl_bus *bus),
		      const struct option *options, const char *usage)
{
	int i, rc = 0, count = 0, err = 0;
	const char * const u[] = {
		usage,
		NULL
	};
	unsigned long id;

	log_init(&bl, "cxl bus", "CXL_PORT_LOG");
	argc = parse_options(argc, argv, options, u, 0);

	if (argc == 0)
		usage_with_options(u, options);
	for (i = 0; i < argc; i++) {
		if (strcmp(argv[i], "all") == 0) {
			argv[0] = "all";
			argc = 1;
			break;
		}

		if (sscanf(argv[i], "root%lu", &id) == 1)
			continue;
		if (sscanf(argv[i], "%lu", &id) == 1)
			continue;

		log_err(&bl, "'%s' is not a valid bus identifier\n", argv[i]);
		err++;
	}

	if (err == argc) {
		usage_with_options(u, options);
		return -EINVAL;
	}

	if (param.debug) {
		cxl_set_log_priority(ctx, LOG_DEBUG);
		bl.log_priority = LOG_DEBUG;
	} else
		bl.log_priority = LOG_INFO;

	rc = 0;
	err = 0;
	count = 0;

	for (i = 0; i < argc; i++) {
		struct cxl_bus *bus;

		bus = find_cxl_bus(ctx, argv[i]);
		if (!bus) {
			log_dbg(&bl, "bus: %s not found\n", argv[i]);
			continue;
		}

		log_dbg(&bl, "run action on bus: %s\n",
			cxl_bus_get_devname(bus));
		rc = action(bus);
		if (rc == 0)
			count++;
		else if (rc && !err)
			err = rc;
	}
	rc = err;

	/*
	 * count if some actions succeeded, 0 if none were attempted,
	 * negative error code otherwise.
	 */
	if (count > 0)
		return count;
	return rc;
}

 int cmd_disable_bus(int argc, const char **argv, struct cxl_ctx *ctx)
 {
	 int count = bus_action(
		 argc, argv, ctx, action_disable, disable_options,
		 "cxl disable-bus <bus0> [<bus1>..<busN>] [<options>]");

	 log_info(&bl, "disabled %d bus%s\n", count >= 0 ? count : 0,
		  count > 1 ? "s" : "");
	 return count >= 0 ? 0 : EXIT_FAILURE;
 }
