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

#include <ccan/array_size/array_size.h>
#include <ndctl/libndctl.h>
#ifdef HAVE_NDCTL_H
#include <linux/ndctl.h>
#else
#include <ndctl.h>
#endif
#include <test-libndctl.h>

/*
 * Kernel provider "nfit_test.0" produces an NFIT with the following attributes:
 *
 *                              (a)               (b)           DIMM   BLK-REGION
 *           +-------------------+--------+--------+--------+
 * +------+  |       pm0.0       | blk2.0 | pm1.0  | blk2.1 |    0      region2
 * | imc0 +--+- - - region0- - - +--------+        +--------+
 * +--+---+  |       pm0.0       | blk3.0 | pm1.0  | blk3.1 |    1      region3
 *    |      +-------------------+--------v        v--------+
 * +--+---+                               |                 |
 * | cpu0 |                                     region1
 * +--+---+                               |                 |
 *    |      +----------------------------^        ^--------+
 * +--+---+  |           blk4.0           | pm1.0  | blk4.0 |    2      region4
 * | imc1 +--+----------------------------|        +--------+
 * +------+  |           blk5.0           | pm1.0  | blk5.0 |    3      region5
 *           +----------------------------+--------+--------+
 *
 * *) In this layout we have four dimms and two memory controllers in one
 *    socket.  Each unique interface ("blk" or "pmem") to DPA space
 *    is identified by a region device with a dynamically assigned id.
 *
 * *) The first portion of dimm0 and dimm1 are interleaved as REGION0.
 *    A single "pmem" namespace is created in the REGION0-"spa"-range
 *    that spans dimm0 and dimm1 with a user-specified name of "pm0.0".
 *    Some of that interleaved "spa" range is reclaimed as "bdw"
 *    accessed space starting at offset (a) into each dimm.  In that
 *    reclaimed space we create two "bdw" "namespaces" from REGION2 and
 *    REGION3 where "blk2.0" and "blk3.0" are just human readable names
 *    that could be set to any user-desired name in the label.
 *
 * *) In the last portion of dimm0 and dimm1 we have an interleaved
 *    "spa" range, REGION1, that spans those two dimms as well as dimm2
 *    and dimm3.  Some of REGION1 allocated to a "pmem" namespace named
 *    "pm1.0" the rest is reclaimed in 4 "bdw" namespaces (for each
 *    dimm in the interleave set), "blk2.1", "blk3.1", "blk4.0", and
 *    "blk5.0".
 *
 * *) The portion of dimm2 and dimm3 that do not participate in the
 *    REGION1 interleaved "spa" range (i.e. the DPA address below
 *    offset (b) are also included in the "blk4.0" and "blk5.0"
 *    namespaces.  Note, that this example shows that "bdw" namespaces
 *    don't need to be contiguous in DPA-space.
 *
 * Kernel provider "nfit_test.1" produces an NFIT with the following attributes:
 *
 * region2
 * +---------------------+
 * |---------------------|
 * ||       pm2.0       ||
 * |---------------------|
 * +---------------------+
 *
 * *) Describes a simple system-physical-address range with no backing
 *    dimm or interleave description.
 */

static const char *NFIT_TEST_MODULE = "nfit_test";
static const char *NFIT_PROVIDER0 = "nfit_test.0";
static const char *NFIT_PROVIDER1 = "nfit_test.1";
#define SZ_128K 0x00020000
#define SZ_7M   0x00700000
#define SZ_8M   0x00800000
#define SZ_11M  0x00b00000
#define SZ_12M  0x00c00000
#define SZ_16M  0x01000000
#define SZ_18M  0x01200000
#define SZ_20M  0x01400000
#define SZ_27M  0x01b00000
#define SZ_28M  0x01c00000
#define SZ_32M  0x02000000
#define SZ_64M  0x04000000

struct dimm {
	unsigned int handle;
	unsigned int phys_id;
};

#define DIMM_HANDLE(n, s, i, c, d) \
	(((n & 0xfff) << 16) | ((s & 0xf) << 12) | ((i & 0xf) << 8) \
	 | ((c & 0xf) << 4) | (d & 0xf))
static struct dimm dimms0[] = {
	{ DIMM_HANDLE(0, 0, 0, 0, 0), 0, },
	{ DIMM_HANDLE(0, 0, 0, 0, 1), 1, },
	{ DIMM_HANDLE(0, 0, 1, 0, 0), 2, },
	{ DIMM_HANDLE(0, 0, 1, 0, 1), 3, },
};

struct region {
	union {
		unsigned int range_index;
		unsigned int handle;
	};
	unsigned int interleave_ways;
	int enabled;
	char *type;
	unsigned long long available_size;
	unsigned long long size;
	struct set {
		int active;
	} iset;
	struct namespace *namespaces[4];
};

struct btt {
	int enabled;
	uuid_t uuid;
	int num_sector_sizes;
	unsigned int sector_sizes[7];
};

