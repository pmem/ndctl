// SPDX-License-Identifier: LGPL-2.1
// Copyright (C) 2014-2020, Intel Corporation. All rights reserved.
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <syslog.h>
#include <libkmod.h>
#include <util/log.h>
#include <util/sysfs.h>
#include <linux/version.h>

#include <ccan/array_size/array_size.h>
#include <ndctl/libndctl.h>
#include <builtin.h>
#include <ndctl.h>
#include <test.h>

#define DIMM_PATH "/sys/devices/platform/nfit_test.0/nfit_test_dimm/test_dimm0"

static int reset_bus(struct ndctl_bus *bus)
{
	struct ndctl_region *region;
	struct ndctl_dimm *dimm;

	/* disable all regions so that set_config_data commands are permitted */
	ndctl_region_foreach(bus, region)
		ndctl_region_disable_invalidate(region);

	ndctl_dimm_foreach(bus, dimm) {
		if (!ndctl_dimm_read_labels(dimm))
			return -ENXIO;
		ndctl_dimm_disable(dimm);
		ndctl_dimm_init_labels(dimm, NDCTL_NS_VERSION_1_2);
		ndctl_dimm_enable(dimm);
	}

	/* set regions back to their default state */
	ndctl_region_foreach(bus, region) {
		ndctl_region_enable(region);
		ndctl_region_set_align(region, sysconf(_SC_PAGESIZE)
				* ndctl_region_get_interleave_ways(region));
	}
	return 0;
}

static int set_dimm_response(const char *dimm_path, int cmd, int error_code,
		struct log_ctx *log_ctx)
{
	char path[1024], buf[SYSFS_ATTR_SIZE];
	int rc;

	if (error_code) {
		sprintf(path, "%s/fail_cmd", dimm_path);
		sprintf(buf, "%#x\n", 1 << cmd);
		rc = __sysfs_write_attr(log_ctx, path, buf);
		if (rc)
			goto out;
		sprintf(path, "%s/fail_cmd_code", dimm_path);
		sprintf(buf, "%d\n", error_code);
		rc = __sysfs_write_attr(log_ctx, path, buf);
		if (rc)
			goto out;
	} else {
		sprintf(path, "%s/fail_cmd", dimm_path);
		sprintf(buf, "0\n");
		rc = __sysfs_write_attr(log_ctx, path, buf);
		if (rc)
			goto out;
	}
out:
	if (rc < 0)
		fprintf(stderr, "%s failed, cmd: %d code: %d\n",
				__func__, cmd, error_code);
	return 0;
}

static int dimms_disable(struct ndctl_bus *bus)
{
	struct ndctl_dimm *dimm;

	ndctl_dimm_foreach(bus, dimm) {
		int rc = ndctl_dimm_disable(dimm);

		if (rc) {
			fprintf(stderr, "dimm: %s failed to disable: %d\n",
				ndctl_dimm_get_devname(dimm), rc);
			return rc;
		}
	}
	return 0;
}

static int test_dimms_enable(struct ndctl_bus *bus, struct ndctl_dimm *victim,
		bool expect)
{
	struct ndctl_dimm *dimm;

	ndctl_dimm_foreach(bus, dimm) {
		int rc = ndctl_dimm_enable(dimm);

		if (((expect != (rc == 0)) && (dimm == victim))
				|| (rc && dimm != victim)) {
			bool __expect = true;

			if (dimm == victim)
				__expect = expect;
			fprintf(stderr, "fail expected %s enable %s victim: %s rc: %d\n",
					ndctl_dimm_get_devname(dimm),
					__expect ? "success" : "failure",
					ndctl_dimm_get_devname(victim), rc);
			return -ENXIO;
		}
	}
	return 0;
}

