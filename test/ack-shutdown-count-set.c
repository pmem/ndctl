// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2018-2020 Intel Corporation. All rights reserved.

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
#include <ndctl.h>
#include <test.h>

static int test_dimm(struct ndctl_dimm *dimm)
{
	struct ndctl_cmd *cmd;
	int rc = 0;

	cmd = ndctl_dimm_cmd_new_ack_shutdown_count(dimm);
	if (!cmd)
		return -ENOMEM;

	rc = ndctl_cmd_submit_xlat(cmd);
	if (rc < 0) {
		fprintf(stderr, "dimm %s LSS enable set failed\n",
				ndctl_dimm_get_devname(dimm));
		goto out;
	}

	printf("DIMM %s LSS enable set\n", ndctl_dimm_get_devname(dimm));

out:
	ndctl_cmd_unref(cmd);
	return rc;
}

static void reset_bus(struct ndctl_bus *bus)
{
	struct ndctl_region *region;
	struct ndctl_dimm *dimm;

	/* disable all regions so that set_config_data commands are permitted */
	ndctl_region_foreach(bus, region)
		ndctl_region_disable_invalidate(region);

	ndctl_dimm_foreach(bus, dimm)
		ndctl_dimm_zero_labels(dimm);
}

static int do_test(struct ndctl_ctx *ctx, struct ndctl_test *test)
{
	struct ndctl_bus *bus = ndctl_bus_get_by_provider(ctx, "nfit_test.0");
	struct ndctl_dimm *dimm;
	struct ndctl_region *region;
	struct log_ctx log_ctx;
	int rc = 0;

	if (!ndctl_test_attempt(test, KERNEL_VERSION(4, 15, 0)))
		return 77;

	if (!bus)
		return -ENXIO;

	log_init(&log_ctx, "test/ack-shutdown-count-set", "NDCTL_TEST");

	ndctl_bus_wait_probe(bus);

	ndctl_region_foreach(bus, region)
		ndctl_region_disable_invalidate(region);

	ndctl_dimm_foreach(bus, dimm) {
		fprintf(stderr, "Testing dimm: %s\n",
				ndctl_dimm_get_devname(dimm));
		rc = test_dimm(dimm);
		if (rc < 0) {
			fprintf(stderr, "dimm %s failed\n",
				ndctl_dimm_get_devname(dimm));
			goto out;
		}
	}

out:
	reset_bus(bus);
	return rc;
}

static int test_ack_shutdown_count_set(int loglevel, struct ndctl_test *test,
		struct ndctl_ctx *ctx)
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

int main(int argc, char *argv[])
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
	rc = test_ack_shutdown_count_set(LOG_DEBUG, test, ctx);
	ndctl_unref(ctx);

	return ndctl_test_result(test, rc);
}
