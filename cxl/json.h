/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2015-2022 Intel Corporation. All rights reserved. */
#ifndef __CXL_UTIL_JSON_H__
#define __CXL_UTIL_JSON_H__
struct cxl_memdev;
struct json_object *util_cxl_memdev_to_json(struct cxl_memdev *memdev,
		unsigned long flags);
struct cxl_bus;
struct json_object *util_cxl_bus_to_json(struct cxl_bus *bus,
					 unsigned long flags);
struct cxl_port;
struct json_object *util_cxl_port_to_json(struct cxl_port *port,
					  unsigned long flags);
struct json_object *util_cxl_endpoint_to_json(struct cxl_endpoint *endpoint,
					      unsigned long flags);
#endif /* __CXL_UTIL_JSON_H__ */
