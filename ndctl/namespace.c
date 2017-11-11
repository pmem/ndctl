/*
 * Copyright(c) 2015-2017 Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <syslog.h>
#include "action.h"
#include <sys/stat.h>
#include <uuid/uuid.h>
#include <sys/types.h>
#include <util/size.h>
#include <util/json.h>
#include <json-c/json.h>
#include <util/filter.h>
#include <ndctl/libndctl.h>
#include <util/parse-options.h>
#include <ccan/minmax/minmax.h>
#include <ccan/array_size/array_size.h>

#ifdef HAVE_NDCTL_H
#include <linux/ndctl.h>
#else
#include <ndctl.h>
#endif

static bool verbose;
static bool force;
static bool repair;
static struct parameters {
	bool do_scan;
	bool mode_default;
	bool align_default;
	bool autolabel;
	const char *bus;
	const char *map;
	const char *type;
	const char *uuid;
	const char *name;
	const char *size;
	const char *mode;
	const char *region;
	const char *reconfig;
	const char *sector_size;
	const char *align;
} param = {
	.autolabel = true,
};

void builtin_xaction_namespace_reset(void)
{
	/*
	 * Initialize parameter data for the unit test case where
	 * multiple calls to cmd_<action>_namespace() are made without
	 * an intervening exit().
	 */
	verbose = false;
	force = false;
	memset(&param, 0, sizeof(param));
}

#define NSLABEL_NAME_LEN 64
struct parsed_parameters {
	enum ndctl_pfn_loc loc;
	uuid_t uuid;
	char name[NSLABEL_NAME_LEN];
	enum ndctl_namespace_mode mode;
	unsigned long long size;
	unsigned long sector_size;
	unsigned long align;
	bool autolabel;
};

