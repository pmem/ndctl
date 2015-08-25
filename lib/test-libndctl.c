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
#include <sys/ioctl.h>

#include <ccan/array_size/array_size.h>
#include <ndctl/libndctl.h>
#ifdef HAVE_NDCTL_H
#include <linux/ndctl.h>
#else
#include <ndctl.h>
#endif
#include <test-libndctl.h>

#define BLKROGET _IO(0x12,94) /* get read-only status (0 = read_write) */
#define BLKROSET _IO(0x12,93) /* set device read-only (0 = read-write) */

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
 * *) Describes a simple system-physical-address range with a non-aliasing backing
 *    dimm.
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
	union {
		unsigned long flags;
		struct {
			unsigned int f_arm:1;
			unsigned int f_save:1;
			unsigned int f_flush:1;
			unsigned int f_smart:1;
			unsigned int f_restore:1;
		};
	};
};

#define DIMM_HANDLE(n, s, i, c, d) \
	(((n & 0xfff) << 16) | ((s & 0xf) << 12) | ((i & 0xf) << 8) \
	 | ((c & 0xf) << 4) | (d & 0xf))
static struct dimm dimms0[] = {
	{ DIMM_HANDLE(0, 0, 0, 0, 0), 0, { 0 }, },
	{ DIMM_HANDLE(0, 0, 0, 0, 1), 1, { 0 }, },
	{ DIMM_HANDLE(0, 0, 1, 0, 0), 2, { 0 }, },
	{ DIMM_HANDLE(0, 0, 1, 0, 1), 3, { 0 }, },
};

static struct dimm dimms1[] = {
	{
		DIMM_HANDLE(0, 0, 0, 0, 0), 0,
		{
			.f_arm = 1,
			.f_save = 1,
			.f_flush = 1,
			.f_smart = 1,
			.f_restore = 1,
		},
	}
};

struct btt {
	int enabled;
	uuid_t uuid;
	int num_sector_sizes;
	unsigned int sector_sizes[7];
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
	struct btt *btts[2];
	struct namespace *namespaces[4];
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
	int num_sector_sizes;
	int ro;
	unsigned long *sector_sizes;
};

static uuid_t null_uuid;
static unsigned long blk_sector_sizes[7] = { 512, 520, 528, 4096, 4104, 4160, 4224, };
static unsigned long pmem_sector_sizes[1] = { 0 };

static struct namespace namespace0_pmem0 = {
	0, "namespace_pmem", &btt_settings, SZ_18M,
	{ 1, 1, 1, 1,
	  1, 1, 1, 1,
	  1, 1, 1, 1,
	  1, 1, 1, 1, }, 1, 1, 1, 0, pmem_sector_sizes,
};

static struct namespace namespace1_pmem0 = {
	0, "namespace_pmem", &btt_settings, SZ_20M,
	{ 2, 2, 2, 2,
	  2, 2, 2, 2,
	  2, 2, 2, 2,
	  2, 2, 2, 2, }, 1, 1, 1, 0, pmem_sector_sizes,
};

static struct namespace namespace2_blk0 = {
	0, "namespace_blk", NULL, SZ_7M,
	{ 3, 3, 3, 3,
	  3, 3, 3, 3,
	  3, 3, 3, 3,
	  3, 3, 3, 3, }, 1, 1, 7, 0, blk_sector_sizes,
};

static struct namespace namespace2_blk1 = {
	1, "namespace_blk", NULL, SZ_11M,
	{ 4, 4, 4, 4,
	  4, 4, 4, 4,
	  4, 4, 4, 4,
	  4, 4, 4, 4, }, 1, 1, 7, 0, blk_sector_sizes,
};

static struct namespace namespace3_blk0 = {
	0, "namespace_blk", NULL, SZ_7M,
	{ 5, 5, 5, 5,
	  5, 5, 5, 5,
	  5, 5, 5, 5,
	  5, 5, 5, 5, }, 1, 1, 7, 0, blk_sector_sizes,
};

static struct namespace namespace3_blk1 = {
	1, "namespace_blk", NULL, SZ_11M,
	{ 6, 6, 6, 6,
	  6, 6, 6, 6,
	  6, 6, 6, 6,
	  6, 6, 6, 6, }, 1, 1, 7, 0, blk_sector_sizes,
};

