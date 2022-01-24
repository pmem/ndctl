// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2015-2020 Intel Corporation. All rights reserved.
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

int cxl_filter_walk(struct cxl_ctx *ctx, struct cxl_filter_params *p)
{
	struct json_object *jplatform = json_object_new_array();
	unsigned long flags = params_to_flags(p);
	struct cxl_memdev *memdev;

	if (!jplatform) {
		dbg(p, "platform object allocation failure\n");
		return -ENOMEM;
	}

	cxl_memdev_foreach(ctx, memdev) {
		struct json_object *jdev;

		if (!util_cxl_memdev_filter(memdev, p->memdev_filter, p->serial_filter))
			continue;
		if (p->memdevs) {
			jdev = util_cxl_memdev_to_json(memdev, flags);
			if (!jdev) {
				dbg(p, "memdev object allocation failure\n");
				continue;
			}
			json_object_array_add(jplatform, jdev);
		}
	}

	util_display_json_array(stdout, jplatform, flags);

	return 0;
}
