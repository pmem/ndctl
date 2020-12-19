// SPDX-License-Identifier: GPL-2.0
/* Copyright (C) 2019-2020 Intel Corporation. All rights reserved. */
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <syslog.h>
#include <unistd.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <util/size.h>
#include <util/json.h>
#include <util/filter.h>
#include <json-c/json.h>
#include <json-c/json_util.h>
#include <daxctl/libdaxctl.h>
#include <util/parse-options.h>
#include <ccan/array_size/array_size.h>

static struct {
	const char *dev;
	const char *mode;
	const char *region;
	const char *size;
	const char *align;
	const char *input;
	bool no_online;
	bool no_movable;
	bool force;
	bool human;
	bool verbose;
} param;

enum dev_mode {
	DAXCTL_DEV_MODE_UNKNOWN,
	DAXCTL_DEV_MODE_DEVDAX,
	DAXCTL_DEV_MODE_RAM,
};

struct mapping {
	unsigned long long start, end, pgoff;
};

static enum dev_mode reconfig_mode = DAXCTL_DEV_MODE_UNKNOWN;
static long long align = -1;
static long long size = -1;
static unsigned long flags;
static struct mapping *maps = NULL;
static long long nmaps = -1;

enum memory_zone {
	MEM_ZONE_MOVABLE,
	MEM_ZONE_NORMAL,
};
static enum memory_zone mem_zone = MEM_ZONE_MOVABLE;

enum device_action {
	ACTION_RECONFIG,
	ACTION_ONLINE,
	ACTION_OFFLINE,
	ACTION_CREATE,
	ACTION_DISABLE,
	ACTION_ENABLE,
	ACTION_DESTROY,
};

#define BASE_OPTIONS() \
OPT_STRING('r', "region", &param.region, "region-id", "filter by region"), \
OPT_BOOLEAN('u', "human", &param.human, "use human friendly number formats"), \
OPT_BOOLEAN('v', "verbose", &param.verbose, "emit more debug messages")

#define RECONFIG_OPTIONS() \
OPT_STRING('m', "mode", &param.mode, "mode", "mode to switch the device to"), \
OPT_BOOLEAN('N', "no-online", &param.no_online, \
	"don't auto-online memory sections"), \
OPT_BOOLEAN('f', "force", &param.force, \
		"attempt to offline memory sections before reconfiguration")

#define CREATE_OPTIONS() \
OPT_STRING('s', "size", &param.size, "size", "size to switch the device to"), \
OPT_STRING('a', "align", &param.align, "align", "alignment to switch the device to"), \
OPT_STRING('\0', "input", &param.input, "input", "input device JSON file")

#define DESTROY_OPTIONS() \
OPT_BOOLEAN('f', "force", &param.force, \
		"attempt to disable before destroying device")

#define ZONE_OPTIONS() \
OPT_BOOLEAN('\0', "no-movable", &param.no_movable, \
		"online memory in ZONE_NORMAL")

static const struct option create_options[] = {
	BASE_OPTIONS(),
	CREATE_OPTIONS(),
	RECONFIG_OPTIONS(),
	ZONE_OPTIONS(),
	OPT_END(),
};

static const struct option reconfig_options[] = {
	BASE_OPTIONS(),
	CREATE_OPTIONS(),
	RECONFIG_OPTIONS(),
	ZONE_OPTIONS(),
	OPT_END(),
};

static const struct option online_options[] = {
	BASE_OPTIONS(),
	ZONE_OPTIONS(),
	OPT_END(),
};

static const struct option offline_options[] = {
	BASE_OPTIONS(),
	OPT_END(),
};

static const struct option disable_options[] = {
	BASE_OPTIONS(),
	OPT_END(),
};

static const struct option enable_options[] = {
	BASE_OPTIONS(),
	OPT_END(),
};

static const struct option destroy_options[] = {
	BASE_OPTIONS(),
	DESTROY_OPTIONS(),
	OPT_END(),
};

