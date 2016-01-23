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