static struct namespace namespace4_blk0 = {
	0, "namespace_blk", &btt_settings, SZ_27M,
	{ 7, 7, 7, 7,
	  7, 7, 7, 7,
	  7, 7, 7, 7,
	  7, 7, 7, 7, }, 1, 1, 7, 0, blk_sector_sizes,
};

static struct namespace namespace5_blk0 = {
	0, "namespace_blk", &btt_settings, SZ_27M,
	{ 8, 8, 8, 8,
	  8, 8, 8, 8,
	  8, 8, 8, 8,
	  8, 8, 8, 8, }, 1, 1, 7, 0, blk_sector_sizes,
};

static struct btt default_btt = {
	0, { 0, }, 7, { 512, 520, 528, 4096, 4104, 4160, 4224, },
};

static struct region regions0[] = {
	{ { 1 }, 2, 1, "pmem", SZ_32M, SZ_32M, { 1 },
		.namespaces = {
			[0] = &namespace0_pmem0,
		},
		.btts = {
			[0] = &default_btt,
		},
	},
	{ { 2 }, 4, 1, "pmem", SZ_64M, SZ_64M, { 1 },
		.namespaces = {
			[0] = &namespace1_pmem0,
		},
		.btts = {
			[0] = &default_btt,
		},
	},
	{ { DIMM_HANDLE(0, 0, 0, 0, 0) }, 1, 1, "blk", SZ_18M, SZ_32M,
		.namespaces = {
			[0] = &namespace2_blk0,
			[1] = &namespace2_blk1,
		},
		.btts = {
			[0] = &default_btt,
		},
	},
	{ { DIMM_HANDLE(0, 0, 0, 0, 1) }, 1, 1, "blk", SZ_18M, SZ_32M,
		.namespaces = {
			[0] = &namespace3_blk0,
			[1] = &namespace3_blk1,
		},
		.btts = {
			[0] = &default_btt,
		},
	},
	{ { DIMM_HANDLE(0, 0, 1, 0, 0) }, 1, 1, "blk", SZ_27M, SZ_32M,
		.namespaces = {
			[0] = &namespace4_blk0,
		},
		.btts = {
			[0] = &default_btt,
		},
	},
	{ { DIMM_HANDLE(0, 0, 1, 0, 1) }, 1, 1, "blk", SZ_27M, SZ_32M,
		.namespaces = {
			[0] = &namespace5_blk0,
		},
		.btts = {
			[0] = &default_btt,
		},
	},
};

static struct namespace namespace1 = {
	0, "namespace_io", &btt_settings, SZ_32M,
	{ 0, 0, 0, 0,
	  0, 0, 0, 0,
	  0, 0, 0, 0,
	  0, 0, 0, 0, }, 0, 0, 1, 1, pmem_sector_sizes,
};

static struct region regions1[] = {
	{ { 1 }, 1, 1, "pmem", 0, SZ_32M,
		.namespaces = {
			[0] = &namespace1,
		},
	},
};

static unsigned long dimm_commands0 = 1UL << ND_CMD_GET_CONFIG_SIZE
		| 1UL << ND_CMD_GET_CONFIG_DATA
		| 1UL << ND_CMD_SET_CONFIG_DATA;

static unsigned long bus_commands0 = 1UL << ND_CMD_ARS_CAP
		| 1UL << ND_CMD_ARS_START
		| 1UL << ND_CMD_ARS_STATUS;

static struct ndctl_dimm *get_dimm_by_handle(struct ndctl_bus *bus, unsigned int handle)
{
	struct ndctl_dimm *dimm;

	ndctl_dimm_foreach(bus, dimm)
		if (ndctl_dimm_get_handle(dimm) == handle)
			return dimm;

	return NULL;
}

static struct ndctl_btt *get_idle_btt(struct ndctl_region *region)
{
	struct ndctl_btt *btt;

	ndctl_btt_foreach(region, btt)
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
static int check_btts(struct ndctl_region *region, struct btt **btts);

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

		rc = check_btts(region, regions[i].btts);
		if (rc)
			return rc;

		if (regions[i].namespaces)
			rc = check_namespaces(region, regions[i].namespaces);
		if (rc)
			break;
	}

	return rc;
}