static struct btt btt_settings = {
	.enabled = 1,
	.uuid = {  0,  1,  2,  3,  4,  5,  6,  7,
		   8, 9,  10, 11, 12, 13, 14, 15
	},
	.num_sector_sizes = 7,
	.sector_sizes =  { 512, 520, 528, 4096, 4104, 4160, 4224, },
};

struct namespace {
	unsigned int id;
	char *type;
	struct btt *btt_settings;
	unsigned long long size;
	uuid_t uuid;
	int do_configure;
	int check_alt_name;
	unsigned long lbasize;
};

static uuid_t null_uuid;

static struct namespace namespace0_pmem0 = {
	0, "namespace_pmem", &btt_settings, SZ_18M,
	{ 1, 1, 1, 1,
	  1, 1, 1, 1,
	  1, 1, 1, 1,
	  1, 1, 1, 1, }, 1, 1, 0,
};

static struct namespace namespace1_pmem0 = {
	0, "namespace_pmem", &btt_settings, SZ_20M,
	{ 2, 2, 2, 2,
	  2, 2, 2, 2,
	  2, 2, 2, 2,
	  2, 2, 2, 2, }, 1, 1, 0,
};

static struct namespace namespace2_blk0 = {
	0, "namespace_blk", NULL, SZ_7M,
	{ 3, 3, 3, 3,
	  3, 3, 3, 3,
	  3, 3, 3, 3,
	  3, 3, 3, 3, }, 1, 1, 512,
};

static struct namespace namespace2_blk1 = {
	1, "namespace_blk", NULL, SZ_11M,
	{ 4, 4, 4, 4,
	  4, 4, 4, 4,
	  4, 4, 4, 4,
	  4, 4, 4, 4, }, 1, 1, 512,
};

static struct namespace namespace3_blk0 = {
	0, "namespace_blk", NULL, SZ_7M,
	{ 5, 5, 5, 5,
	  5, 5, 5, 5,
	  5, 5, 5, 5,
	  5, 5, 5, 5, }, 1, 1, 512,
};

static struct namespace namespace3_blk1 = {
	1, "namespace_blk", NULL, SZ_11M,
	{ 6, 6, 6, 6,
	  6, 6, 6, 6,
	  6, 6, 6, 6,
	  6, 6, 6, 6, }, 1, 1, 512,
};

static struct namespace namespace4_blk0 = {
	0, "namespace_blk", &btt_settings, SZ_27M,
	{ 7, 7, 7, 7,
	  7, 7, 7, 7,
	  7, 7, 7, 7,
	  7, 7, 7, 7, }, 1, 1, 512,
};

static struct namespace namespace5_blk0 = {
	0, "namespace_blk", &btt_settings, SZ_27M,
	{ 8, 8, 8, 8,
	  8, 8, 8, 8,
	  8, 8, 8, 8,
	  8, 8, 8, 8, }, 1, 1, 512,
};

static struct region regions0[] = {
	{ { 1 }, 2, 1, "pmem", SZ_32M, SZ_32M, { 1 },
		{ &namespace0_pmem0, NULL, }, },
	{ { 2 }, 4, 1, "pmem", SZ_64M, SZ_64M, { 1 },
		{ &namespace1_pmem0, NULL, }, },
	{ { DIMM_HANDLE(0, 0, 0, 0, 0) }, 1, 1, "blk", SZ_18M, SZ_32M, { },
		{ &namespace2_blk0, &namespace2_blk1, NULL, }, },
	{ { DIMM_HANDLE(0, 0, 0, 0, 1) }, 1, 1, "blk", SZ_18M, SZ_32M, { },
		{ &namespace3_blk0, &namespace3_blk1, NULL, }, },
	{ { DIMM_HANDLE(0, 0, 1, 0, 0) }, 1, 1, "blk", SZ_27M, SZ_32M, { },
		{ &namespace4_blk0, NULL, }, },
	{ { DIMM_HANDLE(0, 0, 1, 0, 1) }, 1, 1, "blk", SZ_27M, SZ_32M, { },
		{ &namespace5_blk0, NULL, }, },
};

static struct namespace namespace1 = {
	0, "namespace_io", &btt_settings, SZ_32M,
};

static struct region regions1[] = {
	{ { 1 }, 1, 1, "pmem", 0, SZ_32M,
		.namespaces = {
			[0] = &namespace1,
		},
	},
};

static struct btt btts0[] = {
	{ 0, { 0, }, 7, { 512, 520, 528, 4096, 4104, 4160, 4224, }, },
};

static struct btt btts1[] = {
	{ 0, { 0, }, 7, { 512, 520, 528, 4096, 4104, 4160, 4224, }, },
};

static unsigned long commands0 = 1UL << ND_CMD_GET_CONFIG_SIZE
		| 1UL << ND_CMD_GET_CONFIG_DATA
		| 1UL << ND_CMD_SET_CONFIG_DATA;

static struct ndctl_dimm *get_dimm_by_handle(struct ndctl_bus *bus, unsigned int handle)
{
	struct ndctl_dimm *dimm;

