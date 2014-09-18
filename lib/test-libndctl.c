/*
 * libndctl: helper library for the nd (nvdimm, nfit-defined, persistent
 *           memory, ...) sub-system.
 *
 * Copyright (c) 2014, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU Lesser General Public License,
 * version 2.1, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
 */
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include <syslog.h>
#include <libkmod.h>

#include <ccan/array_size/array_size.h>
#include <ndctl/libndctl.h>

/*
 * Kernel provider "nfit_test.0" produces an NFIT with the following attributes:
 *
 *                              (a)               (b)           DIMM   BLK-REGION
 *           +-------------------+--------+--------+--------+
 * +------+  |       pm0.0       | blk2.0 | pm1.0  | blk2.1 |    0      region2
 * | imc0 +--+- - - region0- - - +--------+        +--------+
 * +--+---+  |       pm0.0       | blk3.0 | pm1.0  | blk3.1 |    1      region3
 *    |      +-------------------+--------v        v--------+
 * +--+---+                               |                 |
 * | cpu0 |                                     region1
 * +--+---+                               |                 |
 *    |      +----------------------------^        ^--------+
 * +--+---+  |           blk4.0           | pm1.0  | blk4.0 |    2      region4
 * | imc1 +--+----------------------------|        +--------+
 * +------+  |           blk5.0           | pm1.0  | blk5.0 |    3      region5
 *           +----------------------------+--------+--------+
 *
 * *) In this layout we have four dimms and two memory controllers in one
 *    socket.  Each unique interface ("block" or "pmem") to DPA space
 *    is identified by a region device with a dynamically assigned id.
 *
 * *) The first portion of dimm0 and dimm1 are interleaved as REGION0.
 *    A single "pmem" namespace is created in the REGION0-"spa"-range
 *    that spans dimm0 and dimm1 with a user-specified name of "pm0.0".
 *    Some of that interleaved "spa" range is reclaimed as "bdw"
 *    accessed space starting at offset (a) into each dimm.  In that
 *    reclaimed space we create two "bdw" "namespaces" from REGION2 and
 *    REGION3 where "blk2.0" and "blk3.0" are just human readable names
 *    that could be set to any user-desired name in the label.
 *
 * *) In the last portion of dimm0 and dimm1 we have an interleaved
 *    "spa" range, REGION1, that spans those two dimms as well as dimm2
 *    and dimm3.  Some of REGION1 allocated to a "pmem" namespace named
 *    "pm1.0" the rest is reclaimed in 4 "bdw" namespaces (for each
 *    dimm in the interleave set), "blk2.1", "blk3.1", "blk4.0", and
 *    "blk5.0".
 *
 * *) The portion of dimm2 and dimm3 that do not participate in the
 *    REGION1 interleaved "spa" range (i.e. the DPA address below
 *    offset (b) are also included in the "blk4.0" and "blk5.0"
 *    namespaces.  Note, that this example shows that "bdw" namespaces
 *    don't need to be contiguous in DPA-space.
 *
 * Kernel provider "nfit_test.1" produces an NFIT with the following attributes:
 *
 * region2
 * +---------------------+
 * |---------------------|
 * ||       pm2.0       ||
 * |---------------------|
 * +---------------------+
 *
 * *) Describes a simple system-physical-address range with no backing
 *    dimm or interleave description.
 */

static const char *NFIT_TEST_MODULE = "nfit_test";
static const char *NFIT_PROVIDER0 = "nfit_test.0";
static const char *NFIT_PROVIDER1 = "nfit_test.1";

struct dimm {
	unsigned int handle;
	unsigned int phys_id;
};

#define DIMM_HANDLE(n, s, i, c, d) \
	(((n & 0xfff) << 16) | ((s & 0xf) << 12) | ((i & 0xf) << 8) \
	 | ((c & 0xf) << 4) | (d & 0xf))
static struct dimm dimms[] = {
	{ DIMM_HANDLE(0, 0, 0, 0, 0), 0, },
	{ DIMM_HANDLE(0, 0, 0, 0, 1), 1, },
	{ DIMM_HANDLE(0, 0, 1, 0, 0), 2, },
	{ DIMM_HANDLE(0, 0, 1, 0, 1), 3, },
};

struct region {
	unsigned int id;
	unsigned int spa_index;
	unsigned int interleave_ways;
	char *type;
};

struct namespace {
	unsigned int id;
	char *type;
};

static struct region regions0[] = {
	{ 0, 1, 2, "pmem" },
	{ 1, 2, 4, "pmem" },
	{ 2, 0, 1, "block" },
	{ 3, 0, 1, "block" },
	{ 4, 0, 1, "block" },
	{ 5, 0, 1, "block" },
};

static struct region regions1[] = {
	{ 6, 1, 0, "pmem" },
};

static struct namespace namespaces1[] = {
	{ 0, "namespace_io" },
};

static struct ndctl_bus *get_bus_by_provider(struct ndctl_ctx *ctx,
		const char *provider)
{
	struct ndctl_bus *bus;

        ndctl_bus_foreach(ctx, bus)
		if (strcmp(provider, ndctl_bus_get_provider(bus)) == 0)
			return bus;

	return NULL;
}

static struct ndctl_dimm *get_dimm_by_handle(struct ndctl_bus *bus, unsigned int handle)
{
	struct ndctl_dimm *dimm;

	ndctl_dimm_foreach(bus, dimm)
		if (ndctl_dimm_get_handle(dimm) == handle)
			return dimm;

	return NULL;
}