static int check_btt_create(struct ndctl_region *region, struct ndctl_namespace *ndns,
		struct namespace *namespace)
{
	struct btt *btt_s = namespace->btt_settings;
	int i, fd, retry = 10;
	struct ndctl_btt *btt;
	const char *devname;
	char bdevpath[50];
	void *buf = NULL;
	ssize_t rc;

	if (!namespace->btt_settings)
		return 0;

	if (posix_memalign(&buf, 4096, 4096) != 0)
		return -ENXIO;

	for (i = 0; i < btt_s->num_sector_sizes; i++) {
		struct ndctl_namespace *ns_seed = ndctl_region_get_namespace_seed(region);
		struct ndctl_btt *btt_seed = ndctl_region_get_btt_seed(region);

		btt = get_idle_btt(region);
		if (!btt)
			return -ENXIO;

		devname = ndctl_btt_get_devname(btt);
		ndctl_btt_set_uuid(btt, btt_s->uuid);
		ndctl_btt_set_sector_size(btt, btt_s->sector_sizes[i]);
		ndctl_btt_set_namespace(btt, ndns);
		rc = ndctl_btt_enable(btt);
		if (namespace->ro == (rc == 0)) {
			fprintf(stderr, "%s: expected btt enable %s, %s read-%s\n",
					devname,
					namespace->ro ? "failure" : "success",
					ndctl_region_get_devname(region),
					namespace->ro ? "only" : "write");
			return -ENXIO;
		}

		if (btt_seed == ndctl_region_get_btt_seed(region)
				&& btt == btt_seed) {
			fprintf(stderr, "%s: failed to advance btt seed\n",
					ndctl_region_get_devname(region));
			return -ENXIO;
		}

		/* check new seed creation for BLK regions */
		if (ndctl_region_get_type(region) == ND_DEVICE_REGION_BLK) {
			if (ns_seed == ndctl_region_get_namespace_seed(region)
					&& ndns == ns_seed) {
				fprintf(stderr, "%s: failed to advance namespace seed\n",
						ndctl_region_get_devname(region));
				return -ENXIO;
			}
		}

		if (namespace->ro) {
			ndctl_region_set_ro(region, 0);
			rc = ndctl_btt_enable(btt);
			fprintf(stderr, "%s: failed to enable after setting rw\n",
					devname);
			ndctl_region_set_ro(region, 1);
			return -ENXIO;
		}

		sprintf(bdevpath, "/dev/%s", ndctl_btt_get_block_device(btt));
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
		if (namespace->ro)
			ndctl_region_set_ro(region, 1);
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
		struct ndctl_namespace *ndns, struct namespace *namespace,
		unsigned long lbasize)
{
	char devname[50];
	int rc;

	if (!namespace->do_configure)
		return 0;

	snprintf(devname, sizeof(devname), "namespace%d.%d",
			ndctl_region_get_id(region), namespace->id);

	if (!ndctl_namespace_is_configured(ndns)) {
		rc = ndctl_namespace_set_uuid(ndns, namespace->uuid);
		if (rc)
			fprintf(stderr, "%s: set_uuid failed: %d\n", devname, rc);
		rc = ndctl_namespace_set_alt_name(ndns, devname);
		if (rc)
			fprintf(stderr, "%s: set_alt_name failed: %d\n", devname, rc);
		rc = ndctl_namespace_set_size(ndns, namespace->size);
		if (rc)
			fprintf(stderr, "%s: set_size failed: %d\n", devname, rc);
	}

	if (lbasize)
		rc = ndctl_namespace_set_sector_size(ndns, lbasize);
	if (rc)
		fprintf(stderr, "%s: set_sector_size (%lu) failed: %d\n",
			devname, lbasize, rc);

	rc = ndctl_namespace_is_configured(ndns);
	if (rc < 1)
		fprintf(stderr, "%s: is_configured: %d\n", devname, rc);

	rc = check_btt_create(region, ndns, namespace);
	if (rc < 0) {
		fprintf(stderr, "%s: failed to create btt\n", devname);
		return rc;
	}

	rc = ndctl_namespace_enable(ndns);
	if (rc < 0)
		fprintf(stderr, "%s: enable: %d\n", devname, rc);

	return rc;
}

static int check_btt_autodetect(struct ndctl_bus *bus,
		struct ndctl_namespace *ndns, void *buf,
		struct namespace *namespace)
{
	struct ndctl_region *region = ndctl_namespace_get_region(ndns);
	const char *devname = ndctl_namespace_get_devname(ndns);
	struct btt *auto_btt = namespace->btt_settings;
	struct ndctl_btt *btt, *found = NULL;
	ssize_t rc = -ENXIO;
	char bdev[50];
	int fd, ro;

