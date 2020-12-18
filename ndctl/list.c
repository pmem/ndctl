// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2015-2020 Intel Corporation. All rights reserved.
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>

#include <util/json.h>
#include <util/filter.h>
#include <json-c/json.h>
#include <ndctl/libndctl.h>
#include <util/parse-options.h>
#include <ccan/array_size/array_size.h>

#include <ndctl.h>

static struct {
	bool buses;
	bool dimms;
	bool regions;
	bool namespaces;
	bool idle;
	bool health;
	bool dax;
	bool media_errors;
	bool human;
	bool firmware;
	bool capabilities;
	bool configured;
	int verbose;
} list;

static unsigned long listopts_to_flags(void)
{
	unsigned long flags = 0;

	if (list.idle)
		flags |= UTIL_JSON_IDLE;
	if (list.configured)
		flags |= UTIL_JSON_CONFIGURED;
	if (list.media_errors)
		flags |= UTIL_JSON_MEDIA_ERRORS;
	if (list.dax)
		flags |= UTIL_JSON_DAX | UTIL_JSON_DAX_DEVS;
	if (list.human)
		flags |= UTIL_JSON_HUMAN;
	if (list.verbose)
		flags |= UTIL_JSON_VERBOSE;
	if (list.capabilities)
		flags |= UTIL_JSON_CAPABILITIES;
	if (list.firmware)
		flags |= UTIL_JSON_FIRMWARE;
	return flags;
}

static struct util_filter_params param;

static int did_fail;

