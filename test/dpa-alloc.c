// SPDX-License-Identifier: LGPL-2.1
// Copyright (C) 2014-2020, Intel Corporation. All rights reserved.
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include <limits.h>
#include <syslog.h>
#include <libkmod.h>
#include <uuid/uuid.h>

#include <test.h>
#include <ndctl.h>
#include <util/size.h>
#include <linux/version.h>
#include <ndctl/libndctl.h>
#include <ccan/array_size/array_size.h>

static const char *NFIT_PROVIDER0 = "nfit_test.0";
static const char *NFIT_PROVIDER1 = "nfit_test.1";
#define NUM_NAMESPACES 4

struct test_dpa_namespace {
	struct ndctl_namespace *ndns;
	unsigned long long size;
	uuid_t uuid;
} namespaces[NUM_NAMESPACES];

#define MIN_SIZE SZ_4M

static int do_test(struct ndctl_ctx *ctx, struct ndctl_test *test)
{
	unsigned int default_available_slots, available_slots, i;
	struct ndctl_region *region, *blk_region = NULL;
	struct ndctl_namespace *ndns;
	struct ndctl_dimm *dimm;
	unsigned long size;
	struct ndctl_bus *bus;
	char uuid_str[40];
	int round;
	int rc;

	/* disable nfit_test.1, not used in this test */
	bus = ndctl_bus_get_by_provider(ctx, NFIT_PROVIDER1);
	if (!bus)
		return -ENXIO;
	ndctl_region_foreach(bus, region) {
		ndctl_region_disable_invalidate(region);
		ndctl_region_set_align(region, sysconf(_SC_PAGESIZE)
				* ndctl_region_get_interleave_ways(region));
	}

	/* init nfit_test.0 */
	bus = ndctl_bus_get_by_provider(ctx, NFIT_PROVIDER0);
	if (!bus)
		return -ENXIO;
	ndctl_region_foreach(bus, region) {
		ndctl_region_disable_invalidate(region);
		ndctl_region_set_align(region, sysconf(_SC_PAGESIZE)
				* ndctl_region_get_interleave_ways(region));
	}

	ndctl_dimm_foreach(bus, dimm) {
		rc = ndctl_dimm_zero_labels(dimm);
		if (rc < 0) {
			fprintf(stderr, "failed to zero %s\n",
					ndctl_dimm_get_devname(dimm));
			return rc;
		}
	}

	/*
	 * Find a guineapig BLK region, we know that the dimm with
	 * handle==0 from nfit_test.0 always allocates from highest DPA
	 * to lowest with no excursions into BLK only ranges.
	 */
	ndctl_region_foreach(bus, region) {
		if (ndctl_region_get_type(region) != ND_DEVICE_REGION_BLK)
			continue;
		dimm = ndctl_region_get_first_dimm(region);
		if (!dimm)
			continue;
		if (ndctl_dimm_get_handle(dimm) == 0) {
			blk_region = region;
			break;
		}
	}
	if (!blk_region || ndctl_region_enable(blk_region) < 0) {
		fprintf(stderr, "failed to find a usable BLK region\n");
		return -ENXIO;
	}
	region = blk_region;

	if (ndctl_region_get_available_size(region) / MIN_SIZE < NUM_NAMESPACES) {
		fprintf(stderr, "%s insufficient available_size\n",
				ndctl_region_get_devname(region));
		return -ENXIO;
	}

	default_available_slots = ndctl_dimm_get_available_labels(dimm);

	/* grow namespaces */
	for (i = 0; i < ARRAY_SIZE(namespaces); i++) {
		uuid_t uuid;

		ndns = ndctl_region_get_namespace_seed(region);
		if (!ndns) {
			fprintf(stderr, "%s: failed to get seed: %d\n",
					ndctl_region_get_devname(region), i);
			return -ENXIO;
		}
		uuid_generate_random(uuid);
		ndctl_namespace_set_uuid(ndns, uuid);
		ndctl_namespace_set_sector_size(ndns, 512);
		ndctl_namespace_set_size(ndns, MIN_SIZE);
		rc = ndctl_namespace_enable(ndns);
		if (rc) {
			fprintf(stderr, "failed to enable %s: %d\n",
					ndctl_namespace_get_devname(ndns), rc);
			return rc;
		}
		ndctl_namespace_disable_invalidate(ndns);
		rc = ndctl_namespace_set_size(ndns, SZ_4K);
		if (rc) {
			fprintf(stderr, "failed to init %s to size: %d\n",
					ndctl_namespace_get_devname(ndns),
					SZ_4K);
			return rc;
		}
		namespaces[i].ndns = ndns;
		ndctl_namespace_get_uuid(ndns, namespaces[i].uuid);
	}

	available_slots = ndctl_dimm_get_available_labels(dimm);
	if (available_slots != default_available_slots
			- ARRAY_SIZE(namespaces)) {
		fprintf(stderr, "expected %ld slots available\n",
				default_available_slots
				- ARRAY_SIZE(namespaces));
		return -ENOSPC;
	}

	/* exhaust label space, by round-robin allocating 4K */
	round = 1;
	for (i = 0; i < available_slots; i++) {
		ndns = namespaces[i % ARRAY_SIZE(namespaces)].ndns;
		if (i % ARRAY_SIZE(namespaces) == 0)
			round++;
		size = SZ_4K * round;
		rc = ndctl_namespace_set_size(ndns, size);
		if (rc) {
			fprintf(stderr, "%s: set_size: %lx failed: %d\n",
				ndctl_namespace_get_devname(ndns), size, rc);
			return rc;
		}
	}

	/*
	 * The last namespace we updated should still be modifiable via
	 * the kernel's reserve label
	 */
	i--;
	round++;
	ndns = namespaces[i % ARRAY_SIZE(namespaces)].ndns;
	size = SZ_4K * round;
	rc = ndctl_namespace_set_size(ndns, size);
	if (rc) {
		fprintf(stderr, "%s failed to update while labels full\n",
				ndctl_namespace_get_devname(ndns));
		return rc;
	}

	round--;
	size = SZ_4K * round;
	rc = ndctl_namespace_set_size(ndns, size);
	if (rc) {
		fprintf(stderr, "%s failed to reduce size while labels full\n",
				ndctl_namespace_get_devname(ndns));
		return rc;
	}

	/* do the allocations survive a region cycle? */
	for (i = 0; i < ARRAY_SIZE(namespaces); i++) {
		ndns = namespaces[i].ndns;
		namespaces[i].size = ndctl_namespace_get_size(ndns);
		namespaces[i].ndns = NULL;
	}

	ndctl_region_disable_invalidate(region);
	rc = ndctl_region_enable(region);
	if (rc) {
		fprintf(stderr, "failed to re-enable %s: %d\n",
				ndctl_region_get_devname(region), rc);
		return rc;
	}

	ndctl_namespace_foreach(region, ndns) {
		uuid_t uuid;

		ndctl_namespace_get_uuid(ndns, uuid);
		for (i = 0; i < ARRAY_SIZE(namespaces); i++) {
			if (uuid_compare(uuid, namespaces[i].uuid) == 0) {
				namespaces[i].ndns = ndns;
				break;
			}
		}
	}

	/* validate that they all came back */
	for (i = 0; i < ARRAY_SIZE(namespaces); i++) {
		ndns = namespaces[i].ndns;
		size = ndns ? ndctl_namespace_get_size(ndns) : 0;

		if (ndns && size == namespaces[i].size)
			continue;
		uuid_unparse(namespaces[i].uuid, uuid_str);
		fprintf(stderr, "failed to recover %s\n", uuid_str);
		return -ENODEV;
	}

	/* test deletion and merging */
	ndns = namespaces[0].ndns;
	for (i = 1; i < ARRAY_SIZE(namespaces); i++) {
		struct ndctl_namespace *victim = namespaces[i].ndns;

		uuid_unparse(namespaces[i].uuid, uuid_str);
		size = ndctl_namespace_get_size(victim);
		rc = ndctl_namespace_disable(victim);
		if (rc) {
			fprintf(stderr, "failed to disable %s\n", uuid_str);
			return rc;
		}
		rc = ndctl_namespace_delete(victim);
		if (rc) {
			fprintf(stderr, "failed to delete %s\n", uuid_str);
			return rc;
		}
		size += ndctl_namespace_get_size(ndns);
		rc = ndctl_namespace_set_size(ndns, size);
		if (rc) {
			fprintf(stderr, "failed to merge %s\n", uuid_str);
			return rc;
		}
	}

	/* there can be only one */
	i = 0;
	ndctl_namespace_foreach(region, ndns) {
		unsigned long long sz = ndctl_namespace_get_size(ndns);

		if (sz) {
			i++;
			if (sz == size)
				continue;
			fprintf(stderr, "%s size: %llx expected %lx\n",
					ndctl_namespace_get_devname(ndns),
					sz, size);
			return -ENXIO;
		}
	}
	if (i != 1) {
		fprintf(stderr, "failed to delete namespaces\n");
		return -ENXIO;
	}

	available_slots = ndctl_dimm_get_available_labels(dimm);
	if (available_slots != default_available_slots - 1) {
		fprintf(stderr, "mishandled slot count\n");
		return -ENXIO;
	}

	ndctl_region_foreach(bus, region)
		ndctl_region_disable_invalidate(region);

	return 0;
}

