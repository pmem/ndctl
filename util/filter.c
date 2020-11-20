// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2015-2020 Intel Corporation. All rights reserved.
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <unistd.h>
#include <sys/stat.h>
#include <util/util.h>
#include <sys/types.h>
#include <ndctl/ndctl.h>
#include <util/filter.h>
#include <ndctl/libndctl.h>
#include <daxctl/libdaxctl.h>

struct ndctl_bus *util_bus_filter(struct ndctl_bus *bus, const char *__ident)
{
	char *end = NULL, *ident, *save;
	unsigned long bus_id, id;
	const char *provider, *devname, *name;

	if (!__ident)
		return bus;

	ident = strdup(__ident);
	if (!ident)
		return NULL;

	for (name = strtok_r(ident, " ", &save); name;
			name = strtok_r(NULL, " ", &save)) {
		if (strcmp(name, "all") == 0)
			break;

		bus_id = strtoul(ident, &end, 0);
		if (end == ident || end[0])
			bus_id = ULONG_MAX;

		provider = ndctl_bus_get_provider(bus);
		devname = ndctl_bus_get_devname(bus);
		id = ndctl_bus_get_id(bus);

		if (bus_id < ULONG_MAX && bus_id == id)
			break;

		if (bus_id == ULONG_MAX && (strcmp(provider, name) == 0
				|| strcmp(devname, name) == 0))
			break;
	}
	free(ident);

	if (name)
		return bus;
	return NULL;
}

struct ndctl_region *util_region_filter(struct ndctl_region *region,
		const char *__ident)
{
	char *end = NULL, *ident, *save;
	const char *name, *region_name;
	unsigned long region_id, id;

	if (!__ident)
		return region;

	ident = strdup(__ident);
	if (!ident)
		return NULL;

	for (name = strtok_r(ident, " ", &save); name;
			name = strtok_r(NULL, " ", &save)) {
		if (strcmp(name, "all") == 0)
			break;

		region_id = strtoul(ident, &end, 0);
		if (end == ident || end[0])
			region_id = ULONG_MAX;

		region_name = ndctl_region_get_devname(region);
		id = ndctl_region_get_id(region);

		if (region_id < ULONG_MAX && region_id == id)
			break;

		if (region_id == ULONG_MAX && strcmp(region_name, name) == 0)
			break;
	}
	free(ident);

	if (name)
		return region;
	return NULL;
}

struct ndctl_namespace *util_namespace_filter(struct ndctl_namespace *ndns,
		const char *__ident)
{
	struct ndctl_region *region = ndctl_namespace_get_region(ndns);
	unsigned long region_id, ndns_id;
	const char *name;
	char *ident, *save;

	if (!__ident)
		return ndns;

	ident = strdup(__ident);
	if (!ident)
		return NULL;

	for (name = strtok_r(ident, " ", &save); name;
			name = strtok_r(NULL, " ", &save)) {
		if (strcmp(name, "all") == 0)
			break;

		if (strcmp(name, ndctl_namespace_get_devname(ndns)) == 0)
			break;

		if (sscanf(name, "%ld.%ld", &region_id, &ndns_id) == 2
				&& ndctl_region_get_id(region) == region_id
				&& ndctl_namespace_get_id(ndns) == ndns_id)
			break;
	}
	free(ident);

	if (name)
		return ndns;
	return NULL;
}

struct ndctl_dimm *util_dimm_filter(struct ndctl_dimm *dimm,
		const char *__ident)
{
	char *end = NULL, *ident, *save;
	const char *name, *dimm_name;
	unsigned long dimm_id, id;

	if (!__ident)
		return dimm;

	ident = strdup(__ident);
	if (!ident)
		return NULL;

	for (name = strtok_r(ident, " ", &save); name;
			name = strtok_r(NULL, " ", &save)) {
		if (strcmp(name, "all") == 0)
			break;

		dimm_id = strtoul(ident, &end, 0);
		if (end == ident || end[0])
			dimm_id = ULONG_MAX;

		dimm_name = ndctl_dimm_get_devname(dimm);
		id = ndctl_dimm_get_id(dimm);

		if (dimm_id < ULONG_MAX && dimm_id == id)
			break;

		if (dimm_id == ULONG_MAX && strcmp(dimm_name, name) == 0)
			break;
	}
	free(ident);

	if (name)
		return dimm;
	return NULL;
}

struct ndctl_bus *util_bus_filter_by_dimm(struct ndctl_bus *bus,
		const char *ident)
{
	struct ndctl_dimm *dimm;

	if (!ident || strcmp(ident, "all") == 0)
		return bus;

	ndctl_dimm_foreach(bus, dimm)
		if (util_dimm_filter(dimm, ident))
			return bus;
	return NULL;
}

