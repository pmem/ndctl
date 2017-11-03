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
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include <limits.h>
#include <syslog.h>
#include <libkmod.h>
#include <sys/wait.h>
#include <uuid/uuid.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <linux/version.h>

#include <ccan/array_size/array_size.h>
#include <ndctl/libndctl.h>
#include <daxctl/libdaxctl.h>
#ifdef HAVE_NDCTL_H
#include <linux/ndctl.h>
#else
#include <ndctl.h>
#endif
#include <test.h>

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

static const char *NFIT_PROVIDER0 = "nfit_test.0";
static const char *NFIT_PROVIDER1 = "nfit_test.1";
#define SZ_4K   0x00001000
#define SZ_128K 0x00020000
#define SZ_7M   0x00700000
#define SZ_2M   0x00200000
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
#define SZ_1G   0x40000000

struct dimm {
	unsigned int handle;
	unsigned int phys_id;
	unsigned int subsystem_vendor;
	unsigned short manufacturing_date;
	unsigned char manufacturing_location;
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
	int formats;
	int format[2];
};

#define DIMM_HANDLE(n, s, i, c, d) \
	(((n & 0xfff) << 16) | ((s & 0xf) << 12) | ((i & 0xf) << 8) \
	 | ((c & 0xf) << 4) | (d & 0xf))
static struct dimm dimms0[] = {
	{ DIMM_HANDLE(0, 0, 0, 0, 0), 0, 0, 2016, 10, { 0 }, 2, { 0x201, 0x301, }, },
	{ DIMM_HANDLE(0, 0, 0, 0, 1), 1, 0, 2016, 10, { 0 }, 2, { 0x201, 0x301, }, },
	{ DIMM_HANDLE(0, 0, 1, 0, 0), 2, 0, 2016, 10, { 0 }, 2, { 0x201, 0x301, }, },
	{ DIMM_HANDLE(0, 0, 1, 0, 1), 3, 0, 2016, 10, { 0 }, 2, { 0x201, 0x301, }, },
};

static struct dimm dimms1[] = {
	{
		DIMM_HANDLE(0, 0, 0, 0, 0), 0, 0, 2016, 10, {
			.f_arm = 1,
			.f_save = 1,
			.f_flush = 1,
			.f_smart = 1,
			.f_restore = 1,
		},
		1, { 0x101, },
	},
};

static struct btt {
	int enabled;
	uuid_t uuid;
	int num_sector_sizes;
	unsigned int sector_sizes[7];
} default_btt = {
	0, { 0, }, 7, { 512, 520, 528, 4096, 4104, 4160, 4224, },
};

struct pfn {
	int enabled;
	uuid_t uuid;
	enum ndctl_pfn_loc locs[2];
	unsigned long aligns[4];
};

struct dax {
	int enabled;
	uuid_t uuid;
	enum ndctl_pfn_loc locs[2];
	unsigned long aligns[4];
};

