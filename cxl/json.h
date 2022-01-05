/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2015-2020 Intel Corporation. All rights reserved. */
#ifndef __CXL_UTIL_JSON_H__
#define __CXL_UTIL_JSON_H__
struct cxl_memdev;
struct json_object *util_cxl_memdev_to_json(struct cxl_memdev *memdev,
		unsigned long flags);
#endif /* __CXL_UTIL_JSON_H__ */
