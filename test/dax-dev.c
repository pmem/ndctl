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

#include <test.h>
#include <linux/version.h>
#include <ndctl/libndctl.h>

struct ndctl_namespace *ndctl_get_test_dev(struct ndctl_ctx *ctx)
{
	char path[256];
	const char *bdev;
	int fd, rc = -ENXIO;
	struct ndctl_bus *bus;
	struct ndctl_dax *dax;
	struct ndctl_pfn *pfn;
	struct ndctl_namespace *ndns;
	enum ndctl_namespace_mode mode;
	struct ndctl_region *region, *min = NULL;

	bus = ndctl_bus_get_by_provider(ctx, "e820");
	if (!bus)
		goto out;

	ndctl_region_foreach(bus, region) {
		if (!min) {
			min = region;
			continue;
		}
		if (ndctl_region_get_id(region) < ndctl_region_get_id(min))
			min = region;
	}
	if (!min)
		goto out;
	region = min;

	/* attempt to re-enable the region if a previous test disabled it */
	ndctl_region_enable(region);

	ndns = ndctl_namespace_get_first(region);
	if (!ndns)
		goto out;

	rc = ndctl_namespace_enable(ndns);
	if (rc)
		goto out;

	mode = ndctl_namespace_get_mode(ndns);
	if (mode >= 0 && mode != NDCTL_NS_MODE_MEMORY)
		goto out;

	/* if device-dax mode already established it might contain user data */
	pfn = ndctl_namespace_get_pfn(ndns);
	dax = ndctl_namespace_get_dax(ndns);
	if (dax || pfn)
		goto out;

	/* device is unconfigured, assume that was on purpose */
	bdev = ndctl_namespace_get_block_device(ndns);
	if (!bdev)
		goto out;

	if (snprintf(path, sizeof(path), "/dev/%s", bdev) >= (int) sizeof(path))
		goto out;

	/*
	 * Note, if the bdev goes active after this check we'll still
	 * clobber it in the following tests, see test/dax.sh and
	 * test/device-dax.sh.
	 */
	fd = open(path, O_RDWR | O_EXCL);
	if (fd < 0)
		goto out;
	close(fd);
	rc = 0;

 out:
	return rc ? NULL : ndns;
}

static int emit_e820_device(int loglevel, struct ndctl_test *test)
{
	int err;
	struct ndctl_ctx *ctx;
	struct ndctl_namespace *ndns;

	if (!ndctl_test_attempt(test, KERNEL_VERSION(4, 3, 0)))
		return 77;

	err = ndctl_new(&ctx);
	if (err < 0)
		return err;

	ndctl_set_log_priority(ctx, loglevel);

	ndns = ndctl_get_test_dev(ctx);
	if (!ndns) {
		fprintf(stderr, "%s: failed to find usable victim device\n",
				__func__);
		ndctl_test_skip(test);
		err = 77;
	} else {
		fprintf(stdout, "%s\n", ndctl_namespace_get_devname(ndns));
		err = 0;
	}
	ndctl_unref(ctx);
	return err;
}

int __attribute__((weak)) main(int argc, char *argv[])
{
	struct ndctl_test *test = ndctl_test_new(0);
	int rc;

	if (!test) {
		fprintf(stderr, "failed to initialize test\n");
		return EXIT_FAILURE;
	}

	rc = emit_e820_device(LOG_DEBUG, test);
	return ndctl_test_result(test, rc);
}
