// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2015-2022 Intel Corporation. All rights reserved.
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <util/log.h>
#include <util/json.h>
#include <cxl/libcxl.h>
#include <json-c/json.h>

#include "filter.h"
#include "json.h"

static const char *which_sep(const char *filter)
{
	if (strchr(filter, ' '))
		return " ";
	if (strchr(filter, ','))
		return ",";
	return " ";
}

static struct cxl_bus *util_cxl_bus_filter(struct cxl_bus *bus,
					   const char *__ident)
{
	char *ident, *save;
	const char *arg;
	int bus_id;

	if (!__ident)
		return bus;

	ident = strdup(__ident);
	if (!ident)
		return NULL;

	for (arg = strtok_r(ident, which_sep(__ident), &save); arg;
	     arg = strtok_r(NULL, which_sep(__ident), &save)) {
		if (strcmp(arg, "all") == 0)
			break;

		if ((sscanf(arg, "%d", &bus_id) == 1 ||
		     sscanf(arg, "root%d", &bus_id) == 1) &&
		    cxl_bus_get_id(bus) == bus_id)
			break;

		if (strcmp(arg, cxl_bus_get_devname(bus)) == 0)
			break;

		if (strcmp(arg, cxl_bus_get_provider(bus)) == 0)
			break;
	}

	free(ident);
	if (arg)
		return bus;
	return NULL;
}

static struct cxl_memdev *
util_cxl_memdev_serial_filter(struct cxl_memdev *memdev, const char *__serials)
{
	unsigned long long serial = 0;
	char *serials, *save, *end;
	const char *arg;

	if (!__serials)
		return memdev;

	serials = strdup(__serials);
	if (!serials)
		return NULL;

	for (arg = strtok_r(serials, which_sep(__serials), &save); arg;
	     arg = strtok_r(NULL, which_sep(__serials), &save)) {
		serial = strtoull(arg, &end, 0);
		if (!arg[0] || end[0] != 0)
			continue;
		if (cxl_memdev_get_serial(memdev) == serial)
			break;
	}

	free(serials);
	if (arg)
		return memdev;
	return NULL;
}

struct cxl_memdev *util_cxl_memdev_filter(struct cxl_memdev *memdev,
					  const char *__ident,
					  const char *serials)
{
	char *ident, *save;
	const char *name;
	int memdev_id;

	if (!__ident)
		return util_cxl_memdev_serial_filter(memdev, serials);

	ident = strdup(__ident);
	if (!ident)
		return NULL;

	for (name = strtok_r(ident, which_sep(__ident), &save); name;
	     name = strtok_r(NULL, which_sep(__ident), &save)) {
		if (strcmp(name, "all") == 0)
			break;

		if ((sscanf(name, "%d", &memdev_id) == 1 ||
		     sscanf(name, "mem%d", &memdev_id) == 1) &&
		    cxl_memdev_get_id(memdev) == memdev_id)
			break;

		if (strcmp(name, cxl_memdev_get_devname(memdev)) == 0)
			break;
	}

	free(ident);
	if (name)
		return util_cxl_memdev_serial_filter(memdev, serials);
	return NULL;
}

static unsigned long params_to_flags(struct cxl_filter_params *param)
{
	unsigned long flags = 0;

	if (param->idle)
		flags |= UTIL_JSON_IDLE;
	if (param->human)
		flags |= UTIL_JSON_HUMAN;
	if (param->health)
		flags |= UTIL_JSON_HEALTH;
	return flags;
}

static void splice_array(struct cxl_filter_params *p, struct json_object *jobjs,
			 struct json_object *platform,
			 const char *container_name, bool do_container)
{
	size_t count;

	if (!json_object_array_length(jobjs)) {
		json_object_put(jobjs);
		return;
	}

	if (do_container) {
		struct json_object *container = json_object_new_object();

		if (!container) {
			err(p, "failed to list: %s\n", container_name);
			return;
		}

		json_object_object_add(container, container_name, jobjs);
		json_object_array_add(platform, container);
		return;
	}

	for (count = json_object_array_length(jobjs); count; count--) {
		struct json_object *jobj = json_object_array_get_idx(jobjs, 0);

		json_object_get(jobj);
		json_object_array_del_idx(jobjs, 0, 1);
		json_object_array_add(platform, jobj);
	}
	json_object_put(jobjs);
}

int cxl_filter_walk(struct cxl_ctx *ctx, struct cxl_filter_params *p)
{
	struct json_object *jplatform = json_object_new_array();
	struct json_object *jdevs = NULL, *jbuses = NULL;
	unsigned long flags = params_to_flags(p);
	struct cxl_memdev *memdev;
	int top_level_objs = 0;
	struct cxl_bus *bus;

	if (!jplatform) {
		dbg(p, "platform object allocation failure\n");
		return -ENOMEM;
	}

	jdevs = json_object_new_array();
	if (!jdevs)
		goto err;

	jbuses = json_object_new_array();
	if (!jbuses)
		goto err;

	cxl_memdev_foreach(ctx, memdev) {
		struct json_object *jdev;

		if (!util_cxl_memdev_filter(memdev, p->memdev_filter,
					    p->serial_filter))
			continue;
		if (p->memdevs) {
			jdev = util_cxl_memdev_to_json(memdev, flags);
			if (!jdev) {
				dbg(p, "memdev object allocation failure\n");
				continue;
			}
			json_object_array_add(jdevs, jdev);
		}
	}

	cxl_bus_foreach(ctx, bus) {
		struct json_object *jbus;

		if (!util_cxl_bus_filter(bus, p->bus_filter))
			continue;
		if (p->buses) {
			jbus = util_cxl_bus_to_json(bus, flags);
			if (!jbus) {
				dbg(p, "bus object allocation failure\n");
				continue;
			}
			json_object_array_add(jbuses, jbus);
		}
	}

	if (json_object_array_length(jdevs))
		top_level_objs++;
	if (json_object_array_length(jbuses))
		top_level_objs++;

	splice_array(p, jdevs, jplatform, "anon memdevs", top_level_objs > 1);
	splice_array(p, jbuses, jplatform, "buses", top_level_objs > 1);

	util_display_json_array(stdout, jplatform, flags);

	return 0;
err:
	json_object_put(jdevs);
	json_object_put(jbuses);
	json_object_put(jplatform);
	return -ENOMEM;
}