static struct pfn_default {
	int enabled;
	uuid_t uuid;
	enum ndctl_pfn_loc loc;
	unsigned long align;
} default_pfn = {
	.enabled = 0,
	.uuid = { 0, },
	.loc = NDCTL_PFN_LOC_NONE,
	.align = SZ_2M,
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
	struct pfn_default *pfns[2];
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

static struct pfn pfn_settings = {
	.enabled = 1,
	.uuid = {  1,  2,  3,  4,  5,  6,  7, 0,
		   8, 9,  10, 11, 12, 13, 14, 15
	},
	.locs = { NDCTL_PFN_LOC_RAM, NDCTL_PFN_LOC_PMEM },
};

static struct dax dax_settings = {
	.enabled = 1,
	.uuid = {  1,  2,  3,  4,  5,  6,  7, 0,
		   8, 9,  10, 11, 12, 13, 14, 15
	},
	.locs = { NDCTL_PFN_LOC_RAM, NDCTL_PFN_LOC_PMEM },
};

struct namespace {
	unsigned int id;
	char *type;
	struct btt *btt_settings;
	struct pfn *pfn_settings;
	struct dax *dax_settings;
	unsigned long long size;
	uuid_t uuid;
	int do_configure;
	int check_alt_name;
	int ro;
	int num_sector_sizes;
	unsigned long *sector_sizes;
};

static uuid_t null_uuid;
static unsigned long blk_sector_sizes[] = { 512, 520, 528, 4096, 4104, 4160, 4224, };
static unsigned long pmem_sector_sizes[] = { 512, 4096 };
static unsigned long io_sector_sizes[] = { 0 };

static struct namespace namespace0_pmem0 = {
	0, "namespace_pmem", &btt_settings, &pfn_settings, &dax_settings, SZ_18M,
	{ 1, 1, 1, 1,
	  1, 1, 1, 1,
	  1, 1, 1, 1,
	  1, 1, 1, 1, }, 1, 1, 0,
	ARRAY_SIZE(pmem_sector_sizes), pmem_sector_sizes,
};

static struct namespace namespace1_pmem0 = {
	0, "namespace_pmem", &btt_settings, &pfn_settings, &dax_settings, SZ_20M,
	{ 2, 2, 2, 2,
	  2, 2, 2, 2,
	  2, 2, 2, 2,
	  2, 2, 2, 2, }, 1, 1, 0,
	ARRAY_SIZE(pmem_sector_sizes), pmem_sector_sizes,
};

static struct namespace namespace2_blk0 = {
	0, "namespace_blk", NULL, NULL, NULL, SZ_7M,
	{ 3, 3, 3, 3,
	  3, 3, 3, 3,
	  3, 3, 3, 3,
	  3, 3, 3, 3, }, 1, 1, 0,
	ARRAY_SIZE(blk_sector_sizes), blk_sector_sizes,
};

static struct namespace namespace2_blk1 = {
	1, "namespace_blk", NULL, NULL, NULL, SZ_11M,
	{ 4, 4, 4, 4,
	  4, 4, 4, 4,
	  4, 4, 4, 4,
	  4, 4, 4, 4, }, 1, 1, 0,
	ARRAY_SIZE(blk_sector_sizes), blk_sector_sizes,
};

static struct namespace namespace3_blk0 = {
	0, "namespace_blk", NULL, NULL, NULL, SZ_7M,
	{ 5, 5, 5, 5,
	  5, 5, 5, 5,
	  5, 5, 5, 5,
	  5, 5, 5, 5, }, 1, 1, 0,
	ARRAY_SIZE(blk_sector_sizes), blk_sector_sizes,
};

static struct namespace namespace3_blk1 = {
	1, "namespace_blk", NULL, NULL, NULL, SZ_11M,
	{ 6, 6, 6, 6,
	  6, 6, 6, 6,
	  6, 6, 6, 6,
	  6, 6, 6, 6, }, 1, 1, 0,
	ARRAY_SIZE(blk_sector_sizes), blk_sector_sizes,
};

static struct namespace namespace4_blk0 = {
	0, "namespace_blk", &btt_settings, NULL, NULL, SZ_27M,
	{ 7, 7, 7, 7,
	  7, 7, 7, 7,
	  7, 7, 7, 7,
	  7, 7, 7, 7, }, 1, 1, 0,
	ARRAY_SIZE(blk_sector_sizes), blk_sector_sizes,
};

static struct namespace namespace5_blk0 = {
	0, "namespace_blk", &btt_settings, NULL, NULL, SZ_27M,
	{ 8, 8, 8, 8,
	  8, 8, 8, 8,
	  8, 8, 8, 8,
	  8, 8, 8, 8, }, 1, 1, 0,
	ARRAY_SIZE(blk_sector_sizes), blk_sector_sizes,
};

static struct region regions0[] = {
	{ { 1 }, 2, 1, "pmem", SZ_32M, SZ_32M, { 1 },
		.namespaces = {
			[0] = &namespace0_pmem0,
		},
		.btts = {
			[0] = &default_btt,
		},
		.pfns = {
			[0] = &default_pfn,
		},
	},
	{ { 2 }, 4, 1, "pmem", SZ_64M, SZ_64M, { 1 },
		.namespaces = {
			[0] = &namespace1_pmem0,
		},
		.btts = {
			[0] = &default_btt,
		},
		.pfns = {
			[0] = &default_pfn,
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
	0, "namespace_io", &btt_settings, &pfn_settings, &dax_settings, SZ_32M,
	{ 0, 0, 0, 0,
	  0, 0, 0, 0,
	  0, 0, 0, 0,
	  0, 0, 0, 0, }, -1, 0, 1, ARRAY_SIZE(io_sector_sizes), io_sector_sizes,
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
		| 1UL << ND_CMD_SET_CONFIG_DATA | 1UL << ND_CMD_SMART
		| 1UL << ND_CMD_SMART_THRESHOLD;

#ifdef HAVE_NDCTL_CLEAR_ERROR
#define CLEAR_ERROR_CMDS (1UL << ND_CMD_CLEAR_ERROR)
#else
#define CLEAR_ERROR_CMDS 0
#endif

#ifdef HAVE_NDCTL_ARS
#define ARS_CMDS (1UL << ND_CMD_ARS_CAP | 1UL << ND_CMD_ARS_START \
		| 1UL << ND_CMD_ARS_STATUS)
#else
#define ARS_CMDS 0
#endif

static unsigned long bus_commands0 = CLEAR_ERROR_CMDS | ARS_CMDS;

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

static struct ndctl_pfn *get_idle_pfn(struct ndctl_region *region)
{
	struct ndctl_pfn *pfn;

	ndctl_pfn_foreach(region, pfn)
		if (!ndctl_pfn_is_enabled(pfn) && !ndctl_pfn_is_configured(pfn))
			return pfn;

	return NULL;
}

static struct ndctl_dax *get_idle_dax(struct ndctl_region *region)
{
	struct ndctl_dax *dax;

	ndctl_dax_foreach(region, dax)
		if (!ndctl_dax_is_enabled(dax) && !ndctl_dax_is_configured(dax))
			return dax;

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

enum ns_mode {
	BTT, PFN, DAX,
};
static int check_namespaces(struct ndctl_region *region,
		struct namespace **namespaces, enum ns_mode mode);
static int check_btts(struct ndctl_region *region, struct btt **btts);

static int check_regions(struct ndctl_bus *bus, struct region *regions, int n,
		enum ns_mode mode)
{
	struct ndctl_region *region;
	int i, rc = 0;

	for (i = 0; i < n; i++) {
		struct ndctl_interleave_set *iset;
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

		if (regions[i].namespaces[0])
			rc = check_namespaces(region, regions[i].namespaces,
					mode);
		if (rc)
			break;
	}

	if (rc == 0)
		ndctl_region_foreach(bus, region)
			ndctl_region_disable_invalidate(region);

	return rc;
}

static int validate_dax(struct ndctl_dax *dax)
{
	/* TODO: make nfit_test namespaces dax capable */
	struct ndctl_namespace *ndns = ndctl_dax_get_namespace(dax);
	const char *devname = ndctl_namespace_get_devname(ndns);
	struct ndctl_region *region = ndctl_dax_get_region(dax);
	struct ndctl_ctx *ctx = ndctl_dax_get_ctx(dax);
	struct ndctl_test *test = ndctl_get_private_data(ctx);
	struct daxctl_region *dax_region = NULL, *found;
	int rc = -ENXIO, fd, count, dax_expect;
	struct daxctl_dev *dax_dev, *seed;
	struct daxctl_ctx *dax_ctx;
	uuid_t uuid, region_uuid;
	char devpath[50];

	dax_region = ndctl_dax_get_daxctl_region(dax);
	if (!dax_region) {
		fprintf(stderr, "%s: failed to retrieve daxctl_region\n",
				devname);
		return -ENXIO;
	}

	dax_ctx = ndctl_get_daxctl_ctx(ctx);
	count = 0;
	daxctl_region_foreach(dax_ctx, found)
		if (found == dax_region)
			count++;
	if (count != 1) {
		fprintf(stderr, "%s: failed to iterate to single region instance\n",
				devname);
		return -ENXIO;
	}

	if (ndctl_test_attempt(test, KERNEL_VERSION(4, 10, 0))) {
		if (daxctl_region_get_size(dax_region)
				!= ndctl_dax_get_size(dax)) {
			fprintf(stderr, "%s: expect size: %llu != %llu\n",
					devname, ndctl_dax_get_size(dax),
					daxctl_region_get_size(dax_region));
			return -ENXIO;
		}

		if (daxctl_region_get_align(dax_region)
				!= ndctl_dax_get_align(dax)) {
			fprintf(stderr, "%s: expect align: %lu != %lu\n",
					devname, ndctl_dax_get_align(dax),
					daxctl_region_get_align(dax_region));
			return -ENXIO;
		}
	}

	rc = -ENXIO;
	ndctl_dax_get_uuid(dax, uuid);
	daxctl_region_get_uuid(dax_region, region_uuid);
	if (uuid_compare(uuid, region_uuid) != 0) {
		char expect[40], actual[40];

		uuid_unparse(region_uuid, actual);
		uuid_unparse(uuid, expect);
		fprintf(stderr, "%s: expected uuid: %s got: %s\n",
				devname, expect, actual);
		goto out;
	}

	if ((int) ndctl_region_get_id(region)
			!= daxctl_region_get_id(dax_region)) {
		fprintf(stderr, "%s: expected region id: %d got: %d\n",
				devname, ndctl_region_get_id(region),
				daxctl_region_get_id(dax_region));
		goto out;
	}

	dax_dev = daxctl_dev_get_first(dax_region);
	if (!dax_dev) {
		fprintf(stderr, "%s: failed to find daxctl_dev\n",
				devname);
		goto out;
	}

	seed = daxctl_region_get_dev_seed(dax_region);
	if (dax_dev != seed && daxctl_dev_get_size(dax_dev) <= 0) {
		fprintf(stderr, "%s: expected non-zero sized dax device\n",
				devname);
		goto out;
	}

	sprintf(devpath, "/dev/%s", daxctl_dev_get_devname(dax_dev));
	fd = open(devpath, O_RDWR);
	if (fd < 0) {
		fprintf(stderr, "%s: failed to open %s\n", devname, devpath);
		goto out;
	}
	close(fd);

	count = 0;
	daxctl_dev_foreach(dax_region, dax_dev)
		count++;
	dax_expect = seed ? 2 : 1;
	if (count != dax_expect) {
		fprintf(stderr, "%s: expected %d dax device%s, got %d\n",
				devname, dax_expect, dax_expect == 1 ? "" : "s",
				count);
		rc = -ENXIO;
		goto out;
	}

	rc = 0;

 out:
	daxctl_region_unref(dax_region);
	return rc;
}

static int __check_dax_create(struct ndctl_region *region,
		struct ndctl_namespace *ndns, struct namespace *namespace,
		enum ndctl_pfn_loc loc, uuid_t uuid)
{
	struct ndctl_dax *dax_seed = ndctl_region_get_dax_seed(region);
	struct ndctl_ctx *ctx = ndctl_region_get_ctx(region);
	struct ndctl_test *test = ndctl_get_private_data(ctx);
	enum ndctl_namespace_mode mode;
	struct ndctl_dax *dax;
	const char *devname;
	ssize_t rc;

	dax = get_idle_dax(region);
	if (!dax)
		return -ENXIO;

	devname = ndctl_dax_get_devname(dax);
	ndctl_dax_set_uuid(dax, uuid);
	ndctl_dax_set_location(dax, loc);
	/*
	 * nfit_test uses vmalloc()'d resources so the only feasible
	 * alignment is PAGE_SIZE
	 */
	ndctl_dax_set_align(dax, SZ_4K);

	rc = ndctl_namespace_set_enforce_mode(ndns, NDCTL_NS_MODE_DAX);
	if (ndctl_test_attempt(test, KERNEL_VERSION(4, 13, 0)) && rc < 0) {
		fprintf(stderr, "%s: failed to enforce dax mode\n", devname);
		return rc;
	}
	ndctl_dax_set_namespace(dax, ndns);
	rc = ndctl_dax_enable(dax);
	if (rc) {
		fprintf(stderr, "%s: failed to enable dax\n", devname);
		return rc;
	}

	mode = ndctl_namespace_get_mode(ndns);
	if (mode >= 0 && mode != NDCTL_NS_MODE_DAX)
		fprintf(stderr, "%s: expected dax mode got: %d\n",
				devname, mode);

	if (namespace->ro == (rc == 0)) {
		fprintf(stderr, "%s: expected dax enable %s, %s read-%s\n",
				devname,
				namespace->ro ? "failure" : "success",
				ndctl_region_get_devname(region),
				namespace->ro ? "only" : "write");
		return -ENXIO;
	}

	if (dax_seed == ndctl_region_get_dax_seed(region)
			&& dax == dax_seed) {
		fprintf(stderr, "%s: failed to advance dax seed\n",
				ndctl_region_get_devname(region));
		return -ENXIO;
	}

	if (namespace->ro) {
		ndctl_region_set_ro(region, 0);
		rc = ndctl_dax_enable(dax);
		fprintf(stderr, "%s: failed to enable after setting rw\n",
				devname);
		ndctl_region_set_ro(region, 1);
		return -ENXIO;
	}

	rc = validate_dax(dax);
	if (rc) {
		fprintf(stderr, "%s: %s validate_dax failed\n", __func__,
				devname);
		return rc;
	}

	if (namespace->ro)
		ndctl_region_set_ro(region, 1);

	rc = ndctl_dax_delete(dax);
	if (rc)
		fprintf(stderr, "%s: failed to delete dax (%zd)\n", devname, rc);
	return rc;
}

static int check_dax_create(struct ndctl_region *region,
		struct ndctl_namespace *ndns, struct namespace *namespace)
{
	struct dax *dax_s = namespace->dax_settings;
	void *buf = NULL;
	unsigned int i;
	int rc = 0;

	if (!dax_s)
		return 0;

	for (i = 0; i < ARRAY_SIZE(dax_s->locs); i++) {
		/*
		 * The kernel enforces invalidating the previous info
		 * block when the current uuid is does not validate with
		 * the contents of the info block.
		 */
		dax_s->uuid[0]++;
		rc = __check_dax_create(region, ndns, namespace,
				dax_s->locs[i], dax_s->uuid);
		if (rc)
			break;
	}
	free(buf);
	return rc;
}

static int __check_pfn_create(struct ndctl_region *region,
		struct ndctl_namespace *ndns, struct namespace *namespace,
		void *buf, enum ndctl_pfn_loc loc, uuid_t uuid)
{
	struct ndctl_pfn *pfn_seed = ndctl_region_get_pfn_seed(region);
	struct ndctl_ctx *ctx = ndctl_region_get_ctx(region);
	struct ndctl_test *test = ndctl_get_private_data(ctx);
	enum ndctl_namespace_mode mode;
	struct ndctl_pfn *pfn;
	const char *devname;
	int fd, retry = 10;
	char bdevpath[50];
	ssize_t rc;

	pfn = get_idle_pfn(region);
	if (!pfn)
		return -ENXIO;

	devname = ndctl_pfn_get_devname(pfn);
	ndctl_pfn_set_uuid(pfn, uuid);
	ndctl_pfn_set_location(pfn, loc);
	/*
	 * nfit_test uses vmalloc()'d resources so the only feasible
	 * alignment is PAGE_SIZE
	 */
	ndctl_pfn_set_align(pfn, SZ_4K);
	rc = ndctl_namespace_set_enforce_mode(ndns, NDCTL_NS_MODE_MEMORY);
	if (ndctl_test_attempt(test, KERNEL_VERSION(4, 13, 0)) && rc < 0) {
		fprintf(stderr, "%s: failed to enforce pfn mode\n", devname);
		return rc;
	}
	ndctl_pfn_set_namespace(pfn, ndns);
	rc = ndctl_pfn_enable(pfn);
	if (rc) {
		fprintf(stderr, "%s: failed to enable pfn\n", devname);
		return rc;
	}

	mode = ndctl_namespace_get_mode(ndns);
	if (mode >= 0 && mode != NDCTL_NS_MODE_MEMORY)
		fprintf(stderr, "%s: expected memory mode got: %d\n",
				devname, mode);

	if (namespace->ro == (rc == 0)) {
		fprintf(stderr, "%s: expected pfn enable %s, %s read-%s\n",
				devname,
				namespace->ro ? "failure" : "success",
				ndctl_region_get_devname(region),
				namespace->ro ? "only" : "write");
		return -ENXIO;
	}

	if (pfn_seed == ndctl_region_get_pfn_seed(region)
			&& pfn == pfn_seed) {
		fprintf(stderr, "%s: failed to advance pfn seed\n",
				ndctl_region_get_devname(region));
		return -ENXIO;
	}

	if (namespace->ro) {
		ndctl_region_set_ro(region, 0);
		rc = ndctl_pfn_enable(pfn);
		fprintf(stderr, "%s: failed to enable after setting rw\n",
				devname);
		ndctl_region_set_ro(region, 1);
		return -ENXIO;
	}

	sprintf(bdevpath, "/dev/%s", ndctl_pfn_get_block_device(pfn));
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

	rc = ndctl_pfn_delete(pfn);
	if (rc)
		fprintf(stderr, "%s: failed to delete pfn (%zd)\n", devname, rc);
	return rc;
}

static int check_pfn_create(struct ndctl_region *region,
		struct ndctl_namespace *ndns, struct namespace *namespace)
{
	struct pfn *pfn_s = namespace->pfn_settings;
	void *buf = NULL;
	unsigned int i;
	int rc = 0;

	if (!pfn_s)
		return 0;

	if (posix_memalign(&buf, 4096, 4096) != 0)
		return -ENXIO;

	for (i = 0; i < ARRAY_SIZE(pfn_s->locs); i++) {
		/*
		 * The kernel enforces invalidating the previous info
		 * block when the current uuid is does not validate with
		 * the contents of the info block.
		 */
		pfn_s->uuid[0]++;
		rc = __check_pfn_create(region, ndns, namespace, buf,
				pfn_s->locs[i], pfn_s->uuid);
		if (rc)
			break;
	}
	free(buf);
	return rc;
}

static int check_btt_size(struct ndctl_btt *btt)
{
	struct ndctl_ctx *ctx = ndctl_btt_get_ctx(btt);
	struct ndctl_test *test = ndctl_get_private_data(ctx);
	struct ndctl_namespace *ndns = ndctl_btt_get_namespace(btt);
	unsigned long long ns_size = ndctl_namespace_get_size(ndns);
	unsigned long sect_size = ndctl_btt_get_sector_size(btt);
	unsigned long long actual, expect;
	int size_select, sect_select;
	unsigned long long expect_table[][2] = {
		[0] = {
			[0] = 0x11b5400,
			[1] = 0x8daa000,
		},
		[1] = {
			[0] = 0x13b1400,
			[1] = 0x9d8a000,
		},
		[2] = {
			[0] = 0x1aa3600,
			[1] = 0xd51b000,
		},
	};

	if (sect_size >= SZ_4K)
		sect_select = 1;
	else if (sect_size >= 512)
		sect_select = 0;
	else {
		fprintf(stderr, "%s: %s unexpected sector size: %lx\n",
				__func__, ndctl_btt_get_devname(btt),
				sect_size);
		return -ENXIO;
	}

	switch (ns_size) {
	case SZ_18M:
		size_select = 0;
		break;
	case SZ_20M:
		size_select = 1;
		break;
	case SZ_27M:
		size_select = 2;
		break;
	default:
		fprintf(stderr, "%s: %s unexpected namespace size: %llx\n",
				__func__, ndctl_namespace_get_devname(ndns),
				ns_size);
		return -ENXIO;
	}

	/* prior to 4.8 btt devices did not have a size attribute */
	if (!ndctl_test_attempt(test, KERNEL_VERSION(4, 8, 0)))
		return 0;

	expect = expect_table[size_select][sect_select];
	actual = ndctl_btt_get_size(btt);
	if (expect != actual) {
		fprintf(stderr, "%s: namespace: %s unexpected size: %llx (expected: %llx)\n",
				ndctl_btt_get_devname(btt),
				ndctl_namespace_get_devname(ndns), actual, expect);
		return -ENXIO;
	}

	return 0;
}

static int check_btt_create(struct ndctl_region *region, struct ndctl_namespace *ndns,
		struct namespace *namespace)
{
	struct ndctl_ctx *ctx = ndctl_region_get_ctx(region);
	struct ndctl_test *test = ndctl_get_private_data(ctx);
	struct btt *btt_s = namespace->btt_settings;
	int i, fd, retry = 10;
	struct ndctl_btt *btt;
	const char *devname;
	char bdevpath[50];
	void *buf = NULL;
	ssize_t rc = 0;

	if (!namespace->btt_settings)
		return 0;

	if (posix_memalign(&buf, 4096, 4096) != 0)
		return -ENXIO;

	for (i = 0; i < btt_s->num_sector_sizes; i++) {
		struct ndctl_namespace *ns_seed = ndctl_region_get_namespace_seed(region);
		struct ndctl_btt *btt_seed = ndctl_region_get_btt_seed(region);
		enum ndctl_namespace_mode mode;

		btt = get_idle_btt(region);
		if (!btt)
			goto err;

		devname = ndctl_btt_get_devname(btt);
		ndctl_btt_set_uuid(btt, btt_s->uuid);
		ndctl_btt_set_sector_size(btt, btt_s->sector_sizes[i]);
		rc = ndctl_namespace_set_enforce_mode(ndns, NDCTL_NS_MODE_SAFE);
		if (ndctl_test_attempt(test, KERNEL_VERSION(4, 13, 0)) && rc < 0) {
			fprintf(stderr, "%s: failed to enforce btt mode\n", devname);
			goto err;
		}

		ndctl_btt_set_namespace(btt, ndns);
		rc = ndctl_btt_enable(btt);
		if (namespace->ro == (rc == 0)) {
			fprintf(stderr, "%s: expected btt enable %s, %s read-%s\n",
					devname,
					namespace->ro ? "failure" : "success",
					ndctl_region_get_devname(region),
					namespace->ro ? "only" : "write");
			goto err;
		}

		/* prior to v4.5 the mode attribute did not exist */
		if (ndctl_test_attempt(test, KERNEL_VERSION(4, 5, 0))) {
			mode = ndctl_namespace_get_mode(ndns);
			if (mode >= 0 && mode != NDCTL_NS_MODE_SAFE)
				fprintf(stderr, "%s: expected safe mode got: %d\n",
						devname, mode);
		}

		/* prior to v4.13 the expected sizes were different due to BTT1.1 */
		if (ndctl_test_attempt(test, KERNEL_VERSION(4, 13, 0))) {
			rc = check_btt_size(btt);
			if (rc)
				goto err;
		}

		if (btt_seed == ndctl_region_get_btt_seed(region)
				&& btt == btt_seed) {
			fprintf(stderr, "%s: failed to advance btt seed\n",
					ndctl_region_get_devname(region));
			goto err;
		}

		/* check new seed creation for BLK regions */
		if (ndctl_region_get_type(region) == ND_DEVICE_REGION_BLK) {
			if (ns_seed == ndctl_region_get_namespace_seed(region)
					&& ndns == ns_seed) {
				fprintf(stderr, "%s: failed to advance namespace seed\n",
						ndctl_region_get_devname(region));
				goto err;
			}
		}

		if (namespace->ro) {
			ndctl_region_set_ro(region, 0);
			rc = ndctl_btt_enable(btt);
			fprintf(stderr, "%s: failed to enable after setting rw\n",
					devname);
			ndctl_region_set_ro(region, 1);
			goto err;
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
			break;

		rc = ndctl_btt_delete(btt);
		if (rc)
			fprintf(stderr, "%s: failed to delete btt (%zd)\n", devname, rc);
	}
	free(buf);
	return rc;
 err:
	free(buf);
	return -ENXIO;
}

static int configure_namespace(struct ndctl_region *region,
		struct ndctl_namespace *ndns, struct namespace *namespace,
		unsigned long lbasize, enum ns_mode mode)
{
	char devname[50];
	int rc;

	if (namespace->do_configure <= 0)
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

	if (lbasize) {
		rc = ndctl_namespace_set_sector_size(ndns, lbasize);
		if (rc)
			fprintf(stderr, "%s: set_sector_size (%lu) failed: %d\n",
					devname, lbasize, rc);
	}

	rc = ndctl_namespace_is_configured(ndns);
	if (rc < 1)
		fprintf(stderr, "%s: is_configured: %d\n", devname, rc);

	if (mode == BTT) {
		rc = check_btt_create(region, ndns, namespace);
		if (rc < 0) {
			fprintf(stderr, "%s: failed to create btt\n", devname);
			return rc;
		}
	}

	if (mode == PFN) {
		rc = check_pfn_create(region, ndns, namespace);
		if (rc < 0) {
			fprintf(stderr, "%s: failed to create pfn\n", devname);
			return rc;
		}
	}

	if (mode == DAX) {
		rc = check_dax_create(region, ndns, namespace);
		if (rc < 0) {
			fprintf(stderr, "%s: failed to create dax\n", devname);
			return rc;
		}
	}

	rc = ndctl_namespace_enable(ndns);
	if (rc < 0)
		fprintf(stderr, "%s: enable: %d\n", devname, rc);

	return rc;
}

static int check_pfn_autodetect(struct ndctl_bus *bus,
		struct ndctl_namespace *ndns, void *buf,
		struct namespace *namespace)
{
	struct ndctl_region *region = ndctl_namespace_get_region(ndns);
	struct ndctl_ctx *ctx = ndctl_region_get_ctx(region);
	const char *devname = ndctl_namespace_get_devname(ndns);
	struct ndctl_test *test = ndctl_get_private_data(ctx);
	struct pfn *auto_pfn = namespace->pfn_settings;
	struct ndctl_pfn *pfn, *found = NULL;
	enum ndctl_namespace_mode mode;
	ssize_t rc = -ENXIO;
	char bdev[50];
	int fd, ro;

	ndctl_pfn_foreach(region, pfn) {
		struct ndctl_namespace *pfn_ndns;
		uuid_t uu;

		ndctl_pfn_get_uuid(pfn, uu);
		if (uuid_compare(uu, auto_pfn->uuid) != 0)
			continue;
		if (!ndctl_pfn_is_enabled(pfn))
			continue;
		pfn_ndns = ndctl_pfn_get_namespace(pfn);
		if (strcmp(ndctl_namespace_get_devname(pfn_ndns), devname) != 0)
			continue;
		fprintf(stderr, "%s: pfn_ndns: %p ndns: %p\n", __func__,
				pfn_ndns, ndns);
		found = pfn;
		break;
	}

	if (!found)
		return -ENXIO;

	mode = ndctl_namespace_get_enforce_mode(ndns);
	if (ndctl_test_attempt(test, KERNEL_VERSION(4, 13, 0))
			&& mode != NDCTL_NS_MODE_MEMORY) {
		fprintf(stderr, "%s expected enforce_mode pfn\n", devname);
		return -ENXIO;
	}

	sprintf(bdev, "/dev/%s", ndctl_pfn_get_block_device(pfn));
	fd = open(bdev, O_RDONLY);
	if (fd < 0)
		return -ENXIO;
	rc = ioctl(fd, BLKROGET, &ro);
	if (rc < 0) {
		fprintf(stderr, "%s: failed to open %s\n", __func__, bdev);
		rc = -ENXIO;
		goto out;
	}
	close(fd);
	fd = -1;

	rc = -ENXIO;
	if (ro != namespace->ro) {
		fprintf(stderr, "%s: read-%s expected read-%s by default\n",
				bdev, ro ? "only" : "write",
				namespace->ro ? "only" : "write");
		goto out;
	}

	/* destroy pfn device */
	ndctl_pfn_delete(found);

	/* clear read-write, and enable raw mode */
	ndctl_region_set_ro(region, 0);
	ndctl_namespace_set_raw_mode(ndns, 1);
	ndctl_namespace_enable(ndns);

	/* destroy pfn metadata */
	sprintf(bdev, "/dev/%s", ndctl_namespace_get_block_device(ndns));
	fd = open(bdev, O_RDWR|O_DIRECT|O_EXCL);
	if (fd < 0) {
		fprintf(stderr, "%s: failed to open %s to destroy pfn\n",
				devname, bdev);
		goto out;
	}

	memset(buf, 0, 4096);
	rc = pwrite(fd, buf, 4096, 4096);
	if (rc < 4096) {
		rc = -ENXIO;
		fprintf(stderr, "%s: failed to overwrite pfn on %s\n",
				devname, bdev);
	}
 out:
	ndctl_region_set_ro(region, namespace->ro);
	ndctl_namespace_set_raw_mode(ndns, 0);
	if (fd >= 0)
		close(fd);

	return rc;
}

static int check_dax_autodetect(struct ndctl_bus *bus,
		struct ndctl_namespace *ndns, void *buf,
		struct namespace *namespace)
{
	struct ndctl_region *region = ndctl_namespace_get_region(ndns);
	struct ndctl_ctx *ctx = ndctl_region_get_ctx(region);
	const char *devname = ndctl_namespace_get_devname(ndns);
	struct ndctl_test *test = ndctl_get_private_data(ctx);
	struct dax *auto_dax = namespace->dax_settings;
	struct ndctl_dax *dax, *found = NULL;
	enum ndctl_namespace_mode mode;
	ssize_t rc = -ENXIO;
	char bdev[50];
	int fd;

	ndctl_dax_foreach(region, dax) {
		struct ndctl_namespace *dax_ndns;
		uuid_t uu;

		ndctl_dax_get_uuid(dax, uu);
		if (uuid_compare(uu, auto_dax->uuid) != 0)
			continue;
		if (!ndctl_dax_is_enabled(dax))
			continue;
		dax_ndns = ndctl_dax_get_namespace(dax);
		if (strcmp(ndctl_namespace_get_devname(dax_ndns), devname) != 0)
			continue;
		fprintf(stderr, "%s: dax_ndns: %p ndns: %p\n", __func__,
				dax_ndns, ndns);
		found = dax;
		break;
	}

	if (!found)
		return -ENXIO;

	mode = ndctl_namespace_get_enforce_mode(ndns);
	if (ndctl_test_attempt(test, KERNEL_VERSION(4, 13, 0))
			&& mode != NDCTL_NS_MODE_DAX) {
		fprintf(stderr, "%s expected enforce_mode dax\n", devname);
		return -ENXIO;
	}

	rc = validate_dax(dax);
	if (rc) {
		fprintf(stderr, "%s: %s validate_dax failed\n", __func__,
				devname);
		return rc;
	}

	rc = -ENXIO;

	/* destroy dax device */
	ndctl_dax_delete(found);

	/* clear read-write, and enable raw mode */
	ndctl_region_set_ro(region, 0);
	ndctl_namespace_set_raw_mode(ndns, 1);
	ndctl_namespace_enable(ndns);

	/* destroy dax metadata */
	sprintf(bdev, "/dev/%s", ndctl_namespace_get_block_device(ndns));
	fd = open(bdev, O_RDWR|O_DIRECT|O_EXCL);
	if (fd < 0) {
		fprintf(stderr, "%s: failed to open %s to destroy dax\n",
				devname, bdev);
		goto out;
	}

	memset(buf, 0, 4096);
	rc = pwrite(fd, buf, 4096, 4096);
	if (rc < 4096) {
		rc = -ENXIO;
		fprintf(stderr, "%s: failed to overwrite dax on %s\n",
				devname, bdev);
	}
 out:
	ndctl_region_set_ro(region, namespace->ro);
	ndctl_namespace_set_raw_mode(ndns, 0);
	if (fd >= 0)
		close(fd);

	return rc;
}

static int check_btt_autodetect(struct ndctl_bus *bus,
		struct ndctl_namespace *ndns, void *buf,
		struct namespace *namespace)
{
	struct ndctl_region *region = ndctl_namespace_get_region(ndns);
	struct ndctl_ctx *ctx = ndctl_region_get_ctx(region);
	const char *devname = ndctl_namespace_get_devname(ndns);
	struct ndctl_test *test = ndctl_get_private_data(ctx);
	struct btt *auto_btt = namespace->btt_settings;
	struct ndctl_btt *btt, *found = NULL;
	enum ndctl_namespace_mode mode;
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

	mode = ndctl_namespace_get_enforce_mode(ndns);
	if (ndctl_test_attempt(test, KERNEL_VERSION(4, 13, 0))
			&& mode != NDCTL_NS_MODE_SAFE) {
		fprintf(stderr, "%s expected enforce_mode btt\n", devname);
		return -ENXIO;
	}

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
	close(fd);
	fd = -1;

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
	/* Delete both the first and second 4K pages */
	rc = pwrite(fd, buf, 4096, 4096);
	if (rc < 4096) {
		rc = -ENXIO;
		fprintf(stderr, "%s: failed to overwrite btt on %s\n",
				devname, bdev);
		goto out;
	}
	rc = pwrite(fd, buf, 4096, 0);
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

static int validate_bdev(const char *devname, struct ndctl_btt *btt,
		struct ndctl_pfn *pfn, struct ndctl_namespace *ndns,
		struct namespace *namespace, void *buf)
{
	char bdevpath[50];
	int fd, rc, ro;

	if (btt)
		sprintf(bdevpath, "/dev/%s",
				ndctl_btt_get_block_device(btt));
	else if (pfn)
		sprintf(bdevpath, "/dev/%s",
				ndctl_pfn_get_block_device(pfn));
	else
		sprintf(bdevpath, "/dev/%s",
				ndctl_namespace_get_block_device(ndns));

	fd = open(bdevpath, O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "%s: failed to open(%s, O_RDONLY)\n",
				devname, bdevpath);
		return -ENXIO;
	}

	rc = ioctl(fd, BLKROGET, &ro);
	if (rc < 0) {
		fprintf(stderr, "%s: BLKROGET failed\n",
				devname);
		rc = -errno;
		goto out;
	}

	if (namespace->ro != ro) {
		fprintf(stderr, "%s: read-%s expected: read-%s\n",
				devname, ro ? "only" : "write",
				namespace->ro ? "only" : "write");
		rc = -ENXIO;
		goto out;
	}

	ro = 0;
	rc = ioctl(fd, BLKROSET, &ro);
	if (rc < 0) {
		fprintf(stderr, "%s: BLKROSET failed\n",
				devname);
		rc = -errno;
		goto out;
	}

	close(fd);
	fd = open(bdevpath, O_RDWR|O_DIRECT);
	if (fd < 0) {
		fprintf(stderr, "%s: failed to open(%s, O_RDWR|O_DIRECT)\n",
				devname, bdevpath);
		return -ENXIO;
	}
	if (read(fd, buf, 4096) < 4096) {
		fprintf(stderr, "%s: failed to read %s\n",
				devname, bdevpath);
		rc = -ENXIO;
		goto out;
	}
	if (write(fd, buf, 4096) < 4096) {
		fprintf(stderr, "%s: failed to write %s\n",
				devname, bdevpath);
		rc = -ENXIO;
		goto out;
	}
	rc = 0;
 out:
	close(fd);
	return rc;
}

static int check_namespaces(struct ndctl_region *region,
		struct namespace **namespaces, enum ns_mode mode)
{
	struct ndctl_ctx *ctx = ndctl_region_get_ctx(region);
	struct ndctl_test *test = ndctl_get_private_data(ctx);
	struct ndctl_bus *bus = ndctl_region_get_bus(region);
	struct ndctl_namespace **ndns_save;
	struct namespace *namespace;
	int i, j, rc, retry_cnt = 1;
	void *buf = NULL, *__ndns_save;
	char devname[50];

	if (posix_memalign(&buf, 4096, 4096) != 0)
		return -ENOMEM;

	for (i = 0; (namespace = namespaces[i]); i++)
		if (namespace->do_configure >= 0)
			namespace->do_configure = 1;

 retry:
	ndns_save = NULL;
	for (i = 0; (namespace = namespaces[i]); i++) {
		uuid_t uu;
		struct ndctl_namespace *ndns;
		unsigned long _sizes[] = { 0 }, *sector_sizes = _sizes;
		int num_sector_sizes = (int) ARRAY_SIZE(_sizes);

		snprintf(devname, sizeof(devname), "namespace%d.%d",
				ndctl_region_get_id(region), namespace->id);
		ndns = get_namespace_by_id(region, namespace);
		if (!ndns) {
			fprintf(stderr, "%s: failed to find namespace\n",
					devname);
			break;
		}

		if (ndctl_region_get_type(region) == ND_DEVICE_REGION_PMEM
				&& !ndctl_test_attempt(test, KERNEL_VERSION(4, 13, 0)))
			/* pass, no sector_size support for pmem prior to 4.13 */;
		else {
			num_sector_sizes = namespace->num_sector_sizes;
			sector_sizes = namespace->sector_sizes;
		}

		for (j = 0; j < num_sector_sizes; j++) {
			struct btt *btt_s = NULL;
			struct pfn *pfn_s = NULL;
			struct dax *dax_s = NULL;
			struct ndctl_btt *btt = NULL;
			struct ndctl_pfn *pfn = NULL;
			struct ndctl_dax *dax = NULL;

			rc = configure_namespace(region, ndns, namespace,
					sector_sizes[j], mode);
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
			 * establishing btt|pfn since
			 * check_{btt|pfn}_autodetect() destroyed the
			 * inital instance.
			 */
			if (mode == BTT) {
				btt_s = namespace->do_configure > 0
					? namespace->btt_settings : NULL;
				btt = ndctl_namespace_get_btt(ndns);
				if (!!btt_s != !!btt) {
					fprintf(stderr, "%s expected btt %s by default\n",
							devname, namespace->btt_settings
							? "enabled" : "disabled");
					rc = -ENXIO;
					break;
				}
			}

			if (mode == PFN) {
				pfn_s = namespace->do_configure > 0
					? namespace->pfn_settings : NULL;
				pfn = ndctl_namespace_get_pfn(ndns);
				if (!!pfn_s != !!pfn) {
					fprintf(stderr, "%s expected pfn %s by default\n",
							devname, namespace->pfn_settings
							? "enabled" : "disabled");
					rc = -ENXIO;
					break;
				}
			}

			if (mode == DAX) {
				dax_s = namespace->do_configure > 0
					? namespace->dax_settings : NULL;
				dax = ndctl_namespace_get_dax(ndns);
				if (!!dax_s != !!dax) {
					fprintf(stderr, "%s expected dax %s by default\n",
							devname, namespace->dax_settings
							? "enabled" : "disabled");
					rc = -ENXIO;
					break;
				}
			}

			if (!btt_s && !pfn_s && !dax_s
					&& !ndctl_namespace_is_enabled(ndns)) {
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

			if (sector_sizes[j] && sector_sizes[j]
					!= ndctl_namespace_get_sector_size(ndns)) {
				fprintf(stderr, "%s: expected lbasize: %#lx got: %#x\n",
						devname, sector_sizes[j],
						ndctl_namespace_get_sector_size(ndns));
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

			if (dax)
				rc = validate_dax(dax);
			else
				rc = validate_bdev(devname, btt, pfn, ndns,
						namespace, buf);
			if (rc) {
				fprintf(stderr, "%s: %s validate_%s failed\n",
						__func__, devname, dax ? "dax" : "bdev");
				break;
			}

			if (ndctl_namespace_disable_invalidate(ndns) < 0) {
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

			if (pfn_s && check_pfn_autodetect(bus, ndns, buf,
						namespace) < 0) {
				fprintf(stderr, "%s, failed pfn autodetect\n", devname);
				rc = -ENXIO;
				break;
			}

			if (dax_s && check_dax_autodetect(bus, ndns, buf,
						namespace) < 0) {
				fprintf(stderr, "%s, failed dax autodetect\n", devname);
				rc = -ENXIO;
				break;
			}

			/*
			 * if the namespace is being tested with a btt, there is no
			 * point testing different sector sizes for the namespace itself
			 */
			if (btt_s || pfn_s || dax_s)
				break;

			/*
			 * If this is the last sector size being tested, don't disable
			 * the namespace
			 */
			if (j == num_sector_sizes - 1)
				break;

			/*
			 * If we're in the second time through this, don't loop for
			 * different sector sizes as ->do_configure is disabled
			 */
			if (!retry_cnt)
				break;

			if (ndctl_namespace_disable_invalidate(ndns) < 0) {
				fprintf(stderr, "%s: failed to disable\n", devname);
				break;
			}
		}
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
	struct ndctl_test *test;
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

#ifdef HAVE_NDCTL_SMART
#define __check_smart(dimm, cmd, field) ({ \
	if (ndctl_cmd_smart_get_##field(cmd) != smart_data.field) { \
		fprintf(stderr, "%s dimm: %#x expected field %#x got: %#x\n", \
				__func__, ndctl_dimm_get_handle(dimm), \
				smart_data.field, \
				ndctl_cmd_smart_get_##field(cmd)); \
		ndctl_cmd_unref(cmd); \
		return -ENXIO; \
	} \
})

static int check_smart(struct ndctl_bus *bus, struct ndctl_dimm *dimm,
		struct check_cmd *check)
{
	static const struct nd_smart_payload smart_data = {
		.flags = ND_SMART_HEALTH_VALID | ND_SMART_TEMP_VALID
			| ND_SMART_SPARES_VALID | ND_SMART_ALARM_VALID
			| ND_SMART_USED_VALID | ND_SMART_SHUTDOWN_VALID,
		.health = ND_SMART_NON_CRITICAL_HEALTH,
		.temperature = 23 * 16,
		.spares = 75,
		.alarm_flags = ND_SMART_SPARE_TRIP | ND_SMART_TEMP_TRIP,
		.life_used = 5,
		.shutdown_state = 0,
		.vendor_size = 0,
	};
	struct ndctl_cmd *cmd = ndctl_dimm_cmd_new_smart(dimm);
	int rc;

	if (!cmd) {
		fprintf(stderr, "%s: dimm: %#x failed to create cmd\n",
				__func__, ndctl_dimm_get_handle(dimm));
		return -ENXIO;
	}

	rc = ndctl_cmd_submit(cmd);
	if (rc) {
		fprintf(stderr, "%s: dimm: %#x failed to submit cmd: %d\n",
			__func__, ndctl_dimm_get_handle(dimm), rc);
		ndctl_cmd_unref(cmd);
		return rc;
	}

	__check_smart(dimm, cmd, flags);
	__check_smart(dimm, cmd, health);
	__check_smart(dimm, cmd, temperature);
	__check_smart(dimm, cmd, spares);
	__check_smart(dimm, cmd, alarm_flags);
	__check_smart(dimm, cmd, life_used);
	__check_smart(dimm, cmd, shutdown_state);
	__check_smart(dimm, cmd, vendor_size);

	ndctl_cmd_unref(cmd);
	return 0;
}

#define __check_smart_threshold(dimm, cmd, field) ({ \
	if (ndctl_cmd_smart_threshold_get_##field(cmd) != smart_t_data.field) { \
		fprintf(stderr, "%s dimm: %#x expected field %#x got: %#x\n", \
				__func__, ndctl_dimm_get_handle(dimm), \
				smart_t_data.field, \
				ndctl_cmd_smart_threshold_get_##field(cmd)); \
		ndctl_cmd_unref(cmd); \
		return -ENXIO; \
	} \
})

static int check_smart_threshold(struct ndctl_bus *bus, struct ndctl_dimm *dimm,
		struct check_cmd *check)
{
	static const struct nd_smart_threshold_payload smart_t_data = {
		.alarm_control = ND_SMART_SPARE_TRIP | ND_SMART_TEMP_TRIP,
		.temperature = 40 * 16,
		.spares = 5,
	};
	struct ndctl_cmd *cmd = ndctl_dimm_cmd_new_smart_threshold(dimm);
	struct timeval tm;
	char buf[4096];
	fd_set fds;
	int rc, fd;

	if (!cmd) {
		fprintf(stderr, "%s: dimm: %#x failed to create cmd\n",
				__func__, ndctl_dimm_get_handle(dimm));
		return -ENXIO;
	}

	fd = ndctl_dimm_get_health_eventfd(dimm);
	FD_ZERO(&fds);
	FD_SET(fd, &fds);
	rc = pread(fd, buf, sizeof(buf), 0);
	tm.tv_sec = 0;
	tm.tv_usec = 500;
	rc = select(fd + 1, NULL, NULL, &fds, &tm);
	if (rc) {
		fprintf(stderr, "%s: expected health event timeout\n",
				ndctl_dimm_get_devname(dimm));
		return -ENXIO;
	}

	/*
	 * Starting with v4.9 smart threshold requests trigger the file
	 * descriptor returned by ndctl_dimm_get_health_eventfd().
	 */
	if (ndctl_test_attempt(check->test, KERNEL_VERSION(4, 9, 0))) {
		int pid = fork();

		if (pid == 0) {
			FD_ZERO(&fds);
			FD_SET(fd, &fds);
			tm.tv_sec = 1;
			tm.tv_usec = 0;
			rc = select(fd + 1, NULL, NULL, &fds, &tm);
			if (rc != 1 || !FD_ISSET(fd, &fds))
				exit(EXIT_FAILURE);
			rc = pread(fd, buf, sizeof(buf), 0);
			exit(EXIT_SUCCESS);
		}
	}

	rc = ndctl_cmd_submit(cmd);
	if (rc) {
		fprintf(stderr, "%s: dimm: %#x failed to submit cmd: %d\n",
			__func__, ndctl_dimm_get_handle(dimm), rc);
		ndctl_cmd_unref(cmd);
		return rc;
	}

	if (ndctl_test_attempt(check->test, KERNEL_VERSION(4, 9, 0))) {
		wait(&rc);
		if (WEXITSTATUS(rc) == EXIT_FAILURE) {
			fprintf(stderr, "%s: expect health event trigger\n",
					ndctl_dimm_get_devname(dimm));
			return -ENXIO;
		}
	}

	__check_smart_threshold(dimm, cmd, alarm_control);
	__check_smart_threshold(dimm, cmd, temperature);
	__check_smart_threshold(dimm, cmd, spares);

	ndctl_cmd_unref(cmd);
	return 0;
}
#else
static int check_smart(struct ndctl_bus *bus, struct ndctl_dimm *dimm,
		struct check_cmd *check)
{
	fprintf(stderr, "%s: HAVE_NDCTL_SMART disabled, skipping\n", __func__);
	return 0;
}

static int check_smart_threshold(struct ndctl_bus *bus, struct ndctl_dimm *dimm,
		struct check_cmd *check)
{
	fprintf(stderr, "%s: HAVE_NDCTL_SMART disabled, skipping\n", __func__);
	return 0;
}
#endif

#define BITS_PER_LONG 32
static int check_commands(struct ndctl_bus *bus, struct ndctl_dimm *dimm,
		unsigned long bus_commands, unsigned long dimm_commands,
		struct ndctl_test *test)
{
	/*
	 * For now, by coincidence, these are indexed in test execution
	 * order such that check_get_config_data can assume that
	 * check_get_config_size has updated
	 * check_cmd[ND_CMD_GET_CONFIG_SIZE].cmd and
	 * check_set_config_data can assume that both
	 * check_get_config_size and check_get_config_data have run
	 */
	struct check_cmd __check_dimm_cmds[] = {
		[ND_CMD_GET_CONFIG_SIZE] = { check_get_config_size },
		[ND_CMD_GET_CONFIG_DATA] = { check_get_config_data },
		[ND_CMD_SET_CONFIG_DATA] = { check_set_config_data },
		[ND_CMD_SMART] = { check_smart },
		[ND_CMD_SMART_THRESHOLD] = {
			.check_fn = check_smart_threshold,
			.test = test,
		},
	};

	unsigned int i, rc = 0;

	/*
	 * The kernel did not start emulating v1.2 namespace spec smart data
	 * until 4.9.
	 */
	if (!ndctl_test_attempt(test, KERNEL_VERSION(4, 9, 0)))
		dimm_commands &= ~((1 << ND_CMD_SMART)
				| (1 << ND_CMD_SMART_THRESHOLD));

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

	if (!ndctl_test_attempt(test, KERNEL_VERSION(4, 6, 0)))
		goto out;

 out:
	return rc;
}

static int check_dimms(struct ndctl_bus *bus, struct dimm *dimms, int n,
		unsigned long bus_commands, unsigned long dimm_commands,
		struct ndctl_test *test)
{
	int i, j, rc;

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
					ndctl_dimm_get_devname(dimm),
					dimms[i].flags ? "" : " no");
			return -ENXIO;
		}

		if (ndctl_dimm_failed_save(dimm) != dimms[i].f_save
				|| ndctl_dimm_failed_arm(dimm) != dimms[i].f_arm
				|| ndctl_dimm_failed_restore(dimm) != dimms[i].f_restore
				|| ndctl_dimm_smart_pending(dimm) != dimms[i].f_smart
				|| ndctl_dimm_failed_flush(dimm) != dimms[i].f_flush) {
			fprintf(stderr, "expected: %s%s%s%s%sgot: %s%s%s%s%s\n",
					dimms[i].f_save ? "save_fail " : "",
					dimms[i].f_arm ? "not_armed " : "",
					dimms[i].f_restore ? "restore_fail " : "",
					dimms[i].f_smart ? "smart_event " : "",
					dimms[i].f_flush ? "flush_fail " : "",
					ndctl_dimm_failed_save(dimm) ? "save_fail " : "",
					ndctl_dimm_failed_arm(dimm) ? "not_armed " : "",
					ndctl_dimm_failed_restore(dimm) ? "restore_fail " : "",
					ndctl_dimm_smart_pending(dimm) ? "smart_event " : "",
					ndctl_dimm_failed_flush(dimm) ? "flush_fail " : "");
			return -ENXIO;
		}

		if (ndctl_test_attempt(test, KERNEL_VERSION(4, 7, 0))) {
			if (ndctl_dimm_get_formats(dimm) != dimms[i].formats) {
				fprintf(stderr, "dimm%d expected formats: %d got: %d\n",
						i, dimms[i].formats,
						ndctl_dimm_get_formats(dimm));
				return -ENXIO;
			}
			for (j = 0; j < dimms[i].formats; j++) {
				if (ndctl_dimm_get_formatN(dimm, j) != dimms[i].format[j]) {
					fprintf(stderr,
						"dimm%d expected format[%d]: %d got: %d\n",
							i, j, dimms[i].format[j],
							ndctl_dimm_get_formatN(dimm, j));
					return -ENXIO;
				}
			}
		}

		if (ndctl_test_attempt(test, KERNEL_VERSION(4, 7, 0))) {
			if (ndctl_dimm_get_subsystem_vendor(dimm)
					!= dimms[i].subsystem_vendor) {
				fprintf(stderr,
					"dimm%d expected subsystem vendor: %d got: %d\n",
						i, dimms[i].subsystem_vendor,
						ndctl_dimm_get_subsystem_vendor(dimm));
				return -ENXIO;
			}
		}

		if (ndctl_test_attempt(test, KERNEL_VERSION(4, 8, 0))) {
			if (ndctl_dimm_get_manufacturing_date(dimm)
					!= dimms[i].manufacturing_date) {
				fprintf(stderr,
					"dimm%d expected manufacturing date: %d got: %d\n",
						i, dimms[i].manufacturing_date,
						ndctl_dimm_get_manufacturing_date(dimm));
				return -ENXIO;
			}
		}

		rc = check_commands(bus, dimm, bus_commands, dimm_commands, test);
		if (rc)
			return rc;
	}

	return 0;
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

	/* set regions back to their default state */
	ndctl_region_foreach(bus, region)
		ndctl_region_enable(region);
}

static int do_test0(struct ndctl_ctx *ctx, struct ndctl_test *test)
{
	struct ndctl_bus *bus = ndctl_bus_get_by_provider(ctx, NFIT_PROVIDER0);
	struct ndctl_region *region;
	struct ndctl_dimm *dimm;
	int rc;

	if (!bus)
		return -ENXIO;

	ndctl_bus_wait_probe(bus);

	/* disable all regions so that set_config_data commands are permitted */
	ndctl_region_foreach(bus, region)
		ndctl_region_disable_invalidate(region);

	rc = check_dimms(bus, dimms0, ARRAY_SIZE(dimms0), bus_commands0,
			dimm_commands0, test);
	if (rc)
		return rc;

	ndctl_dimm_foreach(bus, dimm) {
		rc = ndctl_dimm_zero_labels(dimm);
		if (rc < 0) {
			fprintf(stderr, "failed to zero %s\n",
					ndctl_dimm_get_devname(dimm));
			return rc;
		}
	}

	/* set regions back to their default state */
	ndctl_region_foreach(bus, region)
		ndctl_region_enable(region);

	/* pfn and dax tests require vmalloc-enabled nfit_test */
	if (ndctl_test_attempt(test, KERNEL_VERSION(4, 8, 0))) {
		rc = check_regions(bus, regions0, ARRAY_SIZE(regions0), DAX);
		if (rc)
			return rc;
		reset_bus(bus);
	}

	if (ndctl_test_attempt(test, KERNEL_VERSION(4, 8, 0))) {
		rc = check_regions(bus, regions0, ARRAY_SIZE(regions0), PFN);
		if (rc)
			return rc;
		reset_bus(bus);
	}

	return check_regions(bus, regions0, ARRAY_SIZE(regions0), BTT);
}

static int do_test1(struct ndctl_ctx *ctx, struct ndctl_test *test)
{
	struct ndctl_bus *bus = ndctl_bus_get_by_provider(ctx, NFIT_PROVIDER1);
	int rc;

	if (!bus)
		return -ENXIO;

	ndctl_bus_wait_probe(bus);

	/*
	 * Starting with v4.10 the dimm on nfit_test.1 gets a unique
	 * handle.
	 */
	if (ndctl_test_attempt(test, KERNEL_VERSION(4, 10, 0)))
		dimms1[0].handle = DIMM_HANDLE(1, 0, 0, 0, 0);

	rc = check_dimms(bus, dimms1, ARRAY_SIZE(dimms1), 0, 0, test);
	if (rc)
		return rc;

	return check_regions(bus, regions1, ARRAY_SIZE(regions1), BTT);
}

typedef int (*do_test_fn)(struct ndctl_ctx *ctx, struct ndctl_test *test);
static do_test_fn do_test[] = {
	do_test0,
	do_test1,
};

int test_libndctl(int loglevel, struct ndctl_test *test, struct ndctl_ctx *ctx)
{
	unsigned int i;
	struct kmod_module *mod;
	struct kmod_ctx *kmod_ctx;
	struct daxctl_ctx *daxctl_ctx;
	int err, result = EXIT_FAILURE;

	if (!ndctl_test_attempt(test, KERNEL_VERSION(4, 2, 0)))
		return 77;

	ndctl_set_log_priority(ctx, loglevel);
	daxctl_ctx = ndctl_get_daxctl_ctx(ctx);
	daxctl_set_log_priority(daxctl_ctx, loglevel);
	ndctl_set_private_data(ctx, test);

	err = nfit_test_init(&kmod_ctx, &mod, loglevel, test);
	if (err < 0) {
		ndctl_test_skip(test);
		fprintf(stderr, "nfit_test unavailable skipping tests\n");
		return 77;
	}

	for (i = 0; i < ARRAY_SIZE(do_test); i++) {
		err = do_test[i](ctx, test);
		if (err < 0) {
			fprintf(stderr, "ndctl-test%d failed: %d\n", i, err);
			break;
		}
	}

	if (i >= ARRAY_SIZE(do_test))
		result = EXIT_SUCCESS;
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
	rc = test_libndctl(LOG_DEBUG, test, ctx);
	ndctl_unref(ctx);
	return ndctl_test_result(test, rc);
}
