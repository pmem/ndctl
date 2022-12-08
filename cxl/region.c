// SPDX-License-Identifier: GPL-2.0
/* Copyright (C) 2020-2022 Intel Corporation. All rights reserved. */
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <util/log.h>
#include <uuid/uuid.h>
#include <util/json.h>
#include <util/size.h>
#include <cxl/libcxl.h>
#include <json-c/json.h>
#include <util/parse-options.h>
#include <ccan/minmax/minmax.h>
#include <ccan/short_types/short_types.h>

#include "filter.h"
#include "json.h"

static struct region_params {
	const char *bus;
	const char *size;
	const char *type;
	const char *root_decoder;
	const char *region;
	int ways;
	int granularity;
	bool memdevs;
	bool force;
	bool human;
	bool debug;
} param = {
	.ways = INT_MAX,
	.granularity = INT_MAX,
};

struct parsed_params {
	u64 size;
	u64 ep_min_size;
	int ways;
	int granularity;
	const char **targets;
	int num_targets;
	struct cxl_decoder *root_decoder;
	enum cxl_decoder_mode mode;
};

enum region_actions {
	ACTION_CREATE,
	ACTION_ENABLE,
	ACTION_DISABLE,
	ACTION_DESTROY,
};

static struct log_ctx rl;

#define BASE_OPTIONS() \
OPT_STRING('b', "bus", &param.bus, "bus name", \
	   "Limit operation to the specified bus"), \
OPT_STRING('d', "decoder", &param.root_decoder, "root decoder name", \
	   "Limit to / use the specified root decoder"), \
OPT_BOOLEAN(0, "debug", &param.debug, "turn on debug")

#define CREATE_OPTIONS() \
OPT_STRING('s', "size", &param.size, \
	   "size in bytes or with a K/M/G etc. suffix", \
	   "total size desired for the resulting region."), \
OPT_INTEGER('w', "ways", &param.ways, \
	    "number of memdevs participating in the regions interleave set"), \
OPT_INTEGER('g', "granularity", &param.granularity,  \
	    "granularity of the interleave set"), \
OPT_STRING('t', "type", &param.type, \
	   "region type", "region type - 'pmem' or 'ram'"), \
OPT_BOOLEAN('m', "memdevs", &param.memdevs, \
	    "non-option arguments are memdevs"), \
OPT_BOOLEAN('u', "human", &param.human, "use human friendly number formats")

static const struct option create_options[] = {
	BASE_OPTIONS(),
	CREATE_OPTIONS(),
	OPT_END(),
};

static const struct option enable_options[] = {
	BASE_OPTIONS(),
	OPT_END(),
};

static const struct option disable_options[] = {
	BASE_OPTIONS(),
	OPT_END(),
};

static const struct option destroy_options[] = {
	BASE_OPTIONS(),
	OPT_BOOLEAN('f', "force", &param.force,
		    "destroy region even if currently active"),
	OPT_END(),
};

static int parse_create_options(int argc, const char **argv,
				struct parsed_params *p)
{
	int i;

	if (!param.root_decoder) {
		log_err(&rl, "no root decoder specified\n");
		return -EINVAL;
	}

	if (param.type) {
		p->mode = cxl_decoder_mode_from_ident(param.type);
		if (p->mode == CXL_DECODER_MODE_NONE) {
			log_err(&rl, "unsupported type: %s\n", param.type);
			return -EINVAL;
		}
	} else {
		p->mode = CXL_DECODER_MODE_PMEM;
	}

	if (param.size) {
		p->size = parse_size64(param.size);
		if (p->size == ULLONG_MAX) {
			log_err(&rl, "Invalid size: %s\n", param.size);
			return -EINVAL;
		}
	}

	if (param.ways <= 0) {
		log_err(&rl, "Invalid interleave ways: %d\n", param.ways);
		return -EINVAL;
	} else if (param.ways < INT_MAX) {
		p->ways = param.ways;
	} else if (argc) {
		p->ways = argc;
	} else {
		log_err(&rl,
			"couldn't determine interleave ways from options or arguments\n");
		return -EINVAL;
	}

	if (param.granularity < INT_MAX) {
		if (param.granularity <= 0) {
			log_err(&rl, "Invalid interleave granularity: %d\n",
				param.granularity);
			return -EINVAL;
		}
		p->granularity = param.granularity;
	}

