#ifndef _UTIL_FILTER_H_
#define _UTIL_FILTER_H_
struct ndctl_bus *util_bus_filter(struct ndctl_bus *bus, const char *ident);
struct ndctl_region *util_region_filter(struct ndctl_region *region,
		const char *ident);
struct ndctl_namespace *util_namespace_filter(struct ndctl_namespace *ndns,
		const char *ident);
struct ndctl_dimm *util_dimm_filter(struct ndctl_dimm *dimm, const char *ident);
struct ndctl_bus *util_bus_filter_by_dimm(struct ndctl_bus *bus,
		const char *ident);
struct ndctl_region *util_region_filter_by_dimm(struct ndctl_region *region,
		const char *ident);
struct daxctl_dev *util_daxctl_dev_filter(struct daxctl_dev *dev,
		const char *ident);
#endif