static int test_regions_enable(struct ndctl_bus *bus,
		struct ndctl_dimm *victim, struct ndctl_region *victim_region,
		bool region_expect, int namespace_count)
{
	struct ndctl_region *region;

	ndctl_region_foreach(bus, region) {
		struct ndctl_namespace *ndns;
		struct ndctl_dimm *dimm;
		bool has_victim = false;
		int rc, count = 0;

		ndctl_dimm_foreach_in_region(region, dimm) {
			if (dimm == victim) {
				has_victim = true;
				break;
			}
		}

		rc = ndctl_region_enable(region);
		fprintf(stderr, "region: %s enable: %d has_victim: %d\n",
				ndctl_region_get_devname(region), rc, has_victim);
		if (((region_expect != (rc == 0)) && has_victim)
				|| (rc && !has_victim)) {
			bool __expect = true;

			if (has_victim)
				__expect = region_expect;
			fprintf(stderr, "%s: fail expected enable: %s with %s\n",
					ndctl_region_get_devname(region),
					__expect ? "success" : "failure",
					ndctl_dimm_get_devname(victim));
			return -ENXIO;
		}
		if (region != victim_region)
			continue;
		ndctl_namespace_foreach(region, ndns) {
			if (ndctl_namespace_is_enabled(ndns)) {
				fprintf(stderr, "%s: enabled, expected disabled\n",
						ndctl_namespace_get_devname(ndns));
				return -ENXIO;
			}
			fprintf(stderr, "%s: %s: size: %lld\n", __func__,
					ndctl_namespace_get_devname(ndns),
					ndctl_namespace_get_size(ndns));
			count++;
		}
		if (count != namespace_count) {
			fprintf(stderr, "%s: fail expected %d namespaces got %d\n",
					ndctl_region_get_devname(region),
					namespace_count, count);
			return -ENXIO;
		}
	}
	return 0;
}