	ndctl_dimm_foreach(bus, dimm)
		if (ndctl_dimm_get_handle(dimm) == handle)
			return dimm;

	return NULL;
}

static struct ndctl_btt *get_idle_btt(struct ndctl_bus *bus)
{
	struct ndctl_btt *btt;

	ndctl_btt_foreach(bus, btt)
		if (!ndctl_btt_is_enabled(btt) && !ndctl_btt_is_configured(btt))
			return btt;

	return NULL;
}

static struct ndctl_namespace *get_namespace_by_id(struct ndctl_region *region,
		struct namespace *namespace)
{
	struct ndctl_namespace *ndns;

	if (memcmp(namespace->uuid, null_uuid, sizeof(uuid_t)) != 0)
		ndctl_namespace_foreach(region, ndns) {
			uuid_t ndns_uuid;
			int cmp;

			ndctl_namespace_get_uuid(ndns, ndns_uuid);
			cmp = memcmp(ndns_uuid, namespace->uuid, sizeof(uuid_t));
			if (cmp == 0)
				return ndns;
		}

	/* fall back to nominal id if uuid is not configured yet */
	ndctl_namespace_foreach(region, ndns)
		if (ndctl_namespace_get_id(ndns) == namespace->id)
			return ndns;

	return NULL;
}

static struct ndctl_region *get_pmem_region_by_range_index(struct ndctl_bus *bus,
		unsigned int range_index)
{
	struct ndctl_region *region;

	ndctl_region_foreach(bus, region) {
		if (ndctl_region_get_type(region) != ND_DEVICE_REGION_PMEM)
			continue;
		if (ndctl_region_get_range_index(region) == range_index)
			return region;
	}
	return NULL;
}

static struct ndctl_region *get_blk_region_by_dimm_handle(struct ndctl_bus *bus,
		unsigned int handle)
{
	struct ndctl_region *region;

	ndctl_region_foreach(bus, region) {
		struct ndctl_mapping *map;

		if (ndctl_region_get_type(region) != ND_DEVICE_REGION_BLK)
			continue;
		ndctl_mapping_foreach(region, map) {
			struct ndctl_dimm *dimm = ndctl_mapping_get_dimm(map);

			if (ndctl_dimm_get_handle(dimm) == handle)
				return region;
		}
	}
	return NULL;
}

static int check_namespaces(struct ndctl_region *region,
		struct namespace **namespaces);

static int check_regions(struct ndctl_bus *bus, struct region *regions, int n)
{
	int i, rc = 0;

	for (i = 0; i < n; i++) {
		struct ndctl_interleave_set *iset;
		struct ndctl_region *region;
		char devname[50];

		if (strcmp(regions[i].type, "pmem") == 0)
			region = get_pmem_region_by_range_index(bus, regions[i].range_index);
		else
			region = get_blk_region_by_dimm_handle(bus, regions[i].handle);

		if (!region) {
			fprintf(stderr, "failed to find region type: %s ident: %x\n",
					regions[i].type, regions[i].handle);
			return -ENXIO;
		}

		snprintf(devname, sizeof(devname), "region%d",
				ndctl_region_get_id(region));
		if (strcmp(ndctl_region_get_type_name(region), regions[i].type) != 0) {
			fprintf(stderr, "%s: expected type: %s got: %s\n",
					devname, regions[i].type,
					ndctl_region_get_type_name(region));
			return -ENXIO;
		}
		if (ndctl_region_get_interleave_ways(region) != regions[i].interleave_ways) {
			fprintf(stderr, "%s: expected interleave_ways: %d got: %d\n",
					devname, regions[i].interleave_ways,
					ndctl_region_get_interleave_ways(region));
			return -ENXIO;
		}
		if (regions[i].enabled && !ndctl_region_is_enabled(region)) {
			fprintf(stderr, "%s: expected enabled by default\n",
					devname);
			return -ENXIO;
		}

		if (regions[i].available_size != ndctl_region_get_available_size(region)) {
			fprintf(stderr, "%s: expected available_size: %#llx got: %#llx\n",
					devname, regions[i].available_size,
					ndctl_region_get_available_size(region));
			return -ENXIO;
		}

		if (regions[i].size != ndctl_region_get_size(region)) {
			fprintf(stderr, "%s: expected size: %#llx got: %#llx\n",
					devname, regions[i].size,
					ndctl_region_get_size(region));
			return -ENXIO;
		}

		iset = ndctl_region_get_interleave_set(region);
		if (regions[i].iset.active
				&& !(iset && ndctl_interleave_set_is_active(iset) > 0)) {
			fprintf(stderr, "%s: expected interleave set active by default\n",
					devname);
			return -ENXIO;
		} else if (regions[i].iset.active == 0 && iset) {
			fprintf(stderr, "%s: expected no interleave set\n",
					devname);
			return -ENXIO;
		}

		if (ndctl_region_disable_invalidate(region) < 0) {
			fprintf(stderr, "%s: failed to disable\n", devname);
			return -ENXIO;
		}
		if (regions[i].enabled && ndctl_region_enable(region) < 0) {
			fprintf(stderr, "%s: failed to enable\n", devname);
			return -ENXIO;
		}

		if (regions[i].namespaces)
			rc = check_namespaces(region, regions[i].namespaces);
		if (rc)
			break;
	}

	return rc;
}