struct ndctl_bus *util_bus_filter_by_region(struct ndctl_bus *bus,
		const char *ident)
{
	struct ndctl_region *region;

	if (!ident || strcmp(ident, "all") == 0)
		return bus;

	ndctl_region_foreach(bus, region)
		if (util_region_filter(region, ident))
			return bus;
	return NULL;
}

struct ndctl_bus *util_bus_filter_by_namespace(struct ndctl_bus *bus,
		const char *ident)
{
	struct ndctl_region *region;
	struct ndctl_namespace *ndns;

	if (!ident || strcmp(ident, "all") == 0)
		return bus;

	ndctl_region_foreach(bus, region)
		ndctl_namespace_foreach(region, ndns)
			if (util_namespace_filter(ndns, ident))
				return bus;
	return NULL;
}

struct ndctl_region *util_region_filter_by_dimm(struct ndctl_region *region,
		const char *ident)
{
	struct ndctl_dimm *dimm;

	if (!ident || strcmp(ident, "all") == 0)
		return region;

	ndctl_dimm_foreach_in_region(region, dimm)
		if (util_dimm_filter(dimm, ident))
			return region;

	return NULL;
}

struct ndctl_dimm *util_dimm_filter_by_region(struct ndctl_dimm *dimm,
		const char *ident)
{
	struct ndctl_bus *bus = ndctl_dimm_get_bus(dimm);
	struct ndctl_region *region;
	struct ndctl_dimm *check;

	if (!ident || strcmp(ident, "all") == 0)
		return dimm;

	ndctl_region_foreach(bus, region) {
		if (!util_region_filter(region, ident))
			continue;
		ndctl_dimm_foreach_in_region(region, check)
			if (check == dimm)
				return dimm;
	}

	return NULL;
}

struct ndctl_dimm *util_dimm_filter_by_namespace(struct ndctl_dimm *dimm,
		const char *ident)
{
	struct ndctl_bus *bus = ndctl_dimm_get_bus(dimm);
	struct ndctl_namespace *ndns;
	struct ndctl_region *region;
	struct ndctl_dimm *check;

	if (!ident || strcmp(ident, "all") == 0)
		return dimm;

	ndctl_region_foreach(bus, region) {
		ndctl_namespace_foreach(region, ndns) {
			if (!util_namespace_filter(ndns, ident))
				continue;
			ndctl_dimm_foreach_in_region(region, check)
				if (check == dimm)
					return dimm;
		}
	}

	return NULL;
}

struct ndctl_dimm *util_dimm_filter_by_numa_node(struct ndctl_dimm *dimm,
		int numa_node)
{
	struct ndctl_bus *bus = ndctl_dimm_get_bus(dimm);
	struct ndctl_region *region;
	struct ndctl_dimm *check;

	if (numa_node == NUMA_NO_NODE)
		return dimm;

	ndctl_region_foreach(bus, region)
		ndctl_dimm_foreach_in_region(region, check)
			if (check == dimm &&
			    ndctl_region_get_numa_node(region) == numa_node)
				return dimm;

	return NULL;
}

struct ndctl_region *util_region_filter_by_namespace(struct ndctl_region *region,
		const char *ident)
{
	struct ndctl_namespace *ndns;

	if (!ident || strcmp(ident, "all") == 0)
		return region;

	ndctl_namespace_foreach(region, ndns)
		if (util_namespace_filter(ndns, ident))
			return region;
	return NULL;
}

struct daxctl_dev *util_daxctl_dev_filter(struct daxctl_dev *dev,
		const char *ident)
{
	struct daxctl_region *region = daxctl_dev_get_region(dev);
	int region_id, dev_id;

	if (!ident || strcmp(ident, "all") == 0)
		return dev;

	if (strcmp(ident, daxctl_dev_get_devname(dev)) == 0)
		return dev;

	if (sscanf(ident, "%d.%d", &region_id, &dev_id) == 2
			&& daxctl_region_get_id(region) == region_id
			&& daxctl_dev_get_id(dev) == dev_id)
		return dev;

	return NULL;
}

struct daxctl_region *util_daxctl_region_filter(struct daxctl_region *region,
		const char *ident)
{
	int region_id;

	if (!ident || strcmp(ident, "all") == 0)
		return region;

	if ((sscanf(ident, "%d", &region_id) == 1
			|| sscanf(ident, "region%d", &region_id) == 1)
			&& daxctl_region_get_id(region) == region_id)
		return region;

	return NULL;
}

enum ndctl_namespace_mode util_nsmode(const char *mode)
{
	if (!mode)
		return NDCTL_NS_MODE_UNKNOWN;
	if (strcasecmp(mode, "memory") == 0)
		return NDCTL_NS_MODE_FSDAX;
	if (strcasecmp(mode, "fsdax") == 0)
		return NDCTL_NS_MODE_FSDAX;
	if (strcasecmp(mode, "sector") == 0)
		return NDCTL_NS_MODE_SECTOR;
	if (strcasecmp(mode, "safe") == 0)
		return NDCTL_NS_MODE_SECTOR;
	if (strcasecmp(mode, "dax") == 0)
		return NDCTL_NS_MODE_DEVDAX;
	if (strcasecmp(mode, "devdax") == 0)
		return NDCTL_NS_MODE_DEVDAX;
	if (strcasecmp(mode, "raw") == 0)
		return NDCTL_NS_MODE_RAW;

