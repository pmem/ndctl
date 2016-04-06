#ifndef __NDCTL_JSON_H__
#define __NDCTL_JSON_H__
#include <stdbool.h>
#include <ndctl/libndctl.h>

bool util_namespace_active(struct ndctl_namespace *ndns);
struct json_object *util_bus_to_json(struct ndctl_bus *bus);
struct json_object *util_dimm_to_json(struct ndctl_dimm *dimm);
struct json_object *util_mapping_to_json(struct ndctl_mapping *mapping);
struct json_object *util_namespace_to_json(struct ndctl_namespace *ndns);
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
