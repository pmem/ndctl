/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright(c) 2015-2018 Intel Corporation. All rights reserved. */
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include "action.h"
#include <syslog.h>
#include <builtin.h>
#include <util/json.h>
#include <util/filter.h>
#include <json-c/json.h>
#include <util/parse-options.h>
#include <ndctl/libndctl.h>
#include <ccan/array_size/array_size.h>

static struct {
	bool verbose;
} param;

static const struct option bus_options[] = {
	OPT_BOOLEAN('v',"verbose", &param.verbose, "turn on debug"),
	OPT_END(),
};

static int scrub_action(struct ndctl_bus *bus, enum device_action action)
{
	switch (action) {
	case ACTION_WAIT:
		return ndctl_bus_wait_for_scrub_completion(bus);
	case ACTION_START:
		return ndctl_bus_start_scrub(bus);
	default:
		return -EINVAL;
	}
}

static int bus_action(int argc, const char **argv, const char *usage,
		const struct option *options, enum device_action action,
		struct ndctl_ctx *ctx)
{
	const char * const u[] = {
		usage,
		NULL
	};
	struct json_object *jbuses, *jbus;
	int i, rc, success = 0, fail = 0;
	struct ndctl_bus *bus;
	const char *all = "all";

	argc = parse_options(argc, argv, options, u, 0);

	if (param.verbose)
		ndctl_set_log_priority(ctx, LOG_DEBUG);

	if (argc == 0) {
		argc = 1;
		argv = &all;
	} else
		for (i = 0; i < argc; i++)
			if (strcmp(argv[i], "all") == 0) {
				argv[0] = "all";
				argc = 1;
				break;
			}

	jbuses = json_object_new_array();
	if (!jbuses)
		return -ENOMEM;
	for (i = 0; i < argc; i++) {
		int found = 0;

		ndctl_bus_foreach(ctx, bus) {
			if (!util_bus_filter(bus, argv[i]))
				continue;
			found++;
			rc = scrub_action(bus, action);
			if (rc == 0) {
				success++;
				jbus = util_bus_to_json(bus);
				if (jbus)
					json_object_array_add(jbuses, jbus);
			} else if (!fail)
				fail = rc;
		}
		if (!found && param.verbose)
			fprintf(stderr, "no bus matches id: %s\n", argv[i]);
	}

	if (success)
		util_display_json_array(stdout, jbuses,
				JSON_C_TO_STRING_PRETTY);
	else
		json_object_put(jbuses);

	if (success)
		return success;
	return fail ? fail : -ENXIO;
}

int cmd_start_scrub(int argc, const char **argv, void *ctx)
{
	char *usage = "ndctl start-scrub [<bus-id> <bus-id2> ... <bus-idN>] [<options>]";
	int start = bus_action(argc, argv, usage, bus_options,
			ACTION_START, ctx);

	if (start <= 0) {
		fprintf(stderr, "error starting scrub: %s\n",
				strerror(-start));
		return start;
	} else {
		return 0;
	}
}

int cmd_wait_scrub(int argc, const char **argv, void *ctx)
{
	char *usage = "ndctl wait-scrub [<bus-id> <bus-id2> ... <bus-idN>] [<options>]";
	int wait = bus_action(argc, argv, usage, bus_options,
			ACTION_WAIT, ctx);

	if (wait <= 0) {
		fprintf(stderr, "error waiting for scrub completion: %s\n",
				strerror(-wait));
		return wait;
	} else {
		return 0;
	}
}
