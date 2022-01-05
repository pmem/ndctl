/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2015-2020 Intel Corporation. All rights reserved. */
#ifndef __NDCTL_UTIL_JSON_H__
#define __NDCTL_UTIL_JSON_H__
#include <ndctl/libndctl.h>
#include <ccan/short_types/short_types.h>

struct json_object *util_namespace_to_json(struct ndctl_namespace *ndns,
		unsigned long flags);
struct json_object *util_badblock_rec_to_json(u64 block, u64 count,
		unsigned long flags);
struct json_object *util_region_badblocks_to_json(struct ndctl_region *region,
		unsigned int *bb_count, unsigned long flags);
struct json_object *util_bus_to_json(struct ndctl_bus *bus,
		unsigned long flags);
struct json_object *util_dimm_to_json(struct ndctl_dimm *dimm,
		unsigned long flags);
struct json_object *util_mapping_to_json(struct ndctl_mapping *mapping,
		unsigned long flags);
struct json_object *util_dimm_health_to_json(struct ndctl_dimm *dimm);
struct json_object *util_dimm_firmware_to_json(struct ndctl_dimm *dimm,
		unsigned long flags);
struct json_object *util_region_capabilities_to_json(struct ndctl_region *region);
#endif /* __NDCTL_UTIL_JSON_H__ */