	ndctl_btt_foreach(region, btt) {
		struct ndctl_namespace *btt_ndns;
		uuid_t uu;

		ndctl_btt_get_uuid(btt, uu);
		if (uuid_compare(uu, auto_btt->uuid) != 0)
			continue;
		if (!ndctl_btt_is_enabled(btt))
			continue;
		btt_ndns = ndctl_btt_get_namespace(btt);
		if (strcmp(ndctl_namespace_get_devname(btt_ndns), devname) != 0)
			continue;
		fprintf(stderr, "%s: btt_ndns: %p ndns: %p\n", __func__,
				btt_ndns, ndns);
		found = btt;
		break;
	}

	if (!found)
		return -ENXIO;

	sprintf(bdev, "/dev/%s", ndctl_btt_get_block_device(btt));
	fd = open(bdev, O_RDONLY);
	if (fd < 0)
		return -ENXIO;
	rc = ioctl(fd, BLKROGET, &ro);
	if (rc < 0) {
		fprintf(stderr, "%s: failed to open %s\n", __func__, bdev);
		rc = -ENXIO;
		goto out;
	}

	rc = -ENXIO;
	if (ro != namespace->ro) {
		fprintf(stderr, "%s: read-%s expected read-%s by default\n",
				bdev, ro ? "only" : "write",
				namespace->ro ? "only" : "write");
		goto out;
	}

	/* destroy btt device */
	ndctl_btt_delete(found);

	/* clear read-write, and enable raw mode */
	ndctl_region_set_ro(region, 0);
	ndctl_namespace_set_raw_mode(ndns, 1);
	ndctl_namespace_enable(ndns);

	/* destroy btt metadata */
	sprintf(bdev, "/dev/%s", ndctl_namespace_get_block_device(ndns));
	fd = open(bdev, O_RDWR|O_DIRECT|O_EXCL);
	if (fd < 0) {
		fprintf(stderr, "%s: failed to open %s to destroy btt\n",
				devname, bdev);
		goto out;
	}

	memset(buf, 0, 4096);
	rc = pwrite(fd, buf, 4096, 4096);
	if (rc < 4096) {
		rc = -ENXIO;
		fprintf(stderr, "%s: failed to overwrite btt on %s\n",
				devname, bdev);
	}
 out:
	ndctl_region_set_ro(region, namespace->ro);
	ndctl_namespace_set_raw_mode(ndns, 0);
	if (fd >= 0)
		close(fd);

	return rc;
}

static int check_namespaces(struct ndctl_region *region,
		struct namespace **namespaces)
{
	struct ndctl_bus *bus = ndctl_region_get_bus(region);
	struct ndctl_namespace **ndns_save;
	struct namespace *namespace;
	int i, j, rc, retry_cnt = 1;
	void *buf = NULL, *__ndns_save;
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
		int fd = -1, ro;
		uuid_t uu;

		snprintf(devname, sizeof(devname), "namespace%d.%d",
				ndctl_region_get_id(region), namespace->id);
		ndns = get_namespace_by_id(region, namespace);
		if (!ndns) {
			fprintf(stderr, "%s: failed to find namespace\n",
					devname);
			break;
		}