	if (argc > p->ways) {
		for (i = p->ways; i < argc; i++)
			log_err(&rl, "extra argument: %s\n", p->targets[i]);
		return -EINVAL;
	}

	if (argc < p->ways) {
		log_err(&rl,
			"too few target arguments (%d) for interleave ways (%u)\n",
			argc, p->ways);
		return -EINVAL;
	}

	if (p->size && p->ways) {
		if (p->size % p->ways) {
			log_err(&rl,
				"size (%lu) is not an integral multiple of interleave-ways (%u)\n",
				p->size, p->ways);
			return -EINVAL;
		}
	}

	/*
	 * For all practical purposes, -m is the default target type, but
	 * hold off on actively making that decision until a second target
	 * option is available.
	 */
	if (!param.memdevs) {
		log_err(&rl,
			"must specify option for target object types (-m)\n");
		return -EINVAL;
	}

	return 0;
}

static int parse_region_options(int argc, const char **argv,
				struct cxl_ctx *ctx, enum region_actions action,
				const struct option *options,
				struct parsed_params *p, const char *usage)
{
	const char * const u[] = {
		usage,
		NULL
	};

	argc = parse_options(argc, argv, options, u, 0);
	p->targets = argv;
	p->num_targets = argc;

	if (param.debug) {
		cxl_set_log_priority(ctx, LOG_DEBUG);
		rl.log_priority = LOG_DEBUG;
	} else
		rl.log_priority = LOG_INFO;

	switch(action) {
	case ACTION_CREATE:
		return parse_create_options(argc, argv, p);
	default:
		return 0;
	}
}

/**
 * validate_memdev() - match memdev with the target provided,
 *                     and determine its size contribution
 * @memdev: cxl_memdev being tested for a match against the named target
 * @target: target memdev
 * @p:      params structure
 *
 * This is called for each memdev in the system, and only returns 'true' if
 * the memdev name matches the target argument being tested. Additionally,
 * it sets an ep_min_size attribute that always contains the size of the
 * smallest target in the provided list. This is used during the automatic
 * size determination later, to ensure that all targets contribute equally
 * to the region in case of unevenly sized memdevs.
 */
static bool validate_memdev(struct cxl_memdev *memdev, const char *target,
			    struct parsed_params *p)
{
	const char *devname = cxl_memdev_get_devname(memdev);
	u64 size;

	if (strcmp(devname, target) != 0)
		return false;

	size = cxl_memdev_get_pmem_size(memdev);
	if (!p->ep_min_size)
		p->ep_min_size = size;
	else
		p->ep_min_size = min(p->ep_min_size, size);

	return true;
}

static int validate_config_memdevs(struct cxl_ctx *ctx, struct parsed_params *p)
{
	int i, matched = 0;

	for (i = 0; i < p->ways; i++) {
		struct cxl_memdev *memdev;

		cxl_memdev_foreach(ctx, memdev)
			if (validate_memdev(memdev, p->targets[i], p))
				matched++;
	}
	if (matched != p->ways) {
		log_err(&rl,
			"one or more memdevs not found in CXL topology\n");
		return -ENXIO;
	}

	return 0;
}

static int validate_decoder(struct cxl_decoder *decoder,
			    struct parsed_params *p)
{
	const char *devname = cxl_decoder_get_devname(decoder);

	switch(p->mode) {
	case CXL_DECODER_MODE_RAM:
		if (!cxl_decoder_is_volatile_capable(decoder)) {
			log_err(&rl, "%s is not volatile capable\n", devname);
			return -EINVAL;
		}
		break;
	case CXL_DECODER_MODE_PMEM:
		if (!cxl_decoder_is_pmem_capable(decoder)) {
			log_err(&rl, "%s is not pmem capable\n", devname);
			return -EINVAL;
		}
		break;
	default:
		log_err(&rl, "unknown type: %s\n", param.type);
		return -EINVAL;
	}

	/* TODO check if the interleave config is possible under this decoder */

	return 0;
}

static int create_region_validate_config(struct cxl_ctx *ctx,
					 struct parsed_params *p)
{
	struct cxl_bus *bus;
	int rc;

	cxl_bus_foreach(ctx, bus) {
		struct cxl_decoder *decoder;
		struct cxl_port *port;

		if (!util_cxl_bus_filter(bus, param.bus))
			continue;

		port = cxl_bus_get_port(bus);
		if (!cxl_port_is_root(port))
			continue;

		cxl_decoder_foreach (port, decoder) {
			if (util_cxl_decoder_filter(decoder,
						    param.root_decoder)) {
				p->root_decoder = decoder;
				goto found;
			}
		}
	}

found:
	if (p->root_decoder == NULL) {
		log_err(&rl, "%s not found in CXL topology\n",
			param.root_decoder);
		return -ENXIO;
	}

