/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2021 Intel Corporation. All rights reserved. */
#ifndef _CXL_UTIL_FILTER_H_
#define _CXL_UTIL_FILTER_H_

#include <stdbool.h>
#include <util/log.h>

struct cxl_filter_params {
	const char *memdev_filter;
	bool memdevs;
	bool idle;
	bool human;
	bool health;
	struct log_ctx ctx;
};

struct cxl_memdev *util_cxl_memdev_filter(struct cxl_memdev *memdev,
					  const char *ident);
int cxl_filter_walk(struct cxl_ctx *ctx, struct cxl_filter_params *param);
#endif /* _CXL_UTIL_FILTER_H_ */