		for (j = 0; j < namespace->num_sector_sizes; j++) {
			struct btt *btt_s;
			struct ndctl_btt *btt;

			rc = configure_namespace(region, ndns, namespace,
							namespace->sector_sizes[j]);
			if (rc < 0) {
				fprintf(stderr, "%s: failed to configure namespace\n",
						devname);
				break;
			}

			if (strcmp(ndctl_namespace_get_type_name(ndns),
						namespace->type) != 0) {
				fprintf(stderr, "%s: expected type: %s got: %s\n",
						devname,
						ndctl_namespace_get_type_name(ndns),
						namespace->type);
				rc = -ENXIO;
				break;
			}

			/*
			 * On the second time through this loop we skip
			 * establishing btt since check_btt_autodetect()
			 * destroyed the inital instance.
			 */
			btt_s = namespace->do_configure
				? namespace->btt_settings : NULL;

			btt = ndctl_namespace_get_btt(ndns);
			if (!!btt_s != !!btt) {
				fprintf(stderr, "%s expected btt %s by default\n",
						devname, namespace->btt_settings
						? "enabled" : "disabled");
				rc = -ENXIO;
				break;
			}

			if (!btt_s && !ndctl_namespace_is_enabled(ndns)) {
				fprintf(stderr, "%s: expected enabled by default\n",
						devname);
				rc = -ENXIO;
				break;
			}

			if (namespace->size != ndctl_namespace_get_size(ndns)) {
				fprintf(stderr, "%s: expected size: %#llx got: %#llx\n",
						devname, namespace->size,
						ndctl_namespace_get_size(ndns));
				rc = -ENXIO;
				break;
			}

			ndctl_namespace_get_uuid(ndns, uu);
			if (uuid_compare(uu, namespace->uuid) != 0) {
				char expect[40], actual[40];

				uuid_unparse(uu, actual);
				uuid_unparse(namespace->uuid, expect);
				fprintf(stderr, "%s: expected uuid: %s got: %s\n",
						devname, expect, actual);
				rc = -ENXIO;
				break;
			}

			if (namespace->check_alt_name
					&& strcmp(ndctl_namespace_get_alt_name(ndns),
						devname) != 0) {
				fprintf(stderr, "%s: expected alt_name: %s got: %s\n",
						devname, devname,
						ndctl_namespace_get_alt_name(ndns));
				rc = -ENXIO;
				break;
			}

			sprintf(bdevpath, "/dev/%s", btt ? ndctl_btt_get_block_device(btt)
					: ndctl_namespace_get_block_device(ndns));
			fd = open(bdevpath, O_RDONLY);
			if (fd < 0) {
				fprintf(stderr, "%s: failed to open(%s, O_RDONLY)\n",
						devname, bdevpath);
				rc = -ENXIO;
				break;
			}

			rc = ioctl(fd, BLKROGET, &ro);
			if (rc < 0) {
				fprintf(stderr, "%s: BLKROGET failed\n",
						devname);
				rc = -errno;
				break;
			}

			if (namespace->ro != ro) {
				fprintf(stderr, "%s: read-%s expected: read-%s\n",
						devname, ro ? "only" : "write",
						namespace->ro ? "only" : "write");
				rc = -ENXIO;
				break;
			}

			ro = 0;
			rc = ioctl(fd, BLKROSET, &ro);
			if (rc < 0) {
				fprintf(stderr, "%s: BLKROSET failed\n",
						devname);
				rc = -errno;
				break;
			}

			close(fd);
			fd = open(bdevpath, O_RDWR|O_DIRECT);
			if (fd < 0) {
				fprintf(stderr, "%s: failed to open(%s, O_RDWR|O_DIRECT)\n",
						devname, bdevpath);
				rc = -ENXIO;
				break;
			}
			if (read(fd, buf, 4096) < 4096) {
				fprintf(stderr, "%s: failed to read %s\n",
						devname, bdevpath);
				rc = -ENXIO;
				break;
			}
			if (write(fd, buf, 4096) < 4096) {
				fprintf(stderr, "%s: failed to write %s\n",
						devname, bdevpath);
				rc = -ENXIO;
				break;
			}
			close(fd);
			fd = -1;

			if (ndctl_namespace_disable(ndns) < 0) {
				fprintf(stderr, "%s: failed to disable\n", devname);
				rc = -ENXIO;
				break;
			}

			if (ndctl_namespace_enable(ndns) < 0) {
				fprintf(stderr, "%s: failed to enable\n", devname);
				rc = -ENXIO;
				break;
			}

			if (btt_s && check_btt_autodetect(bus, ndns, buf,
						namespace) < 0) {
				fprintf(stderr, "%s, failed btt autodetect\n", devname);
				rc = -ENXIO;
				break;
			}

			/*
			 * if the namespace is being tested with a btt, there is no
			 * point testing different sector sizes for the namespace itself
			 */
			if (btt_s)
				break;

			/*
			 * If this is the last sector size being tested, don't disable
			 * the namespace
			 */
			if (j == namespace->num_sector_sizes - 1)
				break;

			/*
			 * If we're in the second time through this, don't loop for
			 * different sector sizes as ->do_configure is disabled
			 */
			if (!retry_cnt)
				break;

			if (ndctl_namespace_disable(ndns) < 0) {
				fprintf(stderr, "%s: failed to disable\n", devname);
				break;
			}
		}
		if (fd >= 0)
			close(fd);
		namespace->do_configure = 0;

