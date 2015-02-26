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
#include <limits.h>
#include <syslog.h>
#include <libkmod.h>
#include <uuid/uuid.h>

#include <ndctl/libndctl.h>
#ifdef HAVE_NDCTL_H
#include <linux/ndctl.h>
#else
#include <ndctl.h>
#endif
#include <test-dpa-alloc.h>
#include <ccan/array_size/array_size.h>

static const char *NFIT_TEST_MODULE = "nfit_test";
static const char *NFIT_PROVIDER0 = "nfit_test.0";
static const char *NFIT_PROVIDER1 = "nfit_test.1";
#define SZ_4K 0x1000
#define NUM_NAMESPACES 4
#define DEFAULT_AVAILABLE_SLOTS 1015

struct test_dpa_namespace {
	struct ndctl_namespace *ndns;
	unsigned long long size;
	uuid_t uuid;
} namespaces[NUM_NAMESPACES];

static int do_test(struct ndctl_ctx *ctx)
{
	struct ndctl_region *region, *blk_region = NULL;
	unsigned int available_slots, i;
	struct ndctl_namespace *ndns;
	struct ndctl_dimm *dimm;
	unsigned long long size;
	struct ndctl_bus *bus;
	char uuid_str[40];
	int round;
	int rc;

	/* disable nfit_test.1, not used in this test */
	bus = ndctl_bus_get_by_provider(ctx, NFIT_PROVIDER1);
	if (!bus)
		return -ENXIO;
	ndctl_region_foreach(bus, region)
		ndctl_region_disable(region, 1);

	/* init nfit_test.0 */
	bus = ndctl_bus_get_by_provider(ctx, NFIT_PROVIDER0);
	if (!bus)
		return -ENXIO;
	ndctl_region_foreach(bus, region)
		ndctl_region_disable(region, 1);

	ndctl_dimm_foreach(bus, dimm) {
		rc = ndctl_dimm_zero_labels(dimm);
		if (rc < 0) {
			fprintf(stderr, "failed to zero %s\n",
					ndctl_dimm_get_devname(dimm));
			return rc;
		}
	}

	/* find a guinea pig BLK region */
	ndctl_region_foreach(bus, region)
		if (ndctl_region_get_type(region) == ND_DEVICE_REGION_BLOCK) {
			blk_region = region;
			break;
		}

	if (!blk_region || ndctl_region_enable(blk_region) < 0) {
		fprintf(stderr, "failed to find a usable BLK region\n");
		return -ENXIO;
	}
	region = blk_region;

	if (ndctl_region_get_available_size(region) / ND_MIN_NAMESPACE_SIZE
			< NUM_NAMESPACES) {
		fprintf(stderr, "%s insufficient available_size\n",
				ndctl_region_get_devname(region));
		return -ENXIO;
	}

	dimm = ndctl_region_get_first_dimm(region);
	if (!dimm) {
		fprintf(stderr, "%s: failed to find a dimm to test\n",
				ndctl_region_get_devname(region));
		return -ENXIO;
	}

	available_slots = ndctl_dimm_get_available_labels(dimm);
	if (available_slots < DEFAULT_AVAILABLE_SLOTS) {
		fprintf(stderr, "%s: expected %d\n",
				ndctl_dimm_get_devname(dimm),
				DEFAULT_AVAILABLE_SLOTS);
		return -ENXIO;
	}

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
		ndctl_namespace_set_size(ndns, ND_MIN_NAMESPACE_SIZE);
		rc = ndctl_namespace_enable(ndns);
		if (rc) {
			fprintf(stderr, "failed to enable %s: %d\n",
					ndctl_namespace_get_devname(ndns), rc);
			return rc;
		}
		ndctl_namespace_disable(ndns);
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
	if (available_slots != DEFAULT_AVAILABLE_SLOTS
			- ARRAY_SIZE(namespaces)) {
		fprintf(stderr, "expected %ld slots available\n",
				DEFAULT_AVAILABLE_SLOTS
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
			fprintf(stderr, "%s: set_size: %llx failed: %d\n",
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

	ndctl_region_disable(region, 1);
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
		rc = ndctl_namespace_delete(victim);
		if (rc) {
			fprintf(stderr, "failed to delete %s\n", uuid_str);
			return rc;
		}
		rc = ndctl_namespace_set_size(ndns, size
				+ ndctl_namespace_get_size(ndns));
		if (rc) {
			fprintf(stderr, "failed to merge %s\n", uuid_str);
			return rc;
		}
	}

	/* there can be only one */
	i = 0;
	ndctl_namespace_foreach(region, ndns)
		i++;
	if (i != 1) {
		fprintf(stderr, "failed to delete namespaces\n");
		return -ENXIO;
	}

	available_slots = ndctl_dimm_get_available_labels(dimm);
	if (available_slots != DEFAULT_AVAILABLE_SLOTS - 1) {
		fprintf(stderr, "mishandled slot count\n");
		return -ENXIO;
	}

	ndctl_region_foreach(bus, region)
		ndctl_region_disable(region, 1);

	return 0;
}

int test_dpa_alloc(int loglevel)
{
	struct ndctl_ctx *ctx;
	struct kmod_module *mod;
	struct kmod_ctx *kmod_ctx;
	int err, result = EXIT_FAILURE;

	err = ndctl_new(&ctx);
	if (err < 0)
		exit(EXIT_FAILURE);

	ndctl_set_log_priority(ctx, loglevel);

	kmod_ctx = kmod_new(NULL, NULL);
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
	if (err == 0)
		result = EXIT_SUCCESS;
	kmod_module_remove_module(mod, 0);

 err_module:
	kmod_unref(kmod_ctx);
 err_kmod:
	ndctl_unref(ctx);
	return result;
}

int __attribute__((weak)) main(int argc, char *argv[])
{
	return test_dpa_alloc(LOG_DEBUG);
}
