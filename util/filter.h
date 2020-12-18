/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2015-2020 Intel Corporation. All rights reserved. */
#ifndef _UTIL_FILTER_H_
#define _UTIL_FILTER_H_
#include <stdbool.h>
#include <ccan/list/list.h>

struct ndctl_bus *util_bus_filter(struct ndctl_bus *bus, const char *ident);
struct ndctl_region *util_region_filter(struct ndctl_region *region,
		const char *ident);
struct ndctl_namespace *util_namespace_filter(struct ndctl_namespace *ndns,
		const char *ident);
struct ndctl_dimm *util_dimm_filter(struct ndctl_dimm *dimm, const char *ident);
struct ndctl_bus *util_bus_filter_by_dimm(struct ndctl_bus *bus,
		const char *ident);
struct ndctl_bus *util_bus_filter_by_region(struct ndctl_bus *bus,
		const char *ident);
struct ndctl_bus *util_bus_filter_by_namespace(struct ndctl_bus *bus,
		const char *ident);
struct ndctl_region *util_region_filter_by_dimm(struct ndctl_region *region,
		const char *ident);
struct ndctl_dimm *util_dimm_filter_by_region(struct ndctl_dimm *dimm,
		const char *ident);
struct ndctl_dimm *util_dimm_filter_by_namespace(struct ndctl_dimm *dimm,
		const char *ident);
struct ndctl_region *util_region_filter_by_namespace(struct ndctl_region *region,
		const char *ident);
struct daxctl_dev *util_daxctl_dev_filter(struct daxctl_dev *dev,
		const char *ident);
struct daxctl_region *util_daxctl_region_filter(struct daxctl_region *region,
		const char *ident);

enum ndctl_namespace_mode util_nsmode(const char *mode);
const char *util_nsmode_name(enum ndctl_namespace_mode mode);

struct json_object;

/* json object hierarchy for the util_filter_walk() performed by cmd_list() */
struct list_filter_arg {
	struct json_object *jnamespaces;
	struct json_object *jregions;
	struct json_object *jdimms;
	struct json_object *jbuses;
	struct json_object *jregion;
	struct json_object *jbus;
	unsigned long flags;
};

struct monitor_filter_arg {
	struct list_head dimms;
	int maxfd_dimm;
	int num_dimm;
	unsigned long flags;
};

/*
 * struct util_filter_ctx - control and callbacks for util_filter_walk()
 * ->filter_bus() and ->filter_region() return bool because the
 * child-object filter routines can not be called if the parent context
 * is not established. ->filter_dimm() and ->filter_namespace() are leaf
 * objects, so no child dependencies to check.
 */
struct util_filter_ctx {
	bool (*filter_bus)(struct ndctl_bus *bus, struct util_filter_ctx *ctx);
	void (*filter_dimm)(struct ndctl_dimm *dimm, struct util_filter_ctx *ctx);
	bool (*filter_region)(struct ndctl_region *region,
			struct util_filter_ctx *ctx);
	void (*filter_namespace)(struct ndctl_namespace *ndns,
			struct util_filter_ctx *ctx);
	union {
		void *arg;
		struct list_filter_arg *list;
		struct monitor_filter_arg *monitor;
	};
};

struct util_filter_params {
	const char *bus;
	const char *region;
	const char *type;
	const char *dimm;
	const char *mode;
	const char *namespace;
	const char *numa_node;
};

struct ndctl_ctx;
int util_filter_walk(struct ndctl_ctx *ctx, struct util_filter_ctx *fctx,
		struct util_filter_params *param);
#endif
