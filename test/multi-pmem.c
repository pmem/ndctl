// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2015-2020 Intel Corporation. All rights reserved.
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <syslog.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <libkmod.h>
#include <uuid/uuid.h>
#include <sys/types.h>
#include <util/size.h>
#include <linux/falloc.h>
#include <linux/version.h>
#include <ndctl/libndctl.h>
#include <ccan/array_size/array_size.h>

#include <ndctl.h>
#include <builtin.h>
#include <test.h>

#define NUM_NAMESPACES 4
#define SZ_NAMESPACE SZ_16M

static int setup_namespace(struct ndctl_region *region)
{
	struct ndctl_ctx *ctx = ndctl_region_get_ctx(region);
	const char *argv[] = {
		"__func__", "-v", "-m", "raw", "-s", "16M", "-r", "",
	};
	int argc = ARRAY_SIZE(argv);

	argv[argc - 1] = ndctl_region_get_devname(region);
	builtin_xaction_namespace_reset();
	return cmd_create_namespace(argc, argv, ctx);
}

static void destroy_namespace(struct ndctl_namespace *ndns)
{
	struct ndctl_ctx *ctx = ndctl_namespace_get_ctx(ndns);
	const char *argv[] = {
		"__func__", "-v", "-f", "",
	};
	int argc = ARRAY_SIZE(argv);

	argv[argc - 1] = ndctl_namespace_get_devname(ndns);
	builtin_xaction_namespace_reset();
	cmd_destroy_namespace(argc, argv, ctx);
}

/* Check that the namespace device is gone (if it wasn't the seed) */
static int check_deleted(struct ndctl_region *region, const char *devname,
		struct ndctl_test *test)
{
	struct ndctl_namespace *ndns;

	if (!ndctl_test_attempt(test, KERNEL_VERSION(4, 10, 0)))
		return 0;

	ndctl_namespace_foreach(region, ndns) {
		if (strcmp(devname, ndctl_namespace_get_devname(ndns)))
			continue;
		if (ndns == ndctl_region_get_namespace_seed(region))
			continue;
		fprintf(stderr, "multi-pmem: expected %s to be deleted\n",
				devname);
		return -ENXIO;
	}

	return 0;
}