static int sort_mappings(const void *a, const void *b)
{
	json_object **jsoa, **jsob;
	struct json_object *va, *vb;
	unsigned long long pga, pgb;

	jsoa = (json_object **)a;
	jsob = (json_object **)b;
	if (!*jsoa && !*jsob)
		return 0;

	if (!json_object_object_get_ex(*jsoa, "page_offset", &va) ||
	    !json_object_object_get_ex(*jsob, "page_offset", &vb))
		return 0;

	pga = json_object_get_int64(va);
	pgb = json_object_get_int64(vb);

	return pga > pgb;
}

static int parse_device_file(const char *filename)
{
	struct json_object *jobj, *jval = NULL, *jmappings = NULL;
	int i, rc = -EINVAL, region_id, id;
	const char *chardev;
	char  *region = NULL;

	jobj = json_object_from_file(filename);
	if (!jobj)
		return rc;

	if (!json_object_object_get_ex(jobj, "align", &jval))
		return rc;
	param.align = json_object_get_string(jval);

	if (!json_object_object_get_ex(jobj, "size", &jval))
		return rc;
	param.size = json_object_get_string(jval);

	if (!json_object_object_get_ex(jobj, "chardev", &jval))
		return rc;
	chardev = json_object_get_string(jval);
	if (sscanf(chardev, "dax%u.%u", &region_id, &id) != 2)
		return rc;
	if (asprintf(&region, "%u", region_id) < 0)
		return rc;
	param.region = region;

	if (!json_object_object_get_ex(jobj, "mappings", &jmappings))
		return rc;
	json_object_array_sort(jmappings, sort_mappings);

	nmaps = json_object_array_length(jmappings);
	maps = calloc(nmaps, sizeof(*maps));
	if (!maps)
		return -ENOMEM;

	for (i = 0; i < nmaps; i++) {
		struct json_object *j, *val;

		j = json_object_array_get_idx(jmappings, i);
		if (!j)
			goto err;

		if (!json_object_object_get_ex(j, "start", &val))
			goto err;
		maps[i].start = json_object_get_int64(val);

		if (!json_object_object_get_ex(j, "end", &val))
			goto err;
		maps[i].end = json_object_get_int64(val);

		if (!json_object_object_get_ex(j, "page_offset", &val))
			goto err;
		maps[i].pgoff = json_object_get_int64(val);
	}

	return 0;

err:
	free(maps);
	return rc;
}

static const char *parse_device_options(int argc, const char **argv,
		enum device_action action, const struct option *options,
		const char *usage, struct daxctl_ctx *ctx)
{
	const char * const u[] = {
		usage,
		NULL
	};
	unsigned long long units = 1;
	int i, rc = 0;

	argc = parse_options(argc, argv, options, u, 0);

	/* Handle action-agnostic non-option arguments */
	if (argc == 0 &&
	    action != ACTION_CREATE) {
		char *action_string;

		switch (action) {
		case ACTION_RECONFIG:
			action_string = "reconfigure";
			break;
		case ACTION_ONLINE:
			action_string = "online memory for";
			break;
		case ACTION_OFFLINE:
			action_string = "offline memory for";
			break;
		case ACTION_DISABLE:
			action_string = "disable";
			break;
		case ACTION_ENABLE:
			action_string = "enable";
			break;
		case ACTION_DESTROY:
			action_string = "destroy";
			break;
		default:
			action_string = "<>";
			break;
		}
		fprintf(stderr, "specify a device to %s, or \"all\"\n",
			action_string);
		rc = -EINVAL;
	}
	for (i = 1; i < argc; i++) {
		fprintf(stderr, "unknown extra parameter \"%s\"\n", argv[i]);
		rc = -EINVAL;
	}

	if (rc) {
		usage_with_options(u, options);
		return NULL;
	}

	/* Handle action-agnostic options */
	if (param.verbose)
		daxctl_set_log_priority(ctx, LOG_DEBUG);
	if (param.human)
		flags |= UTIL_JSON_HUMAN;

	/* Handle action-specific options */
	switch (action) {
	case ACTION_RECONFIG:
		if (!param.size &&
		    !param.align &&
		    !param.mode) {
			fprintf(stderr, "error: a 'align', 'mode' or 'size' option is required\n");
			usage_with_options(u, reconfig_options);
			rc = -EINVAL;
		}
		if (param.size || param.align) {
			if (param.size)
				size = __parse_size64(param.size, &units);
			if (param.align)
				align = __parse_size64(param.align, &units);
		} else if (strcmp(param.mode, "system-ram") == 0) {
			reconfig_mode = DAXCTL_DEV_MODE_RAM;
			if (param.no_movable)
				mem_zone = MEM_ZONE_NORMAL;
		} else if (strcmp(param.mode, "devdax") == 0) {
			reconfig_mode = DAXCTL_DEV_MODE_DEVDAX;
			if (param.no_online) {
				fprintf(stderr,
					"--no-online is incompatible with --mode=devdax\n");
				rc =  -EINVAL;
			}
		}
		break;
	case ACTION_CREATE:
		if (param.input &&
		    (rc = parse_device_file(param.input)) != 0) {
			fprintf(stderr,
				"error: failed to parse device file: %s\n",
				strerror(-rc));
			break;
		}
		if (param.size)
			size = __parse_size64(param.size, &units);
		if (param.align)
			align = __parse_size64(param.align, &units);
		/* fall through */
	case ACTION_ONLINE:
		if (param.no_movable)
			mem_zone = MEM_ZONE_NORMAL;
		/* fall through */
	case ACTION_DESTROY:
	case ACTION_OFFLINE:
	case ACTION_DISABLE:
	case ACTION_ENABLE:
		/* nothing special */
		break;
	}
	if (rc) {
		usage_with_options(u, options);
		return NULL;
	}

	return argv[0];
}

