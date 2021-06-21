// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2015-2020 Intel Corporation. All rights reserved.
#include <stdio.h>
#include <string.h>
#include <daxctl/libdaxctl.h>

#include "filter.h"

struct daxctl_dev *util_daxctl_dev_filter(struct daxctl_dev *dev,
					  const char *ident)
{
	struct daxctl_region *region = daxctl_dev_get_region(dev);
	int region_id, dev_id;

	if (!ident || strcmp(ident, "all") == 0)
		return dev;

	if (strcmp(ident, daxctl_dev_get_devname(dev)) == 0)
		return dev;

	if (sscanf(ident, "%d.%d", &region_id, &dev_id) == 2 &&
	    daxctl_region_get_id(region) == region_id &&
	    daxctl_dev_get_id(dev) == dev_id)
		return dev;

	return NULL;
}

struct daxctl_region *util_daxctl_region_filter(struct daxctl_region *region,
						const char *ident)
{
	int region_id;

	if (!ident || strcmp(ident, "all") == 0)
		return region;

	if ((sscanf(ident, "%d", &region_id) == 1 ||
	     sscanf(ident, "region%d", &region_id) == 1) &&
	    daxctl_region_get_id(region) == region_id)
		return region;

	return NULL;
}
