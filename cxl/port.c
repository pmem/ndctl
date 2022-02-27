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
	bool memdevs;
	bool endpoint;
} param;

static struct log_ctx pl;

#define BASE_OPTIONS()                                                 \
OPT_BOOLEAN(0, "debug", &param.debug, "turn on debug"),                \
OPT_BOOLEAN('e', "endpoint", &param.endpoint,                          \
	    "target endpoints instead of switch ports")

#define ENABLE_OPTIONS()                                               \
OPT_BOOLEAN('m', "enable-memdevs", &param.memdevs,                   \
	    "enable downstream memdev(s)")

#define DISABLE_OPTIONS()                                              \
OPT_BOOLEAN('f', "force", &param.force,                                \
	    "DANGEROUS: override active memdev safety checks")

static const struct option disable_options[] = {
	BASE_OPTIONS(),
	DISABLE_OPTIONS(),
	OPT_END(),
};

static const struct option enable_options[] = {
	BASE_OPTIONS(),
	ENABLE_OPTIONS(),
	OPT_END(),
};

static int action_disable(struct cxl_port *port)
{
	const char *devname = cxl_port_get_devname(port);
	struct cxl_ctx *ctx = cxl_port_get_ctx(port);
	struct cxl_memdev *memdev;
	int active_memdevs = 0;

	if (!cxl_port_is_enabled(port)) {
		log_dbg(&pl, "%s already disabled\n", devname);
		return 0;
	}

	if (param.endpoint) {
		struct cxl_endpoint *endpoint = cxl_port_to_endpoint(port);

		if (cxl_endpoint_get_memdev(endpoint))
			active_memdevs++;
	}

	cxl_memdev_foreach(ctx, memdev) {
		if (!cxl_port_get_dport_by_memdev(port, memdev))
			continue;
		if (cxl_memdev_is_enabled(memdev))
			active_memdevs++;
	}

	if (active_memdevs && !param.force) {
		/*
		 * TODO: actually detect rather than assume active just
		 * because the memdev is enabled
		 */
		log_err(&pl,
			"%s hosts %d memdev%s which %s part of an active region\n",
			devname, active_memdevs, active_memdevs > 1 ? "s" : "",
			active_memdevs > 1 ? "are" : "is");
		log_err(&pl,
			"See 'cxl list -M -p %s' to see impacted device%s\n",
			devname, active_memdevs > 1 ? "s" : "");
		return -EBUSY;
	}

	return cxl_port_disable_invalidate(port);
}

static int action_enable(struct cxl_port *port)
{
	struct cxl_ctx *ctx = cxl_port_get_ctx(port);
	struct cxl_memdev *memdev;
	int rc;

	rc = cxl_port_enable(port);
	if (rc || !param.memdevs)
		return rc;

	cxl_memdev_foreach(ctx, memdev)
		if (cxl_port_get_dport_by_memdev(port, memdev))
			cxl_memdev_enable(memdev);
	return 0;
}

static struct cxl_port *find_cxl_port(struct cxl_ctx *ctx, const char *ident)
{
	struct cxl_bus *bus;
	struct cxl_port *port;

	cxl_bus_foreach(ctx, bus)
		cxl_port_foreach_all(cxl_bus_get_port(bus), port)
			if (util_cxl_port_filter(port, ident, CXL_PF_SINGLE))
				return port;
	return NULL;
}

static struct cxl_endpoint *find_cxl_endpoint(struct cxl_ctx *ctx,
					      const char *ident)
{
	struct cxl_bus *bus;
	struct cxl_port *port;
	struct cxl_endpoint *endpoint;

	cxl_bus_foreach(ctx, bus)
		cxl_port_foreach_all(cxl_bus_get_port(bus), port)
			cxl_endpoint_foreach(port, endpoint)
				if (util_cxl_endpoint_filter(endpoint, ident))
					return endpoint;
	return NULL;
}



static int port_action(int argc, const char **argv, struct cxl_ctx *ctx,
		       int (*action)(struct cxl_port *port),
		       const struct option *options, const char *usage)
{
	int i, rc = 0, count = 0, err = 0;
	const char * const u[] = {
		usage,
		NULL
	};

	log_init(&pl, "cxl port", "CXL_PORT_LOG");
	argc = parse_options(argc, argv, options, u, 0);

	if (argc == 0)
		usage_with_options(u, options);
	for (i = 0; i < argc; i++) {
		if (strcmp(argv[i], "all") == 0) {
			argc = 1;
			break;
		}
	}

	if (param.debug) {
		cxl_set_log_priority(ctx, LOG_DEBUG);
		pl.log_priority = LOG_DEBUG;
	} else
		pl.log_priority = LOG_INFO;

	rc = 0;
	count = 0;

	for (i = 0; i < argc; i++) {
		struct cxl_port *port;

		if (param.endpoint) {
			struct cxl_endpoint *endpoint;

			endpoint = find_cxl_endpoint(ctx, argv[i]);
			if (!endpoint) {
				log_notice(&pl, "endpoint: %s not found\n",
					   argv[i]);
				continue;
			}
			port = cxl_endpoint_get_port(endpoint);
		} else {
			port = find_cxl_port(ctx, argv[i]);
			if (!port) {
				log_notice(&pl, "port: %s not found\n",
					   argv[i]);
				continue;
			}
		}

		log_dbg(&pl, "run action on port: %s\n",
			cxl_port_get_devname(port));
		rc = action(port);
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

 int cmd_disable_port(int argc, const char **argv, struct cxl_ctx *ctx)
 {
	 int count = port_action(
		 argc, argv, ctx, action_disable, disable_options,
		 "cxl disable-port <port0> [<port1>..<portN>] [<options>]");

	 log_info(&pl, "disabled %d port%s\n", count >= 0 ? count : 0,
		  count > 1 ? "s" : "");
	 return count >= 0 ? 0 : EXIT_FAILURE;
 }

 int cmd_enable_port(int argc, const char **argv, struct cxl_ctx *ctx)
 {
	 int count = port_action(
		 argc, argv, ctx, action_enable, enable_options,
		 "cxl enable-port <port0> [<port1>..<portN>] [<options>]");

	 log_info(&pl, "enabled %d port%s\n", count >= 0 ? count : 0,
		  count > 1 ? "s" : "");
	 return count >= 0 ? 0 : EXIT_FAILURE;
 }
