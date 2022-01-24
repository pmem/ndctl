// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2015-2020 Intel Corporation. All rights reserved.
#include <limits.h>
#include <string.h>
#include <util/json.h>
#include <uuid/uuid.h>
#include <json-c/json.h>
#include <json-c/printbuf.h>
#include <daxctl/libdaxctl.h>

#include "filter.h"
#include "json.h"

struct json_object *util_daxctl_dev_to_json(struct daxctl_dev *dev,
		unsigned long flags)
{
	struct daxctl_memory *mem = daxctl_dev_get_memory(dev);
	const char *devname = daxctl_dev_get_devname(dev);
	struct json_object *jdev, *jobj, *jmappings = NULL;
	struct daxctl_mapping *mapping = NULL;
	int node, movable, align;

	jdev = json_object_new_object();
	if (!devname || !jdev)
		return NULL;

	jobj = json_object_new_string(devname);
	if (jobj)
		json_object_object_add(jdev, "chardev", jobj);

	jobj = util_json_object_size(daxctl_dev_get_size(dev), flags);
	if (jobj)
		json_object_object_add(jdev, "size", jobj);

	node = daxctl_dev_get_target_node(dev);
	if (node >= 0) {
		jobj = json_object_new_int(node);
		if (jobj)
			json_object_object_add(jdev, "target_node", jobj);
	}

	align = daxctl_dev_get_align(dev);
	if (align > 0) {
		jobj = util_json_object_size(daxctl_dev_get_align(dev), flags);
		if (jobj)
			json_object_object_add(jdev, "align", jobj);
	}

	if (mem)
		jobj = json_object_new_string("system-ram");
	else
		jobj = json_object_new_string("devdax");
	if (jobj)
		json_object_object_add(jdev, "mode", jobj);

	if (mem && daxctl_dev_get_resource(dev) != 0) {
		int num_sections = daxctl_memory_num_sections(mem);
		int num_online = daxctl_memory_is_online(mem);

		jobj = json_object_new_int(num_online);
		if (jobj)
			json_object_object_add(jdev, "online_memblocks", jobj);

		jobj = json_object_new_int(num_sections);
		if (jobj)
			json_object_object_add(jdev, "total_memblocks", jobj);

		movable = daxctl_memory_is_movable(mem);
		if (movable == 1)
			jobj = json_object_new_boolean(true);
		else if (movable == 0)
			jobj = json_object_new_boolean(false);
		else
			jobj = NULL;
		if (jobj)
			json_object_object_add(jdev, "movable", jobj);
	}

	if (!daxctl_dev_is_enabled(dev)) {
		jobj = json_object_new_string("disabled");
		if (jobj)
			json_object_object_add(jdev, "state", jobj);
	}

	if (!(flags & UTIL_JSON_DAX_MAPPINGS))
		return jdev;

	daxctl_mapping_foreach(dev, mapping) {
		struct json_object *jmapping;

		if (!jmappings) {
			jmappings = json_object_new_array();
			if (!jmappings)
				continue;

			json_object_object_add(jdev, "mappings", jmappings);
		}

		jmapping = util_daxctl_mapping_to_json(mapping, flags);
		if (!jmapping)
			continue;
		json_object_array_add(jmappings, jmapping);
	}
	return jdev;
}

struct json_object *util_daxctl_devs_to_list(struct daxctl_region *region,
		struct json_object *jdevs, const char *ident,
		unsigned long flags)
{
	struct daxctl_dev *dev;

	daxctl_dev_foreach(region, dev) {
		struct json_object *jdev;

		if (!util_daxctl_dev_filter(dev, ident))
			continue;

		if (!(flags & (UTIL_JSON_IDLE|UTIL_JSON_CONFIGURED))
				&& !daxctl_dev_get_size(dev))
			continue;

		if (!jdevs) {
			jdevs = json_object_new_array();
			if (!jdevs)
				return NULL;
		}

		jdev = util_daxctl_dev_to_json(dev, flags);
		if (!jdev) {
			json_object_put(jdevs);
			return NULL;
		}

		json_object_array_add(jdevs, jdev);
	}

	return jdevs;
}

struct json_object *util_daxctl_region_to_json(struct daxctl_region *region,
		const char *ident, unsigned long flags)
{
	unsigned long align;
	struct json_object *jregion, *jobj;
	unsigned long long available_size, size;

	jregion = json_object_new_object();
	if (!jregion)
		return NULL;

	/*
	 * The flag indicates when we are being called by an agent that
	 * already knows about the parent device information.
	 */
	if (!(flags & UTIL_JSON_DAX)) {
		/* trim off the redundant /sys/devices prefix */
		const char *path = daxctl_region_get_path(region);
		int len = strlen("/sys/devices");
		const char *trim = &path[len];

		if (strncmp(path, "/sys/devices", len) != 0)
			goto err;
		jobj = json_object_new_string(trim);
		if (!jobj)
			goto err;
		json_object_object_add(jregion, "path", jobj);
	}

	jobj = json_object_new_int(daxctl_region_get_id(region));
	if (!jobj)
		goto err;
	json_object_object_add(jregion, "id", jobj);

	size = daxctl_region_get_size(region);
	if (size < ULLONG_MAX) {
		jobj = util_json_object_size(size, flags);
		if (!jobj)
			goto err;
		json_object_object_add(jregion, "size", jobj);
	}

	available_size = daxctl_region_get_available_size(region);
	if (available_size) {
		jobj = util_json_object_size(available_size, flags);
		if (!jobj)
			goto err;
		json_object_object_add(jregion, "available_size", jobj);
	}

	align = daxctl_region_get_align(region);
	if (align < ULONG_MAX) {
		jobj = util_json_new_u64(align);
		if (!jobj)
			goto err;
		json_object_object_add(jregion, "align", jobj);
	}

	if (!(flags & UTIL_JSON_DAX_DEVS))
		return jregion;

	jobj = util_daxctl_devs_to_list(region, NULL, ident, flags);
	if (jobj)
		json_object_object_add(jregion, "devices", jobj);

	return jregion;
 err:
	json_object_put(jregion);
	return NULL;
}

struct json_object *util_daxctl_mapping_to_json(struct daxctl_mapping *mapping,
		unsigned long flags)
{
	struct json_object *jmapping = json_object_new_object();
	struct json_object *jobj;

	if (!jmapping)
		return NULL;

	jobj = util_json_object_hex(daxctl_mapping_get_offset(mapping), flags);
	if (!jobj)
		goto err;
	json_object_object_add(jmapping, "page_offset", jobj);

	jobj = util_json_object_hex(daxctl_mapping_get_start(mapping), flags);
	if (!jobj)
		goto err;
	json_object_object_add(jmapping, "start", jobj);

	jobj = util_json_object_hex(daxctl_mapping_get_end(mapping), flags);
	if (!jobj)
		goto err;
	json_object_object_add(jmapping, "end", jobj);

	jobj = util_json_object_size(daxctl_mapping_get_size(mapping), flags);
	if (!jobj)
		goto err;
	json_object_object_add(jmapping, "size", jobj);

	return jmapping;
 err:
	json_object_put(jmapping);
	return NULL;
}