	rc = validate_decoder(p->root_decoder, p);
	if (rc)
		return rc;

	return validate_config_memdevs(ctx, p);
}

static struct cxl_decoder *
cxl_memdev_target_find_decoder(struct cxl_ctx *ctx, const char *memdev_name)
{
	struct cxl_endpoint *ep = NULL;
	struct cxl_decoder *decoder;
	struct cxl_memdev *memdev;
	struct cxl_port *port;

	cxl_memdev_foreach(ctx, memdev) {
		const char *devname = cxl_memdev_get_devname(memdev);

		if (strcmp(devname, memdev_name) != 0)
			continue;

		ep = cxl_memdev_get_endpoint(memdev);
	}

	if (!ep) {
		log_err(&rl, "could not get an endpoint for %s\n",
			memdev_name);
		return NULL;
	}

	port = cxl_endpoint_get_port(ep);
	if (!port) {
		log_err(&rl, "could not get a port for %s\n",
			memdev_name);
		return NULL;
	}

	cxl_decoder_foreach(port, decoder)
		if (cxl_decoder_get_size(decoder) == 0)
			return decoder;

	log_err(&rl, "could not get a free decoder for %s\n", memdev_name);
	return NULL;
}

#define try(prefix, op, dev, p) \
do { \
	int __rc = prefix##_##op(dev, p); \
	if (__rc) { \
		log_err(&rl, "%s: " #op " failed: %s\n", \
				prefix##_get_devname(dev), \
				strerror(abs(__rc))); \
		rc = __rc; \
		goto err_delete; \
	} \
} while (0)

static int cxl_region_determine_granularity(struct cxl_region *region,
					    struct parsed_params *p)
{
	const char *devname = cxl_region_get_devname(region);
	int granularity, ways;

	/* Default granularity will be the root decoder's granularity */
	granularity = cxl_decoder_get_interleave_granularity(p->root_decoder);
	if (granularity == 0 || granularity == -1) {
		log_err(&rl, "%s: unable to determine root decoder granularity\n",
			devname);
		return -ENXIO;
	}

	/* If no user-supplied granularity, just use the default */
	if (!p->granularity)
		return granularity;

	ways = cxl_decoder_get_interleave_ways(p->root_decoder);
	if (ways == 0 || ways == -1) {
		log_err(&rl, "%s: unable to determine root decoder ways\n",
			devname);
		return -ENXIO;
	}

	/* For ways == 1, any user-supplied granularity is fine */
	if (ways == 1)
		return p->granularity;

	/*
	 * For ways > 1, only allow the same granularity as the selected
	 * root decoder
	 */
	if (p->granularity == granularity)
		return granularity;

	log_err(&rl,
		"%s: For an x%d root, only root decoder granularity (%d) permitted\n",
		devname, ways, granularity);
	return -EINVAL;
}

static int create_region(struct cxl_ctx *ctx, int *count,
			 struct parsed_params *p)
{
	unsigned long flags = UTIL_JSON_TARGETS;
	struct json_object *jregion;
	struct cxl_region *region;
	int i, rc, granularity;
	u64 size, max_extent;
	const char *devname;
	uuid_t uuid;

	rc = create_region_validate_config(ctx, p);
	if (rc)
		return rc;

	if (p->size) {
		size = p->size;
	} else if (p->ep_min_size) {
		size = p->ep_min_size * p->ways;
	} else {
		log_err(&rl, "%s: unable to determine region size\n", __func__);
		return -ENXIO;
	}
	max_extent = cxl_decoder_get_max_available_extent(p->root_decoder);
	if (max_extent == ULLONG_MAX) {
		log_err(&rl, "%s: unable to determine max extent\n",
			cxl_decoder_get_devname(p->root_decoder));
		return -EINVAL;
	}
	if (size > max_extent) {
		log_err(&rl,
			"%s: region size %#lx exceeds max available space\n",
			cxl_decoder_get_devname(p->root_decoder), size);
		return -ENOSPC;
	}

