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

#ifdef HAVE_NDCTL_H
#include <linux/ndctl.h>
#else
#include <ndctl.h>
#endif

static struct {
	bool buses;
	bool dimms;
	bool regions;
	bool namespaces;
	bool idle;
	bool health;
	bool dax;
	bool media_errors;
} list;

static struct {
	const char *bus;
	const char *region;
	const char *type;
	const char *dimm;
	const char *mode;
	const char *namespace;
} param;

static int did_fail;
static int jflag = JSON_C_TO_STRING_PRETTY;

#define fail(fmt, ...) \
do { \
	did_fail = 1; \
	fprintf(stderr, "ndctl-%s:%s:%d: " fmt, \
			VERSION, __func__, __LINE__, ##__VA_ARGS__); \
} while (0)

static enum ndctl_namespace_mode mode_to_type(const char *mode)
{
	if (!mode)
		return -ENXIO;

	if (strcasecmp(param.mode, "memory") == 0)
		return NDCTL_NS_MODE_MEMORY;
	else if (strcasecmp(param.mode, "sector") == 0)
		return NDCTL_NS_MODE_SAFE;
	else if (strcasecmp(param.mode, "safe") == 0)
		return NDCTL_NS_MODE_SAFE;
	else if (strcasecmp(param.mode, "dax") == 0)
		return NDCTL_NS_MODE_DAX;
	else if (strcasecmp(param.mode, "raw") == 0)
		return NDCTL_NS_MODE_RAW;

	return NDCTL_NS_MODE_UNKNOWN;
}

static struct json_object *list_namespaces(struct ndctl_region *region,
		struct json_object *container, struct json_object *jnamespaces,
		bool continue_array)
{
	struct ndctl_namespace *ndns;

	ndctl_namespace_foreach(region, ndns) {
		enum ndctl_namespace_mode mode = ndctl_namespace_get_mode(ndns);
		struct json_object *jndns;

		/* are we emitting namespaces? */
		if (!list.namespaces)
			break;

		if (!util_namespace_filter(ndns, param.namespace))
			continue;

		if (param.mode && mode_to_type(param.mode) != mode)
			continue;

		if (!list.idle && !ndctl_namespace_is_active(ndns))
			continue;

		if (!jnamespaces) {
			jnamespaces = json_object_new_array();
			if (!jnamespaces) {
				fail("\n");
				continue;
			}

			if (container)
				json_object_object_add(container, "namespaces",
						jnamespaces);
		}

		jndns = util_namespace_to_json(ndns, list.idle, list.dax,
				list.media_errors);
		if (!jndns) {
			fail("\n");
			continue;
		}

		json_object_array_add(jnamespaces, jndns);
	}

	/*
	 * We we are collecting namespaces anonymously across the
	 * platform / bus
	 */
	if (continue_array)
		return jnamespaces;
	return NULL;
}

static struct json_object *region_to_json(struct ndctl_region *region,
		bool include_media_errors)
{
	struct json_object *jregion = json_object_new_object();
	struct json_object *jobj, *jbbs, *jmappings = NULL;
	struct ndctl_interleave_set *iset;
	struct ndctl_mapping *mapping;
	unsigned int bb_count;

	if (!jregion)
		return NULL;

	jobj = json_object_new_string(ndctl_region_get_devname(region));
	if (!jobj)
		goto err;
	json_object_object_add(jregion, "dev", jobj);

	jobj = json_object_new_int64(ndctl_region_get_size(region));
	if (!jobj)
		goto err;
	json_object_object_add(jregion, "size", jobj);

	jobj = json_object_new_int64(ndctl_region_get_available_size(region));
	if (!jobj)
		goto err;
	json_object_object_add(jregion, "available_size", jobj);

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

	iset = ndctl_region_get_interleave_set(region);
	if (iset) {
		jobj = json_object_new_int64(
				ndctl_interleave_set_get_cookie(iset));
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

		if (!list.idle && !ndctl_dimm_is_enabled(dimm))
			continue;

		if (!jmappings) {
			jmappings = json_object_new_array();
			if (!jmappings) {
				fail("\n");
				continue;
			}
			json_object_object_add(jregion, "mappings", jmappings);
		}

		jmapping = util_mapping_to_json(mapping);
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

	jbbs = util_region_badblocks_to_json(region, include_media_errors,
			&bb_count);
	if (bb_count) {
		jobj = json_object_new_int(bb_count);
		if (!jobj) {
			json_object_put(jbbs);
			goto err;
		}
		json_object_object_add(jregion, "badblock_count", jobj);
	}
	if (include_media_errors && jbbs)
		json_object_object_add(jregion, "badblocks", jbbs);

	list_namespaces(region, jregion, NULL, false);
	return jregion;
 err:
	fail("\n");
	json_object_put(jregion);
	return NULL;
}

static int num_list_flags(void)
{
	return list.buses + list.dimms + list.regions + list.namespaces;
}

int cmd_list(int argc, const char **argv, void *ctx)
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
		OPT_BOOLEAN('B', "buses", &list.buses, "include bus info"),
		OPT_BOOLEAN('D', "dimms", &list.dimms, "include dimm info"),
		OPT_BOOLEAN('H', "health", &list.health, "include dimm health"),
		OPT_BOOLEAN('R', "regions", &list.regions,
				"include region info"),
		OPT_BOOLEAN('N', "namespaces", &list.namespaces,
				"include namespace info (default)"),
		OPT_BOOLEAN('X', "device-dax", &list.dax,
				"include device-dax info"),
		OPT_BOOLEAN('i', "idle", &list.idle, "include idle devices"),
		OPT_BOOLEAN('M', "media-errors", &list.media_errors,
				"include media errors"),
		OPT_END(),
	};
	const char * const u[] = {
		"ndctl list [<options>]",
		NULL
	};
	struct json_object *jnamespaces = NULL;
	struct json_object *jregions = NULL;
	struct json_object *jdimms = NULL;
	struct json_object *jbuses = NULL;
	struct ndctl_bus *bus;
	unsigned int type = 0;
	int i;

        argc = parse_options(argc, argv, options, u, 0);
	for (i = 0; i < argc; i++)
		error("unknown parameter \"%s\"\n", argv[i]);
	if (param.type && (strcmp(param.type, "pmem") != 0
				&& strcmp(param.type, "blk") != 0)) {
		error("unknown type \"%s\" must be \"pmem\" or \"blk\"\n",
				param.type);
		argc++;
	}

	if (argc)
		usage_with_options(u, options);

	if (num_list_flags() == 0) {
		list.buses = !!param.bus;
		list.regions = !!param.region;
		list.dimms = !!param.dimm;
		if (list.dax && !param.mode)
			param.mode = "dax";
	}

	if (num_list_flags() == 0)
		list.namespaces = true;

	if (param.type) {
		if (strcmp(param.type, "pmem") == 0)
			type = ND_DEVICE_REGION_PMEM;
		else
			type = ND_DEVICE_REGION_BLK;
	}

	if (mode_to_type(param.mode) == NDCTL_NS_MODE_UNKNOWN) {
		error("invalid mode: '%s'\n", param.mode);
		return -EINVAL;
	}

	ndctl_bus_foreach(ctx, bus) {
		struct json_object *jbus = NULL;
		struct ndctl_region *region;
		struct ndctl_dimm *dimm;

		if (!util_bus_filter(bus, param.bus)
				|| !util_bus_filter_by_dimm(bus, param.dimm))
			continue;

		if (list.buses) {
			if (!jbuses) {
				jbuses = json_object_new_array();
				if (!jbuses) {
					fail("\n");
					continue;
				}
			}

			jbus = util_bus_to_json(bus);
			if (!jbus) {
				fail("\n");
				continue;
			}
			json_object_array_add(jbuses, jbus);
		}

		ndctl_dimm_foreach(bus, dimm) {
			struct json_object *jdimm;

			/* are we emitting dimms? */
			if (!list.dimms)
				break;

			if (!util_dimm_filter(dimm, param.dimm))
				continue;

			if (!list.idle && !ndctl_dimm_is_enabled(dimm))
				continue;

			if (!jdimms) {
				jdimms = json_object_new_array();
				if (!jdimms) {
					fail("\n");
					continue;
				}

				if (jbus)
					json_object_object_add(jbus, "dimms", jdimms);
			}

			jdimm = util_dimm_to_json(dimm);
			if (!jdimm) {
				fail("\n");
				continue;
			}

			if (list.health) {
				struct json_object *jhealth;

				jhealth = util_dimm_health_to_json(dimm);
				if (jhealth)
					json_object_object_add(jdimm, "health",
							jhealth);
				else if (ndctl_dimm_is_cmd_supported(dimm,
							ND_CMD_SMART)) {
					/*
					 * Failed to retrieve health data from
					 * a dimm that otherwise supports smart
					 * data retrieval commands.
					 */
					fail("\n");
					continue;
				}
			}

			/*
			 * Without a bus we are collecting dimms anonymously
			 * across the platform.
			 */
			json_object_array_add(jdimms, jdimm);
		}

		ndctl_region_foreach(bus, region) {
			struct json_object *jregion;

			if (!util_region_filter(region, param.region)
					|| !util_region_filter_by_dimm(region,
						param.dimm))
				continue;

			if (type && ndctl_region_get_type(region) != type)
				continue;

			if (!list.regions) {
				jnamespaces = list_namespaces(region, jbus,
						jnamespaces, true);
				continue;
			}

			if (!list.idle && !ndctl_region_is_enabled(region))
				continue;

			if (!jregions) {
				jregions = json_object_new_array();
				if (!jregions) {
					fail("\n");
					continue;
				}

				if (jbus)
					json_object_object_add(jbus, "regions",
							jregions);
			}

			jregion = region_to_json(region, list.media_errors);
			if (!jregion) {
				fail("\n");
				continue;
			}

			/*
			 * Without a bus we are collecting regions anonymously
			 * across the platform.
			 */
			json_object_array_add(jregions, jregion);
		}

		if (jbuses) {
			jdimms = NULL;
			jregions = NULL;
			jnamespaces = NULL;
		}
	}

	if (jbuses)
		util_display_json_array(stdout, jbuses, jflag);
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
		if (jnamespaces)
			json_object_object_add(jplatform, "namespaces",
					jnamespaces);
		printf("%s\n", json_object_to_json_string_ext(jplatform,
					jflag));
		json_object_put(jplatform);
	} else if (jdimms)
		util_display_json_array(stdout, jdimms, jflag);
	else if (jregions)
		util_display_json_array(stdout, jregions, jflag);
	else if (jnamespaces)
		util_display_json_array(stdout, jnamespaces, jflag);

	if (did_fail)
		return -ENOMEM;
	return 0;
}