		__ndns_save = realloc(ndns_save,
				sizeof(struct ndctl_namespace *) * (i + 1));
		if (!__ndns_save) {
			fprintf(stderr, "%s: %s() -ENOMEM\n",
					devname, __func__);
			rc = -ENOMEM;
			break;
		} else {
			ndns_save = __ndns_save;
			ndns_save[i] = ndns;
		}

		if (rc)
			break;
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
		free(ndns_save);
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

static int check_btts(struct ndctl_region *region, struct btt **btts)
{
	struct btt *btt_s;
	int i;

	for (i = 0; (btt_s = btts[i]); i++) {
		struct ndctl_btt *btt;
		char devname[50];
		uuid_t btt_uuid;
		int rc;

		btt = get_idle_btt(region);
		if (!btt) {
			fprintf(stderr, "failed to find idle btt\n");
			return -ENXIO;
		}
		snprintf(devname, sizeof(devname), "btt%d",
				ndctl_btt_get_id(btt));
		ndctl_btt_get_uuid(btt, btt_uuid);
		if (uuid_compare(btt_uuid, btt_s->uuid) != 0) {
			char expect[40], actual[40];

			uuid_unparse(btt_uuid, actual);
			uuid_unparse(btt_s->uuid, expect);
			fprintf(stderr, "%s: expected uuid: %s got: %s\n",
					devname, expect, actual);
			return -ENXIO;
		}
		if (ndctl_btt_get_num_sector_sizes(btt) != btt_s->num_sector_sizes) {
			fprintf(stderr, "%s: expected num_sector_sizes: %d got: %d\n",
					devname, btt_s->num_sector_sizes,
					ndctl_btt_get_num_sector_sizes(btt));
		}
		rc = check_btt_supported_sectors(btt, btt_s);
		if (rc)
			return rc;
		if (btt_s->enabled && ndctl_btt_is_enabled(btt)) {
			fprintf(stderr, "%s: expected disabled by default\n",
					devname);
			return -ENXIO;
		}
	}

	return 0;
}

struct check_cmd {
	int (*check_fn)(struct ndctl_bus *bus, struct ndctl_dimm *dimm, struct check_cmd *check);
	struct ndctl_cmd *cmd;
};

static struct check_cmd *check_cmds;

static int check_get_config_size(struct ndctl_bus *bus, struct ndctl_dimm *dimm,
		struct check_cmd *check)
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

static int check_get_config_data(struct ndctl_bus *bus, struct ndctl_dimm *dimm,
		struct check_cmd *check)
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

static int check_set_config_data(struct ndctl_bus *bus, struct ndctl_dimm *dimm,
		struct check_cmd *check)
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

static int check_ars_cap(struct ndctl_bus *bus, struct ndctl_dimm *dimm,
		struct check_cmd *check)
{
	struct ndctl_cmd *cmd;
	int rc;

	if (check->cmd != NULL) {
		fprintf(stderr, "%s: dimm: %#x expected a NULL command, by default\n",
				__func__, ndctl_dimm_get_handle(dimm));
		return -ENXIO;
	}

	cmd = ndctl_bus_cmd_new_ars_cap(bus, 0, 64);
	if (!cmd) {
		fprintf(stderr, "%s: bus: %s failed to create cmd\n",
				__func__, ndctl_bus_get_provider(bus));
		return -ENOTTY;
	}

