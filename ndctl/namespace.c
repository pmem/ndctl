// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2015-2020 Intel Corporation. All rights reserved.
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <syslog.h>

#include <ndctl.h>
#include "action.h"
#include "namespace.h"
#include <sys/stat.h>
#include <linux/fs.h>
#include <uuid/uuid.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <util/size.h>
#include <util/json.h>
#include <json-c/json.h>
#include <util/filter.h>
#include <ndctl/libndctl.h>
#include <util/parse-options.h>
#include <ccan/minmax/minmax.h>

static bool verbose;
static bool force;
static bool repair;
static bool logfix;
static bool scrub;
static struct parameters {
	bool do_scan;
	bool mode_default;
	bool autolabel;
	bool greedy;
	bool verify;
	bool autorecover;
	bool human;
	bool json;
	bool std_out;
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
	const char *offset;
	const char *outfile;
	const char *infile;
	const char *parent_uuid;
} param = {
	.autolabel = true,
	.autorecover = true,
};

const char *cmd_name = "namespace";

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
	bool autorecover;
};

#define pr_verbose(fmt, ...) \
	({if (verbose) { \
		fprintf(stderr, fmt, ##__VA_ARGS__); \
	} else { \
		do { } while (0); \
	}})

#define debug(fmt, ...) \
	({if (verbose) { \
		fprintf(stderr, "%s:%d: " fmt, __func__, __LINE__, ##__VA_ARGS__); \
	} else { \
		do { } while (0); \
	}})

static int err_count;
#define err(fmt, ...) \
	({ err_count++; error("%s: " fmt, cmd_name, ##__VA_ARGS__); })

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
	"specify a mode for the namespace, 'sector', 'fsdax', 'devdax' or 'raw'"), \
OPT_STRING('M', "map", &param.map, "memmap-location", \
	"specify 'mem' or 'dev' for the location of the memmap"), \
OPT_STRING('l', "sector-size", &param.sector_size, "lba-size", \
	"specify the logical sector size in bytes"), \
OPT_STRING('t', "type", &param.type, "type", \
	"specify the type of namespace to create 'pmem' or 'blk'"), \
OPT_STRING('a', "align", &param.align, "align", \
	"specify the namespace alignment in bytes (default: 2M)"), \
OPT_BOOLEAN('f', "force", &force, "reconfigure namespace even if currently active"), \
OPT_BOOLEAN('L', "autolabel", &param.autolabel, "automatically initialize labels"), \
OPT_BOOLEAN('c', "continue", &param.greedy, \
	"continue creating namespaces as long as the filter criteria are met"), \
OPT_BOOLEAN('R', "autorecover", &param.autorecover, "automatically cleanup on failure")

#define CHECK_OPTIONS() \
OPT_BOOLEAN('R', "repair", &repair, "perform metadata repairs"), \
OPT_BOOLEAN('L', "rewrite-log", &logfix, "regenerate the log"), \
OPT_BOOLEAN('f', "force", &force, "check namespace even if currently active")

#define CLEAR_OPTIONS() \
OPT_BOOLEAN('s', "scrub", &scrub, "run a scrub to find latent errors")

#define READ_INFOBLOCK_OPTIONS() \
OPT_FILENAME('o', "output", &param.outfile, "output-file", \
	"filename to write infoblock contents"), \
OPT_FILENAME('i', "input", &param.infile, "input-file", \
	"filename to read infoblock instead of a namespace"), \
OPT_BOOLEAN('V', "verify", &param.verify, \
	"validate parent uuid, and infoblock checksum"), \
OPT_BOOLEAN('j', "json", &param.json, "parse label data into json"), \
OPT_BOOLEAN('u', "human", &param.human, "use human friendly number formats (implies --json)")

#define WRITE_INFOBLOCK_OPTIONS() \
OPT_FILENAME('o', "output", &param.outfile, "output-file", \
	"filename to write infoblock contents"), \
OPT_BOOLEAN('c', "stdout", &param.std_out, \
	"write the infoblock data to stdout"), \
OPT_STRING('m', "mode", &param.mode, "operation-mode", \
	"specify the infoblock mode, 'fsdax' or 'devdax' (default 'fsdax')"), \
OPT_STRING('s', "size", &param.size, "size", \
	"override the image size to instantiate the infoblock"), \
OPT_STRING('a', "align", &param.align, "align", \
	"specify the expected physical alignment"), \
OPT_STRING('u', "uuid", &param.uuid, "uuid", \
	"specify the uuid for the infoblock (default: autogenerate)"), \
OPT_STRING('M', "map", &param.map, "memmap-location", \
	"specify 'mem' or 'dev' for the location of the memmap"), \
OPT_STRING('p', "parent-uuid", &param.parent_uuid, "parent-uuid", \
	"specify the parent namespace uuid for the infoblock (default: 0)"), \
OPT_STRING('O', "offset", &param.offset, "offset", \
	"EXPERT/DEBUG only: enable namespace inner alignment padding")

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

static const struct option clear_options[] = {
	BASE_OPTIONS(),
	CLEAR_OPTIONS(),
	OPT_END(),
};

static const struct option read_infoblock_options[] = {
	BASE_OPTIONS(),
	READ_INFOBLOCK_OPTIONS(),
	OPT_END(),
};

static const struct option write_infoblock_options[] = {
	BASE_OPTIONS(),
	WRITE_INFOBLOCK_OPTIONS(),
	OPT_END(),
};

static int set_defaults(enum device_action action)
{
	uuid_t uuid;
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
	} else if (!param.reconfig && action == ACTION_CREATE)
		param.type = "pmem";

	if (param.mode) {
		enum ndctl_namespace_mode mode = util_nsmode(param.mode);

		switch (mode) {
		case NDCTL_NS_MODE_UNKNOWN:
			error("invalid mode '%s'\n", param.mode);
			rc = -EINVAL;
			break;
		case NDCTL_NS_MODE_FSDAX:
		case NDCTL_NS_MODE_DEVDAX:
			break;
		default:
			if (action == ACTION_WRITE_INFOBLOCK) {
				error("unsupported mode '%s'\n", param.mode);
				rc = -EINVAL;
			}
			break;
		}
	} else if (action == ACTION_WRITE_INFOBLOCK) {
		param.mode = "fsdax";
	} else if (!param.reconfig && param.type) {
		if (strcmp(param.type, "pmem") == 0)
			param.mode = "fsdax";
		else
			param.mode = "sector";
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
				&& strcmp(param.mode, "fsdax") != 0
				&& strcmp(param.mode, "devdax") != 0) {
			error("--map only valid for an devdax mode pmem namespace\n");
			rc = -EINVAL;
		}
	} else if (!param.reconfig)
		param.map = "dev";

	/* check for incompatible mode and type combinations */
	if (param.type && param.mode && strcmp(param.type, "blk") == 0
			&& (strcmp(param.mode, "fsdax") == 0
				|| strcmp(param.mode, "devdax") == 0)) {
		error("only 'pmem' namespaces support dax operation\n");
		rc = -ENXIO;
	}

	if (param.size && parse_size64(param.size) == ULLONG_MAX) {
		error("failed to parse namespace size '%s'\n",
				param.size);
		rc = -EINVAL;
	}

	if (param.offset && parse_size64(param.offset) == ULLONG_MAX) {
		error("failed to parse physical offset'%s'\n",
				param.offset);
		rc = -EINVAL;
	}

	if (param.align) {
		unsigned long long align = parse_size64(param.align);

		if (align == ULLONG_MAX) {
			error("failed to parse namespace alignment '%s'\n",
					param.align);
			rc = -EINVAL;
		} else if (!is_power_of_2(align)
			|| align < (unsigned long long) sysconf(_SC_PAGE_SIZE)) {
			error("align must be a power-of-2 greater than %ld\n",
					sysconf(_SC_PAGE_SIZE));
			rc = -EINVAL;
		}
	}

	if (param.size) {
		unsigned long long size = parse_size64(param.size);

		if (size == ULLONG_MAX) {
			error("failed to parse namespace size '%s'\n",
					param.size);
			rc = -EINVAL;
		}
	}

	if (param.uuid) {
		if (uuid_parse(param.uuid, uuid)) {
			error("failed to parse uuid: '%s'\n", param.uuid);
			rc = -EINVAL;
		}
	}

	if (param.parent_uuid) {
		if (uuid_parse(param.parent_uuid, uuid)) {
			error("failed to parse uuid: '%s'\n", param.parent_uuid);
			rc = -EINVAL;
		}
	}

	if (param.sector_size) {
		if (parse_size64(param.sector_size) == ULLONG_MAX) {
			error("invalid sector size: %s\n", param.sector_size);
			rc = -EINVAL;
		}
	} else if (((param.type && strcmp(param.type, "blk") == 0)
			|| util_nsmode(param.mode) == NDCTL_NS_MODE_SECTOR)) {
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
		enum device_action action, const struct option *options,
		char *xable_usage)
{
	const char * const u[] = {
		xable_usage,
		NULL
	};
	int i, rc = 0;

	param.do_scan = argc == 1;
        argc = parse_options(argc, argv, options, u, 0);

	rc = set_defaults(action);

	if (argc == 0 && action != ACTION_CREATE) {
		char *action_string;

		switch (action) {
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
			case ACTION_CLEAR:
				action_string = "clear errors for";
				break;
			case ACTION_READ_INFOBLOCK:
				action_string = "read-infoblock";
				break;
			case ACTION_WRITE_INFOBLOCK:
				action_string = "write-infoblock";
				break;
			default:
				action_string = "<>";
				break;
		}

		if ((action != ACTION_READ_INFOBLOCK
					&& action != ACTION_WRITE_INFOBLOCK)
				|| (action == ACTION_WRITE_INFOBLOCK
					&& !param.outfile && !param.std_out)) {
			error("specify a namespace to %s, or \"all\"\n", action_string);
			rc = -EINVAL;
		}
	}
	for (i = action == ACTION_CREATE ? 0 : 1; i < argc; i++) {
		error("unknown extra parameter \"%s\"\n", argv[i]);
		rc = -EINVAL;
	}

	if (action == ACTION_READ_INFOBLOCK && param.infile && argc) {
		error("specify a namespace, or --input, not both\n");
		rc = -EINVAL;
	}

	if (action == ACTION_WRITE_INFOBLOCK && (param.outfile || param.std_out)
			&& argc) {
		error("specify only one of a namespace filter, --output, or --stdout\n");
		rc = -EINVAL;
	}

	if (action == ACTION_WRITE_INFOBLOCK && param.std_out && !param.size) {
		error("--size required with --stdout\n");
		rc = -EINVAL;
	}

	if (rc) {
		usage_with_options(u, options);
		return NULL; /* we won't return from usage_with_options() */
	}

	if (action == ACTION_READ_INFOBLOCK && !param.infile && !argc)
		return NULL;
	return action == ACTION_CREATE ? param.reconfig : argv[0];
}

#define try(prefix, op, dev, p) \
do { \
	int __rc = prefix##_##op(dev, p); \
	if (__rc) { \
		err("%s: " #op " failed: %s\n", \
				prefix##_get_devname(dev), \
				strerror(abs(__rc))); \
		return __rc; \
	} \
} while (0)

static bool do_setup_pfn(struct ndctl_namespace *ndns,
		struct parsed_parameters *p)
{
	if (p->mode != NDCTL_NS_MODE_FSDAX)
		return false;

	/*
	 * Dynamically allocated namespaces always require a pfn
	 * instance, and a pfn device is required to place the memmap
	 * array in device memory.
	 */
	if (!ndns || ndctl_namespace_get_mode(ndns) != NDCTL_NS_MODE_FSDAX
			|| p->loc == NDCTL_PFN_LOC_PMEM)
		return true;

	return false;
}

static int check_dax_align(struct ndctl_namespace *ndns)
{
	unsigned long long resource = ndctl_namespace_get_resource(ndns);
	const char *devname = ndctl_namespace_get_devname(ndns);

	if (resource == ULLONG_MAX) {
		warning("%s unable to validate alignment\n", devname);
		return 0;
	}

	if (IS_ALIGNED(resource, SZ_16M) || force)
		return 0;

	error("%s misaligned to 16M, adjust region alignment and retry\n",
			devname);
	return -EINVAL;
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
		else if (p->mode == NDCTL_NS_MODE_SECTOR)
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
		if (!pfn)
			return -ENXIO;

		rc = check_dax_align(ndns);
		if (rc)
			return rc;
		try(ndctl_pfn, set_uuid, pfn, uuid);
		try(ndctl_pfn, set_location, pfn, p->loc);
		if (ndctl_pfn_has_align(pfn))
			try(ndctl_pfn, set_align, pfn, p->align);
		try(ndctl_pfn, set_namespace, pfn, ndns);
		rc = ndctl_pfn_enable(pfn);
		if (rc && p->autorecover)
			ndctl_pfn_set_namespace(pfn, NULL);
	} else if (p->mode == NDCTL_NS_MODE_DEVDAX) {
		struct ndctl_dax *dax = ndctl_region_get_dax_seed(region);
		if (!dax)
			return -ENXIO;

		rc = check_dax_align(ndns);
		if (rc)
			return rc;
		try(ndctl_dax, set_uuid, dax, uuid);
		try(ndctl_dax, set_location, dax, p->loc);
		/* device-dax assumes 'align' attribute present */
		try(ndctl_dax, set_align, dax, p->align);
		try(ndctl_dax, set_namespace, dax, ndns);
		rc = ndctl_dax_enable(dax);
		if (rc && p->autorecover)
			ndctl_dax_set_namespace(dax, NULL);
	} else if (p->mode == NDCTL_NS_MODE_SECTOR) {
		struct ndctl_btt *btt = ndctl_region_get_btt_seed(region);
		if (!btt)
			return -ENXIO;

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

static int validate_available_capacity(struct ndctl_region *region,
		struct parsed_parameters *p)
{
	unsigned long long available;

	if (ndctl_region_get_nstype(region) == ND_DEVICE_NAMESPACE_IO)
		available = ndctl_region_get_size(region);
	else {
		available = ndctl_region_get_max_available_extent(region);
		if (available == ULLONG_MAX)
			available = ndctl_region_get_available_size(region);
	}
	if (!available || p->size > available) {
		debug("%s: insufficient capacity size: %llx avail: %llx\n",
			ndctl_region_get_devname(region), p->size, available);
		return -EAGAIN;
	}

	if (p->size == 0)
		p->size = available;
	return 0;
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
	unsigned long long size_align, units = 1, resource;
	struct ndctl_pfn *pfn = NULL;
	struct ndctl_dax *dax = NULL;
	unsigned long region_align;
	bool default_size = false;
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
	else
		default_size = true;

	/*
	 * Validate available capacity in the create case, in the
	 * reconfigure case the capacity is already allocated. A default
	 * size will be established from available capacity.
	 */
	if (!ndns) {
		rc = validate_available_capacity(region, p);
		if (rc)
			return rc;
	}

	/*
	 * Block attempts to set a custom size on legacy (label-less)
	 * namespaces
	 */
	if (ndctl_region_get_nstype(region) == ND_DEVICE_NAMESPACE_IO
			&& p->size != ndctl_region_get_size(region)) {
		error("Legacy / label-less namespaces do not support sub-dividing a region\n");
		error("Retry without -s/--size=\n");
		return -EINVAL;
	}

	if (param.uuid) {
		if (uuid_parse(param.uuid, p->uuid) != 0) {
			err("%s: invalid uuid\n", __func__);
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
		err("%s: alt name overflow\n", __func__);
		return -EINVAL;
	}

	if (param.mode) {
		p->mode = util_nsmode(param.mode);
		if (ndctl_region_get_type(region) != ND_DEVICE_REGION_PMEM
				&& (p->mode == NDCTL_NS_MODE_FSDAX
					|| p->mode == NDCTL_NS_MODE_DEVDAX)) {
			err("blk %s does not support %s mode\n", region_name,
					util_nsmode_name(p->mode));
			return -EAGAIN;
		}
	} else if (ndns)
		p->mode = ndctl_namespace_get_mode(ndns);

	if (p->mode == NDCTL_NS_MODE_FSDAX) {
		pfn = ndctl_region_get_pfn_seed(region);
		if (!pfn && param.mode_default) {
			err("%s fsdax mode not available\n", region_name);
			p->mode = NDCTL_NS_MODE_RAW;
		}
		/*
		 * NB: We only fail validation if a pfn-specific option is used
		 */
	} else if (p->mode == NDCTL_NS_MODE_DEVDAX) {
		dax = ndctl_region_get_dax_seed(region);
		if (!dax) {
			error("Kernel does not support %s mode\n",
					util_nsmode_name(p->mode));
			return -ENODEV;
		}
	}

	if (param.align) {
		int i, alignments;

		switch (p->mode) {
		case NDCTL_NS_MODE_FSDAX:
			if (!pfn) {
				error("Kernel does not support setting an alignment in fsdax mode\n");
				return -EINVAL;
			}

			alignments = ndctl_pfn_get_num_alignments(pfn);
			break;

		case NDCTL_NS_MODE_DEVDAX:
			alignments = ndctl_dax_get_num_alignments(dax);
			break;

		default:
			error("%s mode does not support setting an alignment\n",
					util_nsmode_name(p->mode));
			return -ENXIO;
		}

		p->align = parse_size64(param.align);
		for (i = 0; i < alignments; i++) {
			uint64_t a;

			if (p->mode == NDCTL_NS_MODE_FSDAX)
				a = ndctl_pfn_get_supported_alignment(pfn, i);
			else
				a = ndctl_dax_get_supported_alignment(dax, i);

			if (p->align == a)
				break;
		}

		if (i >= alignments) {
			error("unsupported align: %s\n", param.align);
			return -ENXIO;
		}
	} else {
		/*
		 * If we are trying to reconfigure with the same namespace mode,
		 * use the align details from the original namespace. Otherwise
		 * pick the align details from seed namespace
		 */
		if (ndns && p->mode == ndctl_namespace_get_mode(ndns)) {
			struct ndctl_pfn *ns_pfn = ndctl_namespace_get_pfn(ndns);
			struct ndctl_dax *ns_dax = ndctl_namespace_get_dax(ndns);

			if (ns_pfn)
				p->align = ndctl_pfn_get_align(ns_pfn);
			else if (ns_dax)
				p->align = ndctl_dax_get_align(ns_dax);
			else
				p->align = sysconf(_SC_PAGE_SIZE);
		} else
		/*
		 * Use the seed namespace alignment as the default if we need
		 * one. If we don't then use PAGE_SIZE so the size_align
		 * checking works.
		 */
		if (p->mode == NDCTL_NS_MODE_FSDAX) {
			/*
			 * The initial pfn device support in the kernel didn't
			 * have the 'align' sysfs attribute and assumed a 2MB
			 * alignment. Fall back to that if we don't have the
			 * attribute.
			 */
			if (pfn && ndctl_pfn_has_align(pfn))
				p->align = ndctl_pfn_get_align(pfn);
			else
				p->align = SZ_2M;
		} else if (p->mode == NDCTL_NS_MODE_DEVDAX) {
			/*
			 * device dax mode was added after the align attribute
			 * so checking for it is unnecessary.
			 */
			p->align = ndctl_dax_get_align(dax);
		} else {
			p->align = sysconf(_SC_PAGE_SIZE);
		}

		/*
		 * Fallback to a page alignment if the region is not aligned
		 * to the default. This is mainly useful for the nfit_test
		 * use case where it is backed by vmalloc memory.
		 */
		resource = ndctl_region_get_resource(region);
		if (resource < ULLONG_MAX && (resource & (p->align - 1))) {
			debug("%s: falling back to a page alignment\n",
					region_name);
			p->align = sysconf(_SC_PAGE_SIZE);
		}
	}

	region_align = ndctl_region_get_align(region);
	if (region_align < ULONG_MAX && p->size % region_align) {
		err("%s: align setting is %#lx size %#llx is misaligned\n",
				region_name, region_align, p->size);
		return -EINVAL;
	}

	size_align = p->align;

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
		err("'--size=' must align to interleave-width: %d and alignment: %ld\n"
				"did you intend --size=%lld%s?\n",
				ways, p->align, p->size, suffix);
		return -EINVAL;
	}

	/*
	 * Catch attempts to create sub-16M namespaces to match the
	 * kernel's restriction (see nd_namespace_store())
	 */
	if (p->size < SZ_16M && p->mode != NDCTL_NS_MODE_RAW) {
		if (default_size) {
			debug("%s: insufficient capacity for mode: %s\n",
					region_name, util_nsmode_name(p->mode));
			return -EAGAIN;
		}
		error("'--size=' must be >= 16MiB for '%s' mode\n",
				util_nsmode_name(p->mode));
		return -EINVAL;
	}

	if (param.sector_size) {
		struct ndctl_btt *btt;
		int num, i;

		p->sector_size = parse_size64(param.sector_size);
		btt = ndctl_region_get_btt_seed(region);
		if (p->mode == NDCTL_NS_MODE_SECTOR) {
			if (!btt) {
				err("%s: does not support 'sector' mode\n",
						region_name);
				return -EINVAL;
			}
			num = ndctl_btt_get_num_sector_sizes(btt);
			for (i = 0; i < num; i++)
				if (ndctl_btt_get_supported_sector_size(btt, i)
						== p->sector_size)
					break;
			if (i >= num) {
				err("%s: does not support btt sector_size %lu\n",
						region_name, p->sector_size);
				return -EINVAL;
			}
		} else {
			struct ndctl_namespace *seed = ndns;

			if (!seed) {
				seed = ndctl_region_get_namespace_seed(region);
				if (!seed) {
					err("%s: failed to get seed\n", region_name);
					return -ENXIO;
				}
			}
			num = ndctl_namespace_get_num_sector_sizes(seed);
			for (i = 0; i < num; i++)
				if (ndctl_namespace_get_supported_sector_size(seed, i)
						== p->sector_size)
					break;
			if (i >= num) {
				err("%s: does not support namespace sector_size %lu\n",
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
		if (btt && p->mode == NDCTL_NS_MODE_SECTOR)
			p->sector_size = ndctl_btt_get_sector_size(btt);
		else
			p->sector_size = ndctl_namespace_get_sector_size(ndns);
	} else {
		struct ndctl_namespace *seed;

		seed = ndctl_region_get_namespace_seed(region);
		if (!seed) {
			err("%s: failed to get seed\n", region_name);
			return -ENXIO;
		}
		if (ndctl_namespace_get_type(seed) == ND_DEVICE_NAMESPACE_BLK)
			debug("%s: set_defaults() should preclude this?\n",
				region_name);
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

		if (ndns && p->mode != NDCTL_NS_MODE_FSDAX
			&& p->mode != NDCTL_NS_MODE_DEVDAX) {
			err("%s: --map= only valid for fsdax mode namespace\n",
				ndctl_namespace_get_devname(ndns));
			return -EINVAL;
		}
	} else if (p->mode == NDCTL_NS_MODE_FSDAX
			|| p->mode == NDCTL_NS_MODE_DEVDAX)
		p->loc = NDCTL_PFN_LOC_PMEM;

	if (!pfn && do_setup_pfn(ndns, p)) {
		error("operation failed, %s cannot support requested mode\n",
			region_name);
		return -EINVAL;
	}


	p->autolabel = param.autolabel;
	p->autorecover = param.autorecover;

	return 0;
}

static struct ndctl_namespace *region_get_namespace(struct ndctl_region *region)
{
	struct ndctl_namespace *ndns;

	/* prefer the 0th namespace if it is idle */
	ndctl_namespace_foreach(region, ndns)
		if (ndctl_namespace_get_id(ndns) == 0
				&& ndctl_namespace_is_configuration_idle(ndns))
			return ndns;
	return ndctl_region_get_namespace_seed(region);
}

static int namespace_create(struct ndctl_region *region)
{
	const char *devname = ndctl_region_get_devname(region);
	struct ndctl_namespace *ndns;
	struct parsed_parameters p;
	int rc;

	rc = validate_namespace_options(region, NULL, &p);
	if (rc)
		return rc;

	if (ndctl_region_get_ro(region)) {
		debug("%s: read-only, ineligible for namespace creation\n",
			devname);
		return -EAGAIN;
	}

	ndns = region_get_namespace(region);
	if (!ndns || !ndctl_namespace_is_configuration_idle(ndns)) {
		debug("%s: no %s namespace seed\n", devname,
				ndns ? "idle" : "available");
		return -EAGAIN;
	}

	rc = setup_namespace(region, ndns, &p);
	if (rc && p.autorecover) {
		ndctl_namespace_set_enforce_mode(ndns, NDCTL_NS_MODE_RAW);
		ndctl_namespace_delete(ndns);
	}

	return rc;
}

/*
 * Return convention:
 * rc < 0 : Error while zeroing, propagate forward
 * rc == 0 : Successfully cleared the info block, report as destroyed
 * rc > 0 : skipped, do not count
 */
static int zero_info_block(struct ndctl_namespace *ndns)
{
	const char *devname = ndctl_namespace_get_devname(ndns);
	int fd, rc = -ENXIO, info_size = 8192;
	void *buf = NULL, *read_buf = NULL;
	char path[50];

	ndctl_namespace_set_raw_mode(ndns, 1);
	rc = ndctl_namespace_enable(ndns);
	if (rc < 0) {
		err("%s failed to enable for zeroing, continuing\n", devname);
		rc = 1;
		goto out;
	}

	if (posix_memalign(&buf, 4096, info_size) != 0) {
		rc = -ENOMEM;
		goto out;
	}
	if (posix_memalign(&read_buf, 4096, info_size) != 0) {
		rc = -ENOMEM;
		goto out;
	}

	sprintf(path, "/dev/%s", ndctl_namespace_get_block_device(ndns));
	fd = open(path, O_RDWR|O_DIRECT|O_EXCL);
	if (fd < 0) {
		err("%s: failed to open %s to zero info block\n",
				devname, path);
		goto out;
	}

	memset(buf, 0, info_size);
	rc = pread(fd, read_buf, info_size, 0);
	if (rc < info_size) {
		err("%s: failed to read info block, continuing\n",
			devname);
	}
	if (memcmp(buf, read_buf, info_size) == 0) {
		rc = 1;
		goto out_close;
	}

	rc = pwrite(fd, buf, info_size, 0);
	if (rc < info_size) {
		err("%s: failed to zero info block %s\n",
				devname, path);
		rc = -ENXIO;
	} else
		rc = 0;
 out_close:
	close(fd);
 out:
	ndctl_namespace_set_raw_mode(ndns, 0);
	ndctl_namespace_disable_invalidate(ndns);
	free(read_buf);
	free(buf);
	return rc;
}

static int namespace_prep_reconfig(struct ndctl_region *region,
		struct ndctl_namespace *ndns)
{
	const char *devname = ndctl_namespace_get_devname(ndns);
	bool did_zero = false;
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
	}

	rc = ndctl_namespace_disable_safe(ndns);
	if (rc)
		return rc;

	ndctl_namespace_set_enforce_mode(ndns, NDCTL_NS_MODE_RAW);

	rc = zero_info_block(ndns);
	if (rc < 0)
		return rc;
	if (rc == 0)
		did_zero = true;

	switch (ndctl_namespace_get_type(ndns)) {
        case ND_DEVICE_NAMESPACE_PMEM:
        case ND_DEVICE_NAMESPACE_BLK:
		rc = 2;
		break;
	default:
		/*
		 * for legacy namespaces, we we did any info block
		 * zeroing, we need "processed" to be incremented
		 * but otherwise we are skipping in the count
		 */
		if (did_zero)
			rc = 0;
		else
			rc = 1;
		break;
	}

	return rc;
}

static int namespace_destroy(struct ndctl_region *region,
		struct ndctl_namespace *ndns)
{
	const char *devname = ndctl_namespace_get_devname(ndns);
	unsigned long long size;
	int rc;

	rc = namespace_prep_reconfig(region, ndns);
	if (rc < 0)
		return rc;

	size = ndctl_namespace_get_size(ndns);

	/* Labeled namespace, destroy label / allocation */
	if (rc == 2) {
		rc = ndctl_namespace_delete(ndns);
		if (rc)
			debug("%s: failed to reclaim\n", devname);
	}

	/*
	 * Don't report a destroyed namespace when no capacity was
	 * allocated.
	 */
	if (size == 0 && rc == 0)
		rc = 1;

	return rc;
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
		return -ENODEV;

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
		return -ENODEV;

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
		cmd_read = ndctl_dimm_read_label_index(dimm);
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
		err("%s: failed to initialize labels\n",
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

	rc = namespace_prep_reconfig(region, ndns);
	if (rc < 0)
		return rc;

	/* check if we can enable labels on this region */
	if (ndctl_region_get_nstype(region) == ND_DEVICE_NAMESPACE_IO
			&& p.autolabel) {
		/*
		 * If this fails, try to continue label-less, if this
		 * got far enough to invalidate the region than @ndns is
		 * now invalid.
		 */
		rc = enable_labels(region);
		if (rc != -ENODEV)
			ndns = region_get_namespace(region);
		if (!ndns || (rc != -ENODEV
				&& !ndctl_namespace_is_configuration_idle(ndns))) {
			debug("%s: no %s namespace seed\n",
					ndctl_region_get_devname(region),
					ndns ? "idle" : "available");
			return -ENODEV;
		}
	}

	return setup_namespace(region, ndns, &p);
}

int namespace_check(struct ndctl_namespace *ndns, bool verbose, bool force,
		bool repair, bool logfix);

static int bus_send_clear(struct ndctl_bus *bus, unsigned long long start,
		unsigned long long size)
{
	const char *busname = ndctl_bus_get_provider(bus);
	struct ndctl_cmd *cmd_cap, *cmd_clear;
	unsigned long long cleared;
	struct ndctl_range range;
	int rc;

	/* get ars_cap */
	cmd_cap = ndctl_bus_cmd_new_ars_cap(bus, start, size);
	if (!cmd_cap) {
		err("bus: %s failed to create cmd\n", busname);
		return -ENOTTY;
	}

	rc = ndctl_cmd_submit_xlat(cmd_cap);
	if (rc < 0) {
		err("bus: %s failed to submit cmd: %d\n", busname, rc);
		goto out_cap;
	}

	/* send clear_error */
	if (ndctl_cmd_ars_cap_get_range(cmd_cap, &range)) {
		err("bus: %s failed to get ars_cap range\n", busname);
		rc = -ENXIO;
		goto out_cap;
	}

	cmd_clear = ndctl_bus_cmd_new_clear_error(range.address,
					range.length, cmd_cap);
	if (!cmd_clear) {
		err("bus: %s failed to create cmd\n", busname);
		rc = -ENOTTY;
		goto out_cap;
	}

	rc = ndctl_cmd_submit_xlat(cmd_clear);
	if (rc < 0) {
		err("bus: %s failed to submit cmd: %d\n", busname, rc);
		goto out_clr;
	}

	cleared = ndctl_cmd_clear_error_get_cleared(cmd_clear);
	if (cleared != range.length) {
		err("bus: %s expected to clear: %lld actual: %lld\n",
				busname, range.length, cleared);
		rc = -ENXIO;
	}

out_clr:
	ndctl_cmd_unref(cmd_clear);
out_cap:
	ndctl_cmd_unref(cmd_cap);
	return rc;
}

static int nstype_clear_badblocks(struct ndctl_namespace *ndns,
		const char *devname, unsigned long long dev_begin,
		unsigned long long dev_size)
{
	struct ndctl_region *region = ndctl_namespace_get_region(ndns);
	struct ndctl_bus *bus = ndctl_region_get_bus(region);
	unsigned long long region_begin, dev_end;
	unsigned int cleared = 0;
	struct badblock *bb;
	int rc = 0;

	region_begin = ndctl_region_get_resource(region);
	if (region_begin == ULLONG_MAX) {
		rc = -errno;
		if (ndctl_namespace_enable(ndns) < 0)
			error("%s: failed to reenable namespace\n", devname);
		return rc;
	}

	dev_end = dev_begin + dev_size - 1;

	ndctl_region_badblock_foreach(region, bb) {
		unsigned long long bb_begin, bb_end, bb_len;

		bb_begin = region_begin + (bb->offset << 9);
		bb_len = (unsigned long long)bb->len << 9;
		bb_end = bb_begin + bb_len - 1;

		/* bb is not fully contained in the usable area */
		if (bb_begin < dev_begin || bb_end > dev_end)
			continue;

		rc = bus_send_clear(bus, bb_begin, bb_len);
		if (rc) {
			error("%s: failed to clear badblock at {%lld, %u}\n",
				devname, bb->offset, bb->len);
			break;
		}
		cleared += bb->len;
	}
	debug("%s: cleared %u badblocks\n", devname, cleared);

	rc = ndctl_namespace_enable(ndns);
	if (rc < 0)
		return rc;
	return 0;
}

static int dax_clear_badblocks(struct ndctl_dax *dax)
{
	struct ndctl_namespace *ndns = ndctl_dax_get_namespace(dax);
	const char *devname = ndctl_dax_get_devname(dax);
	unsigned long long begin, size;
	int rc;

	begin = ndctl_dax_get_resource(dax);
	if (begin == ULLONG_MAX)
		return -ENXIO;

	size = ndctl_dax_get_size(dax);
	if (size == ULLONG_MAX)
		return -ENXIO;

	rc = ndctl_namespace_disable_safe(ndns);
	if (rc) {
		error("%s: unable to disable namespace: %s\n", devname,
			strerror(-rc));
		return rc;
	}
	return nstype_clear_badblocks(ndns, devname, begin, size);
}

static int pfn_clear_badblocks(struct ndctl_pfn *pfn)
{
	struct ndctl_namespace *ndns = ndctl_pfn_get_namespace(pfn);
	const char *devname = ndctl_pfn_get_devname(pfn);
	unsigned long long begin, size;
	int rc;

	begin = ndctl_pfn_get_resource(pfn);
	if (begin == ULLONG_MAX)
		return -ENXIO;

	size = ndctl_pfn_get_size(pfn);
	if (size == ULLONG_MAX)
		return -ENXIO;

	rc = ndctl_namespace_disable_safe(ndns);
	if (rc) {
		error("%s: unable to disable namespace: %s\n", devname,
			strerror(-rc));
		return rc;
	}
	return nstype_clear_badblocks(ndns, devname, begin, size);
}

static int raw_clear_badblocks(struct ndctl_namespace *ndns)
{
	const char *devname = ndctl_namespace_get_devname(ndns);
	unsigned long long begin, size;
	int rc;

	begin = ndctl_namespace_get_resource(ndns);
	if (begin == ULLONG_MAX)
		return -ENXIO;

	size = ndctl_namespace_get_size(ndns);
	if (size == ULLONG_MAX)
		return -ENXIO;

	rc = ndctl_namespace_disable_safe(ndns);
	if (rc) {
		error("%s: unable to disable namespace: %s\n", devname,
			strerror(-rc));
		return rc;
	}
	return nstype_clear_badblocks(ndns, devname, begin, size);
}

static int namespace_wait_scrub(struct ndctl_namespace *ndns, bool do_scrub)
{
	const char *devname = ndctl_namespace_get_devname(ndns);
	struct ndctl_bus *bus = ndctl_namespace_get_bus(ndns);
	int in_progress, rc;

	in_progress = ndctl_bus_get_scrub_state(bus);
	if (in_progress < 0) {
		error("%s: Unable to determine scrub state: %s\n", devname,
				strerror(-in_progress));
		return in_progress;
	}

	/* start a scrub if asked and if one isn't in progress */
	if (do_scrub && (!in_progress)) {
		rc = ndctl_bus_start_scrub(bus);
		if (rc) {
			error("%s: Unable to start scrub: %s\n", devname,
					strerror(-rc));
			return rc;
		}
	}

	/*
	 * wait for any in-progress scrub, whether started above, or
	 * started automatically at boot time
	 */
	rc = ndctl_bus_wait_for_scrub_completion(bus);
	if (rc) {
		error("%s: Error waiting for scrub: %s\n", devname,
				strerror(-rc));
		return rc;
	}

	return 0;
}

static int namespace_clear_bb(struct ndctl_namespace *ndns, bool do_scrub)
{
	struct ndctl_pfn *pfn = ndctl_namespace_get_pfn(ndns);
	struct ndctl_dax *dax = ndctl_namespace_get_dax(ndns);
	struct ndctl_btt *btt = ndctl_namespace_get_btt(ndns);
	struct json_object *jndns;
	int rc;

	if (btt) {
		/* skip btt error clearing for now */
		debug("%s: skip error clearing for btt\n",
				ndctl_btt_get_devname(btt));
		return 1;
	}

	rc = namespace_wait_scrub(ndns, do_scrub);
	if (rc)
		return rc;

	if (dax)
		rc = dax_clear_badblocks(dax);
	else if (pfn)
		rc = pfn_clear_badblocks(pfn);
	else
		rc = raw_clear_badblocks(ndns);

	if (rc)
		return rc;

	jndns = util_namespace_to_json(ndns, UTIL_JSON_MEDIA_ERRORS);
	if (jndns)
		printf("%s\n", json_object_to_json_string_ext(jndns,
				JSON_C_TO_STRING_PRETTY));
	return 0;
}

struct read_infoblock_ctx {
	struct json_object *jblocks;
	FILE *f_out;
};

#define parse_field(sb, field)						\
do {									\
	jobj = json_object_new_int(le32_to_cpu((sb)->field));		\
	if (!jobj)							\
		goto err;						\
	json_object_object_add(jblock, #field, jobj);			\
} while (0)

#define parse_hex(sb, field, sz)						\
do {										\
	jobj = util_json_object_hex(le##sz##_to_cpu((sb)->field), flags);	\
	if (!jobj)								\
		goto err;							\
	json_object_object_add(jblock, #field, jobj);				\
} while (0)

static json_object *btt_parse(struct btt_sb *btt_sb, struct ndctl_namespace *ndns,
		const char *path, unsigned long flags)
{
	uuid_t uuid;
	char str[40];
	struct json_object *jblock, *jobj;
	const char *cmd = "read-infoblock";
	const bool verify = param.verify;

	if (verify && !verify_infoblock_checksum((union info_block *) btt_sb)) {
		pr_verbose("%s: %s checksum verification failed\n", cmd, __func__);
		return NULL;
	}

	if (ndns) {
		ndctl_namespace_get_uuid(ndns, uuid);
		if (verify && !uuid_is_null(uuid) && memcmp(uuid, btt_sb->parent_uuid,
					sizeof(uuid) != 0)) {
			pr_verbose("%s: %s uuid verification failed\n", cmd, __func__);
			return NULL;
		}
	}

	jblock = json_object_new_object();
	if (!jblock)
		return NULL;

	if (ndns) {
		jobj = json_object_new_string(ndctl_namespace_get_devname(ndns));
		if (!jobj)
			goto err;
		json_object_object_add(jblock, "dev", jobj);
	} else {
		jobj = json_object_new_string(path);
		if (!jobj)
			goto err;
		json_object_object_add(jblock, "file", jobj);
	}

	jobj = json_object_new_string((char *) btt_sb->signature);
	if (!jobj)
		goto err;
	json_object_object_add(jblock, "signature", jobj);

	uuid_unparse((void *) btt_sb->uuid, str);
	jobj = json_object_new_string(str);
	if (!jobj)
		goto err;
	json_object_object_add(jblock, "uuid", jobj);

	uuid_unparse((void *) btt_sb->parent_uuid, str);
	jobj = json_object_new_string(str);
	if (!jobj)
		goto err;
	json_object_object_add(jblock, "parent_uuid", jobj);

	jobj = util_json_object_hex(le32_to_cpu(btt_sb->flags), flags);
	if (!jobj)
		goto err;
	json_object_object_add(jblock, "flags", jobj);

	if (snprintf(str, 4, "%d.%d", btt_sb->version_major,
				btt_sb->version_minor) >= 4)
		goto err;
	jobj = json_object_new_string(str);
	if (!jobj)
		goto err;
	json_object_object_add(jblock, "version", jobj);

	parse_field(btt_sb, external_lbasize);
	parse_field(btt_sb, external_nlba);
	parse_field(btt_sb, internal_lbasize);
	parse_field(btt_sb, internal_nlba);
	parse_field(btt_sb, nfree);
	parse_field(btt_sb, infosize);
	parse_hex(btt_sb, nextoff, 64);
	parse_hex(btt_sb, dataoff, 64);
	parse_hex(btt_sb, mapoff, 64);
	parse_hex(btt_sb, logoff, 64);
	parse_hex(btt_sb, info2off, 64);

	return jblock;
err:
	pr_verbose("%s: failed to create json representation\n", cmd);
	json_object_put(jblock);
	return NULL;
}

static json_object *pfn_parse(struct pfn_sb *pfn_sb, struct ndctl_namespace *ndns,
		const char *path, unsigned long flags)
{
	uuid_t uuid;
	char str[40];
	struct json_object *jblock, *jobj;
	const char *cmd = "read-infoblock";
	const bool verify = param.verify;

	if (verify && !verify_infoblock_checksum((union info_block *) pfn_sb)) {
		pr_verbose("%s: %s checksum verification failed\n", cmd, __func__);
		return NULL;
	}

	if (ndns) {
		ndctl_namespace_get_uuid(ndns, uuid);
		if (verify && !uuid_is_null(uuid) && memcmp(uuid, pfn_sb->parent_uuid,
					sizeof(uuid) != 0)) {
			pr_verbose("%s: %s uuid verification failed\n", cmd, __func__);
			return NULL;
		}
	}

	jblock = json_object_new_object();
	if (!jblock)
		return NULL;

	if (ndns) {
		jobj = json_object_new_string(ndctl_namespace_get_devname(ndns));
		if (!jobj)
			goto err;
		json_object_object_add(jblock, "dev", jobj);
	} else {
		jobj = json_object_new_string(path);
		if (!jobj)
			goto err;
		json_object_object_add(jblock, "file", jobj);
	}

	jobj = json_object_new_string((char *) pfn_sb->signature);
	if (!jobj)
		goto err;
	json_object_object_add(jblock, "signature", jobj);

	uuid_unparse((void *) pfn_sb->uuid, str);
	jobj = json_object_new_string(str);
	if (!jobj)
		goto err;
	json_object_object_add(jblock, "uuid", jobj);

	uuid_unparse((void *) pfn_sb->parent_uuid, str);
	jobj = json_object_new_string(str);
	if (!jobj)
		goto err;
	json_object_object_add(jblock, "parent_uuid", jobj);

	jobj = util_json_object_hex(le32_to_cpu(pfn_sb->flags), flags);
	if (!jobj)
		goto err;
	json_object_object_add(jblock, "flags", jobj);

	if (snprintf(str, 4, "%d.%d", pfn_sb->version_major,
				pfn_sb->version_minor) >= 4)
		goto err;
	jobj = json_object_new_string(str);
	if (!jobj)
		goto err;
	json_object_object_add(jblock, "version", jobj);

	parse_hex(pfn_sb, dataoff, 64);
	parse_hex(pfn_sb, npfns, 64);
	parse_field(pfn_sb, mode);
	parse_hex(pfn_sb, start_pad, 32);
	parse_hex(pfn_sb, end_trunc, 32);
	parse_hex(pfn_sb, align, 32);
	parse_hex(pfn_sb, page_size, 32);
	parse_hex(pfn_sb, page_struct_size, 16);

	return jblock;
err:
	pr_verbose("%s: failed to create json representation\n", cmd);
	json_object_put(jblock);
	return NULL;
}

#define INFOBLOCK_SZ SZ_8K

static int parse_namespace_infoblock(char *_buf, struct ndctl_namespace *ndns,
		const char *path, struct read_infoblock_ctx *ri_ctx)
{
	int rc;
	void *buf = _buf;
	unsigned long flags = param.human ? UTIL_JSON_HUMAN : 0;
	struct btt_sb *btt1_sb = buf + SZ_4K, *btt2_sb = buf;
	struct json_object *jblock = NULL, *jblocks = ri_ctx->jblocks;
	struct pfn_sb *pfn_sb = buf + SZ_4K, *dax_sb = buf + SZ_4K;

	if (!param.json && !param.human) {
		rc = fwrite(buf, 1, INFOBLOCK_SZ, ri_ctx->f_out);
		if (rc != INFOBLOCK_SZ)
			return -EIO;
		fflush(ri_ctx->f_out);
		return 0;
	}

	if (!jblocks) {
		jblocks = json_object_new_array();
		if (!jblocks)
			return -ENOMEM;
		ri_ctx->jblocks = jblocks;
	}

	if (memcmp(btt1_sb->signature, BTT_SIG, BTT_SIG_LEN) == 0) {
		jblock = btt_parse(btt1_sb, ndns, path, flags);
		if (jblock)
			json_object_array_add(jblocks, jblock);
	}

	if (memcmp(btt2_sb->signature, BTT_SIG, BTT_SIG_LEN) == 0) {
		jblock = btt_parse(btt2_sb, ndns, path, flags);
		if (jblock)
			json_object_array_add(jblocks, jblock);
	}

	if (memcmp(pfn_sb->signature, PFN_SIG, PFN_SIG_LEN) == 0) {
		jblock = pfn_parse(pfn_sb, ndns, path, flags);
		if (jblock)
			json_object_array_add(jblocks, jblock);
	}

	if (memcmp(dax_sb->signature, DAX_SIG, PFN_SIG_LEN) == 0) {
		jblock = pfn_parse(dax_sb, ndns, path, flags);
		if (jblock)
			json_object_array_add(jblocks, jblock);
	}

	return 0;
}

static int file_read_infoblock(const char *path, struct ndctl_namespace *ndns,
		struct read_infoblock_ctx *ri_ctx)
{
	const char *devname = ndns ? ndctl_namespace_get_devname(ndns) : "";
	const char *cmd = "read-infoblock";
	void *buf = NULL;
	int fd = -1, rc;

	buf = calloc(1, INFOBLOCK_SZ);
	if (!buf)
		return -ENOMEM;

	if (!path) {
		fd = STDIN_FILENO;
		path = "<stdin>";
	} else
		fd = open(path, O_RDONLY|O_EXCL);

	if (fd < 0) {
		pr_verbose("%s: %s failed to open %s: %s\n",
				cmd, devname, path, strerror(errno));
		rc = -errno;
		goto out;
	}

	rc = read(fd, buf, INFOBLOCK_SZ);
	if (rc < INFOBLOCK_SZ) {
		pr_verbose("%s: %s failed to read %s: %s\n",
				cmd, devname, path, strerror(errno));
		if (rc < 0)
			rc = -errno;
		else
			rc = -EIO;
		goto out;
	}

	rc = parse_namespace_infoblock(buf, ndns, path, ri_ctx);
out:
	free(buf);
	if (fd >= 0 && fd != STDIN_FILENO)
		close(fd);
	return rc;
}

static unsigned long PHYS_PFN(unsigned long long phys)
{
	return phys / sysconf(_SC_PAGE_SIZE);
}

#define SUBSECTION_SHIFT 21
#define SUBSECTION_SIZE (1UL << SUBSECTION_SHIFT)
#define MAX_STRUCT_PAGE_SIZE 64

/* Derived from nd_pfn_init() in kernel version v5.5 */
static int write_pfn_sb(int fd, unsigned long long size, const char *sig,
		void *buf)
{
	unsigned long npfns, align, pfn_align;
	struct pfn_sb *pfn_sb = buf + SZ_4K;
	unsigned long long start, offset;
	uuid_t uuid, parent_uuid;
	u32 end_trunc, start_pad;
	enum pfn_mode mode;
	u64 checksum;
	int rc;

	start = parse_size64(param.offset);
	npfns = PHYS_PFN(size - SZ_8K);
	pfn_align = parse_size64(param.align);
	align = max(pfn_align, SUBSECTION_SIZE);
	if (param.uuid)
		uuid_parse(param.uuid, uuid);
	else
		uuid_generate(uuid);

	if (param.parent_uuid)
		uuid_parse(param.parent_uuid, parent_uuid);
	else
		memset(parent_uuid, 0, sizeof(uuid_t));

	if (strcmp(param.map, "dev") == 0)
		mode = PFN_MODE_PMEM;
	else
		mode = PFN_MODE_RAM;

	/*
	 * Unlike the kernel implementation always set start_pad and
	 * end_trunc relative to the specified --offset= option to allow
	 * testing legacy / "buggy" configurations.
	 */
	start_pad = ALIGN(start, align) - start;
	end_trunc = start + size - ALIGN_DOWN(start + size, align);
	if (mode == PFN_MODE_PMEM) {
		/*
		 * The altmap should be padded out to the block size used
		 * when populating the vmemmap. This *should* be equal to
		 * PMD_SIZE for most architectures.
		 *
		 * Also make sure size of struct page is less than 64. We
		 * want to make sure we use large enough size here so that
		 * we don't have a dynamic reserve space depending on
		 * struct page size. But we also want to make sure we notice
		 * when we end up adding new elements to struct page.
		 */
		offset = ALIGN(start + SZ_8K + MAX_STRUCT_PAGE_SIZE * npfns, align)
			- start;
	} else
		offset = ALIGN(start + SZ_8K, align) - start;

	if (offset >= size) {
		error("unable to satisfy requested alignment\n");
		return -ENXIO;
	}

	npfns = PHYS_PFN(size - offset - end_trunc - start_pad);
	pfn_sb->mode = cpu_to_le32(mode);
	pfn_sb->dataoff = cpu_to_le64(offset);
	pfn_sb->npfns = cpu_to_le64(npfns);
	memcpy(pfn_sb->signature, sig, PFN_SIG_LEN);
	memcpy(pfn_sb->uuid, uuid, 16);
	memcpy(pfn_sb->parent_uuid, parent_uuid, 16);
	pfn_sb->version_major = cpu_to_le16(1);
	pfn_sb->version_minor = cpu_to_le16(4);
	pfn_sb->start_pad = cpu_to_le32(start_pad);
	pfn_sb->end_trunc = cpu_to_le32(end_trunc);
	pfn_sb->align = cpu_to_le32(pfn_align);
	pfn_sb->page_struct_size = cpu_to_le16(MAX_STRUCT_PAGE_SIZE);
	pfn_sb->page_size = cpu_to_le32(sysconf(_SC_PAGE_SIZE));
	checksum = fletcher64(pfn_sb, sizeof(*pfn_sb), 0);
	pfn_sb->checksum = cpu_to_le64(checksum);

	rc = write(fd, buf, INFOBLOCK_SZ);
	if (rc < INFOBLOCK_SZ)
		return -EIO;
	return 0;
}

static int file_write_infoblock(const char *path)
{
	unsigned long long size = parse_size64(param.size);
	int fd = -1, rc;
	void *buf;

	if (param.std_out)
		fd = STDOUT_FILENO;
	else {
		fd = open(path, O_CREAT|O_RDWR, 0644);
		if (fd < 0) {
			error("failed to open: %s\n", path);
			return -errno;
		}

		if (!size) {
			struct stat st;

			rc = fstat(fd, &st);
			if (rc < 0) {
				error("failed to stat: %s\n", path);
				rc = -errno;
				goto out;
			}
			if (S_ISREG(st.st_mode))
				size = st.st_size;
			else if (S_ISBLK(st.st_mode)) {
				rc = ioctl(fd, BLKGETSIZE64, &size);
				if (rc < 0) {
					error("failed to retrieve size: %s\n", path);
					rc = -errno;
					goto out;
				}
			} else {
				error("unsupported file type: %s\n", path);
				rc = -EINVAL;
				goto out;
			}
		}
	}

	buf = calloc(INFOBLOCK_SZ, 1);
	if (!buf) {
		rc = -ENOMEM;
		goto out;
	}

	switch (util_nsmode(param.mode)) {
	case NDCTL_NS_MODE_FSDAX:
		rc = write_pfn_sb(fd, size, PFN_SIG, buf);
		break;
	case NDCTL_NS_MODE_DEVDAX:
		rc = write_pfn_sb(fd, size, DAX_SIG, buf);
		break;
	default:
		rc = -EINVAL;
		break;
	}

	free(buf);
out:
	if (fd >= 0 && fd != STDOUT_FILENO)
		close(fd);
	return rc;
}

static unsigned long ndctl_get_default_alignment(struct ndctl_namespace *ndns)
{
	unsigned long long align = 0;
	struct ndctl_dax *dax = ndctl_namespace_get_dax(ndns);
	struct ndctl_pfn *pfn = ndctl_namespace_get_pfn(ndns);

	if (ndctl_namespace_get_mode(ndns) == NDCTL_NS_MODE_FSDAX && pfn)
		align = ndctl_pfn_get_supported_alignment(pfn, 1);
	else if (ndctl_namespace_get_mode(ndns) == NDCTL_NS_MODE_DEVDAX && dax)
		align = ndctl_dax_get_supported_alignment(dax, 1);

	if (!align)
		align =  sysconf(_SC_PAGE_SIZE);

	return align;
}

static int namespace_rw_infoblock(struct ndctl_namespace *ndns,
		struct read_infoblock_ctx *ri_ctx, int write)
{
	int rc;
	uuid_t uuid;
	char str[40];
	char path[50];
	const char *save;
	const char *cmd = write ? "write-infoblock" : "read-infoblock";
	const char *devname = ndctl_namespace_get_devname(ndns);

	if (ndctl_namespace_is_active(ndns)) {
		pr_verbose("%s: %s enabled, must be disabled\n", cmd, devname);
		return -EBUSY;
	}

	ndctl_namespace_set_raw_mode(ndns, 1);
	rc = ndctl_namespace_enable(ndns);
	if (rc < 0) {
		pr_verbose("%s: %s failed to enable\n", cmd, devname);
		goto out;
	}

	save = param.parent_uuid;
	if (!param.parent_uuid) {
		ndctl_namespace_get_uuid(ndns, uuid);
		uuid_unparse(uuid, str);
		param.parent_uuid = str;
	}

	sprintf(path, "/dev/%s", ndctl_namespace_get_block_device(ndns));
	if (write) {
		unsigned long long align;
		bool align_provided = true;

		if (!param.align) {
			align = ndctl_get_default_alignment(ndns);

			if (asprintf((char **)&param.align, "%llu", align) < 0) {
				rc = -EINVAL;
				goto out;
			}
			align_provided = false;
		}

		if (param.size) {
			unsigned long long size = parse_size64(param.size);
			align = parse_size64(param.align);

			if (align < ULLONG_MAX && !IS_ALIGNED(size, align)) {
				error("--size=%s not aligned to %s\n", param.size,
					param.align);

				rc = -EINVAL;
			}
		}

		if (!rc)
			rc = file_write_infoblock(path);

		if (!align_provided) {
			free((char *)param.align);
			param.align = NULL;
		}
	} else
		rc = file_read_infoblock(path, ndns, ri_ctx);
	param.parent_uuid = save;
out:
	ndctl_namespace_set_raw_mode(ndns, 0);
	ndctl_namespace_disable_invalidate(ndns);
	return rc;
}

static int do_xaction_namespace(const char *namespace,
		enum device_action action, struct ndctl_ctx *ctx,
		int *processed)
{
	struct read_infoblock_ctx ri_ctx = { 0 };
	struct ndctl_namespace *ndns, *_n;
	int rc = -ENXIO, saved_rc = 0;
	struct ndctl_region *region;
	const char *ndns_name;
	struct ndctl_bus *bus;

	*processed = 0;

	if (action == ACTION_READ_INFOBLOCK) {
		if (!param.outfile)
			ri_ctx.f_out = stdout;
		else {
			ri_ctx.f_out = fopen(param.outfile, "w+");
			if (!ri_ctx.f_out) {
				fprintf(stderr, "failed to open: %s: (%s)\n",
						param.outfile, strerror(errno));
				return -errno;
			}
		}

		if (param.infile || !namespace) {
			rc = file_read_infoblock(param.infile, NULL, &ri_ctx);
			if (ri_ctx.jblocks)
				util_display_json_array(ri_ctx.f_out, ri_ctx.jblocks, 0);
			if (rc >= 0)
				(*processed)++;
			return rc;
		}
	}

	if (action == ACTION_WRITE_INFOBLOCK && !namespace) {
		if (!param.align)
			param.align = "2M";

		rc = file_write_infoblock(param.outfile);
		if (rc >= 0)
			(*processed)++;
		return rc;
	}

	if (!namespace && action != ACTION_CREATE)
		return rc;

	if (verbose)
		ndctl_set_log_priority(ctx, LOG_DEBUG);

	if (action == ACTION_ENABLE)
		cmd_name = "enable namespace";
	else if (action == ACTION_DISABLE)
		cmd_name = "disable namespace";
	else if (action == ACTION_CREATE)
		cmd_name = "create namespace";
	else if (action == ACTION_DESTROY)
		cmd_name = "destroy namespace";
	else if (action == ACTION_CHECK)
		cmd_name = "check namespace";
	else if (action == ACTION_CLEAR)
		cmd_name = "clear errors namespace";

        ndctl_bus_foreach(ctx, bus) {
		bool do_scrub;

		if (!util_bus_filter(bus, param.bus))
			continue;

		/* only request scrubbing once per bus */
		do_scrub = scrub;

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
				if (rc == 0) {
					(*processed)++;
					if (param.greedy)
						continue;
				} else if (param.greedy && force) {
						saved_rc = rc;
						continue;
				}
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
					if (rc == 0)
						(*processed)++;
					break;
				case ACTION_ENABLE:
					rc = ndctl_namespace_enable(ndns);
					if (rc >= 0) {
						(*processed)++;
						rc = 0;
					}
					break;
				case ACTION_DESTROY:
					rc = namespace_destroy(region, ndns);
					if (rc == 0)
						(*processed)++;
					/* return success if skipped */
					if (rc > 0)
						rc = 0;
					break;
				case ACTION_CHECK:
					rc = namespace_check(ndns, verbose,
							force, repair, logfix);
					if (rc == 0)
						(*processed)++;
					break;
				case ACTION_CLEAR:
					rc = namespace_clear_bb(ndns, do_scrub);

					/* one scrub per bus is sufficient */
					do_scrub = false;
					if (rc == 0)
						(*processed)++;
					break;
				case ACTION_CREATE:
					rc = namespace_reconfig(region, ndns);
					if (rc == 0)
						*processed = 1;
					return rc;
				case ACTION_READ_INFOBLOCK:
					rc = namespace_rw_infoblock(ndns, &ri_ctx, READ);
					if (rc == 0)
						(*processed)++;
					break;
				case ACTION_WRITE_INFOBLOCK:
					rc = namespace_rw_infoblock(ndns, NULL, WRITE);
					if (rc == 0)
						(*processed)++;
					break;
				default:
					rc = -EINVAL;
					break;
				}
			}
		}
	}

	if (ri_ctx.jblocks)
		util_display_json_array(ri_ctx.f_out, ri_ctx.jblocks, 0);

	if (ri_ctx.f_out && ri_ctx.f_out != stdout)
		fclose(ri_ctx.f_out);

	if (action == ACTION_CREATE && rc == -EAGAIN) {
		/*
		 * Namespace creation searched through all candidate
		 * regions and all of them said "nope, I don't have
		 * enough capacity", so report -ENOSPC. Except during
		 * greedy namespace creation using --continue as we
		 * may have created some namespaces already, and the
		 * last one in the region search may preexist.
		 */
		if (param.greedy && (*processed) > 0)
			rc = 0;
		else
			rc = -ENOSPC;
	}
	if (saved_rc)
		rc = saved_rc;

	return rc;
}

int cmd_disable_namespace(int argc, const char **argv, struct ndctl_ctx *ctx)
{
	char *xable_usage = "ndctl disable-namespace <namespace> [<options>]";
	const char *namespace = parse_namespace_options(argc, argv,
			ACTION_DISABLE, base_options, xable_usage);
	int disabled, rc;

	rc = do_xaction_namespace(namespace, ACTION_DISABLE, ctx, &disabled);
	if (rc < 0 && !err_count)
		fprintf(stderr, "error disabling namespaces: %s\n",
				strerror(-rc));

	fprintf(stderr, "disabled %d namespace%s\n", disabled,
			disabled == 1 ? "" : "s");
	return rc;
}

int cmd_enable_namespace(int argc, const char **argv, struct ndctl_ctx *ctx)
{
	char *xable_usage = "ndctl enable-namespace <namespace> [<options>]";
	const char *namespace = parse_namespace_options(argc, argv,
			ACTION_ENABLE, base_options, xable_usage);
	int enabled, rc;

	rc = do_xaction_namespace(namespace, ACTION_ENABLE, ctx, &enabled);
	if (rc < 0 && !err_count)
		fprintf(stderr, "error enabling namespaces: %s\n",
				strerror(-rc));
	fprintf(stderr, "enabled %d namespace%s\n", enabled,
			enabled == 1 ? "" : "s");
	return rc;
}

int cmd_create_namespace(int argc, const char **argv, struct ndctl_ctx *ctx)
{
	char *xable_usage = "ndctl create-namespace [<options>]";
	const char *namespace = parse_namespace_options(argc, argv,
			ACTION_CREATE, create_options, xable_usage);
	int created, rc;

	rc = do_xaction_namespace(namespace, ACTION_CREATE, ctx, &created);
	if (rc < 0 && created < 1 && param.do_scan) {
		/*
		 * In the default scan case we try pmem first and then
		 * fallback to blk before giving up.
		 */
		memset(&param, 0, sizeof(param));
		param.type = "blk";
		set_defaults(ACTION_CREATE);
		rc = do_xaction_namespace(NULL, ACTION_CREATE, ctx, &created);
	}

	if (param.greedy)
		fprintf(stderr, "created %d namespace%s\n", created,
			created == 1 ? "" : "s");
	if ((rc < 0 || (!namespace && created < 1)) && !err_count) {
		fprintf(stderr, "failed to %s namespace: %s\n", namespace
				? "reconfigure" : "create", strerror(-rc));
		if (!namespace)
			rc = -ENODEV;
	}

	return rc;
}

int cmd_destroy_namespace(int argc , const char **argv, struct ndctl_ctx *ctx)
{
	char *xable_usage = "ndctl destroy-namespace <namespace> [<options>]";
	const char *namespace = parse_namespace_options(argc, argv,
			ACTION_DESTROY, destroy_options, xable_usage);
	int destroyed, rc;

	rc = do_xaction_namespace(namespace, ACTION_DESTROY, ctx, &destroyed);
	if (rc < 0 && !err_count)
		fprintf(stderr, "error destroying namespaces: %s\n",
				strerror(-rc));
	fprintf(stderr, "destroyed %d namespace%s\n", destroyed,
			destroyed == 1 ? "" : "s");
	return rc;
}

int cmd_check_namespace(int argc , const char **argv, struct ndctl_ctx *ctx)
{
	char *xable_usage = "ndctl check-namespace <namespace> [<options>]";
	const char *namespace = parse_namespace_options(argc, argv,
			ACTION_CHECK, check_options, xable_usage);
	int checked, rc;

	rc = do_xaction_namespace(namespace, ACTION_CHECK, ctx, &checked);
	if (rc < 0 && !err_count)
		fprintf(stderr, "error checking namespaces: %s\n",
				strerror(-rc));
	fprintf(stderr, "checked %d namespace%s\n", checked,
			checked == 1 ? "" : "s");
	return rc;
}

int cmd_clear_errors(int argc , const char **argv, struct ndctl_ctx *ctx)
{
	char *xable_usage = "ndctl clear_errors <namespace> [<options>]";
	const char *namespace = parse_namespace_options(argc, argv,
			ACTION_CLEAR, clear_options, xable_usage);
	int cleared, rc;

	rc = do_xaction_namespace(namespace, ACTION_CLEAR, ctx, &cleared);
	if (rc < 0 && !err_count)
		fprintf(stderr, "error clearing namespaces: %s\n",
				strerror(-rc));
	fprintf(stderr, "cleared %d namespace%s\n", cleared,
			cleared == 1 ? "" : "s");
	return rc;
}

int cmd_read_infoblock(int argc, const char **argv, struct ndctl_ctx *ctx)
{
	char *xable_usage = "ndctl read-infoblock <namespace> [<options>]";
	const char *namespace = parse_namespace_options(argc, argv,
			ACTION_READ_INFOBLOCK, read_infoblock_options,
			xable_usage);
	int read, rc;

	rc = do_xaction_namespace(namespace, ACTION_READ_INFOBLOCK, ctx, &read);
	if (rc < 0)
		fprintf(stderr, "error reading infoblock data: %s\n",
				strerror(-rc));
	fprintf(stderr, "read %d infoblock%s\n", read, read == 1 ? "" : "s");
	return rc;
}

int cmd_write_infoblock(int argc, const char **argv, struct ndctl_ctx *ctx)
{
	char *xable_usage = "ndctl write-infoblock <namespace> [<options>]";
	const char *namespace = parse_namespace_options(argc, argv,
			ACTION_WRITE_INFOBLOCK, write_infoblock_options,
			xable_usage);
	int write, rc;

	rc = do_xaction_namespace(namespace, ACTION_WRITE_INFOBLOCK, ctx,
			&write);
	if (rc < 0)
		fprintf(stderr, "error checking infoblocks: %s\n",
				strerror(-rc));
	fprintf(stderr, "wrote %d infoblock%s\n", write, write == 1 ? "" : "s");
	return rc;
}
