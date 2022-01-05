/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2015-2020 Intel Corporation. All rights reserved. */
#ifndef __UTIL_JSON_H__
#define __UTIL_JSON_H__
#include <stdio.h>
#include <stdbool.h>

enum util_json_flags {
	UTIL_JSON_IDLE		= (1 << 0),
	UTIL_JSON_MEDIA_ERRORS	= (1 << 1),
	UTIL_JSON_DAX		= (1 << 2),
	UTIL_JSON_DAX_DEVS	= (1 << 3),
	UTIL_JSON_HUMAN		= (1 << 4),
	UTIL_JSON_VERBOSE	= (1 << 5),
	UTIL_JSON_CAPABILITIES	= (1 << 6),
	UTIL_JSON_CONFIGURED	= (1 << 7),
	UTIL_JSON_FIRMWARE	= (1 << 8),
	UTIL_JSON_DAX_MAPPINGS	= (1 << 9),
	UTIL_JSON_HEALTH	= (1 << 10),
};

struct json_object;
void util_display_json_array(FILE *f_out, struct json_object *jarray,
		unsigned long flags);
struct json_object *util_json_object_size(unsigned long long size,
		unsigned long flags);
struct json_object *util_json_object_hex(unsigned long long val,
		unsigned long flags);
#endif /* __UTIL_JSON_H__ */
