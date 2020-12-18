// SPDX-License-Identifier: LGPL-2.1
// Copyright (C) 2015-2020, Intel Corporation. All rights reserved.
#include <errno.h>
#include <fcntl.h>
#include <linux/fs.h>
#include <ndctl/libndctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <syslog.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <ndctl.h>
#include <unistd.h>
#include <uuid/uuid.h>
#include <linux/version.h>
#include <test.h>
#include <libkmod.h>
#include <ccan/array_size/array_size.h>

/* The purpose of this test is to verify that we can successfully do I/O to
 * multiple nd_blk namespaces that have discontiguous segments.  It first
 * sets up two namespaces, each 1/2 the total size of the NVDIMM and each with
 * two discontiguous segments, arranged like this:
 *
 * +-------+-------+-------+-------+
 * |  nd0  |  nd1  |  nd0  |  nd1  |
 * +-------+-------+-------+-------+
 *
 * It then runs some I/O to the beginning, middle and end of each of these
 * namespaces, checking data integrity.  The I/O to the middle of the
 * namespace will hit two pages, one on either side of the segment boundary.
 */
#define err(msg)\
	fprintf(stderr, "%s:%d: %s (%s)\n", __func__, __LINE__, msg, strerror(errno))

static struct ndctl_namespace *create_blk_namespace(int region_fraction,
		struct ndctl_region *region)
{
	struct ndctl_namespace *ndns, *seed_ns = NULL;
	unsigned long long size;
	uuid_t uuid;

	ndctl_region_set_align(region, sysconf(_SC_PAGESIZE));
	ndctl_namespace_foreach(region, ndns)
		if (ndctl_namespace_get_size(ndns) == 0) {
			seed_ns = ndns;
			break;
		}

	if (!seed_ns)
		return NULL;

	uuid_generate(uuid);
	size = ndctl_region_get_size(region)/region_fraction;

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
	if (ndctl_namespace_disable_invalidate(ndns) < 0)
		return -ENODEV;

	if (ndctl_namespace_delete(ndns) < 0)
		return -ENODEV;

	return 0;
}

static int ns_do_io(const char *bdev)
{
	int fd, i;
	int rc = 0;
	const int page_size = 4096;
	const int num_pages = 4;
	unsigned long num_dev_pages, num_blocks;
	off_t addr;

	void *random_page[num_pages];
	void *blk_page[num_pages];

	rc = posix_memalign(random_page, page_size, page_size * num_pages);
	if (rc) {
		fprintf(stderr, "posix_memalign failure\n");
		return rc;
	}

	rc = posix_memalign(blk_page, page_size, page_size * num_pages);
	if (rc) {
		fprintf(stderr, "posix_memalign failure\n");
		goto err_free_blk;
	}

	for (i = 1; i < num_pages; i++) {
		random_page[i] = (char*)random_page[0] + page_size * i;
		blk_page[i] = (char*)blk_page[0] + page_size * i;
	}

	/* read random data into random_page */
	if ((fd = open("/dev/urandom", O_RDONLY)) < 0) {
		err("open");
		rc = -ENODEV;
		goto err_free_all;
	}

	rc = read(fd, random_page[0], page_size * num_pages);
	if (rc < 0) {
		err("read");
		close(fd);
		goto err_free_all;
	}

	close(fd);

	if ((fd = open(bdev, O_RDWR|O_DIRECT)) < 0) {
		err("open");
		rc = -ENODEV;
		goto err_free_all;
	}

	ioctl(fd, BLKGETSIZE, &num_blocks);
	num_dev_pages = num_blocks / 8;

	/* write the random data out to each of the segments */
	rc = pwrite(fd, random_page[0], page_size, 0);
	if (rc < 0) {
		err("write");
		goto err_close;
	}

	/* two pages that span the region discontinuity */
	addr = page_size * (num_dev_pages/2 - 1);
	rc = pwrite(fd, random_page[1], page_size*2, addr);
	if (rc < 0) {
		err("write");
		goto err_close;
	}

	addr = page_size * (num_dev_pages - 1);
	rc = pwrite(fd, random_page[3], page_size, addr);
	if (rc < 0) {
		err("write");
		goto err_close;
	}

	/* read back the random data into blk_page */
	rc = pread(fd, blk_page[0], page_size, 0);
	if (rc < 0) {
		err("read");
		goto err_close;
	}

	/* two pages that span the region discontinuity */
	addr = page_size * (num_dev_pages/2 - 1);
	rc = pread(fd, blk_page[1], page_size*2, addr);
	if (rc < 0) {
		err("read");
		goto err_close;
	}

	addr = page_size * (num_dev_pages - 1);
	rc = pread(fd, blk_page[3], page_size, addr);
	if (rc < 0) {
		err("read");
		goto err_close;
	}

	/* verify the data */
	if (memcmp(random_page[0], blk_page[0], page_size * num_pages)) {
		fprintf(stderr, "Block data miscompare\n");
		rc = -EIO;
		goto err_close;
	}

	rc = 0;
 err_close:
	close(fd);
 err_free_all:
	free(random_page[0]);
 err_free_blk:
	free(blk_page[0]);
	return rc;
}

static const char *comm = "test-blk-namespaces";