	return NDCTL_NS_MODE_UNKNOWN;
}

const char *util_nsmode_name(enum ndctl_namespace_mode mode)
{
	static const char * const modes[] = {
		[NDCTL_NS_MODE_FSDAX] = "fsdax",
		[NDCTL_NS_MODE_DEVDAX] = "devdax",
		[NDCTL_NS_MODE_RAW] = "raw",
		[NDCTL_NS_MODE_SECTOR] = "sector",
		[NDCTL_NS_MODE_UNKNOWN] = "unknown",
	};

	return modes[mode];
}

int util_filter_walk(struct ndctl_ctx *ctx, struct util_filter_ctx *fctx,
		struct util_filter_params *param)
{
	struct ndctl_bus *bus;
	unsigned int type = 0;
	int numa_node = NUMA_NO_NODE;
	char *end = NULL;

	if (param->type && (strcmp(param->type, "pmem") != 0
				&& strcmp(param->type, "blk") != 0)) {
		error("unknown type \"%s\" must be \"pmem\" or \"blk\"\n",
				param->type);
		return -EINVAL;
	}

	if (param->type) {
		if (strcmp(param->type, "pmem") == 0)
			type = ND_DEVICE_REGION_PMEM;
		else
			type = ND_DEVICE_REGION_BLK;
	}

	if (param->mode && util_nsmode(param->mode) == NDCTL_NS_MODE_UNKNOWN) {
		error("invalid mode: '%s'\n", param->mode);
		return -EINVAL;
	}

	if (param->numa_node && strcmp(param->numa_node, "all") != 0) {
		struct stat st;

		if (stat("/sys/devices/system/node", &st) != 0) {
			error("This system does not support NUMA");
			return -EINVAL;
		}

		numa_node = strtol(param->numa_node, &end, 0);
		if (end == param->numa_node || end[0]) {
			error("invalid numa_node: '%s'\n", param->numa_node);
			return -EINVAL;
		}
	}

	ndctl_bus_foreach(ctx, bus) {
		struct ndctl_region *region;
		struct ndctl_dimm *dimm;

		if (!util_bus_filter(bus, param->bus)
				|| !util_bus_filter_by_dimm(bus, param->dimm)
				|| !util_bus_filter_by_region(bus, param->region)
				|| !util_bus_filter_by_namespace(bus, param->namespace))
			continue;

		if (!fctx->filter_bus(bus, fctx))
			continue;

		ndctl_dimm_foreach(bus, dimm) {
			if (!fctx->filter_dimm)
				break;

			if (!util_dimm_filter(dimm, param->dimm)
					|| !util_dimm_filter_by_region(dimm,
						param->region)
					|| !util_dimm_filter_by_namespace(dimm,
						param->namespace)
					|| !util_dimm_filter_by_numa_node(dimm,
						numa_node))
				continue;

			fctx->filter_dimm(dimm, fctx);
		}

		ndctl_region_foreach(bus, region) {
			struct ndctl_namespace *ndns;

			if (!util_region_filter(region, param->region)
					|| !util_region_filter_by_dimm(region,
						param->dimm)
					|| !util_region_filter_by_namespace(region,
						param->namespace))
				continue;

			/*
			 * if numa_node attribute is not available for regions
			 * (which is true for pre 5.4 kernels), don't skip the
			 * region if namespace is also requested, let the
			 * namespace filter handle the NUMA node filtering.
			 */
			if (numa_node != NUMA_NO_NODE &&
			    !ndctl_region_has_numa(region) &&
			    !fctx->filter_namespace) {
				fprintf(stderr,
					"This kernel does not provide NUMA node information per-region\n");
				continue;
			}

			if (ndctl_region_has_numa(region) &&
			    numa_node != NUMA_NO_NODE &&
			    ndctl_region_get_numa_node(region) != numa_node)
				continue;

			if (type && ndctl_region_get_type(region) != type)
				continue;

			if (!fctx->filter_region(region, fctx))
				continue;

			ndctl_namespace_foreach(region, ndns) {
				enum ndctl_namespace_mode mode;

				if (!fctx->filter_namespace)
					break;
				if (!util_namespace_filter(ndns, param->namespace))
					continue;

				mode = ndctl_namespace_get_mode(ndns);
				if (param->mode && util_nsmode(param->mode) != mode)
					continue;

				if (numa_node != NUMA_NO_NODE &&
				    ndctl_namespace_get_numa_node(ndns) != numa_node)
					continue;

				fctx->filter_namespace(ndns, fctx);
			}
		}
	}
	return 0;
}