static int dev_online_memory(struct daxctl_dev *dev)
{
	struct daxctl_memory *mem = daxctl_dev_get_memory(dev);
	const char *devname = daxctl_dev_get_devname(dev);
	int num_sections, num_on, rc;

	if (!mem) {
		fprintf(stderr, "%s: failed to get the memory object\n",
			devname);
		return -ENXIO;
	}

	/* get total number of sections and sections already online */
	num_sections = daxctl_memory_num_sections(mem);
	if (num_sections < 0) {
		fprintf(stderr, "%s: failed to get number of memory sections\n",
			devname);
		return num_sections;
	}

	num_on = daxctl_memory_is_online(mem);
	if (num_on < 0) {
		fprintf(stderr, "%s: failed to determine online state: %s\n",
			devname, strerror(-num_on));
		return num_on;
	}
	if (num_on)
		fprintf(stderr,
		    "%s:\n  WARNING: detected a race while onlining memory\n"
		    "  Some memory may not be in the expected zone. It is\n"
		    "  recommended to disable any other onlining mechanisms,\n"
		    "  and retry. If onlining is to be left to other agents,\n"
		    "  use the --no-online option to suppress this warning\n",
		    devname);

	if (num_on == num_sections) {
		fprintf(stderr, "%s: all memory sections (%d) already online\n",
			devname, num_on);
		return 1;
	}
	if (num_on > 0)
		fprintf(stderr, "%s: %d memory section%s already online\n",
			devname, num_on,
			num_on == 1 ? "" : "s");

	/* online the remaining sections */
	if (param.no_movable)
		rc = daxctl_memory_online_no_movable(mem);
	else
		rc = daxctl_memory_online(mem);
	if (rc < 0) {
		fprintf(stderr, "%s: failed to online memory: %s\n",
			devname, strerror(-rc));
		return rc;
	}
	if (param.verbose)
		fprintf(stderr, "%s: %d memory section%s onlined\n", devname, rc,
			rc == 1 ? "" : "s");

	/* all sections should now be online */
	num_on = daxctl_memory_is_online(mem);
	if (num_on < 0) {
		fprintf(stderr, "%s: failed to determine online state: %s\n",
			devname, strerror(-num_on));
		return num_on;
	}
	if (num_on < num_sections) {
		fprintf(stderr, "%s: failed to online %d memory sections\n",
			devname, num_sections - num_on);
		return -ENXIO;
	}

	return 0;
}