	rc = ndctl_cmd_submit(cmd);
	if (rc) {
		fprintf(stderr, "%s: bus: %s failed to submit cmd: %d\n",
				__func__, ndctl_bus_get_provider(bus), rc);
		ndctl_cmd_unref(cmd);
		return rc;
	}

	if (ndctl_cmd_ars_cap_get_size(cmd) != 256) {
		fprintf(stderr, "%s: bus: %s expect size: %d got: %d\n",
				__func__, ndctl_bus_get_provider(bus), 256,
				ndctl_cmd_ars_cap_get_size(cmd));
		ndctl_cmd_unref(cmd);
		return -ENXIO;
	}

	check->cmd = cmd;
	return 0;
}

static int check_ars_start(struct ndctl_bus *bus, struct ndctl_dimm *dimm,
		struct check_cmd *check)
{
	struct ndctl_cmd *cmd_ars_cap = check_cmds[ND_CMD_ARS_CAP].cmd;
	struct ndctl_cmd *cmd;
	int rc;

	if (check->cmd != NULL) {
		fprintf(stderr, "%s: dimm: %#x expected a NULL command, by default\n",
				__func__, ndctl_dimm_get_handle(dimm));
		return -ENXIO;
	}

	cmd = ndctl_bus_cmd_new_ars_start(cmd_ars_cap, ND_ARS_PERSISTENT);
	if (!cmd) {
		fprintf(stderr, "%s: bus: %s failed to create cmd\n",
				__func__, ndctl_bus_get_provider(bus));
		return -ENOTTY;
	}

	rc = ndctl_cmd_submit(cmd);
	if (rc) {
		fprintf(stderr, "%s: bus: %s failed to submit cmd: %d\n",
				__func__, ndctl_bus_get_provider(bus), rc);
		ndctl_cmd_unref(cmd);
		return rc;
	}

	check->cmd = cmd;
	return 0;
}

static int check_ars_status(struct ndctl_bus *bus, struct ndctl_dimm *dimm,
		struct check_cmd *check)
{
	struct ndctl_cmd *cmd_ars_cap = check_cmds[ND_CMD_ARS_CAP].cmd;
	struct ndctl_cmd *cmd;
	unsigned int i;
	int rc;

	if (check->cmd != NULL) {
		fprintf(stderr, "%s: dimm: %#x expected a NULL command, by default\n",
				__func__, ndctl_dimm_get_handle(dimm));
		return -ENXIO;
	}
	cmd = ndctl_bus_cmd_new_ars_status(cmd_ars_cap);
	if (!cmd) {
		fprintf(stderr, "%s: bus: %s failed to create cmd\n",
				__func__, ndctl_bus_get_provider(bus));
		return -ENOTTY;
	}

	do {
		rc = ndctl_cmd_submit(cmd);
		if (rc) {
			fprintf(stderr, "%s: bus: %s failed to submit cmd: %d\n",
				__func__, ndctl_bus_get_provider(bus), rc);
			ndctl_cmd_unref(cmd);
			return rc;
		}
	} while (ndctl_cmd_ars_in_progress(cmd));

	for (i = 0; i < ndctl_cmd_ars_num_records(cmd); i++) {
		fprintf(stderr, "%s: record[%d].addr: 0x%x\n", __func__, i,
			ndctl_cmd_ars_get_record_addr(cmd, i));
		fprintf(stderr, "%s: record[%d].length: 0x%x\n", __func__, i,
			ndctl_cmd_ars_get_record_len(cmd, i));
	}