static struct ndctl_region *get_region_by_id(struct ndctl_bus *bus,
		unsigned int id)
{
	struct ndctl_region *region;

	ndctl_region_foreach(bus, region)
		if (ndctl_region_get_id(region) == id)
			return region;

	return NULL;
}

static struct ndctl_namespace *get_namespace_by_id(struct ndctl_region *region,
		unsigned int id)
{
	struct ndctl_namespace *ndns;

	ndctl_namespace_foreach(region, ndns)
		if (ndctl_namespace_get_id(ndns) == id)
			return ndns;

	return NULL;
}

static int check_regions(struct ndctl_bus *bus, struct region *regions, int n)
{
	int i;

	for (i = 0; i < n; i++) {
		struct ndctl_region *region;

		region = get_region_by_id(bus, regions[i].id);
		if (!region) {
			fprintf(stderr, "failed to find region: %d\n", regions[i].id);
			return -ENXIO;
		}
		if (strcmp(ndctl_region_get_type_name(region), regions[i].type) != 0) {
			fprintf(stderr, "region%d expected type: %s got: %s\n",
					regions[i].id, regions[i].type,
					ndctl_region_get_type_name(region));
			return -ENXIO;
		}
		if (ndctl_region_get_interleave_ways(region) != regions[i].interleave_ways) {
			fprintf(stderr, "region%d expected interleave_ways: %d got: %d\n",
					regions[i].id, regions[i].interleave_ways,
					ndctl_region_get_interleave_ways(region));
			return -ENXIO;
		}
		if (ndctl_region_get_spa_index(region) != regions[i].spa_index) {
			fprintf(stderr, "region%d expected spa_index: %d got: %d\n",
					regions[i].id, regions[i].spa_index,
					ndctl_region_get_spa_index(region));
			return -ENXIO;
		}
	}

	return 0;
}

static int check_namespaces(struct ndctl_region *region,
		struct namespace *namespaces, int n)
{
	int i;

	if (!region)
		return -ENXIO;

	for (i = 0; i < n; i++) {
		struct ndctl_namespace *ndns;

		ndns = get_namespace_by_id(region, namespaces[i].id);
		if (!ndns) {
			fprintf(stderr, "failed to find namespace: %d\n",
					namespaces[i].id);
			return -ENXIO;
		}
		if (strcmp(ndctl_namespace_get_type_name(ndns),
					namespaces[i].type) != 0) {
			fprintf(stderr, "namespace expected type: %s got: %s\n",
					ndctl_namespace_get_type_name(ndns),
					namespaces[i].type);
			return -ENXIO;
		}
	}

	return 0;
}

static int do_test0(struct ndctl_ctx *ctx)
{
	struct ndctl_bus *bus = get_bus_by_provider(ctx, NFIT_PROVIDER0);
	unsigned int i;

	if (!bus)
		return -ENXIO;

	for (i = 0; i < ARRAY_SIZE(dimms); i++) {
		struct ndctl_dimm *dimm = get_dimm_by_handle(bus, dimms[i].handle);

		if (!dimm) {
			fprintf(stderr, "failed to find dimm: %d\n", dimms[i].phys_id);
			return -ENXIO;
		}

		if (ndctl_dimm_get_phys_id(dimm) != dimms[i].phys_id) {
			fprintf(stderr, "dimm%d expected phys_id: %d got: %d\n",
					i, dimms[i].phys_id,
					ndctl_dimm_get_phys_id(dimm));
			return -ENXIO;
		}
	}

	return check_regions(bus, regions0, ARRAY_SIZE(regions0));
}

static int do_test1(struct ndctl_ctx *ctx)
{
	struct ndctl_bus *bus = get_bus_by_provider(ctx, NFIT_PROVIDER1);
	struct ndctl_region *region;
	int rc;

	if (!bus)
		return -ENXIO;

	rc = check_regions(bus, regions1, ARRAY_SIZE(regions1));
	if (rc)
		return rc;

	region = get_region_by_id(bus, regions1[0].id);

	return check_namespaces(region, namespaces1, ARRAY_SIZE(namespaces1));
}

typedef int (*do_test_fn)(struct ndctl_ctx *ctx);
static do_test_fn do_test[] = {
	do_test0,
	do_test1,
};

int main(int argc, char *argv[])
{
	unsigned int i;
	struct ndctl_ctx *ctx;
	struct kmod_module *mod;
	struct kmod_ctx *kmod_ctx;
	int err, result = EXIT_FAILURE;
	const char *null_config = NULL;

	err = ndctl_new(&ctx);
	if (err < 0)
		exit(EXIT_FAILURE);

	ndctl_set_log_priority(ctx, LOG_DEBUG);

	kmod_ctx = kmod_new(NULL, &null_config);
	if (!kmod_ctx)
		goto err_kmod;

	err = kmod_module_new_from_name(kmod_ctx, NFIT_TEST_MODULE, &mod);
	if (err < 0)
		goto err_module;

	err = kmod_module_probe_insert_module(mod, KMOD_PROBE_APPLY_BLACKLIST,
			NULL, NULL, NULL, NULL);
	if (err < 0)
		goto err_module;

	for (i = 0; i < ARRAY_SIZE(do_test); i++) {
		err = do_test[i](ctx);
		if (err < 0) {
			fprintf(stderr, "ndctl-test%d failed: %d\n", i, err);
			break;
		}
	}

	if (i >= ARRAY_SIZE(do_test))
		result = EXIT_SUCCESS;
	kmod_module_remove_module(mod, 0);

 err_module:
	kmod_unref(kmod_ctx);
 err_kmod:
	ndctl_unref(ctx);
	return result;
}