int test_dpa_alloc(int loglevel, struct ndctl_test *test, struct ndctl_ctx *ctx)
{
	struct kmod_module *mod;
	struct kmod_ctx *kmod_ctx;
	int err, result = EXIT_FAILURE;

	if (!ndctl_test_attempt(test, KERNEL_VERSION(4, 2, 0)))
		return 77;

	ndctl_set_log_priority(ctx, loglevel);
	err = nfit_test_init(&kmod_ctx, &mod, NULL, loglevel, test);
	if (err < 0) {
		ndctl_test_skip(test);
		fprintf(stderr, "nfit_test unavailable skipping tests\n");
		return 77;
	}

	err = do_test(ctx, test);
	if (err == 0)
		result = EXIT_SUCCESS;
	kmod_module_remove_module(mod, 0);
	kmod_unref(kmod_ctx);
	return result;
}

int __attribute__((weak)) main(int argc, char *argv[])
{
	struct ndctl_test *test = ndctl_test_new(0);
	struct ndctl_ctx *ctx;
	int rc;

	if (!test) {
		fprintf(stderr, "failed to initialize test\n");
		return EXIT_FAILURE;
	}

	rc = ndctl_new(&ctx);
	if (rc)
		return ndctl_test_result(test, rc);

	rc = test_dpa_alloc(LOG_DEBUG, test, ctx);
	ndctl_unref(ctx);
	return ndctl_test_result(test, rc);
}
