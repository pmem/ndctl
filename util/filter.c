#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <util/filter.h>
#include <ndctl/libndctl.h>

struct ndctl_bus *util_bus_filter(struct ndctl_bus *bus, const char *ident)
{
	char *end = NULL;
	const char *provider;
	unsigned long bus_id, id;

	if (!ident || strcmp(ident, "all") == 0)
		return bus;

	bus_id = strtoul(ident, &end, 0);
	if (end == ident || end[0])
		bus_id = ULONG_MAX;

	provider = ndctl_bus_get_provider(bus);
	id = ndctl_bus_get_id(bus);

	if (bus_id < ULONG_MAX && bus_id == id)
		return bus;

	if (bus_id == ULONG_MAX && strcmp(ident, provider) == 0)
		return bus;

	return NULL;
}

struct ndctl_region *util_region_filter(struct ndctl_region *region,
		const char *ident)
{
	char *end = NULL;
	const char *name;
	unsigned long region_id, id;

	if (!ident || strcmp(ident, "all") == 0)
		return region;

	region_id = strtoul(ident, &end, 0);
	if (end == ident || end[0])
		region_id = ULONG_MAX;

	name = ndctl_region_get_devname(region);
	id = ndctl_region_get_id(region);

	if (region_id < ULONG_MAX && region_id == id)
		return region;

	if (region_id == ULONG_MAX && strcmp(ident, name) == 0)
		return region;

	return NULL;
}

struct ndctl_namespace *util_namespace_filter(struct ndctl_namespace *ndns,
		const char *ident)
{
	struct ndctl_region *region = ndctl_namespace_get_region(ndns);
	unsigned long region_id, ndns_id;

	if (!ident || strcmp(ident, "all") == 0)
		return ndns;

	if (strcmp(ident, ndctl_namespace_get_devname(ndns)) == 0)
		return ndns;

	if (sscanf(ident, "%ld.%ld", &region_id, &ndns_id) == 2
			&& ndctl_region_get_id(region) == region_id
			&& ndctl_namespace_get_id(ndns) == ndns_id)
		return ndns;

	return NULL;
}

struct ndctl_dimm *util_dimm_filter(struct ndctl_dimm *dimm, const char *ident)
{
	char *end = NULL;
	const char *name;
	unsigned long dimm_id, id;

	if (!ident || strcmp(ident, "all") == 0)
		return dimm;

	dimm_id = strtoul(ident, &end, 0);
	if (end == ident || end[0])
		dimm_id = ULONG_MAX;

	name = ndctl_dimm_get_devname(dimm);
	id = ndctl_dimm_get_id(dimm);

	if (dimm_id < ULONG_MAX && dimm_id == id)
		return dimm;

	if (dimm_id == ULONG_MAX && strcmp(ident, name) == 0)
		return dimm;

	return NULL;
}

struct ndctl_bus *util_bus_filter_by_dimm(struct ndctl_bus *bus,
		const char *ident)
{
	char *end = NULL;
	const char *name;
	struct ndctl_dimm *dimm;
	unsigned long dimm_id, id;

	if (!ident || strcmp(ident, "all") == 0)
		return bus;

	dimm_id = strtoul(ident, &end, 0);
	if (end == ident || end[0])
		dimm_id = ULONG_MAX;

	ndctl_dimm_foreach(bus, dimm) {
		id = ndctl_dimm_get_id(dimm);
		name = ndctl_dimm_get_devname(dimm);

		if (dimm_id < ULONG_MAX && dimm_id == id)
			return bus;

		if (dimm_id == ULONG_MAX && strcmp(ident, name) == 0)
			return bus;
	}

	return NULL;
}

struct ndctl_region *util_region_filter_by_dimm(struct ndctl_region *region,
		const char *ident)
{
	char *end = NULL;
	const char *name;
	struct ndctl_dimm *dimm;
	unsigned long dimm_id, id;

	if (!ident || strcmp(ident, "all") == 0)
		return region;

	dimm_id = strtoul(ident, &end, 0);
	if (end == ident || end[0])
		dimm_id = ULONG_MAX;

	ndctl_dimm_foreach_in_region(region, dimm) {
		id = ndctl_dimm_get_id(dimm);
		name = ndctl_dimm_get_devname(dimm);

		if (dimm_id < ULONG_MAX && dimm_id == id)
			return region;

		if (dimm_id == ULONG_MAX && strcmp(ident, name) == 0)
			return region;
	}

	return NULL;
}
