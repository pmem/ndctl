/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2021 Intel Corporation. All rights reserved. */
#ifndef _CXL_UTIL_FILTER_H_
#define _CXL_UTIL_FILTER_H_

#include <stdbool.h>
#include <util/log.h>

struct cxl_filter_params {
	const char *memdev_filter;
	const char *serial_filter;
	const char *bus_filter;
	const char *port_filter;
	const char *endpoint_filter;
	const char *decoder_filter;
	const char *region_filter;
	bool single;
	bool endpoints;
	bool decoders;
	bool regions;
	bool targets;
	bool memdevs;
	bool ports;
	bool buses;
	bool idle;
	bool human;
	bool health;
	bool partition;
	struct log_ctx ctx;
};

struct cxl_memdev *util_cxl_memdev_filter(struct cxl_memdev *memdev,
					  const char *__ident,
					  const char *serials);
struct cxl_port *util_cxl_port_filter_by_memdev(struct cxl_port *port,
						const char *ident,
						const char *serial);
struct cxl_region *util_cxl_region_filter(struct cxl_region *region,
					    const char *__ident);

enum cxl_port_filter_mode {
	CXL_PF_SINGLE,
	CXL_PF_ANCESTRY,
};

struct cxl_port *util_cxl_port_filter(struct cxl_port *port, const char *ident,
				      enum cxl_port_filter_mode mode);
struct cxl_bus *util_cxl_bus_filter(struct cxl_bus *bus, const char *__ident);
struct cxl_endpoint *util_cxl_endpoint_filter(struct cxl_endpoint *endpoint,
					      const char *__ident);
struct cxl_target *util_cxl_target_filter_by_memdev(struct cxl_target *target,
						    const char *ident,
						    const char *serial);
struct cxl_dport *util_cxl_dport_filter_by_memdev(struct cxl_dport *dport,
						  const char *ident,
						  const char *serial);
struct cxl_decoder *util_cxl_decoder_filter(struct cxl_decoder *decoder,
					    const char *__ident);
int cxl_filter_walk(struct cxl_ctx *ctx, struct cxl_filter_params *param);
bool cxl_filter_has(const char *needle, const char *__filter);
#endif /* _CXL_UTIL_FILTER_H_ */
