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
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/ioctl.h>

#include <util/log.h>
#include <util/size.h>
#include <util/json.h>
#include <json-c/json.h>
#include <util/filter.h>
#include <ndctl/libndctl.h>
#include <util/parse-options.h>
#include <ccan/array_size/array_size.h>
#include <ccan/short_types/short_types.h>
#ifdef HAVE_NDCTL_H
#include <linux/ndctl.h>
#else
#include <ndctl.h>
#endif

#include "private.h"
#include <builtin.h>
#include <test.h>

static bool verbose;
static struct parameters {
	const char *bus;
	const char *region;
	const char *namespace;
	const char *block;
	const char *count;
	bool clear;
	bool status;
	bool no_notify;
	bool human;
} param;

static struct inject_ctx {
	u64 block;
	u64 count;
	unsigned int op_mask;
	unsigned long flags;
	bool notify;
} ictx;

#define BASE_OPTIONS() \
OPT_STRING('b', "bus", &param.bus, "bus-id", \
	"limit namespace to a bus with an id or provider of <bus-id>"), \
OPT_STRING('r', "region", &param.region, "region-id", \
	"limit namespace to a region with an id or name of <region-id>"), \
OPT_BOOLEAN('v', "verbose", &verbose, "emit extra debug messages to stderr")

#define INJECT_OPTIONS() \
OPT_STRING('B', "block", &param.block, "namespace block offset (512B)", \
	"specify the block at which to (un)inject the error"), \
OPT_STRING('n', "count", &param.count, "count", \
	"specify the number of blocks of errors to (un)inject"), \
OPT_BOOLEAN('d', "uninject", &param.clear, \
	"un-inject a previously injected error"), \
OPT_BOOLEAN('t', "status", &param.status, "get error injection status"), \
OPT_BOOLEAN('N', "no-notify", &param.no_notify, "firmware should not notify OS"), \
OPT_BOOLEAN('u', "human", &param.human, "use human friendly number formats ")

static const struct option inject_options[] = {
	BASE_OPTIONS(),
	INJECT_OPTIONS(),
	OPT_END(),
};

enum {
	OP_INJECT = 0,
	OP_CLEAR,
	OP_STATUS,
};

static int inject_init(void)
{
	if (!param.clear && !param.status) {
		ictx.op_mask |= 1 << OP_INJECT;
		ictx.notify = true;
		if (param.no_notify)
			ictx.notify = false;
	}
	if (param.clear) {
		if (param.status) {
			error("status is invalid with inject or uninject\n");
			return -EINVAL;
		}
		ictx.op_mask |= 1 << OP_CLEAR;
	}
	if (param.status) {
		if (param.block || param.count) {
			error("status is invalid with inject or uninject\n");
			return -EINVAL;
		}
		ictx.op_mask |= 1 << OP_STATUS;
	}

	if (ictx.op_mask == 0) {
		error("Unable to determine operation\n");
		return -EINVAL;
	}
	ictx.op_mask &= (
		(1 << OP_INJECT) |
		(1 << OP_CLEAR) |
		(1 << OP_STATUS));

	if (param.block) {
		ictx.block = parse_size64(param.block);
		if (ictx.block == ULLONG_MAX) {
			error("Invalid block: %s\n", param.block);
			return -EINVAL;
		}
	}
	if (param.count) {
		ictx.count = parse_size64(param.count);
		if (ictx.count == ULLONG_MAX) {
			error("Invalid count: %s\n", param.count);
			return -EINVAL;
		}
	}

	/* For inject or clear, an block and count are required */
	if (ictx.op_mask & ((1 << OP_INJECT) | (1 << OP_CLEAR))) {
		if (!param.block || !param.count) {
			error("block and count required for inject/uninject\n");
			return -EINVAL;
		}
	}

	if (param.human)
		ictx.flags |= UTIL_JSON_HUMAN;

	return 0;
}

static int ns_errors_to_json(struct ndctl_namespace *ndns,
		unsigned int start_count)
{
	unsigned long flags = ictx.flags | UTIL_JSON_MEDIA_ERRORS;
	struct ndctl_bus *bus = ndctl_namespace_get_bus(ndns);
	struct json_object *jndns;
	unsigned int count;
	int rc, tmo = 30;

	/* only wait for scrubs for the inject and notify case */
	if ((ictx.op_mask & (1 << OP_INJECT)) && ictx.notify) {
		do {
			/* wait for a scrub to start */
			count = ndctl_bus_get_scrub_count(bus);
			if (count == UINT_MAX) {
				fprintf(stderr, "Unable to get scrub count\n");
				return -ENXIO;
			}
			sleep(1);
		} while (count <= start_count && --tmo > 0);

		rc = ndctl_bus_wait_for_scrub_completion(bus);
		if (rc) {
			fprintf(stderr, "Error waiting for scrub\n");
			return rc;
		}
	}

	jndns = util_namespace_to_json(ndns, flags);
	if (jndns)
		printf("%s\n", json_object_to_json_string_ext(jndns,
				JSON_C_TO_STRING_PRETTY));
	return 0;
}