static int check_btt_create(struct ndctl_bus *bus, struct ndctl_namespace *ndns,
		struct btt *create_btt)
{
	int i, fd, retry = 10;
	struct ndctl_btt *btt;
	const char *devname;
	char bdevpath[50];
	void *buf = NULL;
	ssize_t rc;

	if (!create_btt)
		return 0;

	if (posix_memalign(&buf, 4096, 4096) != 0)
		return -ENXIO;

	for (i = 0; i < create_btt->num_sector_sizes; i++) {
		btt = get_idle_btt(bus);
		if (!btt)
			return -ENXIO;

		sprintf(bdevpath, "/dev/%s", ndctl_namespace_get_block_device(ndns));
		ndctl_btt_set_uuid(btt, create_btt->uuid);
		ndctl_btt_set_sector_size(btt, create_btt->sector_sizes[i]);
		ndctl_btt_set_backing_dev(btt, bdevpath);
		ndctl_btt_enable(btt);

		sprintf(bdevpath, "/dev/%s", ndctl_btt_get_block_device(btt));
		devname = ndctl_btt_get_devname(btt);
		rc = -ENXIO;
		fd = open(bdevpath, O_RDWR|O_DIRECT);
		if (fd < 0)
			fprintf(stderr, "%s: failed to open %s\n",
					devname, bdevpath);

		while (fd >= 0) {
			rc = pread(fd, buf, 4096, 0);
			if (rc < 4096) {
				/* TODO: track down how this happens! */
				if (errno == ENOENT && retry--) {
					usleep(5000);
					continue;
				}
				fprintf(stderr, "%s: failed to read %s: %d %zd (%s)\n",
						devname, bdevpath, -errno, rc,
						strerror(errno));
				rc = -ENXIO;
				break;
			}
			if (write(fd, buf, 4096) < 4096) {
				fprintf(stderr, "%s: failed to write %s\n",
						devname, bdevpath);
				rc = -ENXIO;
				break;
			}
			rc = 0;
			break;
		}
		if (fd >= 0)
			close(fd);

		if (rc)
			return rc;

		rc = ndctl_btt_delete(btt);
		if (rc)
			fprintf(stderr, "%s: failed to delete btt (%zd)\n", devname, rc);
	}
	free(buf);
	return rc;
}

static int configure_namespace(struct ndctl_region *region,
		struct ndctl_namespace *ndns, struct namespace *namespace)
{
	char devname[50];
	int rc;

	if (!namespace->do_configure)
		return 0;

	snprintf(devname, sizeof(devname), "namespace%d.%d",
			ndctl_region_get_id(region), namespace->id);

	if (ndctl_namespace_is_configured(ndns)) {
		fprintf(stderr, "%s: expected an unconfigured namespace by default\n",
				devname);
		return -ENXIO;
	}

	rc = ndctl_namespace_set_uuid(ndns, namespace->uuid);
	if (rc)
		fprintf(stderr, "%s: set_uuid failed: %d\n", devname, rc);
	rc = ndctl_namespace_set_alt_name(ndns, devname);
	if (rc)
		fprintf(stderr, "%s: set_alt_name failed: %d\n", devname, rc);
	rc = ndctl_namespace_set_size(ndns, namespace->size);
	if (rc)
		fprintf(stderr, "%s: set_size failed: %d\n", devname, rc);

	if (namespace->lbasize)
		rc = ndctl_namespace_set_sector_size(ndns, namespace->lbasize);
	if (rc)
		fprintf(stderr, "%s: set_sector_size failed: %d\n", devname, rc);

	rc = ndctl_namespace_is_configured(ndns);
	if (rc < 1)
		fprintf(stderr, "%s: is_configured: %d\n", devname, rc);

	rc = ndctl_namespace_enable(ndns);
	if (rc)
		fprintf(stderr, "%s: enable: %d\n", devname, rc);

	return rc;
}

static int check_btt_autodetect(struct ndctl_bus *bus,
		struct ndctl_namespace *ndns, void *buf, struct btt *auto_btt)
{
	const char *ndns_bdev = ndctl_namespace_get_block_device(ndns);
	const char *devname = ndctl_namespace_get_devname(ndns);
	struct ndctl_btt *btt, *found = NULL;
	const char *btt_bdev;
	ssize_t rc;
	int fd;

