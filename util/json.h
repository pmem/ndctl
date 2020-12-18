/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2015-2020 Intel Corporation. All rights reserved. */
#ifndef __NDCTL_JSON_H__
#define __NDCTL_JSON_H__
#include <stdio.h>
#include <stdbool.h>
#include <ndctl/libndctl.h>
#include <daxctl/libdaxctl.h>
#include <ccan/short_types/short_types.h>

enum util_json_flags {
	UTIL_JSON_IDLE		= (1 << 0),
	UTIL_JSON_MEDIA_ERRORS	= (1 << 1),
	UTIL_JSON_DAX		= (1 << 2),
	UTIL_JSON_DAX_DEVS	= (1 << 3),
	UTIL_JSON_HUMAN		= (1 << 4),
	UTIL_JSON_VERBOSE	= (1 << 5),
	UTIL_JSON_CAPABILITIES	= (1 << 6),
	UTIL_JSON_CONFIGURED	= (1 << 7),
	UTIL_JSON_FIRMWARE	= (1 << 8),
	UTIL_JSON_DAX_MAPPINGS	= (1 << 9),
};

struct json_object;
void util_display_json_array(FILE *f_out, struct json_object *jarray,
		unsigned long flags);
struct json_object *util_bus_to_json(struct ndctl_bus *bus,
		unsigned long flags);
struct json_object *util_dimm_to_json(struct ndctl_dimm *dimm,
		unsigned long flags);
struct json_object *util_mapping_to_json(struct ndctl_mapping *mapping,
		unsigned long flags);
struct json_object *util_daxctl_mapping_to_json(struct daxctl_mapping *mapping,
		unsigned long flags);
struct json_object *util_namespace_to_json(struct ndctl_namespace *ndns,
		unsigned long flags);
struct json_object *util_badblock_rec_to_json(u64 block, u64 count,
		unsigned long flags);
struct daxctl_region;
struct daxctl_dev;
struct json_object *util_region_badblocks_to_json(struct ndctl_region *region,
		unsigned int *bb_count, unsigned long flags);
struct json_object *util_daxctl_region_to_json(struct daxctl_region *region,
		const char *ident, unsigned long flags);
struct json_object *util_daxctl_dev_to_json(struct daxctl_dev *dev,
		unsigned long flags);
struct json_object *util_daxctl_devs_to_list(struct daxctl_region *region,
		struct json_object *jdevs, const char *ident,
		unsigned long flags);
struct json_object *util_json_object_size(unsigned long long size,
		unsigned long flags);
struct json_object *util_json_object_hex(unsigned long long val,
		unsigned long flags);
struct json_object *util_dimm_health_to_json(struct ndctl_dimm *dimm);
struct json_object *util_dimm_firmware_to_json(struct ndctl_dimm *dimm,
		unsigned long flags);
struct json_object *util_region_capabilities_to_json(struct ndctl_region *region);
#endif /* __NDCTL_JSON_H__ */