static int dev_offline_memory(struct daxctl_dev *dev)
{
	struct daxctl_memory *mem = daxctl_dev_get_memory(dev);
	const char *devname = daxctl_dev_get_devname(dev);
	int num_sections, num_on, num_off, rc;

	if (!mem) {
		fprintf(stderr, "%s: failed to get the memory object\n",
			devname);
		return -ENXIO;
	}

	/* get total number of sections and sections already offline */
	num_sections = daxctl_memory_num_sections(mem);
	if (num_sections < 0) {
		fprintf(stderr, "%s: failed to get number of memory sections\n",
			devname);
		return num_sections;
	}

	num_on = daxctl_memory_is_online(mem);
	if (num_on < 0) {
		fprintf(stderr, "%s: failed to determine online state: %s\n",
			devname, strerror(-num_on));
		return num_on;
	}

	num_off = num_sections - num_on;
	if (num_off == num_sections) {
		fprintf(stderr, "%s: all memory sections (%d) already offline\n",
			devname, num_off);
		return 1;
	}
	if (num_off)
		fprintf(stderr, "%s: %d memory section%s already offline\n",
			devname, num_off,
			num_off == 1 ? "" : "s");

	/* offline the remaining sections */
	rc = daxctl_memory_offline(mem);
	if (rc < 0) {
		fprintf(stderr, "%s: failed to offline memory: %s\n",
			devname, strerror(-rc));
		return rc;
	}
	if (param.verbose)
		fprintf(stderr, "%s: %d memory section%s offlined\n", devname, rc,
			rc == 1 ? "" : "s");

	/* all sections should now be ofline */
	num_on = daxctl_memory_is_online(mem);
	if (num_on < 0) {
		fprintf(stderr, "%s: failed to determine online state: %s\n",
			devname, strerror(-num_on));
		return num_on;
	}
	if (num_on) {
		fprintf(stderr, "%s: failed to offline %d memory sections\n",
			devname, num_on);
		return -ENXIO;
	}

	return 0;
}

static int dev_resize(struct daxctl_dev *dev, unsigned long long val)
{
	int rc;

	rc = daxctl_dev_set_size(dev, val);
	if (rc < 0)
		return rc;

	return 0;
}

static int dev_destroy(struct daxctl_dev *dev)
{
	const char *devname = daxctl_dev_get_devname(dev);
	int rc;

	if (daxctl_dev_is_enabled(dev) && !param.force) {
		fprintf(stderr, "%s is active, specify --force for deletion\n",
			devname);
		return -ENXIO;
	} else {
		rc = daxctl_dev_disable(dev);
		if (rc) {
			fprintf(stderr, "%s: disable failed: %s\n",
				daxctl_dev_get_devname(dev), strerror(-rc));
			return rc;
		}
	}

	rc = daxctl_dev_set_size(dev, 0);
	if (rc < 0)
		return rc;

	rc = daxctl_region_destroy_dev(daxctl_dev_get_region(dev), dev);
	if (rc < 0)
		return rc;

	return 0;
}

static int disable_devdax_device(struct daxctl_dev *dev)
{
	struct daxctl_memory *mem = daxctl_dev_get_memory(dev);
	const char *devname = daxctl_dev_get_devname(dev);
	int rc;

	if (mem) {
		fprintf(stderr, "%s was already in system-ram mode\n",
			devname);
		return 1;
	}
	rc = daxctl_dev_disable(dev);
	if (rc) {
		fprintf(stderr, "%s: disable failed: %s\n",
			daxctl_dev_get_devname(dev), strerror(-rc));
		return rc;
	}
	return 0;
}

static int reconfig_mode_system_ram(struct daxctl_dev *dev)
{
	int rc, skip_enable = 0;

	if (daxctl_dev_is_enabled(dev)) {
		rc = disable_devdax_device(dev);
		if (rc < 0)
			return rc;
		if (rc > 0)
			skip_enable = 1;
	}

	if (!skip_enable) {
		rc = daxctl_dev_enable_ram(dev);
		if (rc)
			return rc;
	}

	if (param.no_online)
		return 0;

	return dev_online_memory(dev);
}