	ndctl_btt_foreach(bus, btt) {
		uuid_t uu;

		ndctl_btt_get_uuid(btt, uu);
		if (uuid_compare(uu, auto_btt->uuid) != 0)
			continue;
		if (!ndctl_btt_is_enabled(btt))
			continue;
		btt_bdev = ndctl_btt_get_backing_dev(btt);
		if (strcmp(btt_bdev+5, ndns_bdev) != 0)
			continue;
		found = btt;
		break;
	}

	if (!found)
		return -ENXIO;

	btt_bdev = strdup(btt_bdev);
	if (!btt_bdev) {
		fprintf(stderr, "%s: failed to dup btt_bdev\n", devname);
		return -ENXIO;
	}

	ndctl_btt_delete(found);

	/* destroy btt */
	fd = open(btt_bdev, O_RDWR|O_DIRECT|O_EXCL);
	if (fd < 0) {
		fprintf(stderr, "%s: failed to open %s to destroy btt\n",
				devname, btt_bdev);
		free((char *) btt_bdev);
		return -ENXIO;
	}

	memset(buf, 0, 4096);
	rc = pwrite(fd, buf, 4096, 4096);
	close(fd);
	if (rc < 4096) {
		rc = -ENXIO;
		fprintf(stderr, "%s: failed to overwrite btt on %s\n",
				devname, btt_bdev);
	}
	free((char *) btt_bdev);

	return rc;
}

static int check_namespaces(struct ndctl_region *region,
		struct namespace **namespaces)
{
	struct ndctl_bus *bus = ndctl_region_get_bus(region);
	struct ndctl_namespace **ndns_save;
	struct namespace *namespace;
	int i, rc, retry_cnt = 1;
	void *buf = NULL;
	char devname[50];

	if (!region)
		return -ENXIO;

	if (posix_memalign(&buf, 4096, 4096) != 0)
		return -ENOMEM;

retry:
	ndns_save = NULL;
	for (i = 0; (namespace = namespaces[i]); i++) {
		struct ndctl_namespace *ndns;
		char bdevpath[50];
		uuid_t uu;
		int fd;

		snprintf(devname, sizeof(devname), "namespace%d.%d",
				ndctl_region_get_id(region), namespace->id);
		ndns = get_namespace_by_id(region, namespace);
		if (!ndns) {
			fprintf(stderr, "%s: failed to find namespace\n",
					devname);
			break;
		}

		rc = configure_namespace(region, ndns, namespace);
		if (rc) {
			fprintf(stderr, "%s: failed to configure namespace\n",
					devname);
			break;
		}
		namespace->do_configure = 0;

		if (strcmp(ndctl_namespace_get_type_name(ndns),
					namespace->type) != 0) {
			fprintf(stderr, "%s: expected type: %s got: %s\n",
					devname,
					ndctl_namespace_get_type_name(ndns),
					namespace->type);
			break;
		}

		if (!ndctl_namespace_is_enabled(ndns)) {
			fprintf(stderr, "%s: expected enabled by default\n",
					devname);
			break;
		}

		if (namespace->size != ndctl_namespace_get_size(ndns)) {
			fprintf(stderr, "%s: expected size: %#llx got: %#llx\n",
					devname, namespace->size,
					ndctl_namespace_get_size(ndns));
			break;
		}

		ndctl_namespace_get_uuid(ndns, uu);
		if (uuid_compare(uu, namespace->uuid) != 0) {
			char expect[40], actual[40];

			uuid_unparse(uu, actual);
			uuid_unparse(namespace->uuid, expect);
			fprintf(stderr, "%s: expected uuid: %s got: %s\n",
					devname, expect, actual);
			break;
		}

		if (namespace->check_alt_name
				&& strcmp(ndctl_namespace_get_alt_name(ndns),
					devname) != 0) {
			fprintf(stderr, "%s: expected alt_name: %s got: %s\n",
					devname, devname,
					ndctl_namespace_get_alt_name(ndns));
			break;
		}

		if (!namespace->btt_settings)
			goto skip_io;

		sprintf(bdevpath, "/dev/%s", ndctl_namespace_get_block_device(ndns));
		fd = open(bdevpath, O_RDWR|O_DIRECT);
		if (fd < 0) {
			fprintf(stderr, "%s: failed to open %s\n",
					devname, bdevpath);
			break;
		}
		if (read(fd, buf, 4096) < 4096) {
			fprintf(stderr, "%s: failed to read %s\n",
					devname, bdevpath);
			close(fd);
			break;
		}
		if (write(fd, buf, 4096) < 4096) {
			fprintf(stderr, "%s: failed to write %s\n",
					devname, bdevpath);
			close(fd);
			break;
		}
		close(fd);

		if (check_btt_create(bus, ndns, namespace->btt_settings) < 0) {
			fprintf(stderr, "%s: failed to create btt\n", devname);
			break;
		}

 skip_io:
		if (ndctl_namespace_disable(ndns) < 0) {
			fprintf(stderr, "%s: failed to disable\n", devname);
			break;
		}

		if (ndctl_namespace_enable(ndns) < 0) {
			fprintf(stderr, "%s: failed to enable\n", devname);
			break;
		}

		if (namespace->btt_settings
				&& check_btt_autodetect(bus, ndns, buf,
					namespace->btt_settings) < 0) {
			fprintf(stderr, "%s, failed btt autodetect\n", devname);
			break;
		}
		ndns_save = realloc(ndns_save,
				sizeof(struct ndctl_namespace *) * (i + 1));
		if (!ndns_save) {
			fprintf(stderr, "%s: %s() -ENOMEM\n",
					devname, __func__);
			rc = -ENOMEM;
			break;
		}
		ndns_save[i] = ndns;
	}

