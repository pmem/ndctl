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
#ifndef __NDCTL_JSON_H__
#define __NDCTL_JSON_H__
#include <stdio.h>
#include <stdbool.h>
#include <ndctl/libndctl.h>

enum util_json_flags {
	UTIL_JSON_IDLE = (1 << 0),
	UTIL_JSON_MEDIA_ERRORS = (1 << 1),
	UTIL_JSON_DAX = (1 << 2),
};

struct json_object;
void util_display_json_array(FILE *f_out, struct json_object *jarray, int jflag);
struct json_object *util_bus_to_json(struct ndctl_bus *bus);
struct json_object *util_dimm_to_json(struct ndctl_dimm *dimm);
struct json_object *util_mapping_to_json(struct ndctl_mapping *mapping);
struct json_object *util_namespace_to_json(struct ndctl_namespace *ndns,
		unsigned long flags);
struct daxctl_region;
struct daxctl_dev;
struct json_object *util_region_badblocks_to_json(struct ndctl_region *region,
		unsigned int *bb_count, unsigned long flags);
struct json_object *util_daxctl_region_to_json(struct daxctl_region *region,
		const char *ident, unsigned long flags);
struct json_object *util_daxctl_dev_to_json(struct daxctl_dev *dev);
struct json_object *util_daxctl_devs_to_list(struct daxctl_region *region,
		struct json_object *jdevs, const char *ident,
		unsigned long flags);
#ifdef HAVE_NDCTL_SMART
struct json_object *util_dimm_health_to_json(struct ndctl_dimm *dimm);
#else
static inline struct json_object *util_dimm_health_to_json(
		struct ndctl_dimm *dimm)
{
	return NULL;
}
#endif
#endif /* __NDCTL_JSON_H__ */
