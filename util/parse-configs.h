// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2021, FUJITSU LIMITED. ALL rights reserved.

#include <dirent.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <util/util.h>

enum parse_conf_type {
	CONFIG_STRING,
	CONFIG_END,
	MONITOR_CALLBACK,
};

int filter_conf_files(const struct dirent *dir);

struct config;
typedef int parse_conf_cb(const struct config *, const char *config_file);

struct config {
	enum parse_conf_type type;
	const char *key;
	void *value;
	void *defval;
	parse_conf_cb *callback;
};

#define check_vtype(v, type) ( BUILD_BUG_ON_ZERO(!__builtin_types_compatible_p(typeof(v), type)) + v )

#define CONF_END() { .type = CONFIG_END }
#define CONF_STR(k,v,d) \
	{ .type = CONFIG_STRING, .key = (k), .value = check_vtype(v, const char **), .defval = (d) }
#define CONF_MONITOR(k,f) \
	{ .type = MONITOR_CALLBACK, .key = (k), .callback = (f)}

int parse_configs_prefix(const char *config_path, const char *prefix,
			 const struct config *configs);
