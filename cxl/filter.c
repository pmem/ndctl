// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2015-2020 Intel Corporation. All rights reserved.
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <cxl/libcxl.h>
#include "filter.h"

struct cxl_memdev *util_cxl_memdev_filter(struct cxl_memdev *memdev,
					  const char *__ident)
{
	char *ident, *save;
	const char *name;
	int memdev_id;

	if (!__ident)
		return memdev;

	ident = strdup(__ident);
	if (!ident)
		return NULL;

	for (name = strtok_r(ident, " ", &save); name;
	     name = strtok_r(NULL, " ", &save)) {
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
		return memdev;
	return NULL;
}
