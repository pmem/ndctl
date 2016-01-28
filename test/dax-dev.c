/*
 * Copyright (c) 2014-2015, Intel Corporation.
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

#include <test.h>
#include <linux/version.h>
#include <ndctl/libndctl.h>

static int emit_e820_device(int loglevel, struct ndctl_test *test)
{
	int err, fd;
	char path[256];
	const char *bdev;
	struct ndctl_ctx *ctx;
	struct ndctl_bus *bus;
	struct ndctl_region *region;
	struct ndctl_namespace *ndns;
	enum ndctl_namespace_mode mode;

	if (!ndctl_test_attempt(test, KERNEL_VERSION(4, 3, 0)))
		return 77;

	err = ndctl_new(&ctx);
	if (err < 0)
		return err;

	ndctl_set_log_priority(ctx, loglevel);
	err = -ENXIO;
	bus = ndctl_bus_get_by_provider(ctx, "e820");
	if (!bus)
		goto out;

	region = ndctl_region_get_first(bus);
	if (!region)
		goto out;

	ndns = ndctl_namespace_get_first(region);
	if (!ndns)
		goto out;

	mode = ndctl_namespace_get_mode(ndns);
	if (mode >= 0 && mode != NDCTL_NS_MODE_MEMORY)
		goto out;

	bdev = ndctl_namespace_get_block_device(ndns);
	if (!bdev)
		goto out;

	if (snprintf(path, sizeof(path), "/dev/%s", bdev) >= (int) sizeof(path))
		goto out;

	/*
	 * Note, if the bdev goes active after this check we'll still
	 * clobber it in the following tests, see test/dax.sh.
	 */
	fd = open(path, O_RDWR | O_EXCL);
	if (fd < 0)
		goto out;
	err = 0;
	fprintf(stdout, "%s\n", path);

 out:
	if (err)
		fprintf(stderr, "%s: failed to find usable victim device\n",
				__func__);
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