	if (p->mode == CXL_DECODER_MODE_PMEM) {
		region = cxl_decoder_create_pmem_region(p->root_decoder);
		if (!region) {
			log_err(&rl, "failed to create region under %s\n",
				param.root_decoder);
			return -ENXIO;
		}
	} else {
		log_err(&rl, "region type '%s' not supported yet\n",
			param.type);
		return -EOPNOTSUPP;
	}

	devname = cxl_region_get_devname(region);

	rc = cxl_region_determine_granularity(region, p);
	if (rc < 0)
		goto err_delete;
	granularity = rc;

	uuid_generate(uuid);
	try(cxl_region, set_interleave_granularity, region, granularity);
	try(cxl_region, set_interleave_ways, region, p->ways);
	try(cxl_region, set_uuid, region, uuid);
	try(cxl_region, set_size, region, size);

	for (i = 0; i < p->ways; i++) {
		struct cxl_decoder *ep_decoder = NULL;

		ep_decoder = cxl_memdev_target_find_decoder(ctx, p->targets[i]);
		if (!ep_decoder) {
			rc = -ENXIO;
			goto err_delete;
		}
		if (cxl_decoder_get_mode(ep_decoder) != p->mode) {
			/*
			 * The memdev_target_find_decoder() helper returns a free
			 * decoder whose size has been checked for 0.
			 * Thus it is safe to change the mode here if needed.
			 */
			try(cxl_decoder, set_dpa_size, ep_decoder, 0);
			try(cxl_decoder, set_mode, ep_decoder, p->mode);
		}
		try(cxl_decoder, set_dpa_size, ep_decoder, size/p->ways);
		rc = cxl_region_set_target(region, i, ep_decoder);
		if (rc) {
			log_err(&rl, "%s: failed to set target%d to %s\n",
				devname, i, p->targets[i]);
			goto err_delete;
		}
	}

	rc = cxl_region_decode_commit(region);
	if (rc) {
		log_err(&rl, "%s: failed to commit decode: %s\n", devname,
			strerror(-rc));
		goto err_delete;
	}

	rc = cxl_region_enable(region);
	if (rc) {
		log_err(&rl, "%s: failed to enable: %s\n", devname,
			strerror(-rc));
		goto err_delete;
	}
	*count = 1;

	if (isatty(1))
		flags |= UTIL_JSON_HUMAN;
	jregion = util_cxl_region_to_json(region, flags);
	if (jregion)
		printf("%s\n", json_object_to_json_string_ext(jregion,
					JSON_C_TO_STRING_PRETTY));

	return 0;

err_delete:
	cxl_region_delete(region);
	return rc;
}

static int destroy_region(struct cxl_region *region)
{
	const char *devname = cxl_region_get_devname(region);
	unsigned int ways, i;
	int rc;

	/* First, unbind/disable the region if needed */
	if (cxl_region_is_enabled(region)) {
		if (param.force) {
			rc = cxl_region_disable(region);
			if (rc) {
				log_err(&rl, "%s: error disabling region: %s\n",
					devname, strerror(-rc));
				return rc;
			}
		} else {
			log_err(&rl, "%s active. Disable it or use --force\n",
				devname);
			return -EBUSY;
		}
	}

	/* Reset the region decode in preparation for removal */
	rc = cxl_region_decode_reset(region);
	if (rc) {
		log_err(&rl, "%s: failed to reset decode: %s\n", devname,
			strerror(-rc));
		return rc;
	}

	/* Reset all endpoint decoders and region targets */
	ways = cxl_region_get_interleave_ways(region);
	if (ways == 0 || ways == UINT_MAX) {
		log_err(&rl, "%s: error getting interleave ways\n", devname);
		return -ENXIO;
	}

	for (i = 0; i < ways; i++) {
		struct cxl_decoder *ep_decoder;

		ep_decoder = cxl_region_get_target_decoder(region, i);
		if (!ep_decoder)
			return -ENXIO;

		rc = cxl_region_clear_target(region, i);
		if (rc) {
			log_err(&rl, "%s: clearing target%d failed: %s\n",
				devname, i, strerror(abs(rc)));
			return rc;
		}

		rc = cxl_decoder_set_dpa_size(ep_decoder, 0);
		if (rc) {
			log_err(&rl, "%s: set_dpa_size failed: %s\n",
				cxl_decoder_get_devname(ep_decoder),
				strerror(abs(rc)));
			return rc;
		}
	}

	/* Finally, delete the region */
	return cxl_region_delete(region);
}

