#ifndef _NDCTL_FILTER_H_
#define _NDCTL_FILTER_H_
struct ndctl_bus *util_bus_filter(struct ndctl_bus *bus, const char *ident);
struct ndctl_region *util_region_filter(struct ndctl_region *region,
		const char *ident);
#endif