static int disable_system_ram_device(struct daxctl_dev *dev)
{
	struct daxctl_memory *mem = daxctl_dev_get_memory(dev);
	const char *devname = daxctl_dev_get_devname(dev);
	int rc;

	if (!mem) {
		fprintf(stderr, "%s was already in devdax mode\n", devname);
		return 1;
	}

	if (param.force) {
		rc = dev_offline_memory(dev);
		if (rc < 0)
			return rc;
	}

	rc = daxctl_memory_is_online(mem);
	if (rc < 0) {
		fprintf(stderr, "%s: failed to determine online state: %s\n",
			devname, strerror(-rc));
		return rc;
	}
	if (rc > 0) {
		if (param.verbose) {
			fprintf(stderr, "%s: found %d memory sections online\n",
				devname, rc);
			fprintf(stderr, "%s: refusing to change modes\n",
				devname);
		}
		return -EBUSY;
	}
	rc = daxctl_dev_disable(dev);
	if (rc) {
		fprintf(stderr, "%s: disable failed: %s\n",
			daxctl_dev_get_devname(dev), strerror(-rc));
		return rc;
	}
	return 0;
}

static int reconfig_mode_devdax(struct daxctl_dev *dev)
{
	int rc;

	if (daxctl_dev_is_enabled(dev)) {
		rc = disable_system_ram_device(dev);
		if (rc)
			return rc;
	}

	rc = daxctl_dev_enable_devdax(dev);
	if (rc)
		return rc;

	return 0;
}

static int do_create(struct daxctl_region *region, long long val,
		     struct json_object **jdevs)
{
	struct json_object *jdev;
	struct daxctl_dev *dev;
	int i, rc = 0;
	long long alloc = 0;

	if (daxctl_region_create_dev(region))
		return -ENOSPC;

	dev = daxctl_region_get_dev_seed(region);
	if (!dev)
		return -ENOSPC;

	if (val == -1)
		val = daxctl_region_get_available_size(region);

	if (val <= 0)
		return -ENOSPC;

	if (align > 0) {
		rc = daxctl_dev_set_align(dev, align);
		if (rc < 0)
			return rc;
	}

	/* @maps is ordered by page_offset */
	for (i = 0; i < nmaps; i++) {
		rc = daxctl_dev_set_mapping(dev, maps[i].start, maps[i].end);
		if (rc < 0)
			return rc;
		alloc += (maps[i].end - maps[i].start + 1);
	}

	if (nmaps > 0 && val > 0 && alloc != val) {
		fprintf(stderr, "%s: allocated %lld but specified size %lld\n",
			daxctl_dev_get_devname(dev), alloc, val);
	} else {
		rc = daxctl_dev_set_size(dev, val);
		if (rc < 0)
			return rc;
	}

	rc = daxctl_dev_enable_devdax(dev);
	if (rc) {
		fprintf(stderr, "%s: enable failed: %s\n",
			daxctl_dev_get_devname(dev), strerror(-rc));
		return rc;
	}

	*jdevs = json_object_new_array();
	if (*jdevs) {
		jdev = util_daxctl_dev_to_json(dev, flags);
		if (jdev)
			json_object_array_add(*jdevs, jdev);
	}

	return 0;
}

static int do_reconfig(struct daxctl_dev *dev, enum dev_mode mode,
		struct json_object **jdevs)
{
	const char *devname = daxctl_dev_get_devname(dev);
	struct json_object *jdev;
	int rc = 0;

	if (align > 0) {
		rc = daxctl_dev_set_align(dev, align);
		if (rc < 0)
			return rc;
	}

	if (size >= 0) {
		rc = dev_resize(dev, size);
		return rc;
	}

	switch (mode) {
	case DAXCTL_DEV_MODE_RAM:
		rc = reconfig_mode_system_ram(dev);
		break;
	case DAXCTL_DEV_MODE_DEVDAX:
		rc = reconfig_mode_devdax(dev);
		break;
	default:
		fprintf(stderr, "%s: unknown mode requested: %d\n",
			devname, mode);
		rc = -EINVAL;
	}

	if (rc < 0)
		return rc;

	*jdevs = json_object_new_array();
	if (*jdevs) {
		jdev = util_daxctl_dev_to_json(dev, flags);
		if (jdev)
			json_object_array_add(*jdevs, jdev);
	}

	return 0;
}

