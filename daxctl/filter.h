/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2015-2020 Intel Corporation. All rights reserved. */
#ifndef _DAXCTL_UTIL_FILTER_H_
#define _DAXCTL_UTIL_FILTER_H_
#include <stdbool.h>
#include <ccan/list/list.h>

struct daxctl_dev *util_daxctl_dev_filter(struct daxctl_dev *dev,
		const char *ident);
struct daxctl_region *util_daxctl_region_filter(struct daxctl_region *region,
		const char *ident);
#endif /* _DAXCTL_UTIL_FILTER_H_ */
