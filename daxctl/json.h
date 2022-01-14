/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2015-2020 Intel Corporation. All rights reserved. */
#ifndef __DAXCTL_JSON_H__
#define __DAXCTL_JSON_H__
#include <daxctl/libdaxctl.h>

struct json_object *util_daxctl_mapping_to_json(struct daxctl_mapping *mapping,
		unsigned long flags);
struct daxctl_region;
struct daxctl_dev;
struct json_object *util_daxctl_region_to_json(struct daxctl_region *region,
		const char *ident, unsigned long flags);
struct json_object *util_daxctl_dev_to_json(struct daxctl_dev *dev,
		unsigned long flags);
struct json_object *util_daxctl_devs_to_list(struct daxctl_region *region,
		struct json_object *jdevs, const char *ident,
		unsigned long flags);
#endif /*  __CXL_UTIL_JSON_H__ */