static int do_xline(struct daxctl_dev *dev, enum device_action action)
{
	struct daxctl_memory *mem = daxctl_dev_get_memory(dev);
	const char *devname = daxctl_dev_get_devname(dev);
	int rc;

	if (!mem) {
		fprintf(stderr,
			"%s: memory operations are not applicable in devdax mode\n",
			devname);
		return -ENXIO;
	}

	switch (action) {
	case ACTION_ONLINE:
		rc = dev_online_memory(dev);
		break;
	case ACTION_OFFLINE:
		rc = dev_offline_memory(dev);
		break;
	default:
		fprintf(stderr, "%s: invalid action: %d\n", devname, action);
		rc = -EINVAL;
	}
	return rc;
}

static int do_xble(struct daxctl_dev *dev, enum device_action action)
{
	struct daxctl_memory *mem = daxctl_dev_get_memory(dev);
	const char *devname = daxctl_dev_get_devname(dev);
	int rc;

	if (mem) {
		fprintf(stderr,
			"%s: status operations are only applicable in devdax mode\n",
			devname);
		return -ENXIO;
	}

	switch (action) {
	case ACTION_ENABLE:
		rc = daxctl_dev_enable_devdax(dev);
		if (rc) {
			fprintf(stderr, "%s: enable failed: %s\n",
				daxctl_dev_get_devname(dev), strerror(-rc));
			return rc;
		}
		break;
	case ACTION_DISABLE:
		rc = daxctl_dev_disable(dev);
		if (rc) {
			fprintf(stderr, "%s: disable failed: %s\n",
				daxctl_dev_get_devname(dev), strerror(-rc));
			return rc;
		}
		break;
	default:
		fprintf(stderr, "%s: invalid action: %d\n", devname, action);
		rc = -EINVAL;
	}
	return rc;
}

static int do_xaction_region(enum device_action action,
		struct daxctl_ctx *ctx, int *processed)
{
	struct json_object *jdevs = NULL;
	struct daxctl_region *region;
	int rc = -ENXIO;

	*processed = 0;

	daxctl_region_foreach(ctx, region) {
		if (!util_daxctl_region_filter(region, param.region))
			continue;

		switch (action) {
		case ACTION_CREATE:
			rc = do_create(region, size, &jdevs);
			if (rc == 0)
				(*processed)++;
			break;
		default:
			rc = -EINVAL;
			break;
		}
	}
	free(maps);

	/*
	 * jdevs is the containing json array for all devices we are reporting
	 * on. It therefore needs to be outside the region/device iterators,
	 * and passed in to the do_<action> functions to add their objects to
	 */
	if (jdevs)
		util_display_json_array(stdout, jdevs, flags);

	return rc;
}

static int do_xaction_device(const char *device, enum device_action action,
		struct daxctl_ctx *ctx, int *processed)
{
	struct json_object *jdevs = NULL;
	struct daxctl_region *region;
	struct daxctl_dev *dev;
	int rc = -ENXIO;

	*processed = 0;

	daxctl_region_foreach(ctx, region) {
		if (!util_daxctl_region_filter(region, param.region))
			continue;

		daxctl_dev_foreach(region, dev) {
			if (!util_daxctl_dev_filter(dev, device))
				continue;

			switch (action) {
			case ACTION_RECONFIG:
				rc = do_reconfig(dev, reconfig_mode, &jdevs);
				if (rc == 0)
					(*processed)++;
				break;
			case ACTION_ONLINE:
				rc = do_xline(dev, action);
				if (rc == 0)
					(*processed)++;
				break;
			case ACTION_OFFLINE:
				rc = do_xline(dev, action);
				if (rc == 0)
					(*processed)++;
				break;
			case ACTION_ENABLE:
				rc = do_xble(dev, action);
				if (rc == 0)
					(*processed)++;
				break;
			case ACTION_DISABLE:
				rc = do_xble(dev, action);
				if (rc == 0)
					(*processed)++;
				break;
			case ACTION_DESTROY:
				rc = dev_destroy(dev);
				if (rc == 0)
					(*processed)++;
				break;
			default:
				rc = -EINVAL;
				break;
			}
		}
	}

	/*
	 * jdevs is the containing json array for all devices we are reporting
	 * on. It therefore needs to be outside the region/device iterators,
	 * and passed in to the do_<action> functions to add their objects to
	 */
	if (jdevs)
		util_display_json_array(stdout, jdevs, flags);

	return rc;
}

