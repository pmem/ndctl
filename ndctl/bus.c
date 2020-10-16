// SPDX-License-Identifier: GPL-2.0
/* Copyright (C) 2015-2020 Intel Corporation. All rights reserved. */
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
#include <ndctl/libndctl.h>
#include <util/parse-options.h>
#include <ccan/array_size/array_size.h>

static struct {
	bool verbose;
	bool force;
	bool idle;
	bool dryrun;
	unsigned int poll_interval;
} param = {
	.idle = true,
};


#define BASE_OPTIONS() \
	OPT_BOOLEAN('v',"verbose", &param.verbose, "turn on debug")

#define WAIT_OPTIONS() \
	OPT_UINTEGER('p', "poll", &param.poll_interval, "poll interval (seconds)")

#define ACTIVATE_OPTIONS() \
	OPT_BOOLEAN('I', "idle", &param.idle, \
			"allow platform-injected idle over activate (default)"), \
	OPT_BOOLEAN('f', "force", &param.force, "try to force live activation"), \
	OPT_BOOLEAN('n', "dry-run", &param.dryrun, \
			"perform all setup/validation steps, skip the activate")

static const struct option start_options[] = {
	BASE_OPTIONS(),
	OPT_END(),
};

static const struct option wait_options[] = {
	BASE_OPTIONS(),
	WAIT_OPTIONS(),
	OPT_END(),
};

static const struct option activate_options[] = {
	BASE_OPTIONS(),
	ACTIVATE_OPTIONS(),
	OPT_END(),
};

static int activate_firmware(struct ndctl_bus *bus)
{
	const char *provider = ndctl_bus_get_provider(bus);
	const char *devname = ndctl_bus_get_devname(bus);
	enum ndctl_fwa_method method;
	bool do_clear_noidle = false;
	enum ndctl_fwa_state state;
	struct ndctl_dimm *dimm;
	bool has_fwupd = false;
	int rc;

	ndctl_dimm_foreach(bus, dimm) {
		rc = ndctl_dimm_fw_update_supported(dimm);
		if (rc == 0) {
			has_fwupd = true;
			break;
		}
	}

	if (!has_fwupd) {
		fprintf(stderr, "%s: %s: has no devices that support firmware update.\n",
				provider, devname);
		return -EOPNOTSUPP;
	}

	method = ndctl_bus_get_fw_activate_method(bus);
	if (method == NDCTL_FWA_METHOD_RESET) {
		fprintf(stderr, "%s: %s: requires a platform reset to activate firmware\n",
				provider, devname);
		return -EOPNOTSUPP;
	}

	if (!param.idle) {
		rc = ndctl_bus_set_fw_activate_noidle(bus);
		if (rc) {
			fprintf(stderr, "%s: %s: failed to disable platform idling.\n",
					provider, devname);
			/* not fatal, continue... */
		}
		do_clear_noidle = true;
	}

	if (method == NDCTL_FWA_METHOD_SUSPEND && param.force)
		method = NDCTL_FWA_METHOD_LIVE;

	rc = 0;
	if (!param.dryrun) {
		state = ndctl_bus_get_fw_activate_state(bus);
		if (state != NDCTL_FWA_ARMED && state != NDCTL_FWA_ARM_OVERFLOW) {
			fprintf(stderr, "%s: %s: no devices armed\n",
					provider, devname);
			rc = -ENXIO;
			goto out;
		}

		rc = ndctl_bus_activate_firmware(bus, method);
	}

	if (rc) {
		fprintf(stderr, "%s: %s: firmware activation failed (%s)\n",
				provider, devname, strerror(-rc));
		goto out;
	}

out:
	if (do_clear_noidle)
		ndctl_bus_clear_fw_activate_noidle(bus);
	return rc;
}

static int scrub_action(struct ndctl_bus *bus, enum device_action action)
{
	switch (action) {
	case ACTION_WAIT:
		return ndctl_bus_poll_scrub_completion(bus,
				param.poll_interval, 0);
	case ACTION_START:
		return ndctl_bus_start_scrub(bus);
	default:
		return -EINVAL;
	}
}

static void collect_result(struct json_object *jbuses, struct ndctl_bus *bus,
		enum device_action action)
{
	unsigned long flags = UTIL_JSON_FIRMWARE | UTIL_JSON_HUMAN;
	struct json_object *jbus, *jdimms;
	struct ndctl_dimm *dimm;

	jbus = util_bus_to_json(bus, flags);
	if (jbus)
		json_object_array_add(jbuses, jbus);
	if (action != ACTION_ACTIVATE)
		return;

	jdimms = json_object_new_array();
	if (!jdimms)
		return;

	ndctl_dimm_foreach(bus, dimm) {
		struct json_object *jdimm;

		jdimm = util_dimm_to_json(dimm, flags);
		if (jdimm)
			json_object_array_add(jdimms, jdimm);
	}
	if (json_object_array_length(jdimms) > 0)
		json_object_object_add(jbus, "dimms", jdimms);
	else
		json_object_put(jdimms);
}

static int bus_action(int argc, const char **argv, const char *usage,
		const struct option *options, enum device_action action,
		struct ndctl_ctx *ctx)
{
	const char * const u[] = {
		usage,
		NULL
	};
	int i, rc, success = 0, fail = 0;
	struct json_object *jbuses;
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
			switch (action) {
			case ACTION_WAIT:
			case ACTION_START:
				rc = scrub_action(bus, action);
				break;
			case ACTION_ACTIVATE:
				rc = activate_firmware(bus);
				break;
			default:
				rc = -EINVAL;
			}

			if (rc == 0) {
				success++;
				collect_result(jbuses, bus, action);
			} else if (!fail)
				fail = rc;

		}
		if (!found && param.verbose)
			fprintf(stderr, "no bus matches id: %s\n", argv[i]);
	}

	if (success)
		util_display_json_array(stdout, jbuses, UTIL_JSON_FIRMWARE);
	else
		json_object_put(jbuses);

	if (success)
		return success;
	return fail ? fail : -ENXIO;
}

int cmd_start_scrub(int argc, const char **argv, struct ndctl_ctx *ctx)
{
	char *usage = "ndctl start-scrub [<bus-id> <bus-id2> ... <bus-idN>] [<options>]";
	int start = bus_action(argc, argv, usage, start_options,
			ACTION_START, ctx);

	if (start <= 0) {
		fprintf(stderr, "error starting scrub: %s\n",
				strerror(-start));
		return start;
	} else {
		return 0;
	}
}

int cmd_wait_scrub(int argc, const char **argv, struct ndctl_ctx *ctx)
{
	char *usage = "ndctl wait-scrub [<bus-id> <bus-id2> ... <bus-idN>] [<options>]";
	int wait = bus_action(argc, argv, usage, wait_options,
			ACTION_WAIT, ctx);

	if (wait <= 0) {
		fprintf(stderr, "error waiting for scrub completion: %s\n",
				strerror(-wait));
		return wait;
	} else {
		return 0;
	}
}

int cmd_activate_firmware(int argc, const char **argv, struct ndctl_ctx *ctx)
{
	char *usage = "ndctl activate-firmware[<bus-id> <bus-id2> ... <bus-idN>] [<options>]";
	int rc = bus_action(argc, argv, usage, activate_options,
			ACTION_ACTIVATE, ctx);

	if (rc <= 0) {
		fprintf(stderr, "error activating firmware: %s\n",
				strerror(-rc));
		return rc;
	}
	return 0;
}