	check->cmd = cmd;
	return 0;
}

#define BITS_PER_LONG 32
static int check_commands(struct ndctl_bus *bus, struct ndctl_dimm *dimm,
		unsigned long bus_commands, unsigned long dimm_commands)
{
	/*
	 * For now, by coincidence, these are indexed in test execution
	 * order such that check_get_config_data can assume that
	 * check_get_config_size has updated
	 * check_cmd[ND_CMD_GET_CONFIG_SIZE].cmd and
	 * check_set_config_data can assume that both
	 * check_get_config_size and check_get_config_data have run
	 */
	static struct check_cmd __check_dimm_cmds[] = {
		[ND_CMD_GET_CONFIG_SIZE] = { check_get_config_size },
		[ND_CMD_GET_CONFIG_DATA] = { check_get_config_data },
		[ND_CMD_SET_CONFIG_DATA] = { check_set_config_data },
		[ND_CMD_SMART_THRESHOLD] = { },
	};
	static struct check_cmd __check_bus_cmds[] = {
		[ND_CMD_ARS_CAP] = { check_ars_cap },
		[ND_CMD_ARS_START] = { check_ars_start },
		[ND_CMD_ARS_STATUS] = { check_ars_status },
	};
	unsigned int i, rc = 0;

	/* Check DIMM commands */
	check_cmds = __check_dimm_cmds;
	for (i = 0; i < BITS_PER_LONG; i++) {
		struct check_cmd *check = &check_cmds[i];

		if ((dimm_commands & (1UL << i)) == 0)
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
		rc = check->check_fn(bus, dimm, check);
		if (rc)
			break;
	}

	for (i = 0; i < ARRAY_SIZE(__check_dimm_cmds); i++) {
		if (__check_dimm_cmds[i].cmd)
			ndctl_cmd_unref(__check_dimm_cmds[i].cmd);
		__check_dimm_cmds[i].cmd = NULL;
	}
	if (rc)
		goto out;

	/* Check Bus commands */
	check_cmds = __check_bus_cmds;
	for (i = 1; i < BITS_PER_LONG; i++) {
		struct check_cmd *check = &check_cmds[i];

		if ((bus_commands & (1UL << i)) == 0)
			continue;
		if (!ndctl_bus_is_cmd_supported(bus, i)) {
			fprintf(stderr, "%s: bus: %s expected cmd: %s supported\n",
					__func__,
					ndctl_bus_get_provider(bus),
					ndctl_bus_get_cmd_name(bus, i));
			return -ENXIO;
		}

		if (!check->check_fn)
			continue;
		rc = check->check_fn(bus, dimm, check);
		if (rc)
			break;
	}

	for (i = 1; i < ARRAY_SIZE(__check_bus_cmds); i++) {
		if (__check_bus_cmds[i].cmd)
			ndctl_cmd_unref(__check_bus_cmds[i].cmd);
		__check_bus_cmds[i].cmd = NULL;
	}

 out:
	return rc;
}

static int check_dimms(struct ndctl_bus *bus, struct dimm *dimms, int n,
		unsigned long bus_commands, unsigned long dimm_commands)
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

		if (ndctl_dimm_has_errors(dimm) != !!dimms[i].flags) {
			fprintf(stderr, "bus: %s dimm%d %s expected%s errors\n",
					ndctl_bus_get_provider(bus), i,
					dimms[i].flags ? "" : " no",
					ndctl_dimm_get_devname(dimm));
			return -ENXIO;
		}

		if (ndctl_dimm_failed_save(dimm) != dimms[i].f_save
				|| ndctl_dimm_failed_arm(dimm) != dimms[i].f_arm
				|| ndctl_dimm_failed_restore(dimm) != dimms[i].f_restore
				|| ndctl_dimm_smart_pending(dimm) != dimms[i].f_smart
				|| ndctl_dimm_failed_flush(dimm) != dimms[i].f_flush) {
			fprintf(stderr, "expected: %s%s%s%s%sgot: %s%s%s%s%s\n",
					dimms[i].f_save ? "save " : "",
					dimms[i].f_arm ? "arm " : "",
					dimms[i].f_restore ? "restore " : "",
					dimms[i].f_smart ? "smart " : "",
					dimms[i].f_flush ? "flush " : "",
					ndctl_dimm_failed_save(dimm) ? "save " : "",
					ndctl_dimm_failed_arm(dimm) ? "arm " : "",
					ndctl_dimm_failed_restore(dimm) ? "restore " : "",
					ndctl_dimm_smart_pending(dimm) ? "smart " : "",
					ndctl_dimm_failed_flush(dimm) ? "flush " : "");
			return -ENXIO;
		}

		rc = check_commands(bus, dimm, bus_commands, dimm_commands);
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

	rc = check_dimms(bus, dimms0, ARRAY_SIZE(dimms0), bus_commands0,
			dimm_commands0);
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

	rc = check_dimms(bus, dimms1, ARRAY_SIZE(dimms1), 0, 0);
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
