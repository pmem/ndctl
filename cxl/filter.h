/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2015-2020 Intel Corporation. All rights reserved. */
#ifndef _CXL_UTIL_FILTER_H_
#define _CXL_UTIL_FILTER_H_
struct cxl_memdev *util_cxl_memdev_filter(struct cxl_memdev *memdev,
		const char *ident);
#endif /* _CXL_UTIL_FILTER_H_ */
