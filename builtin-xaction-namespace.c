/*
 * Copyright(c) 2015-2016 Intel Corporation. All rights reserved.
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
#include <sys/stat.h>
#include <uuid/uuid.h>
#include <sys/types.h>
#include <util/size.h>
#include <util/json.h>
#include <json-c/json.h>
#include <util/filter.h>
#include <ndctl/libndctl.h>
#include <util/parse-options.h>
#include <ccan/array_size/array_size.h>

#ifdef HAVE_NDCTL_H
#include <linux/ndctl.h>
#else
#include <ndctl.h>
#endif

static bool verbose;
static bool force;
static struct parameters {
	bool do_scan;
	bool mode_default;
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
} param;

#define NSLABEL_NAME_LEN 64
struct parsed_parameters {
	enum ndctl_pfn_loc loc;
	uuid_t uuid;
	char name[NSLABEL_NAME_LEN];
	enum ndctl_namespace_mode mode;
	unsigned long long size;
	unsigned long sector_size;
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
OPT_BOOLEAN('f', "force", &force, "reconfigure namespace even if currently active")

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

enum namespace_action {
	ACTION_ENABLE,
	ACTION_DISABLE,
	ACTION_CREATE,
	ACTION_DESTROY,
};

static int set_defaults(enum namespace_action mode)
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

		if (param.type && param.mode && strcmp(param.type, "pmem") == 0
				&& strcmp(param.mode, "safe") != 0) {
			error("'pmem' namespaces do not support setting 'sector size'\n");
			rc = -EINVAL;
		}
	} else if (!param.reconfig)
		param.sector_size = "4096";

	return rc;
}

/*
 * parse_namespace_options - basic parsing sanity checks before we start
 * looking at actual namespace devices and available resources.
 */
static const char *parse_namespace_options(int argc, const char **argv,
		enum namespace_action mode, const struct option *options,
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
		error("specify a namespace to %s, or \"all\"\n",
				mode == ACTION_ENABLE ? "enable" : "disable");
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

	if (ndctl_namespace_get_type(ndns) == ND_DEVICE_NAMESPACE_BLK)
		try(ndctl_namespace, set_sector_size, ndns, p->sector_size);

	uuid_generate(uuid);
	if (do_setup_pfn(ndns, p)) {
		struct ndctl_pfn *pfn = ndctl_region_get_pfn_seed(region);

		try(ndctl_pfn, set_uuid, pfn, uuid);
		try(ndctl_pfn, set_location, pfn, p->loc);
		try(ndctl_pfn, set_align, pfn, SZ_2M);
		try(ndctl_pfn, set_namespace, pfn, ndns);
		rc = ndctl_pfn_enable(pfn);
	} else if (p->mode == NDCTL_NS_MODE_DAX) {
		struct ndctl_dax *dax = ndctl_region_get_dax_seed(region);

		try(ndctl_dax, set_uuid, dax, uuid);
		try(ndctl_dax, set_location, dax, p->loc);
		try(ndctl_dax, set_align, dax, SZ_2M);
		try(ndctl_dax, set_namespace, dax, ndns);
		rc = ndctl_dax_enable(dax);
	} else if (p->mode == NDCTL_NS_MODE_SAFE) {
		struct ndctl_btt *btt = ndctl_region_get_btt_seed(region);

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
		struct json_object *jndns = util_namespace_to_json(ndns);

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
	int rc = 0;

	memset(p, 0, sizeof(*p));

	if (param.size)
		p->size = parse_size64(param.size);
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

		if (ndns && ndctl_namespace_get_type(ndns)
				== ND_DEVICE_NAMESPACE_BLK
				&& p->mode == NDCTL_NS_MODE_MEMORY) {
			debug("%s: blk namespace do not support memory mode\n",
					ndctl_namespace_get_devname(ndns));
				return -EINVAL;
		}
	} else if (ndns)
		p->mode = ndctl_namespace_get_mode(ndns);

	if (param.sector_size) {
		p->sector_size = parse_size64(param.sector_size);

		if (ndns && p->mode != NDCTL_NS_MODE_SAFE
				&& ndctl_namespace_get_type(ndns)
				== ND_DEVICE_NAMESPACE_PMEM) {
			debug("%s: does not support sector_size modification\n",
					ndctl_namespace_get_devname(ndns));
				return -EINVAL;
		}
	} else if (ndns) {
		struct ndctl_btt *btt = ndctl_namespace_get_btt(ndns);

		if (btt)
			p->sector_size = ndctl_btt_get_sector_size(btt);
		else if (ndctl_namespace_get_type(ndns)
				== ND_DEVICE_NAMESPACE_BLK)
			p->sector_size = ndctl_namespace_get_sector_size(ndns);
		else if (p->mode == NDCTL_NS_MODE_SAFE)
				p->sector_size = 4096;
	}

	if (param.map) {
		if (!strcmp(param.map, "mem"))
			p->loc = NDCTL_PFN_LOC_RAM;
		else
			p->loc = NDCTL_PFN_LOC_PMEM;

		if (ndns && p->mode != NDCTL_NS_MODE_MEMORY) {
			debug("%s: --map= only valid for memory mode namespace\n",
				ndctl_namespace_get_devname(ndns));
			return -EINVAL;
		}
	} else if (p->mode == NDCTL_NS_MODE_MEMORY || NDCTL_NS_MODE_DAX)
		p->loc = NDCTL_PFN_LOC_PMEM;

	/* check if we need, and whether the kernel supports, pfn devices */
	if (do_setup_pfn(ndns, p)) {
		struct ndctl_pfn *pfn = ndctl_region_get_pfn_seed(region);

		if (!pfn && param.mode_default) {
			debug("memory mode not available\n");
			p->mode = NDCTL_NS_MODE_RAW;
		} else if (!pfn) {
			error("operation failed, memory mode not available\n");
			return -EINVAL;
		}
	}

	/* check if we need, and whether the kernel supports, dax devices */
	if (p->mode == NDCTL_NS_MODE_DAX) {
		struct ndctl_dax *dax = ndctl_region_get_dax_seed(region);

		if (!dax) {
			error("operation failed, dax mode not available\n");
			return -EINVAL;
		}
	}

	return 0;
}