static int do_test(struct ndctl_ctx *ctx, struct ndctl_test *test)
{
	struct ndctl_bus *bus = ndctl_bus_get_by_provider(ctx, "nfit_test.0");
	struct ndctl_region *region, *victim_region = NULL;
	struct ndctl_dimm *dimm, *victim = NULL;
	char path[1024], buf[SYSFS_ATTR_SIZE];
	struct log_ctx log_ctx;
	unsigned int handle;
	int rc, err = 0;

	if (!ndctl_test_attempt(test, KERNEL_VERSION(4, 9, 0)))
		return 77;

	if (!bus)
		return -ENXIO;

	log_init(&log_ctx, "test/dsm-fail", "NDCTL_TEST");

	if (reset_bus(bus)) {
		fprintf(stderr, "failed to read labels\n");
		return -ENXIO;
	}

	sprintf(path, "%s/handle", DIMM_PATH);
	rc = __sysfs_read_attr(&log_ctx, path, buf);
	if (rc) {
		fprintf(stderr, "failed to retrieve test dimm handle\n");
		return -ENXIO;
	}

	handle = strtoul(buf, NULL, 0);

	ndctl_dimm_foreach(bus, dimm)
		if (ndctl_dimm_get_handle(dimm) == handle) {
			victim = dimm;
			break;
		}

	if (!victim) {
		fprintf(stderr, "failed to find victim dimm\n");
		return -ENXIO;
	}
	fprintf(stderr, "victim: %s\n", ndctl_dimm_get_devname(victim));

	ndctl_region_foreach(bus, region) {
		if (ndctl_region_get_type(region) != ND_DEVICE_REGION_PMEM)
			continue;
		ndctl_dimm_foreach_in_region(region, dimm) {
			const char *argv[] = {
				"__func__", "-v", "-r",
				ndctl_region_get_devname(region),
				"-s", "4M", "-m", "raw",
			};
			struct ndctl_namespace *ndns;
			int count, i;

			if (dimm != victim)
				continue;
			/*
			 * Validate that we only have the one seed
			 * namespace, and then create one so that we can
			 * verify namespace enumeration while locked.
			 */
			count = 0;
			ndctl_namespace_foreach(region, ndns)
				count++;
			if (count != 1) {
				fprintf(stderr, "%s: found %d namespaces expected 1\n",
						ndctl_region_get_devname(region),
						count);
				rc = -ENXIO;
				goto out;
			}
			if (ndctl_region_get_size(region)
					!= ndctl_region_get_available_size(region)) {
				fprintf(stderr, "%s: expected empty region\n",
						ndctl_region_get_devname(region));
				rc = -ENXIO;
				goto out;
			}
			for (i = 0; i < 2; i++) {
				builtin_xaction_namespace_reset();
				rc = cmd_create_namespace(ARRAY_SIZE(argv), argv,
						ndctl_region_get_ctx(region));
				if (rc) {
					fprintf(stderr, "%s: failed to create namespace\n",
							ndctl_region_get_devname(region));
					rc = -ENXIO;
					goto out;
				}
			}
			victim_region = region;
		}
		if (victim_region)
			break;
	}

	/* disable all regions so that we can disable a dimm */
	ndctl_region_foreach(bus, region)
		ndctl_region_disable_invalidate(region);

	rc = dimms_disable(bus);
	if (rc)
		goto out;


	rc = set_dimm_response(DIMM_PATH, ND_CMD_GET_CONFIG_SIZE, -EACCES,
			&log_ctx);
	if (rc)
		goto out;
	rc = test_dimms_enable(bus, victim, true);
	if (rc)
		goto out;
	rc = test_regions_enable(bus, victim, victim_region, true, 2);
	if (rc)
		goto out;
	rc = set_dimm_response(DIMM_PATH, ND_CMD_GET_CONFIG_SIZE, 0, &log_ctx);
	if (rc)
		goto out;

	ndctl_region_foreach(bus, region)
		ndctl_region_disable_invalidate(region);
	rc = dimms_disable(bus);
	if (rc)
		goto out;

	rc = set_dimm_response(DIMM_PATH, ND_CMD_GET_CONFIG_DATA, -EACCES,
			&log_ctx);
	if (rc)
		goto out;

	rc = test_dimms_enable(bus, victim, false);
	if (rc)
		goto out;
	rc = test_regions_enable(bus, victim, victim_region, false, 0);
	if (rc)
		goto out;
	rc = set_dimm_response(DIMM_PATH, ND_CMD_GET_CONFIG_DATA, 0, &log_ctx);
	if (rc)
		goto out;
	rc = dimms_disable(bus);
	if (rc)
		goto out;

 out:
	err = rc;
	sprintf(path, "%s/fail_cmd", DIMM_PATH);
	sprintf(buf, "0\n");
	rc = __sysfs_write_attr(&log_ctx, path, buf);
	if (rc)
		fprintf(stderr, "%s: failed to clear fail_cmd mask\n",
				ndctl_dimm_get_devname(victim));
	rc = ndctl_dimm_enable(victim);
	if (rc) {
		fprintf(stderr, "failed to enable victim: %s after clearing error\n",
				ndctl_dimm_get_devname(victim));
		rc = -ENXIO;
	}
	reset_bus(bus);

	if (rc)
		err = rc;
	return err;
}

int test_dsm_fail(int loglevel, struct ndctl_test *test, struct ndctl_ctx *ctx)
{
	struct kmod_module *mod;
	struct kmod_ctx *kmod_ctx;
	int result = EXIT_FAILURE, err;

	ndctl_set_log_priority(ctx, loglevel);
	err = nfit_test_init(&kmod_ctx, &mod, NULL, loglevel, test);
	if (err < 0) {
		result = 77;
		ndctl_test_skip(test);
		fprintf(stderr, "%s unavailable skipping tests\n",
				"nfit_test");
		return result;
	}

	result = do_test(ctx, test);
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
	rc = test_dsm_fail(LOG_DEBUG, test, ctx);
	ndctl_unref(ctx);
	return ndctl_test_result(test, rc);
}
