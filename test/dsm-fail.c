/*
 * Copyright (c) 2014-2016, Intel Corporation.
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
#include <ctype.h>
#include <errno.h>
#include <syslog.h>
#include <libkmod.h>
#include <util/log.h>
#include <util/sysfs.h>
#include <linux/version.h>

#include <ccan/array_size/array_size.h>
#include <ndctl/libndctl.h>
#ifdef HAVE_NDCTL_H
#include <linux/ndctl.h>
#else
#include <ndctl.h>
#endif
#include <test.h>

#define DIMM_PATH "/sys/devices/platform/nfit_test.0/nfit_test_dimm/test_dimm0"

static void reset_bus(struct ndctl_bus *bus)
{
	struct ndctl_region *region;
	struct ndctl_dimm *dimm;

	/* disable all regions so that set_config_data commands are permitted */
	ndctl_region_foreach(bus, region)
		ndctl_region_disable_invalidate(region);

	ndctl_dimm_foreach(bus, dimm)
		ndctl_dimm_zero_labels(dimm);

	/* set regions back to their default state */
	ndctl_region_foreach(bus, region)
		ndctl_region_enable(region);
}

static int do_test(struct ndctl_ctx *ctx, struct ndctl_test *test)
{
	struct ndctl_bus *bus = ndctl_bus_get_by_provider(ctx, "nfit_test.0");
	struct ndctl_dimm *dimm, *victim = NULL;
	char path[1024], buf[SYSFS_ATTR_SIZE];
	struct ndctl_region *region;
	struct log_ctx log_ctx;
	unsigned int handle;
	int rc, err = 0;

	if (!ndctl_test_attempt(test, KERNEL_VERSION(4, 9, 0)))
		return 77;

	if (!bus)
		return -ENXIO;

	log_init(&log_ctx, "test/dsm-fail", "NDCTL_TEST");

	ndctl_bus_wait_probe(bus);

	/* disable all regions so that we can disable a dimm */
	ndctl_region_foreach(bus, region)
		ndctl_region_disable_invalidate(region);

	sprintf(path, "%s/handle", DIMM_PATH);
	rc = __sysfs_read_attr(&log_ctx, path, buf);
	if (rc) {
		fprintf(stderr, "failed to retrieve test dimm handle\n");
		return -ENXIO;
	}

	handle = strtoul(buf, NULL, 0);

	ndctl_dimm_foreach(bus, dimm) {
		if (ndctl_dimm_get_handle(dimm) == handle)
			victim = dimm;

		if (ndctl_dimm_disable(dimm)) {
			fprintf(stderr, "failed to disable: %s\n",
					ndctl_dimm_get_devname(dimm));
			return -ENXIO;
		}
	}

	if (!victim) {
		fprintf(stderr, "failed to find victim dimm\n");
		return -ENXIO;
	}
	fprintf(stderr, "victim: %s\n", ndctl_dimm_get_devname(victim));

	sprintf(path, "%s/fail_cmd", DIMM_PATH);
	sprintf(buf, "%#x\n", 1 << ND_CMD_GET_CONFIG_SIZE);
	rc = __sysfs_write_attr(&log_ctx, path, buf);
	if (rc) {
		fprintf(stderr, "failed to set fail cmd mask\n");
		return -ENXIO;
	}

	ndctl_dimm_foreach(bus, dimm) {
		rc = ndctl_dimm_enable(dimm);
		fprintf(stderr, "dimm: %s enable: %d\n",
				ndctl_dimm_get_devname(dimm), rc);
		if ((rc == 0) == (dimm == victim)) {
			fprintf(stderr, "fail expected %s enable %s victim: %s\n",
					ndctl_dimm_get_devname(dimm),
					(dimm == victim) ? "failure" : "success",
					ndctl_dimm_get_devname(victim));
			err = -ENXIO;
			goto out;
		}
	}

	ndctl_region_foreach(bus, region) {
		bool has_victim = false;

		ndctl_dimm_foreach_in_region(region, dimm) {
			if (dimm == victim) {
				has_victim = true;
				break;
			}
		}

		rc = ndctl_region_enable(region);
		fprintf(stderr, "region: %s enable: %d has_victim: %d\n",
				ndctl_region_get_devname(region), rc, has_victim);
		if ((rc == 0) == has_victim) {
			fprintf(stderr, "fail expected %s enable %s with %s disabled\n",
					ndctl_region_get_devname(region),
					has_victim ? "failure" : "success",
					ndctl_dimm_get_devname(victim));
			err = -ENXIO;
			goto out;
		}
	}

 out:
	sprintf(buf, "0\n");
	rc = __sysfs_write_attr(&log_ctx, path, buf);
	if (rc) {
		fprintf(stderr, "%s: failed to clear fail_cmd mask\n",
				ndctl_dimm_get_devname(victim));
		err = -ENXIO;
	}
	rc = ndctl_dimm_enable(victim);
	if (rc) {
		fprintf(stderr, "failed to enable victim: %s after clearing error\n",
				ndctl_dimm_get_devname(victim));
		err = -ENXIO;
	}
	reset_bus(bus);

	return err;
}

int test_dsm_fail(int loglevel, struct ndctl_test *test, struct ndctl_ctx *ctx)
{
	struct kmod_module *mod;
	struct kmod_ctx *kmod_ctx;
	int result = EXIT_FAILURE, err;

	ndctl_set_log_priority(ctx, loglevel);
	err = nfit_test_init(&kmod_ctx, &mod, loglevel);
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