static int namespace_create(struct ndctl_region *region)
{
	const char *devname = ndctl_region_get_devname(region);
	unsigned long long available;
	struct ndctl_namespace *ndns;
	struct parsed_parameters p;

	if (validate_namespace_options(region, NULL, &p))
		return -EINVAL;

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

	ndns = ndctl_region_get_namespace_seed(region);
	if (is_namespace_active(ndns)) {
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
	const char *bdev = NULL;
	bool dax_active = false;
	char path[50];
	int fd, rc;

	if (ndctl_region_get_ro(region)) {
		error("%s: read-only, re-configuration disabled\n",
				devname);
		return -ENXIO;
	}

	if (pfn && ndctl_pfn_is_enabled(pfn))
		bdev = ndctl_pfn_get_block_device(pfn);
	else if (dax && ndctl_dax_is_enabled(dax))
		dax_active = true;
	else if (btt && ndctl_btt_is_enabled(btt))
		bdev = ndctl_btt_get_block_device(btt);
	else if (ndctl_namespace_is_enabled(ndns))
		bdev = ndctl_namespace_get_block_device(ndns);

	if ((bdev || dax_active) && !force) {
		error("%s is active, specify --force for re-configuration\n",
				devname);
		return -EBUSY;
	} else if (bdev) {
		sprintf(path, "/dev/%s", bdev);
		fd = open(path, O_RDWR|O_EXCL);
		if (fd >= 0) {
			/*
			 * Got it, now block new mounts while we have it
			 * pinned.
			 */
			ndctl_namespace_disable_invalidate(ndns);
			close(fd);
		} else {
			/*
			 * Yes, TOCTOU hole, but if you're racing namespace
			 * creation you have other problems, and there's nothing
			 * stopping the !bdev case from racing to mount an fs or
			 * re-enabling the namepace.
			 */
			error("%s: %s failed exlusive open: %s\n",
					devname, bdev, strerror(errno));
			return -errno;
		}
	}

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

static int namespace_reconfig(struct ndctl_region *region,
		struct ndctl_namespace *ndns)
{
	struct parsed_parameters p;
	int rc;

	if (validate_namespace_options(region, ndns, &p))
		return -EINVAL;

	rc = namespace_destroy(region, ndns);
	if (rc)
		return rc;

	ndns = ndctl_region_get_namespace_seed(region);
	if (is_namespace_active(ndns)) {
		debug("%s: no %s namespace seed\n",
				ndctl_region_get_devname(region),
				ndns ? "idle" : "available");
		return -ENODEV;
	}

	return setup_namespace(region, ndns, &p);
}

static int do_xaction_namespace(const char *namespace,
		enum namespace_action action)
{
	struct ndctl_namespace *ndns, *_n;
	int rc = -ENXIO, success = 0;
	struct ndctl_region *region;
	const char *ndns_name;
	struct ndctl_ctx *ctx;
	struct ndctl_bus *bus;

	if (!namespace && action != ACTION_CREATE)
		return rc;

	rc = ndctl_new(&ctx);
	if (rc < 0)
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
				if (rc == -EAGAIN) {
					rc = 0;
					continue;
				}
				if (rc == 0)
					rc = 1;
				goto done;
			}
			ndctl_namespace_foreach_safe(region, ndns, _n) {
				ndns_name = ndctl_namespace_get_devname(ndns);

				if (strcmp(namespace, "all") != 0
						&& strcmp(namespace, ndns_name) != 0)
					continue;
				switch (action) {
				case ACTION_DISABLE:
					rc = ndctl_namespace_disable_invalidate(ndns);
					break;
				case ACTION_ENABLE:
					rc = ndctl_namespace_enable(ndns);
					break;
				case ACTION_DESTROY:
					rc = namespace_destroy(region, ndns);
					break;
				case ACTION_CREATE:
					rc = namespace_reconfig(region, ndns);
					if (rc < 0)
						goto done;
					rc = 1;
					goto done;
				}
				if (rc >= 0)
					success++;
			}
		}
	}

	rc = success;
 done:
	ndctl_unref(ctx);
	return rc;
}

