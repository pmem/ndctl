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
 *                                 (a)                 (b)       DIMM   BLK-REGION
 *            +-------------------+--------+--------+--------+
 *  +------+  |       pm0.0       | blk2.0 | pm1.0  | blk2.1 | 000:0000  region2
 *  | imc0 +--+- - - region0- - - +--------+        +--------+
 *  +--+---+  |       pm0.0       | blk3.0 | pm1.0  | blk3.1 | 000:0001  region3
 *     |      +-------------------+--------v        v--------+
 *  +--+---+                               |                 |
 *  | cpu0 |                                     region1
 *  +--+---+                               |                 |
 *     |      +----------------------------^        ^--------+
 *  +--+---+  |           blk4.0           | pm1.0  | blk4.0 | 000:0100  region4
 *  | imc1 +--+----------------------------|        +--------+
 *  +------+  |           blk5.0           | pm1.0  | blk5.0 | 000:0101  region5
 *            +----------------------------+--------+--------+
 *
 *  *) In this layout we have four dimms and two memory controllers in one
 *     socket.  Each block-accessible range on each dimm is allocated a
 *     "region" index and each interleave set (cross dimm span) is allocated a
 *     "region" index.
 *
 *  *) The first portion of dimm-000:0000 and dimm-000:0001 are
 *     interleaved as REGION0.  Some of that interleave set is reclaimed as
 *     block space starting at offregion (a) into each dimm.  In that reclaimed
 *     space we create two block-mode-namespaces from the dimms'
 *     control-regions/block-data-windows blk2.0 and blk3.0 respectively.
 *
 *  *) In the last portion of dimm-000:0000 and dimm-000:0001 we have an
 *     interleave set, REGION1, that spans those two dimms as well as
 *     dimm-000:0100 and dimm-000:0101.  Some of REGION1 is reclaimed as block
 *     space and the remainder is instantiated as namespace blk1.0.
 *
 *  *) The portion of dimm-000:0100 and dimm-000:0101 that do not
 *     participate in REGION1 are instantiated as blk4.0 and blk5.0, but note
 *     that those block namespaces also incorporate the unused portion of
 *     REGION1 (starting at offregion (b)) in those dimms respectively.
 */

static const char *NFIT_TEST_MODULE = "nfit_test";
static const char *NFIT_PROVIDER = "nfit_test.0";
#define DIMM_HANDLE(n, s, i, c, d) \
	(((n & 0xfff) << 16) | ((s & 0xf) << 12) | ((i & 0xf) << 8) \
	 | ((c & 0xf) << 4) | (d & 0xf))
static unsigned int dimms[] = {
	DIMM_HANDLE(0, 0, 0, 0, 0),
	DIMM_HANDLE(0, 0, 0, 0, 1),
	DIMM_HANDLE(0, 0, 1, 0, 0),
	DIMM_HANDLE(0, 0, 1, 0, 1),
};

static struct region {
	unsigned int id;
	unsigned int interleave_ways;
	char *type;
} regions[] = {
	{ 0, 2, "pm" },
	{ 1, 4, "pm" },
	{ 2, 1, "block" },
	{ 3, 1, "block" },
	{ 4, 1, "block" },
	{ 5, 1, "block" },
};

static struct ndctl_bus *get_bus_by_provider(struct ndctl_ctx *ctx,
		const char *provider)
{
	struct ndctl_bus *bus;

        ndctl_bus_foreach(ctx, bus)
		if (strcmp(NFIT_PROVIDER, ndctl_bus_get_provider(bus)) == 0)
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



static int do_test(struct ndctl_ctx *ctx)
{
	struct ndctl_bus *bus = get_bus_by_provider(ctx, NFIT_PROVIDER);
	unsigned int i;

	if (!bus)
		return -ENXIO;

	for (i = 0; i < ARRAY_SIZE(dimms); i++) {
		struct ndctl_dimm *dimm = get_dimm_by_handle(bus, dimms[i]);

		if (!dimm) {
			fprintf(stderr, "failed to find dimm: %d\n", dimms[i]);
			return -ENXIO;
		}
	}

	for (i = 0; i < ARRAY_SIZE(regions); i++) {
		struct ndctl_region *region;

		region = get_region_by_id(bus, regions[i].id);
		if (!region) {
			fprintf(stderr, "failed to find region: %d\n", regions[i].id);
			return -ENXIO;
		}
		if (strcmp(ndctl_region_get_type(region), regions[i].type) != 0) {
			fprintf(stderr, "region expected type: %s got: %s\n",
				ndctl_region_get_type(region), regions[i].type);
			return -ENXIO;
		}
		if (ndctl_region_get_interleave_ways(region) != regions[i].interleave_ways) {
			fprintf(stderr, "region expected interleave_ways: %d got: %d\n",
					ndctl_region_get_interleave_ways(region),
					regions[i].interleave_ways);
			return -ENXIO;
		}
	}

	return 0;
}

int main(int argc, char *argv[])
{
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

	err = do_test(ctx);
	if (err < 0)
		fprintf(stderr, "ndctl-test failed: %d\n", err);
	else
		result = EXIT_SUCCESS;
	kmod_module_remove_module(mod, 0);

 err_module:
	kmod_unref(kmod_ctx);
 err_kmod:
	ndctl_unref(ctx);
	return result;
}