#define fail(fmt, ...) \
do { \
	did_fail = 1; \
	fprintf(stderr, "ndctl-%s:%s:%d: " fmt, \
			VERSION, __func__, __LINE__, ##__VA_ARGS__); \
} while (0)

static struct json_object *region_to_json(struct ndctl_region *region,
		unsigned long flags)
{
	struct json_object *jregion = json_object_new_object();
	struct json_object *jobj, *jbbs, *jmappings = NULL;
	struct ndctl_interleave_set *iset;
	struct ndctl_mapping *mapping;
	unsigned int bb_count = 0;
	unsigned long long extent, align;
	enum ndctl_persistence_domain pd;
	int numa, target;

	if (!jregion)
		return NULL;

	jobj = json_object_new_string(ndctl_region_get_devname(region));
	if (!jobj)
		goto err;
	json_object_object_add(jregion, "dev", jobj);

	jobj = util_json_object_size(ndctl_region_get_size(region), flags);
	if (!jobj)
		goto err;
	json_object_object_add(jregion, "size", jobj);

	align = ndctl_region_get_align(region);
	if (align < ULLONG_MAX) {
		jobj = util_json_object_size(align, flags);
		if (!jobj)
			goto err;
		json_object_object_add(jregion, "align", jobj);
	}

	jobj = util_json_object_size(ndctl_region_get_available_size(region),
			flags);
	if (!jobj)
		goto err;
	json_object_object_add(jregion, "available_size", jobj);

	extent = ndctl_region_get_max_available_extent(region);
	if (extent != ULLONG_MAX) {
		jobj = util_json_object_size(extent, flags);
		if (!jobj)
			goto err;
		json_object_object_add(jregion, "max_available_extent", jobj);
	}

	switch (ndctl_region_get_type(region)) {
	case ND_DEVICE_REGION_PMEM:
		jobj = json_object_new_string("pmem");
		break;
	case ND_DEVICE_REGION_BLK:
		jobj = json_object_new_string("blk");
		break;
	default:
		jobj = NULL;
	}
	if (!jobj)
		goto err;
	json_object_object_add(jregion, "type", jobj);

	numa = ndctl_region_get_numa_node(region);
	if (numa >= 0 && flags & UTIL_JSON_VERBOSE) {
		jobj = json_object_new_int(numa);
		if (jobj)
			json_object_object_add(jregion, "numa_node", jobj);
	}

	target = ndctl_region_get_target_node(region);
	if (target >= 0 && flags & UTIL_JSON_VERBOSE) {
		jobj = json_object_new_int(target);
		if (jobj)
			json_object_object_add(jregion, "target_node", jobj);
	}

	iset = ndctl_region_get_interleave_set(region);
	if (iset) {
		jobj = util_json_object_hex(
				ndctl_interleave_set_get_cookie(iset), flags);
		if (!jobj)
			fail("\n");
		else
			json_object_object_add(jregion, "iset_id", jobj);
	}

	ndctl_mapping_foreach(region, mapping) {
		struct ndctl_dimm *dimm = ndctl_mapping_get_dimm(mapping);
		struct json_object *jmapping;

		if (!list.dimms)
			break;

		if (!util_dimm_filter(dimm, param.dimm))
			continue;

		if (!list.configured && !list.idle
				&& !ndctl_dimm_is_enabled(dimm))
			continue;

		if (!jmappings) {
			jmappings = json_object_new_array();
			if (!jmappings) {
				fail("\n");
				continue;
			}
			json_object_object_add(jregion, "mappings", jmappings);
		}

		jmapping = util_mapping_to_json(mapping, listopts_to_flags());
		if (!jmapping) {
			fail("\n");
			continue;
		}
		json_object_array_add(jmappings, jmapping);
	}

	if (!ndctl_region_is_enabled(region)) {
		jobj = json_object_new_string("disabled");
		if (!jobj)
			goto err;
		json_object_object_add(jregion, "state", jobj);
	}

	jbbs = util_region_badblocks_to_json(region, &bb_count, flags);
	if (bb_count) {
		jobj = json_object_new_int(bb_count);
		if (!jobj) {
			json_object_put(jbbs);
			goto err;
		}
		json_object_object_add(jregion, "badblock_count", jobj);
	}
	if ((flags & UTIL_JSON_MEDIA_ERRORS) && jbbs)
		json_object_object_add(jregion, "badblocks", jbbs);

	if (flags & UTIL_JSON_CAPABILITIES) {
		jobj = util_region_capabilities_to_json(region);
		if (jobj)
			json_object_object_add(jregion, "capabilities", jobj);
	}

	pd = ndctl_region_get_persistence_domain(region);
	switch (pd) {
	case PERSISTENCE_CPU_CACHE:
		jobj = json_object_new_string("cpu_cache");
		break;
	case PERSISTENCE_MEM_CTRL:
		jobj = json_object_new_string("memory_controller");
		break;
	case PERSISTENCE_NONE:
		jobj = json_object_new_string("none");
		break;
	default:
		jobj = json_object_new_string("unknown");
		break;
	}

	if (jobj)
		json_object_object_add(jregion, "persistence_domain", jobj);

	return jregion;
 err:
	fail("\n");
	json_object_put(jregion);
	return NULL;
}

static void filter_namespace(struct ndctl_namespace *ndns,
		struct util_filter_ctx *ctx)
{
	struct json_object *jndns;
	struct list_filter_arg *lfa = ctx->list;
	struct json_object *container = lfa->jregion ? lfa->jregion : lfa->jbus;
	unsigned long long size = ndctl_namespace_get_size(ndns);

	if (ndctl_namespace_is_active(ndns))
		/* pass */;
	else if (list.idle)
		/* pass */;
	else if (list.configured && (size > 0 && size < ULLONG_MAX))
		/* pass */;
	else
		return;

	if (!lfa->jnamespaces) {
		lfa->jnamespaces = json_object_new_array();
		if (!lfa->jnamespaces) {
			fail("\n");
			return;
		}

		if (container)
			json_object_object_add(container, "namespaces",
					lfa->jnamespaces);
	}

	jndns = util_namespace_to_json(ndns, lfa->flags);
	if (!jndns) {
		fail("\n");
		return;
	}

	json_object_array_add(lfa->jnamespaces, jndns);
}

static bool filter_region(struct ndctl_region *region,
		struct util_filter_ctx *ctx)
{
	struct list_filter_arg *lfa = ctx->list;
	struct json_object *jbus = lfa->jbus;
	struct json_object *jregion;

	if (!list.regions)
		return true;

	if (!list.configured && !list.idle && !ndctl_region_is_enabled(region))
		return true;

	if (!lfa->jregions) {
		lfa->jregions = json_object_new_array();
		if (!lfa->jregions) {
			fail("\n");
			return false;
		}

		if (jbus)
			json_object_object_add(jbus, "regions",
					lfa->jregions);
	}

	jregion = region_to_json(region, lfa->flags);
	if (!jregion) {
		fail("\n");
		return false;
	}
	lfa->jregion = jregion;

	/*
	 * We've started a new region, any previous jnamespaces will
	 * have been parented to the last region. Clear out jnamespaces
	 * so we start a new array per region.
	 */
	lfa->jnamespaces = NULL;

	/*
	 * Without a bus we are collecting regions anonymously across
	 * the platform.
	 */
	json_object_array_add(lfa->jregions, jregion);
	return true;
}

static void filter_dimm(struct ndctl_dimm *dimm, struct util_filter_ctx *ctx)
{
	struct list_filter_arg *lfa = ctx->list;
	struct json_object *jdimm;

	if (!list.configured && !list.idle && !ndctl_dimm_is_enabled(dimm))
		return;

	if (!lfa->jdimms) {
		lfa->jdimms = json_object_new_array();
		if (!lfa->jdimms) {
			fail("\n");
			return;
		}

		if (lfa->jbus)
			json_object_object_add(lfa->jbus, "dimms", lfa->jdimms);
	}

	jdimm = util_dimm_to_json(dimm, lfa->flags);
	if (!jdimm) {
		fail("\n");
		return;
	}

	if (list.health) {
		struct json_object *jhealth;

		jhealth = util_dimm_health_to_json(dimm);
		if (jhealth)
			json_object_object_add(jdimm, "health", jhealth);
		else if (ndctl_dimm_is_cmd_supported(dimm, ND_CMD_SMART)) {
			/*
			 * Failed to retrieve health data from a dimm
			 * that otherwise supports smart data retrieval
			 * commands.
			 */
			fail("\n");
			return;
		}
	}

	/*
	 * Without a bus we are collecting dimms anonymously across the
	 * platform.
	 */
	json_object_array_add(lfa->jdimms, jdimm);
}

static bool filter_bus(struct ndctl_bus *bus, struct util_filter_ctx *ctx)
{
	struct list_filter_arg *lfa = ctx->list;

	/*
	 * These sub-objects are local to a bus and, if present, have
	 * been added as a child of a parent object on the last
	 * iteration.
	 */
	if (lfa->jbuses) {
		lfa->jdimms = NULL;
		lfa->jregion = NULL;
		lfa->jregions = NULL;
		lfa->jnamespaces = NULL;
	}

	if (!list.buses)
		return true;

	if (!lfa->jbuses) {
		lfa->jbuses = json_object_new_array();
		if (!lfa->jbuses) {
			fail("\n");
			return false;
		}
	}

	lfa->jbus = util_bus_to_json(bus, lfa->flags);
	if (!lfa->jbus) {
		fail("\n");
		return false;
	}

	json_object_array_add(lfa->jbuses, lfa->jbus);
	return true;
}

static int list_display(struct list_filter_arg *lfa)
{
	struct json_object *jnamespaces = lfa->jnamespaces;
	struct json_object *jregions = lfa->jregions;
	struct json_object *jdimms = lfa->jdimms;
	struct json_object *jbuses = lfa->jbuses;

	if (jbuses)
		util_display_json_array(stdout, jbuses, lfa->flags);
	else if ((!!jdimms + !!jregions + !!jnamespaces) > 1) {
		struct json_object *jplatform = json_object_new_object();

		if (!jplatform) {
			fail("\n");
			return -ENOMEM;
		}

		if (jdimms)
			json_object_object_add(jplatform, "dimms", jdimms);
		if (jregions)
			json_object_object_add(jplatform, "regions", jregions);
		if (jnamespaces && !jregions)
			json_object_object_add(jplatform, "namespaces",
					jnamespaces);
		printf("%s\n", json_object_to_json_string_ext(jplatform,
					JSON_C_TO_STRING_PRETTY));
		json_object_put(jplatform);
	} else if (jdimms)
		util_display_json_array(stdout, jdimms, lfa->flags);
	else if (jregions)
		util_display_json_array(stdout, jregions, lfa->flags);
	else if (jnamespaces)
		util_display_json_array(stdout, jnamespaces, lfa->flags);
	return 0;
}

static int num_list_flags(void)
{
	return list.buses + list.dimms + list.regions + list.namespaces;
}

int cmd_list(int argc, const char **argv, struct ndctl_ctx *ctx)
{
	const struct option options[] = {
		OPT_STRING('b', "bus", &param.bus, "bus-id", "filter by bus"),
		OPT_STRING('r', "region", &param.region, "region-id",
				"filter by region"),
		OPT_STRING('d', "dimm", &param.dimm, "dimm-id",
				"filter by dimm"),
		OPT_STRING('n', "namespace", &param.namespace, "namespace-id",
				"filter by namespace id"),
		OPT_STRING('m', "mode", &param.mode, "namespace-mode",
				"filter by namespace mode"),
		OPT_STRING('t', "type", &param.type, "region-type",
				"filter by region-type"),
		OPT_STRING('U', "numa-node", &param.numa_node, "numa node",
				"filter by numa node"),
		OPT_BOOLEAN('B', "buses", &list.buses, "include bus info"),
		OPT_BOOLEAN('D', "dimms", &list.dimms, "include dimm info"),
		OPT_BOOLEAN('F', "firmware", &list.firmware, "include firmware info"),
		OPT_BOOLEAN('H', "health", &list.health, "include dimm health"),
		OPT_BOOLEAN('R', "regions", &list.regions,
				"include region info"),
		OPT_BOOLEAN('N', "namespaces", &list.namespaces,
				"include namespace info (default)"),
		OPT_BOOLEAN('X', "device-dax", &list.dax,
				"include device-dax info"),
		OPT_BOOLEAN('C', "capabilities", &list.capabilities,
				"include region capability info"),
		OPT_BOOLEAN('i', "idle", &list.idle, "include idle devices"),
		OPT_BOOLEAN('c', "configured", &list.configured,
				"include configured namespaces, disabled or not"),
		OPT_BOOLEAN('M', "media-errors", &list.media_errors,
				"include media errors"),
		OPT_BOOLEAN('u', "human", &list.human,
				"use human friendly number formats "),
		OPT_INCR('v', "verbose", &list.verbose,
				"increase output detail"),
		OPT_END(),
	};
	const char * const u[] = {
		"ndctl list [<options>]",
		NULL
	};
	bool lint = !!secure_getenv("NDCTL_LIST_LINT");
	struct util_filter_ctx fctx = { 0 };
	struct list_filter_arg lfa = { 0 };
	int i, rc;

        argc = parse_options(argc, argv, options, u, 0);
	for (i = 0; i < argc; i++)
		error("unknown parameter \"%s\"\n", argv[i]);
	if (argc)
		usage_with_options(u, options);

	if (num_list_flags() == 0) {
		list.buses = !!param.bus;
		list.regions = !!param.region;
		list.dimms = !!param.dimm;
		if (list.dax && !param.mode)
			param.mode = "dax";
	}

	switch (list.verbose) {
	default:
	case 3:
		list.idle = true;
		list.firmware = true;
		list.health = true;
		list.capabilities = true;
	case 2:
		if (!lint) {
			list.dimms = true;
			list.buses = true;
			list.regions = true;
		} else if (num_list_flags() == 0) {
			list.dimms = true;
			list.buses = true;
			list.regions = true;
			list.namespaces = true;
		}
	case 1:
		list.media_errors = true;
		if (!lint)
			list.namespaces = true;
		list.dax = true;
	case 0:
		break;
	}

	if (num_list_flags() == 0)
		list.namespaces = true;

	fctx.filter_bus = filter_bus;
	fctx.filter_dimm = list.dimms ? filter_dimm : NULL;
	fctx.filter_region = filter_region;
	fctx.filter_namespace = list.namespaces ? filter_namespace : NULL;
	fctx.list = &lfa;
	lfa.flags = listopts_to_flags();

	rc = util_filter_walk(ctx, &fctx, &param);
	if (rc)
		return rc;

	if (list_display(&lfa) || did_fail)
		return -ENOMEM;
	return 0;
}