int test_blk_namespaces(int log_level, struct ndctl_test *test,
		struct ndctl_ctx *ctx)
{
	char bdev[50];
	int rc = -ENXIO;
	struct ndctl_bus *bus;
	struct ndctl_dimm *dimm;
	struct kmod_module *mod = NULL;
	struct kmod_ctx *kmod_ctx = NULL;
	struct ndctl_namespace *ndns[2];
	struct ndctl_region *region, *blk_region = NULL;

	if (!ndctl_test_attempt(test, KERNEL_VERSION(4, 2, 0)))
		return 77;

	ndctl_set_log_priority(ctx, log_level);

	bus = ndctl_bus_get_by_provider(ctx, "ACPI.NFIT");
	if (bus) {
		/* skip this bus if no BLK regions */
		ndctl_region_foreach(bus, region)
			if (ndctl_region_get_nstype(region)
					== ND_DEVICE_NAMESPACE_BLK)
				break;
		if (!region)
			bus = NULL;
	}

	if (!bus) {
		fprintf(stderr, "ACPI.NFIT unavailable falling back to nfit_test\n");
		rc = nfit_test_init(&kmod_ctx, &mod, NULL, log_level, test);
		ndctl_invalidate(ctx);
		bus = ndctl_bus_get_by_provider(ctx, "nfit_test.0");
		if (rc < 0 || !bus) {
			ndctl_test_skip(test);
			fprintf(stderr, "nfit_test unavailable skipping tests\n");
			return 77;
		}
	}

	fprintf(stderr, "%s: found provider: %s\n", comm,
			ndctl_bus_get_provider(bus));

	/* get the system to a clean state */
        ndctl_region_foreach(bus, region)
                ndctl_region_disable_invalidate(region);

        ndctl_dimm_foreach(bus, dimm) {
                rc = ndctl_dimm_zero_labels(dimm);
                if (rc < 0) {
                        fprintf(stderr, "failed to zero %s\n",
                                        ndctl_dimm_get_devname(dimm));
			goto err_module;
                }
        }

	/* create our config */
	ndctl_region_foreach(bus, region)
		if (strcmp(ndctl_region_get_type_name(region), "blk") == 0) {
			blk_region = region;
			break;
		}

	if (!blk_region || ndctl_region_enable(blk_region) < 0) {
		fprintf(stderr, "%s: failed to find block region\n", comm);
		rc = -ENODEV;
		goto err_cleanup;
	}

	rc = -ENODEV;
	ndns[0] = create_blk_namespace(4, blk_region);
	if (!ndns[0]) {
		fprintf(stderr, "%s: failed to create block namespace\n", comm);
		goto err_cleanup;
	}

	ndns[1] = create_blk_namespace(4, blk_region);
	if (!ndns[1]) {
		fprintf(stderr, "%s: failed to create block namespace\n", comm);
		goto err_cleanup;
	}

	rc = disable_blk_namespace(ndns[0]);
	if (rc < 0) {
		fprintf(stderr, "%s: failed to disable block namespace\n", comm);
		goto err_cleanup;
	}

	ndns[0] = create_blk_namespace(2, blk_region);
	if (!ndns[0]) {
		fprintf(stderr, "%s: failed to create block namespace\n", comm);
		rc = -ENODEV;
		goto err_cleanup;
	}

	rc = disable_blk_namespace(ndns[1]);
	if (rc < 0) {
		fprintf(stderr, "%s: failed to disable block namespace\n", comm);
		goto err_cleanup;
	}

	rc = -ENODEV;
	ndns[1] = create_blk_namespace(2, blk_region);
	if (!ndns[1]) {
		fprintf(stderr, "%s: failed to create block namespace\n", comm);
		goto err_cleanup;
	}

	/* okay, all set up, do some I/O */
	rc = -EIO;
	sprintf(bdev, "/dev/%s", ndctl_namespace_get_block_device(ndns[0]));
	if (ns_do_io(bdev))
		goto err_cleanup;
	sprintf(bdev, "/dev/%s", ndctl_namespace_get_block_device(ndns[1]));
	if (ns_do_io(bdev))
		goto err_cleanup;
	rc = 0;

 err_cleanup:
	/* unload nfit_test */
	bus = ndctl_bus_get_by_provider(ctx, "nfit_test.0");
	if (bus)
		ndctl_region_foreach(bus, region)
			ndctl_region_disable_invalidate(region);
	bus = ndctl_bus_get_by_provider(ctx, "nfit_test.1");
	if (bus)
		ndctl_region_foreach(bus, region)
			ndctl_region_disable_invalidate(region);
	if (mod)
		kmod_module_remove_module(mod, 0);

 err_module:
	if (kmod_ctx)
		kmod_unref(kmod_ctx);
	return rc;
}

int __attribute__((weak)) main(int argc, char *argv[])
{
	struct ndctl_test *test = ndctl_test_new(0);
	struct ndctl_ctx *ctx;
	int rc;

	comm = argv[0];
	if (!test) {
		fprintf(stderr, "failed to initialize test\n");
		return EXIT_FAILURE;
	}

	rc = ndctl_new(&ctx);
	if (rc)
		return ndctl_test_result(test, rc);

	rc = test_blk_namespaces(LOG_DEBUG, test, ctx);
	ndctl_unref(ctx);
	return ndctl_test_result(test, rc);
}