int cmd_disable_namespace(int argc, const char **argv)
{
	char *xable_usage = "ndctl disable-namespace <namespace> [<options>]";
	const char *namespace = parse_namespace_options(argc, argv,
			ACTION_DISABLE, base_options, xable_usage);
	int disabled = do_xaction_namespace(namespace, ACTION_DISABLE);

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
		return disabled;
	}
}

int cmd_enable_namespace(int argc, const char **argv)
{
	char *xable_usage = "ndctl enable-namespace <namespace> [<options>]";
	const char *namespace = parse_namespace_options(argc, argv,
			ACTION_ENABLE, base_options, xable_usage);
	int enabled = do_xaction_namespace(namespace, ACTION_ENABLE);

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

int cmd_create_namespace(int argc, const char **argv)
{
	char *xable_usage = "ndctl create-namespace [<options>]";
	const char *namespace = parse_namespace_options(argc, argv,
			ACTION_CREATE, create_options, xable_usage);
	int created = do_xaction_namespace(namespace, ACTION_CREATE);

	if (created < 1 && param.do_scan) {
		/*
		 * In the default scan case we try pmem first and then
		 * fallback to blk before giving up.
		 */
		memset(&param, 0, sizeof(param));
		param.type = "blk";
		set_defaults(ACTION_CREATE);
		created = do_xaction_namespace(NULL, ACTION_CREATE);
	}

	if (created < 0 || (!namespace && created < 1)) {
		fprintf(stderr, "failed to %s namespace\n", namespace
				? "reconfigure" : "create");
		if (!namespace)
			created = -ENODEV;
	}

	if (created < 0)
		return created;
	return 0;
}

int cmd_destroy_namespace(int argc , const char **argv)
{
	char *xable_usage = "ndctl destroy-namespace <namespace> [<options>]";
	const char *namespace = parse_namespace_options(argc, argv,
			ACTION_DESTROY, destroy_options, xable_usage);
	int destroyed = do_xaction_namespace(namespace, ACTION_DESTROY);

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