	if (namespace || ndctl_region_disable_preserve(region) != 0) {
		rc = -ENXIO;
		if (!namespace)
			fprintf(stderr, "failed to disable region%d\n",
					ndctl_region_get_id(region));
		goto out;
	}

	/*
	 * On the second time through configure_namespace() is skipped
	 * to test assembling namespace(s) from an existing label set
	 */
	if (retry_cnt--) {
		ndctl_region_enable(region);
		goto retry;
	}

	rc = 0;
	for (i--; i >= 0; i--) {
		struct ndctl_namespace *ndns = ndns_save[i];

		snprintf(devname, sizeof(devname), "namespace%d.%d",
				ndctl_region_get_id(region),
				ndctl_namespace_get_id(ndns));
		if (ndctl_namespace_is_valid(ndns)) {
			fprintf(stderr, "%s: failed to invalidate\n", devname);
			rc = -ENXIO;
			break;
		}
	}
	ndctl_region_cleanup(region);
 out:
	free(ndns_save);
	free(buf);

	return rc;
}

static int check_btt_supported_sectors(struct ndctl_btt *btt, struct btt *expect_btt)
{
	int s, t;
	char devname[50];

	snprintf(devname, sizeof(devname), "btt%d", ndctl_btt_get_id(btt));
	for (s = 0; s < expect_btt->num_sector_sizes; s++) {
		for (t = 0; t < expect_btt->num_sector_sizes; t++) {
			if (ndctl_btt_get_supported_sector_size(btt, t)
					== expect_btt->sector_sizes[s])
				break;
		}
		if (t >= expect_btt->num_sector_sizes) {
			fprintf(stderr, "%s: expected sector_size: %d to be supported\n",
					devname, expect_btt->sector_sizes[s]);
			return -ENXIO;
		}
	}

	return 0;
}

static int check_btts(struct ndctl_bus *bus, struct btt *btts, int n)
{
	int i;

	for (i = 0; i < n; i++) {
		struct ndctl_btt *btt;
		char devname[50];
		uuid_t btt_uuid;
		int rc;

		btt = get_idle_btt(bus);
		if (!btt) {
			fprintf(stderr, "failed to find idle btt\n");
			return -ENXIO;
		}
		snprintf(devname, sizeof(devname), "btt%d",
				ndctl_btt_get_id(btt));
		ndctl_btt_get_uuid(btt, btt_uuid);
		if (uuid_compare(btt_uuid, btts[i].uuid) != 0) {
			char expect[40], actual[40];

			uuid_unparse(btt_uuid, actual);
			uuid_unparse(btts[i].uuid, expect);
			fprintf(stderr, "%s: expected uuid: %s got: %s\n",
					devname, expect, actual);
			return -ENXIO;
		}
		if (ndctl_btt_get_num_sector_sizes(btt) != btts[i].num_sector_sizes) {
			fprintf(stderr, "%s: expected num_sector_sizes: %d got: %d\n",
					devname, btts[i].num_sector_sizes,
					ndctl_btt_get_num_sector_sizes(btt));
		}
		rc = check_btt_supported_sectors(btt, &btts[i]);
		if (rc)
			return rc;
		if (btts[i].enabled && ndctl_btt_is_enabled(btt)) {
			fprintf(stderr, "%s: expected disabled by default\n",
					devname);
			return -ENXIO;
		}
	}

	return 0;
}

struct check_cmd {
	int (*check_fn)(struct ndctl_dimm *dimm, struct check_cmd *check);
	struct ndctl_cmd *cmd;
};

static struct check_cmd *check_cmds;

static int check_get_config_size(struct ndctl_dimm *dimm, struct check_cmd *check)
{
	struct ndctl_cmd *cmd;
	int rc;

	if (check->cmd != NULL) {
		fprintf(stderr, "%s: dimm: %#x expected a NULL command, by default\n",
				__func__, ndctl_dimm_get_handle(dimm));
		return -ENXIO;
	}

	cmd = ndctl_dimm_cmd_new_cfg_size(dimm);
	if (!cmd) {
		fprintf(stderr, "%s: dimm: %#x failed to create cmd\n",
				__func__, ndctl_dimm_get_handle(dimm));
		return -ENOTTY;
	}

	rc = ndctl_cmd_submit(cmd);
	if (rc) {
		fprintf(stderr, "%s: dimm: %#x failed to submit cmd: %d\n",
			__func__, ndctl_dimm_get_handle(dimm), rc);
		ndctl_cmd_unref(cmd);
		return rc;
	}

	if (ndctl_cmd_cfg_size_get_size(cmd) != SZ_128K) {
		fprintf(stderr, "%s: dimm: %#x expect size: %d got: %d\n",
				__func__, ndctl_dimm_get_handle(dimm), SZ_128K,
				ndctl_cmd_cfg_size_get_size(cmd));
		ndctl_cmd_unref(cmd);
		return -ENXIO;
	}

	check->cmd = cmd;
	return 0;
}

