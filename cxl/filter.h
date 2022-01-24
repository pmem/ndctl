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
	bool single;
	bool endpoints;
	bool decoders;
	bool memdevs;
	bool ports;
	bool buses;
	bool idle;
	bool human;
	bool health;
	struct log_ctx ctx;
};

struct cxl_memdev *util_cxl_memdev_filter(struct cxl_memdev *memdev,
					  const char *__ident,
					  const char *serials);
int cxl_filter_walk(struct cxl_ctx *ctx, struct cxl_filter_params *param);
bool cxl_filter_has(const char *needle, const char *__filter);
#endif /* _CXL_UTIL_FILTER_H_ */
