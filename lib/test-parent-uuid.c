/*
 * blk_namespaces: tests functionality of multiple block namespaces
 *
 * Copyright (c) 2015, Intel Corporation.
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
#include <test-parent-uuid.h>

#include <ndctl/libndctl.h>

#ifdef HAVE_NDCTL_H
#include <linux/ndctl.h>
#else
#include <ndctl.h>
#endif


static const char *NFIT_TEST_MODULE = "nfit_test";
static const char *PROVIDER = "nfit_test.0";

static struct ndctl_bus *get_bus_by_provider(struct ndctl_ctx *ctx,
		const char *provider)
{
	struct ndctl_bus *bus;

        ndctl_bus_foreach(ctx, bus)
		if (strcmp(provider, ndctl_bus_get_provider(bus)) == 0)
			return bus;

	return NULL;
}

static struct ndctl_btt *get_idle_btt(struct ndctl_region *region)
{
	struct ndctl_btt *btt;

	ndctl_btt_foreach(region, btt)
		if (!ndctl_btt_is_enabled(btt)
				&& !ndctl_btt_is_configured(btt))
			return btt;
	return NULL;
}

static struct ndctl_namespace *create_blk_namespace(int region_fraction,
		struct ndctl_region *region, unsigned long long req_size,
		uuid_t uuid)
{
	struct ndctl_namespace *ndns, *seed_ns = NULL;
	unsigned long long size;

	ndctl_namespace_foreach(region, ndns)
		if (ndctl_namespace_get_size(ndns) == 0) {
			seed_ns = ndns;
			break;
		}

	if (!seed_ns)
		return NULL;

	size = ndctl_region_get_size(region)/region_fraction;
	if (req_size)
		size = req_size;

	if (ndctl_namespace_set_uuid(seed_ns, uuid) < 0)
		return NULL;

	if (ndctl_namespace_set_size(seed_ns, size) < 0)
		return NULL;

	if (ndctl_namespace_set_sector_size(seed_ns, 512) < 0)
		return NULL;

	if (ndctl_namespace_enable(seed_ns) < 0)
		return NULL;

	return seed_ns;
}

static int disable_blk_namespace(struct ndctl_namespace *ndns)
{
	if (ndctl_namespace_disable(ndns) < 0)
		return -ENODEV;

	if (ndctl_namespace_delete(ndns) < 0)
		return -ENODEV;

	return 0;
}

static struct ndctl_btt *check_valid_btt(struct ndctl_region *region,
		struct ndctl_namespace *ndns, uuid_t btt_uuid)
{
	struct ndctl_btt *btt = NULL;
	ndctl_btt_foreach(region, btt) {
		struct ndctl_namespace *btt_ndns;
		uuid_t uu;

		ndctl_btt_get_uuid(btt, uu);
		if (uuid_compare(uu, btt_uuid) != 0)
			continue;
		if (!ndctl_btt_is_enabled(btt))
			continue;
		btt_ndns = ndctl_btt_get_namespace(btt);
		if (strcmp(ndctl_namespace_get_devname(btt_ndns),
				ndctl_namespace_get_devname(ndns)) != 0)
			continue;
		return btt;
	}
	return NULL;
}


static int do_test(struct ndctl_ctx *ctx)
{
	int rc;
	struct ndctl_bus *bus;
	struct ndctl_btt *btt, *found = NULL;
	struct ndctl_region *region, *blk_region;
	struct ndctl_namespace *ndns;
	unsigned long long ns_size = 18874368;
	uuid_t uuid = {0,  1,  2,  3,  4,  5,  6,  7, 8, 9, 10, 11, 12, 13, 14, 16};
	uuid_t btt_uuid;

	bus = get_bus_by_provider(ctx, PROVIDER);
	if (!bus) {
		fprintf(stderr, "failed to find NFIT-provider: %s\n", PROVIDER);
		rc = -ENODEV;
		goto err_nobus;
	}

	ndctl_region_foreach(bus, region)
		if (ndctl_region_get_nstype(region) == ND_DEVICE_NAMESPACE_BLK) {
			blk_region = region;
			break;
		}

	if (!blk_region) {
		fprintf(stderr, "failed to find block region\n");
		rc = -ENODEV;
		goto err_cleanup;
	}

	/* create a blk namespace */
	ndns = create_blk_namespace(1, blk_region, ns_size, uuid);
	if (!ndns) {
		fprintf(stderr, "failed to create block namespace\n");
		goto err_cleanup;
	}

	/* create a btt for this namespace */
	uuid_generate(btt_uuid);
	btt = get_idle_btt(region);
	if (!btt)
		return -ENXIO;

	ndctl_btt_set_uuid(btt, btt_uuid);
	ndctl_btt_set_sector_size(btt, 512);
	ndctl_btt_set_namespace(btt, ndns);
	ndctl_namespace_disable(ndns);
	rc = ndctl_btt_enable(btt);
	if (rc) {
		fprintf(stderr, "failed to create btt 0\n");
		goto err_cleanup;
	}

	/* disable the btt */
	ndctl_btt_delete(btt);

	/* re-create the namespace - this should auto-enable the btt */
	disable_blk_namespace(ndns);
	ndns = create_blk_namespace(1, blk_region, ns_size, uuid);
	if (!ndns) {
		fprintf(stderr, "failed to re-create block namespace\n");
		goto err_cleanup;
	}

	/* Verify btt was auto-created */
	found = check_valid_btt(blk_region, ndns, btt_uuid);
	if (!found) {
		rc = -ENXIO;
		goto err_cleanup;
	}
	btt = found;

	/*disable the btt and namespace again */
	ndctl_btt_delete(btt);
	disable_blk_namespace(ndns);

	/* recreate the namespace with a different uuid */
	uuid_generate(uuid);
	ndns = create_blk_namespace(1, blk_region, ns_size, uuid);
	if (!ndns) {
		fprintf(stderr, "failed to re-create block namespace\n");
		goto err_cleanup;
	}

	/* make sure there is no btt on this namespace */
	found = check_valid_btt(blk_region, ndns, btt_uuid);
	if (found) {
		fprintf(stderr, "found a stale btt\n");
		rc = -ENXIO;
		goto err_cleanup;
	}

err_cleanup:
	ndctl_btt_foreach(blk_region, btt)
		ndctl_btt_delete(btt);

	ndctl_namespace_foreach(blk_region, ndns)
		if (ndctl_namespace_get_size(ndns) != 0)
			disable_blk_namespace(ndns);
	ndctl_region_foreach(bus, region)
		ndctl_region_disable_invalidate(region);


 err_nobus:
	ndctl_unref(ctx);
	return rc;
}

int test_parent_uuid(int loglevel)
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
	return test_parent_uuid(LOG_DEBUG);
}