static int check_get_config_data(struct ndctl_dimm *dimm, struct check_cmd *check)
{
	struct ndctl_cmd *cmd_size = check_cmds[ND_CMD_GET_CONFIG_SIZE].cmd;
	struct ndctl_cmd *cmd = ndctl_dimm_cmd_new_cfg_read(cmd_size);
	static char buf[SZ_128K];
	ssize_t rc;

	if (!cmd) {
		fprintf(stderr, "%s: dimm: %#x failed to create cmd\n",
				__func__, ndctl_dimm_get_handle(dimm));
		return -ENOTTY;
	}

	rc = ndctl_cmd_submit(cmd);
	if (rc) {
		fprintf(stderr, "%s: dimm: %#x failed to submit cmd: %zd\n",
			__func__, ndctl_dimm_get_handle(dimm), rc);
		ndctl_cmd_unref(cmd);
		return rc;
	}

	rc = ndctl_cmd_cfg_read_get_data(cmd, buf, SZ_128K, 0);
	if (rc != SZ_128K) {
		fprintf(stderr, "%s: dimm: %#x expected read %d bytes, got: %zd\n",
			__func__, ndctl_dimm_get_handle(dimm), SZ_128K, rc);
		ndctl_cmd_unref(cmd);
		return -ENXIO;
	}

	check->cmd = cmd;
	return 0;
}

static int check_set_config_data(struct ndctl_dimm *dimm, struct check_cmd *check)
{
	struct ndctl_cmd *cmd_read = check_cmds[ND_CMD_GET_CONFIG_DATA].cmd;
	struct ndctl_cmd *cmd = ndctl_dimm_cmd_new_cfg_write(cmd_read);
	char buf[20], result[sizeof(buf)];
	size_t rc;

	if (!cmd) {
		fprintf(stderr, "%s: dimm: %#x failed to create cmd\n",
				__func__, ndctl_dimm_get_handle(dimm));
		return -ENOTTY;
	}

	memset(buf, 0, sizeof(buf));
	ndctl_cmd_cfg_write_set_data(cmd, buf, sizeof(buf), 0);
	rc = ndctl_cmd_submit(cmd);
	if (rc) {
		fprintf(stderr, "%s: dimm: %#x failed to submit cmd: %zd\n",
			__func__, ndctl_dimm_get_handle(dimm), rc);
		ndctl_cmd_unref(cmd);
		return rc;
	}

	rc = ndctl_cmd_submit(cmd_read);
	if (rc) {
		fprintf(stderr, "%s: dimm: %#x failed to submit read1: %zd\n",
				__func__, ndctl_dimm_get_handle(dimm), rc);
		ndctl_cmd_unref(cmd);
		return rc;
	}
	ndctl_cmd_cfg_read_get_data(cmd_read, result, sizeof(result), 0);
	if (memcmp(result, buf, sizeof(result)) != 0) {
		fprintf(stderr, "%s: dimm: %#x read1 data miscompare: %zd\n",
				__func__, ndctl_dimm_get_handle(dimm), rc);
		ndctl_cmd_unref(cmd);
		return -ENXIO;
	}

	sprintf(buf, "dimm-%#x", ndctl_dimm_get_handle(dimm));
	ndctl_cmd_cfg_write_set_data(cmd, buf, sizeof(buf), 0);
	rc = ndctl_cmd_submit(cmd);
	if (rc) {
		fprintf(stderr, "%s: dimm: %#x failed to submit cmd: %zd\n",
			__func__, ndctl_dimm_get_handle(dimm), rc);
		ndctl_cmd_unref(cmd);
		return rc;
	}

	rc = ndctl_cmd_submit(cmd_read);
	if (rc) {
		fprintf(stderr, "%s: dimm: %#x failed to submit read2: %zd\n",
				__func__, ndctl_dimm_get_handle(dimm), rc);
		ndctl_cmd_unref(cmd);
		return rc;
	}
	ndctl_cmd_cfg_read_get_data(cmd_read, result, sizeof(result), 0);
	if (memcmp(result, buf, sizeof(result)) != 0) {
		fprintf(stderr, "%s: dimm: %#x read2 data miscompare: %zd\n",
				__func__, ndctl_dimm_get_handle(dimm), rc);
		ndctl_cmd_unref(cmd);
		return rc;
	}

	check->cmd = cmd;
	return 0;
}

