#ifndef __NDCTL_JSON_H__
#define __NDCTL_JSON_H__
#include <stdio.h>
#include <stdbool.h>
#include <ndctl/libndctl.h>

struct json_object;
void util_display_json_array(FILE *f_out, struct json_object *jarray, int jflag);
struct json_object *util_bus_to_json(struct ndctl_bus *bus);
struct json_object *util_dimm_to_json(struct ndctl_dimm *dimm);
struct json_object *util_mapping_to_json(struct ndctl_mapping *mapping);
struct json_object *util_namespace_to_json(struct ndctl_namespace *ndns,
		bool include_idle, bool include_dax, bool include_media_errs);
struct daxctl_region;
struct daxctl_dev;
struct json_object *util_region_badblocks_to_json(struct ndctl_region *region,
		bool include_media_errors, unsigned int *bb_count);
struct json_object *util_daxctl_region_to_json(struct daxctl_region *region,
		bool include_devs, const char *ident, bool include_idle);
struct json_object *util_daxctl_dev_to_json(struct daxctl_dev *dev);
struct json_object *util_daxctl_devs_to_list(struct daxctl_region *region,
		struct json_object *jdevs, const char *ident,
		bool include_idle);
#ifdef HAVE_NDCTL_SMART
struct json_object *util_dimm_health_to_json(struct ndctl_dimm *dimm);
#else
static inline struct json_object *util_dimm_health_to_json(
		struct ndctl_dimm *dimm)
{
	return NULL;
}
#endif
#endif /* __NDCTL_JSON_H__ */