int cmd_create_device(int argc, const char **argv, struct daxctl_ctx *ctx)
{
	char *usage = "daxctl create-device [<options>]";
	int processed, rc;

	parse_device_options(argc, argv, ACTION_CREATE,
			create_options, usage, ctx);

	rc = do_xaction_region(ACTION_CREATE, ctx, &processed);
	if (rc < 0)
		fprintf(stderr, "error creating devices: %s\n",
				strerror(-rc));

	fprintf(stderr, "created %d device%s\n", processed,
			processed == 1 ? "" : "s");
	return rc;
}

int cmd_destroy_device(int argc, const char **argv, struct daxctl_ctx *ctx)
{
	char *usage = "daxctl destroy-device <device> [<options>]";
	const char *device = parse_device_options(argc, argv, ACTION_DESTROY,
			destroy_options, usage, ctx);
	int processed, rc;

	rc = do_xaction_device(device, ACTION_DESTROY, ctx, &processed);
	if (rc < 0)
		fprintf(stderr, "error destroying devices: %s\n",
				strerror(-rc));

	fprintf(stderr, "destroyed %d device%s\n", processed,
			processed == 1 ? "" : "s");
	return rc;
}

int cmd_reconfig_device(int argc, const char **argv, struct daxctl_ctx *ctx)
{
	char *usage = "daxctl reconfigure-device <device> [<options>]";
	const char *device = parse_device_options(argc, argv, ACTION_RECONFIG,
			reconfig_options, usage, ctx);
	int processed, rc;

	rc = do_xaction_device(device, ACTION_RECONFIG, ctx, &processed);
	if (rc < 0)
		fprintf(stderr, "error reconfiguring devices: %s\n",
				strerror(-rc));

	fprintf(stderr, "reconfigured %d device%s\n", processed,
			processed == 1 ? "" : "s");
	return rc;
}

int cmd_disable_device(int argc, const char **argv, struct daxctl_ctx *ctx)
{
	char *usage = "daxctl disable-device <device>";
	const char *device = parse_device_options(argc, argv, ACTION_DISABLE,
			disable_options, usage, ctx);
	int processed, rc;

	rc = do_xaction_device(device, ACTION_DISABLE, ctx, &processed);
	if (rc < 0)
		fprintf(stderr, "error disabling device: %s\n",
				strerror(-rc));

	fprintf(stderr, "disabled %d device%s\n", processed,
			processed == 1 ? "" : "s");
	return rc;
}

int cmd_enable_device(int argc, const char **argv, struct daxctl_ctx *ctx)
{
	char *usage = "daxctl enable-device <device>";
	const char *device = parse_device_options(argc, argv, ACTION_DISABLE,
			enable_options, usage, ctx);
	int processed, rc;

	rc = do_xaction_device(device, ACTION_ENABLE, ctx, &processed);
	if (rc < 0)
		fprintf(stderr, "error enabling device: %s\n",
				strerror(-rc));

	fprintf(stderr, "enabled %d device%s\n", processed,
			processed == 1 ? "" : "s");
	return rc;
}

int cmd_online_memory(int argc, const char **argv, struct daxctl_ctx *ctx)
{
	char *usage = "daxctl online-memory <device> [<options>]";
	const char *device = parse_device_options(argc, argv, ACTION_ONLINE,
			online_options, usage, ctx);
	int processed, rc;

	rc = do_xaction_device(device, ACTION_ONLINE, ctx, &processed);
	if (rc < 0)
		fprintf(stderr, "error onlining memory: %s\n",
				strerror(-rc));

	fprintf(stderr, "onlined memory for %d device%s\n", processed,
			processed == 1 ? "" : "s");
	return rc;
}

int cmd_offline_memory(int argc, const char **argv, struct daxctl_ctx *ctx)
{
	char *usage = "daxctl offline-memory <device> [<options>]";
	const char *device = parse_device_options(argc, argv, ACTION_OFFLINE,
			offline_options, usage, ctx);
	int processed, rc;

	rc = do_xaction_device(device, ACTION_OFFLINE, ctx, &processed);
	if (rc < 0)
		fprintf(stderr, "error offlining memory: %s\n",
				strerror(-rc));

	fprintf(stderr, "offlined memory for %d device%s\n", processed,
			processed == 1 ? "" : "s");
	return rc;
}