static int do_region_xable(struct cxl_region *region, enum region_actions action)
{
	switch (action) {
	case ACTION_ENABLE:
		return cxl_region_enable(region);
	case ACTION_DISABLE:
		return cxl_region_disable(region);
	case ACTION_DESTROY:
		return destroy_region(region);
	default:
		return -EINVAL;
	}
}

static int decoder_region_action(struct parsed_params *p,
				 struct cxl_decoder *decoder,
				 enum region_actions action, int *count)
{
	struct cxl_region *region, *_r;
	int rc = 0, err_rc = 0;

	cxl_region_foreach_safe (decoder, region, _r) {
		int i, match = 0;

		for (i = 0; i < p->num_targets; i++) {
			if (util_cxl_region_filter(region, p->targets[i])) {
				match = 1;
				break;
			}
		}
		if (!match)
			continue;

		rc = do_region_xable(region, action);
		if (rc == 0) {
			*count += 1;
		} else {
			log_err(&rl, "%s: failed: %s\n",
				cxl_region_get_devname(region), strerror(-rc));
			err_rc = rc;
		}
	}
	return err_rc ? err_rc : rc;
}

static int region_action(int argc, const char **argv, struct cxl_ctx *ctx,
			 enum region_actions action,
			 const struct option *options, struct parsed_params *p,
			 int *count, const char *u)
{
	int rc = 0, err_rc = 0;
	struct cxl_bus *bus;

	log_init(&rl, "cxl region", "CXL_REGION_LOG");
	rc = parse_region_options(argc, argv, ctx, action, options, p, u);
	if (rc)
		return rc;

	if (action == ACTION_CREATE)
		return create_region(ctx, count, p);

	cxl_bus_foreach(ctx, bus) {
		struct cxl_decoder *decoder;
		struct cxl_port *port;

		if (!util_cxl_bus_filter(bus, param.bus))
			continue;

		port = cxl_bus_get_port(bus);
		if (!cxl_port_is_root(port))
			continue;

		cxl_decoder_foreach (port, decoder) {
			if (!util_cxl_decoder_filter(decoder,
						     param.root_decoder))
				continue;
			rc = decoder_region_action(p, decoder, action, count);
			if (rc)
				err_rc = rc;
		}
	}

	if (err_rc) {
		log_err(&rl, "one or more failures, last failure: %s\n",
			strerror(-err_rc));
		return err_rc;
	}
	return rc;
}

int cmd_create_region(int argc, const char **argv, struct cxl_ctx *ctx)
{
	const char *u = "cxl create-region <target0> ... [<options>]";
	struct parsed_params p = { 0 };
	int rc, count = 0;

	rc = region_action(argc, argv, ctx, ACTION_CREATE, create_options, &p,
			   &count, u);
	log_info(&rl, "created %d region%s\n", count, count == 1 ? "" : "s");
	return rc == 0 ? 0 : EXIT_FAILURE;
}

int cmd_enable_region(int argc, const char **argv, struct cxl_ctx *ctx)
{
	const char *u = "cxl enable-region <region0> ... [<options>]";
	struct parsed_params p = { 0 };
	int rc, count = 0;

	rc = region_action(argc, argv, ctx, ACTION_ENABLE, enable_options, &p,
			   &count, u);
	log_info(&rl, "enabled %d region%s\n", count, count == 1 ? "" : "s");
	return rc == 0 ? 0 : EXIT_FAILURE;
}

int cmd_disable_region(int argc, const char **argv, struct cxl_ctx *ctx)
{
	const char *u = "cxl disable-region <region0> ... [<options>]";
	struct parsed_params p = { 0 };
	int rc, count = 0;

	rc = region_action(argc, argv, ctx, ACTION_DISABLE, disable_options, &p,
			   &count, u);
	log_info(&rl, "disabled %d region%s\n", count, count == 1 ? "" : "s");
	return rc == 0 ? 0 : EXIT_FAILURE;
}

int cmd_destroy_region(int argc, const char **argv, struct cxl_ctx *ctx)
{
	const char *u = "cxl destroy-region <region0> ... [<options>]";
	struct parsed_params p = { 0 };
	int rc, count = 0;

	rc = region_action(argc, argv, ctx, ACTION_DESTROY, destroy_options, &p,
			   &count, u);
	log_info(&rl, "destroyed %d region%s\n", count, count == 1 ? "" : "s");
	return rc == 0 ? 0 : EXIT_FAILURE;
}
