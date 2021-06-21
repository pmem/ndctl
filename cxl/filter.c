// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2015-2020 Intel Corporation. All rights reserved.
#include <stdio.h>
#include <string.h>
#include <cxl/libcxl.h>
#include "filter.h"

struct cxl_memdev *util_cxl_memdev_filter(struct cxl_memdev *memdev,
                                         const char *ident)
{
       int memdev_id;

       if (!ident || strcmp(ident, "all") == 0)
               return memdev;

       if (strcmp(ident, cxl_memdev_get_devname(memdev)) == 0)
               return memdev;

       if ((sscanf(ident, "%d", &memdev_id) == 1
                       || sscanf(ident, "mem%d", &memdev_id) == 1)
                       && cxl_memdev_get_id(memdev) == memdev_id)
               return memdev;

       return NULL;
}