#define BITS_PER_LONG 32
static int check_commands(struct ndctl_bus *bus, struct ndctl_dimm *dimm,
		unsigned long commands)
{
	/*
	 * For now, by coincidence, these are indexed in test execution
	 * order such that check_get_config_data can assume that
	 * check_get_config_size has updated
	 * check_cmd[ND_CMD_GET_CONFIG_SIZE].cmd and
	 * check_set_config_data can assume that both
	 * check_get_config_size and check_get_config_data have run
	 */
	static struct check_cmd __check_cmds[] = {
		[ND_CMD_GET_CONFIG_SIZE] = { check_get_config_size },
		[ND_CMD_GET_CONFIG_DATA] = { check_get_config_data },
		[ND_CMD_SET_CONFIG_DATA] = { check_set_config_data },
		[ND_CMD_SMART_THRESHOLD] = { },
	};
	unsigned int i, rc;

	check_cmds = __check_cmds;
	for (i = 0; i < BITS_PER_LONG; i++) {
		struct check_cmd *check = &check_cmds[i];

		if ((commands & (1UL << i)) == 0)
			continue;
		if (!ndctl_dimm_is_cmd_supported(dimm, i)) {
			fprintf(stderr, "%s: bus: %s dimm%d expected cmd: %s supported\n",
					__func__,
					ndctl_bus_get_provider(bus),
					ndctl_dimm_get_id(dimm),
					ndctl_dimm_get_cmd_name(dimm, i));
			return -ENXIO;
		}

		if (!check->check_fn)
			continue;
		rc = check->check_fn(dimm, check);
		if (rc)
			break;
	}

	for (i = 0; i < ARRAY_SIZE(__check_cmds); i++) {
		if (__check_cmds[i].cmd)
			ndctl_cmd_unref(__check_cmds[i].cmd);
		__check_cmds[i].cmd = NULL;
	}

	return rc;
}

static int check_dimms(struct ndctl_bus *bus, struct dimm *dimms, int n,
		unsigned long commands)
{
	int i, rc;

	for (i = 0; i < n; i++) {
		struct ndctl_dimm *dimm = get_dimm_by_handle(bus, dimms[i].handle);

		if (!dimm) {
			fprintf(stderr, "failed to find dimm: %d\n", dimms[i].phys_id);
			return -ENXIO;
		}

		if (ndctl_dimm_get_phys_id(dimm) != dimms[i].phys_id) {
			fprintf(stderr, "dimm%d expected phys_id: %d got: %d\n",
					i, dimms[i].phys_id,
					ndctl_dimm_get_phys_id(dimm));
			return -ENXIO;
		}

		rc = check_commands(bus, dimm, commands);
		if (rc)
			return rc;
	}

	return 0;
}

static int do_test0(struct ndctl_ctx *ctx)
{
	struct ndctl_bus *bus = ndctl_bus_get_by_provider(ctx, NFIT_PROVIDER0);
	struct ndctl_region *region;
	int rc;

	if (!bus)
		return -ENXIO;

	/* disable all regions so that set_config_data commands are permitted */
	ndctl_region_foreach(bus, region)
		ndctl_region_disable_invalidate(region);

	rc = check_dimms(bus, dimms0, ARRAY_SIZE(dimms0), commands0);
	if (rc)
		return rc;

	rc = check_btts(bus, btts0, ARRAY_SIZE(btts0));
	if (rc)
		return rc;

	/* set regions back to their default state */
	ndctl_region_foreach(bus, region)
		ndctl_region_enable(region);

	return check_regions(bus, regions0, ARRAY_SIZE(regions0));
}

static int do_test1(struct ndctl_ctx *ctx)
{
	struct ndctl_bus *bus = ndctl_bus_get_by_provider(ctx, NFIT_PROVIDER1);
	int rc;

	if (!bus)
		return -ENXIO;

	rc = check_btts(bus, btts1, ARRAY_SIZE(btts1));
	if (rc)
		return rc;

	return check_regions(bus, regions1, ARRAY_SIZE(regions1));
}

typedef int (*do_test_fn)(struct ndctl_ctx *ctx);
static do_test_fn do_test[] = {
	do_test0,
	do_test1,
};

int test_libndctl(int loglevel)
{
	unsigned int i;
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
	kmod_set_log_priority(kmod_ctx, loglevel);

	err = kmod_module_new_from_name(kmod_ctx, NFIT_TEST_MODULE, &mod);
	if (err < 0)
		goto err_module;

	err = kmod_module_probe_insert_module(mod, KMOD_PROBE_APPLY_BLACKLIST,
			NULL, NULL, NULL, NULL);
	if (err < 0)
		goto err_module;

	for (i = 0; i < ARRAY_SIZE(do_test); i++) {
		err = do_test[i](ctx);
		if (err < 0) {
			fprintf(stderr, "ndctl-test%d failed: %d\n", i, err);
			break;
		}
	}

	if (i >= ARRAY_SIZE(do_test))
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
	return test_libndctl(LOG_DEBUG);
}