#define debug(fmt, ...) \
	({if (verbose) { \
		fprintf(stderr, "%s:%d: " fmt, __func__, __LINE__, ##__VA_ARGS__); \
	} else { \
		do { } while (0); \
	}})

#define BASE_OPTIONS() \
OPT_STRING('b', "bus", &param.bus, "bus-id", \
	"limit namespace to a bus with an id or provider of <bus-id>"), \
OPT_STRING('r', "region", &param.region, "region-id", \
	"limit namespace to a region with an id or name of <region-id>"), \
OPT_BOOLEAN('v', "verbose", &verbose, "emit extra debug messages to stderr")

#define CREATE_OPTIONS() \
OPT_STRING('e', "reconfig", &param.reconfig, "reconfig namespace", \
	"reconfigure existing namespace"), \
OPT_STRING('u', "uuid", &param.uuid, "uuid", \
	"specify the uuid for the namespace (default: autogenerate)"), \
OPT_STRING('n', "name", &param.name, "name", \
	"specify an optional free form name for the namespace"), \
OPT_STRING('s', "size", &param.size, "size", \
	"specify the namespace size in bytes (default: available capacity)"), \
OPT_STRING('m', "mode", &param.mode, "operation-mode", \
	"specify a mode for the namespace, 'sector', 'memory', or 'raw'"), \
OPT_STRING('M', "map", &param.map, "memmap-location", \
	"specify 'mem' or 'dev' for the location of the memmap"), \
OPT_STRING('l', "sector-size", &param.sector_size, "lba-size", \
	"specify the logical sector size in bytes"), \
OPT_STRING('t', "type", &param.type, "type", \
	"specify the type of namespace to create 'pmem' or 'blk'"), \
OPT_STRING('a', "align", &param.align, "align", \
	"specify the namespace alignment in bytes (default: 2M)"), \
OPT_BOOLEAN('f', "force", &force, "reconfigure namespace even if currently active"), \
OPT_BOOLEAN('L', "autolabel", &param.autolabel, "automatically initialize labels")

#define CHECK_OPTIONS() \
OPT_BOOLEAN('R', "repair", &repair, "perform metadata repairs"), \
OPT_BOOLEAN('f', "force", &force, "check namespace even if currently active")

static const struct option base_options[] = {
	BASE_OPTIONS(),
	OPT_END(),
};

static const struct option destroy_options[] = {
	BASE_OPTIONS(),
	OPT_BOOLEAN('f', "force", &force,
			"destroy namespace even if currently active"),
	OPT_END(),
};

static const struct option create_options[] = {
	BASE_OPTIONS(),
	CREATE_OPTIONS(),
	OPT_END(),
};

static const struct option check_options[] = {
	BASE_OPTIONS(),
	CHECK_OPTIONS(),
	OPT_END(),
};

static int set_defaults(enum device_action mode)
{
	int rc = 0;

	if (param.type) {
		if (strcmp(param.type, "pmem") == 0)
			/* pass */;
		else if (strcmp(param.type, "blk") == 0)
			/* pass */;
		else {
			error("invalid type '%s', must be either 'pmem' or 'blk'\n",
				param.type);
			rc = -EINVAL;
		}
	} else if (!param.reconfig && mode == ACTION_CREATE)
		param.type = "pmem";

	if (param.mode) {
		if (strcmp(param.mode, "safe") == 0)
			/* pass */;
		else if (strcmp(param.mode, "sector") == 0)
		      param.mode = "safe"; /* pass */
		else if (strcmp(param.mode, "memory") == 0)
		      /* pass */;
		else if (strcmp(param.mode, "raw") == 0)
		      /* pass */;
		else if (strcmp(param.mode, "dax") == 0)
		      /* pass */;
		else {
			error("invalid mode '%s'\n", param.mode);
			rc = -EINVAL;
		}
	} else if (!param.reconfig && param.type) {
		if (strcmp(param.type, "pmem") == 0)
			param.mode = "memory";
		else
			param.mode = "safe";
		param.mode_default = true;
	}

	if (param.map) {
		if (strcmp(param.map, "mem") == 0)
			/* pass */;
		else if (strcmp(param.map, "dev") == 0)
			/* pass */;
		else {
			error("invalid map location '%s'\n", param.map);
			rc = -EINVAL;
		}

		if (!param.reconfig && param.mode
				&& strcmp(param.mode, "memory") != 0) {
			error("--map only valid for a memory mode pmem namespace\n");
			rc = -EINVAL;
		}
	} else if (!param.reconfig)
		param.map = "dev";

	/* check for incompatible mode and type combinations */
	if (param.type && param.mode && strcmp(param.type, "blk") == 0
			&& strcmp(param.mode, "memory") == 0) {
		error("only 'pmem' namespaces can be placed into 'memory' mode\n");
		rc = -ENXIO;
	}

	if (param.size && parse_size64(param.size) == ULLONG_MAX) {
		error("failed to parse namespace size '%s'\n",
				param.size);
		rc = -EINVAL;
	}

	if (param.align && parse_size64(param.align) == ULLONG_MAX) {
		error("failed to parse namespace alignment '%s'\n",
				param.align);
		rc = -EINVAL;
	} else if (!param.align) {
		param.align = "2M";
		param.align_default = true;
	}

	if (param.uuid) {
		uuid_t uuid;

		if (uuid_parse(param.uuid, uuid)) {
			error("failed to parse uuid: '%s'\n", param.uuid);
			rc = -EINVAL;
		}
	}

	if (param.sector_size) {
		if (parse_size64(param.sector_size) == ULLONG_MAX) {
			error("invalid sector size: %s\n", param.sector_size);
			rc = -EINVAL;
		}
	} else if (!param.reconfig
			&& ((param.type && strcmp(param.type, "blk") == 0)
				|| (param.mode
					&& strcmp(param.mode, "safe") == 0))) {
			/* default sector size for blk-type or safe-mode */
			param.sector_size = "4096";
	}

	return rc;
}

/*
 * parse_namespace_options - basic parsing sanity checks before we start
 * looking at actual namespace devices and available resources.
 */
static const char *parse_namespace_options(int argc, const char **argv,
		enum device_action mode, const struct option *options,
		char *xable_usage)
{
	const char * const u[] = {
		xable_usage,
		NULL
	};
	int i, rc = 0;

	param.do_scan = argc == 1;
        argc = parse_options(argc, argv, options, u, 0);

	rc = set_defaults(mode);

	if (argc == 0 && mode != ACTION_CREATE) {
		char *action_string;

		switch (mode) {
			case ACTION_ENABLE:
				action_string = "enable";
				break;
			case ACTION_DISABLE:
				action_string = "disable";
				break;
			case ACTION_DESTROY:
				action_string = "destroy";
				break;
			case ACTION_CHECK:
				action_string = "check";
				break;
			default:
				action_string = "<>";
				break;
		}
		error("specify a namespace to %s, or \"all\"\n", action_string);
		rc = -EINVAL;
	}
	for (i = mode == ACTION_CREATE ? 0 : 1; i < argc; i++) {
		error("unknown extra parameter \"%s\"\n", argv[i]);
		rc = -EINVAL;
	}

	if (rc) {
		usage_with_options(u, options);
		return NULL; /* we won't return from usage_with_options() */
	}

	return mode == ACTION_CREATE ? param.reconfig : argv[0];
}

#define try(prefix, op, dev, p) \
do { \
	int __rc = prefix##_##op(dev, p); \
	if (__rc) { \
		debug("%s: " #op " failed: %d\n", \
				prefix##_get_devname(dev), __rc); \
		return __rc; \
	} \
} while (0)

static bool do_setup_pfn(struct ndctl_namespace *ndns,
		struct parsed_parameters *p)
{
	if (p->mode != NDCTL_NS_MODE_MEMORY)
		return false;

	/*
	 * Dynamically allocated namespaces always require a pfn
	 * instance, and a pfn device is required to place the memmap
	 * array in device memory.
	 */
	if (!ndns || ndctl_namespace_get_mode(ndns) != NDCTL_NS_MODE_MEMORY
			|| p->loc == NDCTL_PFN_LOC_PMEM)
		return true;

	return false;
}

static int setup_namespace(struct ndctl_region *region,
		struct ndctl_namespace *ndns, struct parsed_parameters *p)
{
	uuid_t uuid;
	int rc;

	if (ndctl_namespace_get_type(ndns) != ND_DEVICE_NAMESPACE_IO) {
		try(ndctl_namespace, set_uuid, ndns, p->uuid);
		try(ndctl_namespace, set_alt_name, ndns, p->name);
		try(ndctl_namespace, set_size, ndns, p->size);
	}

	if (p->sector_size && p->sector_size < UINT_MAX) {
		int i, num = ndctl_namespace_get_num_sector_sizes(ndns);

		/*
		 * With autolabel support we need to recheck if the
		 * namespace gained sector_size support late in
		 * namespace_reconfig().
		 */
		for (i = 0; i < num; i++)
			if (ndctl_namespace_get_supported_sector_size(ndns, i)
					== p->sector_size)
				break;
		if (i < num)
			try(ndctl_namespace, set_sector_size, ndns,
					p->sector_size);
		else if (p->mode == NDCTL_NS_MODE_SAFE)
			/* pass, the btt sector_size will override */;
		else if (p->sector_size != 512) {
			error("%s: sector_size: %ld not supported\n",
					ndctl_namespace_get_devname(ndns),
					p->sector_size);
			return -EINVAL;
		}
	}

	uuid_generate(uuid);

	/*
	 * Note, this call to ndctl_namespace_set_mode() is not error
	 * checked since kernels older than 4.13 do not support this
	 * property of namespaces and it is an opportunistic enforcement
	 * mechanism.
	 */
	ndctl_namespace_set_enforce_mode(ndns, p->mode);

	if (do_setup_pfn(ndns, p)) {
		struct ndctl_pfn *pfn = ndctl_region_get_pfn_seed(region);

		try(ndctl_pfn, set_uuid, pfn, uuid);
		try(ndctl_pfn, set_location, pfn, p->loc);
		if (ndctl_pfn_has_align(pfn))
			try(ndctl_pfn, set_align, pfn, p->align);
		try(ndctl_pfn, set_namespace, pfn, ndns);
		rc = ndctl_pfn_enable(pfn);
	} else if (p->mode == NDCTL_NS_MODE_DAX) {
		struct ndctl_dax *dax = ndctl_region_get_dax_seed(region);

		try(ndctl_dax, set_uuid, dax, uuid);
		try(ndctl_dax, set_location, dax, p->loc);
		/* device-dax assumes 'align' attribute present */
		try(ndctl_dax, set_align, dax, p->align);
		try(ndctl_dax, set_namespace, dax, ndns);
		rc = ndctl_dax_enable(dax);
	} else if (p->mode == NDCTL_NS_MODE_SAFE) {
		struct ndctl_btt *btt = ndctl_region_get_btt_seed(region);

		/*
		 * Handle the case of btt on a pmem namespace where the
		 * pmem kernel support is pre-v1.2 namespace labels
		 * support (does not support sector size settings).
		 */
		if (p->sector_size == UINT_MAX)
			p->sector_size = 4096;
		try(ndctl_btt, set_uuid, btt, uuid);
		try(ndctl_btt, set_sector_size, btt, p->sector_size);
		try(ndctl_btt, set_namespace, btt, ndns);
		rc = ndctl_btt_enable(btt);
	} else
		rc = ndctl_namespace_enable(ndns);

	if (rc) {
		error("%s: failed to enable\n",
				ndctl_namespace_get_devname(ndns));
	} else {
		unsigned long flags = UTIL_JSON_DAX | UTIL_JSON_DAX_DEVS;
		struct json_object *jndns;

		if (isatty(1))
			flags |= UTIL_JSON_HUMAN;
		jndns = util_namespace_to_json(ndns, flags);
		if (jndns)
			printf("%s\n", json_object_to_json_string_ext(jndns,
						JSON_C_TO_STRING_PRETTY));
	}
	return rc;
}

static int is_namespace_active(struct ndctl_namespace *ndns)
{
	return ndns && (ndctl_namespace_is_enabled(ndns)
		|| ndctl_namespace_get_pfn(ndns)
		|| ndctl_namespace_get_dax(ndns)
		|| ndctl_namespace_get_btt(ndns));
}

/*
 * validate_namespace_options - init parameters for setup_namespace
 * @region: parent of the namespace to create / reconfigure
 * @ndns: specified when we are reconfiguring, NULL otherwise
 * @p: parameters to fill
 *
 * parse_namespace_options() will have already done basic verification
 * of the parameters and set defaults in the !reconfigure case.  When
 * reconfiguring fill in any unset options with defaults from the
 * namespace itself.
 *
 * Given that parse_namespace_options() runs before we have identified
 * the target namespace we need to do basic sanity checks here for
 * pmem-only attributes specified for blk namespace and vice versa.
 */
static int validate_namespace_options(struct ndctl_region *region,
		struct ndctl_namespace *ndns, struct parsed_parameters *p)
{
	const char *region_name = ndctl_region_get_devname(region);
	unsigned long long size_align = SZ_4K, units = 1;
	unsigned int ways;
	int rc = 0;

	memset(p, 0, sizeof(*p));

	if (!ndctl_region_is_enabled(region)) {
		debug("%s: disabled, skipping...\n", region_name);
		return -EAGAIN;
	}

	if (param.size)
		p->size = __parse_size64(param.size, &units);
	else if (ndns)
		p->size = ndctl_namespace_get_size(ndns);

	if (param.uuid) {
		if (uuid_parse(param.uuid, p->uuid) != 0) {
			debug("%s: invalid uuid\n", __func__);
			return -EINVAL;
		}
	} else
		uuid_generate(p->uuid);

	if (param.name)
		rc = snprintf(p->name, sizeof(p->name), "%s", param.name);
	else if (ndns)
		rc = snprintf(p->name, sizeof(p->name), "%s",
				ndctl_namespace_get_alt_name(ndns));
	if (rc >= (int) sizeof(p->name)) {
		debug("%s: alt name overflow\n", __func__);
		return -EINVAL;
	}

	if (param.mode) {
		if (strcmp(param.mode, "memory") == 0)
			p->mode = NDCTL_NS_MODE_MEMORY;
		else if (strcmp(param.mode, "sector") == 0)
			p->mode = NDCTL_NS_MODE_SAFE;
		else if (strcmp(param.mode, "safe") == 0)
			p->mode = NDCTL_NS_MODE_SAFE;
		else if (strcmp(param.mode, "dax") == 0)
			p->mode = NDCTL_NS_MODE_DAX;
		else
			p->mode = NDCTL_NS_MODE_RAW;

		if (ndctl_region_get_type(region) != ND_DEVICE_REGION_PMEM
				&& (p->mode == NDCTL_NS_MODE_MEMORY
					|| p->mode == NDCTL_NS_MODE_DAX)) {
			debug("blk %s does not support %s mode\n", region_name,
					p->mode == NDCTL_NS_MODE_MEMORY
					? "memory" : "dax");
			return -EAGAIN;
		}
	} else if (ndns)
		p->mode = ndctl_namespace_get_mode(ndns);

	if (param.align) {
		struct ndctl_pfn *pfn = ndctl_region_get_pfn_seed(region);
		struct ndctl_dax *dax = ndctl_region_get_dax_seed(region);

		p->align = parse_size64(param.align);

		if (p->mode == NDCTL_NS_MODE_MEMORY && p->align != SZ_2M
				&& (!pfn || !ndctl_pfn_has_align(pfn))) {
			/*
			 * Initial pfn device support in the kernel
			 * supported a 2M default alignment when
			 * ndctl_pfn_has_align() returns false.
			 */
			debug("%s not support 'align' for memory mode\n",
					region_name);
			return -EAGAIN;
		} else if (p->mode == NDCTL_NS_MODE_DAX
				&& (!dax || !ndctl_dax_has_align(dax))) {
			/*
			 * Unlike the pfn case, we require the kernel to
			 * have 'align' support for device-dax.
			 */
			debug("%s not support 'align' for dax mode\n",
					region_name);
			return -EAGAIN;
		} else if (!param.align_default
				&& (p->mode == NDCTL_NS_MODE_SAFE
					|| p->mode == NDCTL_NS_MODE_RAW)) {
			/*
			 * Specifying an alignment has no effect for
			 * raw, or btt mode namespaces.
			 */
			error("%s mode does not support setting an alignment\n",
					p->mode == NDCTL_NS_MODE_SAFE
					? "sector" : "raw");
			return -ENXIO;
		}

		/*
		 * Fallback to a 4K default alignment if the region is
		 * not 2MB (typical default) aligned. This mainly helps
		 * the nfit_test use case where it is backed by vmalloc
		 * memory.
		 */
		if (param.align_default && (ndctl_region_get_resource(region)
					& (SZ_2M - 1))) {
			debug("%s: falling back to a 4K alignment\n",
					region_name);
			p->align = SZ_4K;
		}

		switch (p->align) {
		case SZ_4K:
		case SZ_2M:
		case SZ_1G:
			break;
		default:
			error("unsupported align: %s\n", param.align);
			return -ENXIO;
		}

		/*
		 * 'raw' and 'sector' mode namespaces don't support an
		 * alignment attribute.
		 */
		if (p->mode == NDCTL_NS_MODE_MEMORY
				|| p->mode == NDCTL_NS_MODE_DAX)
			size_align = p->align;
	}

	/* (re-)validate that the size satisfies the alignment */
	ways = ndctl_region_get_interleave_ways(region);
	if (p->size % (size_align * ways)) {
		char *suffix = "";

		if (units == SZ_1K)
			suffix = "K";
		else if (units == SZ_1M)
			suffix = "M";
		else if (units == SZ_1G)
			suffix = "G";
		else if (units == SZ_1T)
			suffix = "T";

		/*
		 * Make the recommendation in the units of the '--size'
		 * option
		 */
		size_align = max(units, size_align) * ways;

		p->size /= size_align;
		p->size++;
		p->size *= size_align;
		p->size /= units;
		error("'--size=' must align to interleave-width: %d and alignment: %ld\n"
				"  did you intend --size=%lld%s?\n", ways, param.align
				? p->align : SZ_4K, p->size, suffix);
		return -EINVAL;
	}

	if (param.sector_size) {
		struct ndctl_btt *btt;
		int num, i;

		p->sector_size = parse_size64(param.sector_size);
		btt = ndctl_region_get_btt_seed(region);
		if (p->mode == NDCTL_NS_MODE_SAFE) {
			if (!btt) {
				debug("%s: does not support 'sector' mode\n",
						region_name);
				return -EINVAL;
			}
			num = ndctl_btt_get_num_sector_sizes(btt);
			for (i = 0; i < num; i++)
				if (ndctl_btt_get_supported_sector_size(btt, i)
						== p->sector_size)
					break;
			if (i >= num) {
				debug("%s: does not support btt sector_size %lu\n",
						region_name, p->sector_size);
				return -EINVAL;
			}
		} else {
			struct ndctl_namespace *seed = ndns;

			if (!seed)
				seed = ndctl_region_get_namespace_seed(region);
			num = ndctl_namespace_get_num_sector_sizes(seed);
			for (i = 0; i < num; i++)
				if (ndctl_namespace_get_supported_sector_size(seed, i)
						== p->sector_size)
					break;
			if (i >= num) {
				debug("%s: does not support namespace sector_size %lu\n",
						region_name, p->sector_size);
				return -EINVAL;
			}
		}
	} else if (ndns) {
		struct ndctl_btt *btt = ndctl_namespace_get_btt(ndns);

		/*
		 * If the target mode is still 'safe' carry forward the
		 * sector size, otherwise fall back to what the
		 * namespace supports.
		 */
		if (btt && p->mode == NDCTL_NS_MODE_SAFE)
			p->sector_size = ndctl_btt_get_sector_size(btt);
		else
			p->sector_size = ndctl_namespace_get_sector_size(ndns);
	} else {
		struct ndctl_namespace *seed;

		seed = ndctl_region_get_namespace_seed(region);
		if (ndctl_namespace_get_type(seed) == ND_DEVICE_NAMESPACE_BLK)
			debug("%s: set_defaults() should preclude this?\n",
				ndctl_region_get_devname(region));
		/*
		 * Pick a default sector size for a pmem namespace based
		 * on what the kernel supports.
		 */
		if (ndctl_namespace_get_num_sector_sizes(seed) == 0)
			p->sector_size = UINT_MAX;
		else
			p->sector_size = 512;
	}

	if (param.map) {
		if (!strcmp(param.map, "mem"))
			p->loc = NDCTL_PFN_LOC_RAM;
		else
			p->loc = NDCTL_PFN_LOC_PMEM;

		if (ndns && p->mode != NDCTL_NS_MODE_MEMORY
			&& p->mode != NDCTL_NS_MODE_DAX) {
			debug("%s: --map= only valid for memory mode namespace\n",
				ndctl_namespace_get_devname(ndns));
			return -EINVAL;
		}
	} else if (p->mode == NDCTL_NS_MODE_MEMORY
			|| p->mode == NDCTL_NS_MODE_DAX)
		p->loc = NDCTL_PFN_LOC_PMEM;

	/* check if we need, and whether the kernel supports, pfn devices */
	if (do_setup_pfn(ndns, p)) {
		struct ndctl_pfn *pfn = ndctl_region_get_pfn_seed(region);

		if (!pfn && param.mode_default) {
			debug("%s memory mode not available\n", region_name);
			p->mode = NDCTL_NS_MODE_RAW;
		} else if (!pfn) {
			error("operation failed, %s memory mode not available\n",
					region_name);
			return -EINVAL;
		}
	}

	/* check if we need, and whether the kernel supports, dax devices */
	if (p->mode == NDCTL_NS_MODE_DAX) {
		struct ndctl_dax *dax = ndctl_region_get_dax_seed(region);

		if (!dax) {
			error("operation failed, %s dax mode not available\n",
					region_name);
			return -EINVAL;
		}
	}

	p->autolabel = param.autolabel;

	return 0;
}

static struct ndctl_namespace *region_get_namespace(struct ndctl_region *region)
{
	struct ndctl_namespace *ndns;

	/* prefer the 0th namespace if it is idle */
	ndctl_namespace_foreach(region, ndns)
		if (ndctl_namespace_get_id(ndns) == 0
				&& !is_namespace_active(ndns))
			return ndns;
	return ndctl_region_get_namespace_seed(region);
}

static int namespace_create(struct ndctl_region *region)
{
	const char *devname = ndctl_region_get_devname(region);
	unsigned long long available;
	struct ndctl_namespace *ndns;
	struct parsed_parameters p;
	int rc;

	rc = validate_namespace_options(region, NULL, &p);
	if (rc)
		return rc;

	if (ndctl_region_get_ro(region)) {
		debug("%s: read-only, inelligible for namespace creation\n",
			devname);
		return -EAGAIN;
	}

	available = ndctl_region_get_available_size(region);
	if (!available || p.size > available) {
		debug("%s: insufficient capacity size: %llx avail: %llx\n",
			devname, p.size, available);
		return -EAGAIN;
	}

	if (p.size == 0)
		p.size = available;

	ndns = region_get_namespace(region);
	if (!ndns || is_namespace_active(ndns)) {
		debug("%s: no %s namespace seed\n", devname,
				ndns ? "idle" : "available");
		return -ENODEV;
	}

	return setup_namespace(region, ndns, &p);
}

static int zero_info_block(struct ndctl_namespace *ndns)
{
	const char *devname = ndctl_namespace_get_devname(ndns);
	int fd, rc = -ENXIO;
	void *buf = NULL;
	char path[50];

	ndctl_namespace_set_raw_mode(ndns, 1);
	rc = ndctl_namespace_enable(ndns);
	if (rc < 0) {
		debug("%s failed to enable for zeroing, continuing\n", devname);
		rc = 0;
		goto out;
	}

	if (posix_memalign(&buf, 4096, 4096) != 0)
		return -ENXIO;

	sprintf(path, "/dev/%s", ndctl_namespace_get_block_device(ndns));
	fd = open(path, O_RDWR|O_DIRECT|O_EXCL);
	if (fd < 0) {
		debug("%s: failed to open %s to zero info block\n",
				devname, path);
		goto out;
	}

	memset(buf, 0, 4096);
	rc = pwrite(fd, buf, 4096, 4096);
	if (rc < 4096) {
		debug("%s: failed to zero info block %s\n",
				devname, path);
		rc = -ENXIO;
	} else
		rc = 0;
	close(fd);
 out:
	ndctl_namespace_set_raw_mode(ndns, 0);
	ndctl_namespace_disable_invalidate(ndns);
	free(buf);
	return rc;
}

static int namespace_destroy(struct ndctl_region *region,
		struct ndctl_namespace *ndns)
{
	const char *devname = ndctl_namespace_get_devname(ndns);
	struct ndctl_pfn *pfn = ndctl_namespace_get_pfn(ndns);
	struct ndctl_dax *dax = ndctl_namespace_get_dax(ndns);
	struct ndctl_btt *btt = ndctl_namespace_get_btt(ndns);
	int rc;

	if (ndctl_region_get_ro(region)) {
		error("%s: read-only, re-configuration disabled\n",
				devname);
		return -ENXIO;
	}

	if (ndctl_namespace_is_active(ndns) && !force) {
		error("%s is active, specify --force for re-configuration\n",
				devname);
		return -EBUSY;
	} else {
		rc = ndctl_namespace_disable_safe(ndns);
		if (rc)
			return rc;
	}

	ndctl_namespace_set_enforce_mode(ndns, NDCTL_NS_MODE_RAW);

	if (pfn || btt || dax) {
		rc = zero_info_block(ndns);
		if (rc)
			return rc;
	}

	rc = ndctl_namespace_delete(ndns);
	if (rc)
		debug("%s: failed to reclaim\n", devname);

	return 0;
}

static int enable_labels(struct ndctl_region *region)
{
	int mappings = ndctl_region_get_mappings(region);
	struct ndctl_cmd *cmd_read = NULL;
	enum ndctl_namespace_version v;
	struct ndctl_dimm *dimm;
	int count;

	/* no dimms => no labels */
	if (!mappings)
		return 0;

	count = 0;
	ndctl_dimm_foreach_in_region(region, dimm) {
		if (!ndctl_dimm_is_cmd_supported(dimm, ND_CMD_GET_CONFIG_SIZE))
			break;
		if (!ndctl_dimm_is_cmd_supported(dimm, ND_CMD_GET_CONFIG_DATA))
			break;
		if (!ndctl_dimm_is_cmd_supported(dimm, ND_CMD_SET_CONFIG_DATA))
			break;
		count++;
	}

	/* all the dimms must support labeling */
	if (count != mappings)
		return 0;

	ndctl_region_disable_invalidate(region);
	count = 0;
	ndctl_dimm_foreach_in_region(region, dimm)
		if (ndctl_dimm_is_active(dimm)) {
			warning("%s is active in %s, failing autolabel\n",
					ndctl_dimm_get_devname(dimm),
					ndctl_region_get_devname(region));
			count++;
		}

	/* some of the dimms belong to multiple regions?? */
	if (count)
		goto out;

	v = NDCTL_NS_VERSION_1_2;
retry:
	ndctl_dimm_foreach_in_region(region, dimm) {
		int num_labels, avail;

		ndctl_cmd_unref(cmd_read);
		cmd_read = ndctl_dimm_read_labels(dimm);
		if (!cmd_read)
			continue;

		num_labels = ndctl_dimm_init_labels(dimm, v);
		if (num_labels < 0)
			continue;

		ndctl_dimm_disable(dimm);
		ndctl_dimm_enable(dimm);

		/*
		 * If the kernel appears to not understand v1.2 labels,
		 * try v1.1. Note, we increment avail by 1 to account
		 * for the one free label that the kernel always
		 * maintains for ongoing updates.
		 */
		avail = ndctl_dimm_get_available_labels(dimm) + 1;
		if (num_labels != avail && v == NDCTL_NS_VERSION_1_2) {
			v = NDCTL_NS_VERSION_1_1;
			goto retry;
		}

	}
	ndctl_cmd_unref(cmd_read);
out:
	ndctl_region_enable(region);
	if (ndctl_region_get_nstype(region) != ND_DEVICE_NAMESPACE_PMEM) {
		debug("%s: failed to initialize labels\n",
				ndctl_region_get_devname(region));
		return -EBUSY;
	}

	return 0;
}

static int namespace_reconfig(struct ndctl_region *region,
		struct ndctl_namespace *ndns)
{
	struct parsed_parameters p;
	int rc;

	rc = validate_namespace_options(region, ndns, &p);
	if (rc)
		return rc;

	rc = namespace_destroy(region, ndns);
	if (rc)
		return rc;

	/* check if we can enable labels on this region */
	if (ndctl_region_get_nstype(region) == ND_DEVICE_NAMESPACE_IO
			&& p.autolabel) {
		/* if this fails, try to continue label-less */
		enable_labels(region);
	}

	ndns = region_get_namespace(region);
	if (!ndns || is_namespace_active(ndns)) {
		debug("%s: no %s namespace seed\n",
				ndctl_region_get_devname(region),
				ndns ? "idle" : "available");
		return -ENODEV;
	}

	return setup_namespace(region, ndns, &p);
}

int namespace_check(struct ndctl_namespace *ndns, bool verbose, bool force,
		bool repair);

static int do_xaction_namespace(const char *namespace,
		enum device_action action, struct ndctl_ctx *ctx)
{
	struct ndctl_namespace *ndns, *_n;
	int rc = -ENXIO, success = 0;
	struct ndctl_region *region;
	const char *ndns_name;
	struct ndctl_bus *bus;

	if (!namespace && action != ACTION_CREATE)
		return rc;

	if (verbose)
		ndctl_set_log_priority(ctx, LOG_DEBUG);

        ndctl_bus_foreach(ctx, bus) {
		if (!util_bus_filter(bus, param.bus))
			continue;

		ndctl_region_foreach(bus, region) {
			if (!util_region_filter(region, param.region))
				continue;

			if (param.type) {
				if (strcmp(param.type, "pmem") == 0
						&& ndctl_region_get_type(region)
						== ND_DEVICE_REGION_PMEM)
					/* pass */;
				else if (strcmp(param.type, "blk") == 0
						&& ndctl_region_get_type(region)
						== ND_DEVICE_REGION_BLK)
					/* pass */;
				else
					continue;
			}

			if (action == ACTION_CREATE && !namespace) {
				rc = namespace_create(region);
				if (rc == -EAGAIN)
					continue;
				if (rc == 0)
					rc = 1;
				return rc;
			}
			ndctl_namespace_foreach_safe(region, ndns, _n) {
				ndns_name = ndctl_namespace_get_devname(ndns);

				if (strcmp(namespace, "all") != 0
						&& strcmp(namespace, ndns_name) != 0)
					continue;
				switch (action) {
				case ACTION_DISABLE:
					rc = ndctl_namespace_disable_safe(ndns);
					break;
				case ACTION_ENABLE:
					rc = ndctl_namespace_enable(ndns);
					break;
				case ACTION_DESTROY:
					rc = namespace_destroy(region, ndns);
					break;
				case ACTION_CHECK:
					rc = namespace_check(ndns, verbose,
							force, repair);
					if (rc < 0)
						return rc;
					break;
				case ACTION_CREATE:
					rc = namespace_reconfig(region, ndns);
					if (rc < 0)
						return rc;
					return 1;
				}
				if (rc >= 0)
					success++;
			}
		}
	}

	if (success)
		return success;
	return rc;
}

int cmd_disable_namespace(int argc, const char **argv, void *ctx)
{
	char *xable_usage = "ndctl disable-namespace <namespace> [<options>]";
	const char *namespace = parse_namespace_options(argc, argv,
			ACTION_DISABLE, base_options, xable_usage);
	int disabled = do_xaction_namespace(namespace, ACTION_DISABLE, ctx);

	if (disabled < 0) {
		fprintf(stderr, "error disabling namespaces: %s\n",
				strerror(-disabled));
		return disabled;
	} else if (disabled == 0) {
		fprintf(stderr, "disabled 0 namespaces\n");
		return -ENXIO;
	} else {
		fprintf(stderr, "disabled %d namespace%s\n", disabled,
				disabled > 1 ? "s" : "");
		return 0;
	}
}

int cmd_enable_namespace(int argc, const char **argv, void *ctx)
{
	char *xable_usage = "ndctl enable-namespace <namespace> [<options>]";
	const char *namespace = parse_namespace_options(argc, argv,
			ACTION_ENABLE, base_options, xable_usage);
	int enabled = do_xaction_namespace(namespace, ACTION_ENABLE, ctx);

	if (enabled < 0) {
		fprintf(stderr, "error enabling namespaces: %s\n",
				strerror(-enabled));
		return enabled;
	} else if (enabled == 0) {
		fprintf(stderr, "enabled 0 namespaces\n");
		return 0;
	} else {
		fprintf(stderr, "enabled %d namespace%s\n", enabled,
				enabled > 1 ? "s" : "");
		return 0;
	}
}

int cmd_create_namespace(int argc, const char **argv, void *ctx)
{
	char *xable_usage = "ndctl create-namespace [<options>]";
	const char *namespace = parse_namespace_options(argc, argv,
			ACTION_CREATE, create_options, xable_usage);
	int created = do_xaction_namespace(namespace, ACTION_CREATE, ctx);

	if (created < 1 && param.do_scan) {
		/*
		 * In the default scan case we try pmem first and then
		 * fallback to blk before giving up.
		 */
		memset(&param, 0, sizeof(param));
		param.type = "blk";
		set_defaults(ACTION_CREATE);
		created = do_xaction_namespace(NULL, ACTION_CREATE, ctx);
	}

	if (created < 0 || (!namespace && created < 1)) {
		fprintf(stderr, "failed to %s namespace: %s\n", namespace
				? "reconfigure" : "create", strerror(-created));
		if (!namespace)
			created = -ENODEV;
	}

	if (created < 0)
		return created;
	return 0;
}

int cmd_destroy_namespace(int argc , const char **argv, void *ctx)
{
	char *xable_usage = "ndctl destroy-namespace <namespace> [<options>]";
	const char *namespace = parse_namespace_options(argc, argv,
			ACTION_DESTROY, destroy_options, xable_usage);
	int destroyed = do_xaction_namespace(namespace, ACTION_DESTROY, ctx);

	if (destroyed < 0) {
		fprintf(stderr, "error destroying namespaces: %s\n",
				strerror(-destroyed));
		return destroyed;
	} else if (destroyed == 0) {
		fprintf(stderr, "destroyed 0 namespaces\n");
		return 0;
	} else {
		fprintf(stderr, "destroyed %d namespace%s\n", destroyed,
				destroyed > 1 ? "s" : "");
		return 0;
	}
}

int cmd_check_namespace(int argc , const char **argv, void *ctx)
{
	char *xable_usage = "ndctl check-namespace <namespace> [<options>]";
	const char *namespace = parse_namespace_options(argc, argv,
			ACTION_CHECK, check_options, xable_usage);
	int checked;

	checked = do_xaction_namespace(namespace, ACTION_CHECK, ctx);
	if (checked < 0) {
		fprintf(stderr, "error checking namespaces: %s\n",
				strerror(-checked));
		return checked;
	} else if (checked == 0) {
		fprintf(stderr, "checked 0 namespaces\n");
		return 0;
	} else {
		fprintf(stderr, "checked %d namespace%s\n", checked,
				checked > 1 ? "s" : "");
		return 0;
	}
}
