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
#include <unistd.h>
#include <uuid/uuid.h>
#include <libkmod.h>
#include <linux/version.h>
#include <test.h>

#include <ccan/array_size/array_size.h>
#include <ndctl.h>

#define err(msg)\
	fprintf(stderr, "%s:%d: %s (%s)\n", __func__, __LINE__, msg, strerror(errno))

static struct ndctl_namespace *create_pmem_namespace(struct ndctl_region *region)
{
	struct ndctl_namespace *seed_ns = NULL;
	unsigned long long size;
	uuid_t uuid;

	seed_ns = ndctl_region_get_namespace_seed(region);
	if (!seed_ns)
		return NULL;

	uuid_generate(uuid);
	size = ndctl_region_get_size(region);

	if (ndctl_namespace_set_uuid(seed_ns, uuid) < 0)
		return NULL;

	if (ndctl_namespace_set_size(seed_ns, size) < 0)
		return NULL;

	if (ndctl_namespace_enable(seed_ns) < 0)
		return NULL;

	return seed_ns;
}

static int disable_pmem_namespace(struct ndctl_namespace *ndns)
{
	if (ndctl_namespace_disable_invalidate(ndns) < 0)
		return -ENODEV;

	if (ndctl_namespace_delete(ndns) < 0)
		return -ENODEV;

	return 0;
}

static int ns_do_io(const char *bdev)
{
	unsigned long num_dev_pages, num_blocks;
	const int page_size = 4096;
	const int num_pages = 2;
	off_t addr;
	int rc = 0;
	int fd, i;

	void *random_page[num_pages];
	void *pmem_page[num_pages];

	rc = posix_memalign(random_page, page_size, page_size * num_pages);
	if (rc) {
		fprintf(stderr, "posix_memalign failure\n");
		return rc;
	}

	rc = posix_memalign(pmem_page, page_size, page_size * num_pages);
	if (rc) {
		fprintf(stderr, "posix_memalign failure\n");
		goto err_free_pmem;
	}

	for (i = 1; i < num_pages; i++) {
		random_page[i] = (char*)random_page[0] + page_size * i;
		pmem_page[i] = (char*)pmem_page[0] + page_size * i;
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

	/* figure out our dev size */
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

	addr = page_size * (num_dev_pages - 1);
	rc = pwrite(fd, random_page[1], page_size, addr);
	if (rc < 0) {
		err("write");
		goto err_close;
	}

	/* read back the random data into pmem_page */
	rc = pread(fd, pmem_page[0], page_size, 0);
	if (rc < 0) {
		err("read");
		goto err_close;
	}

	addr = page_size * (num_dev_pages - 1);
	rc = pread(fd, pmem_page[1], page_size, addr);
	if (rc < 0) {
		err("read");
		goto err_close;
	}

	/* verify the data */
	if (memcmp(random_page[0], pmem_page[0], page_size * num_pages)) {
		fprintf(stderr, "PMEM data miscompare\n");
		rc = -EIO;
		goto err_close;
	}

	rc = 0;
 err_close:
	close(fd);
 err_free_all:
	free(random_page[0]);
 err_free_pmem:
	free(pmem_page[0]);
	return rc;
}

static const char *comm = "test-pmem-namespaces";

int test_pmem_namespaces(int log_level, struct ndctl_test *test,
		struct ndctl_ctx *ctx)
{
	struct ndctl_region *region, *pmem_region = NULL;
	struct kmod_ctx *kmod_ctx = NULL;
	struct kmod_module *mod = NULL;
	struct ndctl_namespace *ndns;
	struct ndctl_dimm *dimm;
	struct ndctl_bus *bus;
	int rc = -ENXIO;
	char bdev[50];

	if (!ndctl_test_attempt(test, KERNEL_VERSION(4, 2, 0)))
		return 77;

	ndctl_set_log_priority(ctx, log_level);

	bus = ndctl_bus_get_by_provider(ctx, "ACPI.NFIT");
	if (bus) {
		/* skip this bus if no label-enabled PMEM regions */
		ndctl_region_foreach(bus, region)
			if (ndctl_region_get_nstype(region)
					== ND_DEVICE_NAMESPACE_PMEM)
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
			rc = 77;
			ndctl_test_skip(test);
			fprintf(stderr, "nfit_test unavailable skipping tests\n");
			goto err_module;
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
			goto err;
		}
	}

	/* create our config */
	ndctl_region_foreach(bus, region)
		if (strcmp(ndctl_region_get_type_name(region), "pmem") == 0) {
			pmem_region = region;
			break;
		}

	if (!pmem_region || ndctl_region_enable(pmem_region) < 0) {
		fprintf(stderr, "%s: failed to find PMEM region\n", comm);
		rc = -ENODEV;
		goto err;
	}

	rc = -ENODEV;
	ndns = create_pmem_namespace(pmem_region);
	if (!ndns) {
		fprintf(stderr, "%s: failed to create PMEM namespace\n", comm);
		goto err;
	}

	sprintf(bdev, "/dev/%s", ndctl_namespace_get_block_device(ndns));
	rc = ns_do_io(bdev);

	disable_pmem_namespace(ndns);

 err:
	/* unload nfit_test */
	bus = ndctl_bus_get_by_provider(ctx, "nfit_test.0");
	if (bus)
		ndctl_region_foreach(bus, region)
			ndctl_region_disable_invalidate(region);
	bus = ndctl_bus_get_by_provider(ctx, "nfit_test.1");
	if (bus)
		ndctl_region_foreach(bus, region)
			ndctl_region_disable_invalidate(region);
	kmod_module_remove_module(mod, 0);

 err_module:
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

	rc = test_pmem_namespaces(LOG_DEBUG, test, ctx);
	ndctl_unref(ctx);
	return ndctl_test_result(test, rc);
}