static int inject_error(struct ndctl_namespace *ndns, u64 offset, u64 length,
		bool notify)
{
	struct ndctl_bus *bus = ndctl_namespace_get_bus(ndns);
	unsigned int scrub_count;
	int rc;

	scrub_count = ndctl_bus_get_scrub_count(bus);
	if (scrub_count == UINT_MAX) {
		fprintf(stderr, "Unable to get scrub count\n");
		return -ENXIO;
	}

	rc = ndctl_namespace_inject_error(ndns, offset, length, notify);
	if (rc) {
		fprintf(stderr, "Unable to inject error: %d\n", rc);
		return rc;
	}

	return ns_errors_to_json(ndns, scrub_count);
}

static int uninject_error(struct ndctl_namespace *ndns, u64 offset, u64 length)
{
	int rc;

	rc = ndctl_namespace_uninject_error(ndns, offset, length);
	if (rc) {
		fprintf(stderr, "Unable to uninject error: %d\n", rc);
		return rc;
	}

	printf("Warning: Un-injecting previously injected errors here will\n");
	printf("not cause the kernel to 'forget' its badblock entries. Those\n");
	printf("have to be cleared through the normal process of writing\n");
	printf("the affected blocks\n\n");
	return ns_errors_to_json(ndns, 0);
}

static int injection_status(struct ndctl_namespace *ndns)
{
	unsigned long long block, count, bbs = 0;
	struct json_object *jbbs, *jbb, *jobj;
	struct ndctl_bb *bb;
	int rc;

	rc = ndctl_namespace_injection_status(ndns);
	if (rc) {
		fprintf(stderr, "Unable to get injection status: %d\n", rc);
		return rc;
	}

	jobj = json_object_new_object();
	if (!jobj)
		return -ENOMEM;
	jbbs = json_object_new_array();
	if (!jbbs) {
		json_object_put(jobj);
		return -ENOMEM;
	}

	ndctl_namespace_bb_foreach(ndns, bb) {
		if (!bb)
			break;

		block = ndctl_bb_get_block(bb);
		count = ndctl_bb_get_count(bb);
		jbb = util_badblock_rec_to_json(block, count, ictx.flags);
		if (!jbb)
			break;
		json_object_array_add(jbbs, jbb);
		bbs++;
	}

	if (bbs) {
		json_object_object_add(jobj, "badblocks", jbbs);
		printf("%s\n", json_object_to_json_string_ext(jobj,
			JSON_C_TO_STRING_PRETTY));
	}

	json_object_put(jbbs);
	json_object_put(jobj);

	return rc;
}

static int err_inject_ns(struct ndctl_namespace *ndns)
{
	unsigned int op_mask;
	int rc;

	op_mask = ictx.op_mask;
	while (op_mask) {
		if (op_mask & (1 << OP_INJECT)) {
			rc = inject_error(ndns, ictx.block, ictx.count,
				ictx.notify);
			if (rc)
				return rc;
			op_mask &= ~(1 << OP_INJECT);
		}
		if (op_mask & (1 << OP_CLEAR)) {
			rc = uninject_error(ndns, ictx.block, ictx.count);
			if (rc)
				return rc;
			op_mask &= ~(1 << OP_CLEAR);
		}
		if (op_mask & (1 << OP_STATUS)) {
			rc = injection_status(ndns);
			if (rc)
				return rc;
			op_mask &= ~(1 << OP_STATUS);
		}
	}

	return rc;
}

static int do_inject(const char *namespace, struct ndctl_ctx *ctx)
{
	struct ndctl_namespace *ndns;
	struct ndctl_region *region;
	const char *ndns_name;
	struct ndctl_bus *bus;
	int rc = -ENXIO;

	if (namespace == NULL)
		return rc;

	if (verbose)
		ndctl_set_log_priority(ctx, LOG_DEBUG);

        ndctl_bus_foreach(ctx, bus) {
		if (!util_bus_filter(bus, param.bus))
			continue;

		ndctl_region_foreach(bus, region) {
			if (!util_region_filter(region, param.region))
				continue;

			ndctl_namespace_foreach(region, ndns) {
				ndns_name = ndctl_namespace_get_devname(ndns);

				if (strcmp(namespace, ndns_name) != 0)
					continue;

				if (!ndctl_bus_has_error_injection(bus)) {
					fprintf(stderr,
						"%s: error injection not supported\n",
						ndns_name);
					return -EOPNOTSUPP;
				}
				return err_inject_ns(ndns);
			}
		}
	}

	return 0;
}

int cmd_inject_error(int argc, const char **argv, void *ctx)
{
	const char * const u[] = {
		"ndctl inject-error <namespace> [<options>]",
		NULL
	};
	int i, rc;

        argc = parse_options(argc, argv, inject_options, u, 0);
	rc = inject_init();
	if (rc)
		return rc;

	if (argc == 0)
		error("specify a namespace to inject error to\n");
	for (i = 1; i < argc; i++)
		error("unknown extra parameter \"%s\"\n", argv[i]);
	if (argc == 0 || argc > 1) {
		usage_with_options(u, inject_options);
		return -ENODEV; /* we won't return from usage_with_options() */
	}

	return do_inject(argv[0], ctx);
}