static int do_multi_pmem(struct ndctl_ctx *ctx, struct ndctl_test *test)
{
	int i;
	char devname[100];
	struct ndctl_bus *bus;
	uuid_t uuid[NUM_NAMESPACES];
	struct ndctl_namespace *ndns;
	struct ndctl_dimm *dimm_target, *dimm;
	struct ndctl_region *region, *target = NULL;
	struct ndctl_namespace *namespaces[NUM_NAMESPACES];
	unsigned long long blk_avail, blk_avail_orig, expect;

	if (!ndctl_test_attempt(test, KERNEL_VERSION(4, 9, 0))) {
		ndctl_test_skip(test);
		return 77;
	}

	bus = ndctl_bus_get_by_provider(ctx, "nfit_test.0");
	if (!bus)
		return -ENXIO;

	/* disable all regions so that set_config_data commands are permitted */
	ndctl_region_foreach(bus, region)
		ndctl_region_disable_invalidate(region);

	ndctl_dimm_foreach(bus, dimm) {
		int rc = ndctl_dimm_zero_labels(dimm);

		if (rc < 0) {
			fprintf(stderr, "failed to zero %s\n",
					ndctl_dimm_get_devname(dimm));
			return rc;
		}
	}

	/*
	 * Set regions back to their default state and find our target
	 * region.
	 */
	ndctl_region_foreach(bus, region) {
		ndctl_region_enable(region);
		if (ndctl_region_get_available_size(region)
				== SZ_NAMESPACE * NUM_NAMESPACES)
			target = region;
	}

	if (!target) {
		fprintf(stderr, "multi-pmem: failed to find target region\n");
		return -ENXIO;
	}
	region = target;

	for (i = 0; i < (int) ARRAY_SIZE(uuid); i++) {
		if (setup_namespace(region) != 0) {
			fprintf(stderr, "multi-pmem: failed to setup namespace: %d\n", i);
			return -ENXIO;
		}
		sprintf(devname, "namespace%d.%d",
				ndctl_region_get_id(region), i);
		ndctl_namespace_foreach(region, ndns)
			if (strcmp(ndctl_namespace_get_devname(ndns), devname) == 0
					&& ndctl_namespace_is_enabled(ndns))
				break;
		if (!ndns) {
			fprintf(stderr, "multi-pmem: failed to find namespace: %s\n",
					devname);
			return -ENXIO;
		}
		ndctl_namespace_get_uuid(ndns, uuid[i]);
	}

	/* bounce the region and verify everything came back as expected */
	ndctl_region_disable_invalidate(region);
	ndctl_region_enable(region);

	for (i = 0; i < (int) ARRAY_SIZE(uuid); i++) {
		char uuid_str1[40], uuid_str2[40];
		uuid_t uuid_check;

		sprintf(devname, "namespace%d.%d",
				ndctl_region_get_id(region), i);
		ndctl_namespace_foreach(region, ndns)
			if (strcmp(ndctl_namespace_get_devname(ndns), devname) == 0
					&& ndctl_namespace_is_enabled(ndns))
				break;
		if (!ndns) {
			fprintf(stderr, "multi-pmem: failed to restore namespace: %s\n",
					devname);
			return -ENXIO;
		}

		ndctl_namespace_get_uuid(ndns, uuid_check);
		uuid_unparse(uuid_check, uuid_str2);
		uuid_unparse(uuid[i], uuid_str1);
		if (uuid_compare(uuid_check, uuid[i]) != 0) {
			fprintf(stderr, "multi-pmem: expected uuid[%d]: %s, got %s\n",
					i, uuid_str1, uuid_str2);
			return -ENXIO;
		}
		namespaces[i] = ndns;
	}

	/*
	 * Check that aliased blk capacity does not increase until the
	 * highest dpa pmem-namespace is deleted.
	 */
	dimm_target = ndctl_region_get_first_dimm(region);
	if (!dimm_target) {
		fprintf(stderr, "multi-pmem: failed to retrieve dimm from %s\n",
				ndctl_region_get_devname(region));
		return -ENXIO;
	}

	dimm = NULL;
	ndctl_region_foreach(bus, region) {
		if (ndctl_region_get_type(region) != ND_DEVICE_REGION_BLK)
			continue;
		ndctl_dimm_foreach_in_region(region, dimm)
			if (dimm == dimm_target)
				break;
		if (dimm)
			break;
	}

	blk_avail_orig = ndctl_region_get_available_size(region);
	for (i = 1; i < NUM_NAMESPACES - 1; i++) {
		ndns = namespaces[i];
		sprintf(devname, "%s", ndctl_namespace_get_devname(ndns));
		destroy_namespace(ndns);
		blk_avail = ndctl_region_get_available_size(region);
		if (blk_avail != blk_avail_orig) {
			fprintf(stderr, "multi-pmem: destroy %s %llx avail, expect %llx\n",
					devname, blk_avail, blk_avail_orig);
			return -ENXIO;
		}

		if (check_deleted(target, devname, test) != 0)
			return -ENXIO;
	}

	ndns = namespaces[NUM_NAMESPACES - 1];
	sprintf(devname, "%s", ndctl_namespace_get_devname(ndns));
	destroy_namespace(ndns);
	blk_avail = ndctl_region_get_available_size(region);
	expect = (SZ_NAMESPACE / ndctl_region_get_interleave_ways(target))
		* (NUM_NAMESPACES - 1) + blk_avail_orig;
	if (blk_avail != expect) {
		fprintf(stderr, "multi-pmem: destroy %s %llx avail, expect %llx\n",
				devname, blk_avail, expect);
		return -ENXIO;
	}

	if (check_deleted(target, devname, test) != 0)
		return -ENXIO;

	ndctl_bus_foreach(ctx, bus) {
		if (strncmp(ndctl_bus_get_provider(bus), "nfit_test", 9) != 0)
			continue;
		ndctl_region_foreach(bus, region)
			ndctl_region_disable_invalidate(region);
	}

	return 0;
}

int test_multi_pmem(int loglevel, struct ndctl_test *test, struct ndctl_ctx *ctx)
{
	struct kmod_module *mod;
	struct kmod_ctx *kmod_ctx;
	int err, result = EXIT_FAILURE;

	if (!ndctl_test_attempt(test, KERNEL_VERSION(4, 2, 0)))
		return 77;

	ndctl_set_log_priority(ctx, loglevel);

	err = nfit_test_init(&kmod_ctx, &mod, NULL, loglevel, test);
	if (err < 0) {
		result = 77;
		ndctl_test_skip(test);
		fprintf(stderr, "%s unavailable skipping tests\n",
				"nfit_test");
		return result;
	}

	result = do_multi_pmem(ctx, test);

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
	rc = test_multi_pmem(LOG_DEBUG, test, ctx);
	ndctl_unref(ctx);
	return ndctl_test_result(test, rc);
}
