/*
 * libndctl: helper library for the nd (nvdimm, nfit-defined, persistent
 *           memory, ...) sub-system.
 *
 * Copyright (c) 2014, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU Lesser General Public License,
 * version 2.1, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
 */
#ifndef _LIBNDCTL_H_
#define _LIBNDCTL_H_

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * ndctl_ctx
 *
 * library user context - reads the config and system
 * environment, user variables, allows custom logging
 */
struct ndctl_ctx;
struct ndctl_ctx *ndctl_ref(struct ndctl_ctx *ctx);
struct ndctl_ctx *ndctl_unref(struct ndctl_ctx *ctx);
int ndctl_new(struct ndctl_ctx **ctx);
void ndctl_set_log_fn(struct ndctl_ctx *ctx,
                  void (*log_fn)(struct ndctl_ctx *ctx,
                                 int priority, const char *file, int line, const char *fn,
                                 const char *format, va_list args));
int ndctl_get_log_priority(struct ndctl_ctx *ctx);
void ndctl_set_log_priority(struct ndctl_ctx *ctx, int priority);
void *ndctl_get_userdata(struct ndctl_ctx *ctx);
void ndctl_set_userdata(struct ndctl_ctx *ctx, void *userdata);

/*
 * ndctl_bus
 *
 * Access the list of busses in the system.  Usually only one provided
 * by "ACPI.NFIT".  The nfit_test module provides one or more test
 * busses with provider names of the format "nfit_test.N"
 */
struct ndctl_bus;
struct ndctl_bus *ndctl_bus_get_first(struct ndctl_ctx *ctx);
struct ndctl_bus *ndctl_bus_get_next(struct ndctl_bus *bus);
#define ndctl_bus_foreach(ctx, bus) \
        for (bus = ndctl_bus_get_first(ctx); \
             bus != NULL; \
             bus = ndctl_bus_get_next(bus))
struct ndctl_ctx *ndctl_bus_get_ctx(struct ndctl_bus *bus);
unsigned int ndctl_bus_get_major(struct ndctl_bus *bus);
unsigned int ndctl_bus_get_minor(struct ndctl_bus *bus);
unsigned int ndctl_bus_get_format(struct ndctl_bus *bus);
unsigned int ndctl_bus_get_revision(struct ndctl_bus *bus);
unsigned int ndctl_bus_get_id(struct ndctl_bus *bus);
const char *ndctl_bus_get_provider(struct ndctl_bus *bus);

/*
 * ndctl_dimm
 *
 * access the list of dimms
 */
struct ndctl_dimm;
struct ndctl_dimm *ndctl_dimm_get_first(struct ndctl_bus *bus);
struct ndctl_dimm *ndctl_dimm_get_next(struct ndctl_dimm *dimm);
#define ndctl_dimm_foreach(bus, dimm) \
        for (dimm = ndctl_dimm_get_first(bus); \
             dimm != NULL; \
             dimm = ndctl_dimm_get_next(dimm))
unsigned int ndctl_dimm_get_handle(struct ndctl_dimm *dimm);
unsigned int ndctl_dimm_get_node(struct ndctl_dimm *dimm);
unsigned int ndctl_dimm_get_socket(struct ndctl_dimm *dimm);
unsigned int ndctl_dimm_get_imc(struct ndctl_dimm *dimm);
unsigned int ndctl_dimm_get_channel(struct ndctl_dimm *dimm);
unsigned int ndctl_dimm_get_dimm(struct ndctl_dimm *dimm);
struct ndctl_bus *ndctl_dimm_get_bus(struct ndctl_dimm *dimm);
struct ndctl_ctx *ndctl_dimm_get_ctx(struct ndctl_dimm *dimm);
struct ndctl_dimm *ndctl_dimm_get_by_handle(struct ndctl_bus *bus,
		unsigned int handle);

/*
 * ndctl_region
 *
 * access the list of regions
 */
struct ndctl_region;
struct ndctl_region *ndctl_region_get_first(struct ndctl_bus *bus);
struct ndctl_region *ndctl_region_get_next(struct ndctl_region *region);
#define ndctl_region_foreach(bus, region) \
        for (region = ndctl_region_get_first(bus); \
             region != NULL; \
             region = ndctl_region_get_next(region))
unsigned int ndctl_region_get_id(struct ndctl_region *region);
unsigned int ndctl_region_get_interleave_ways(struct ndctl_region *region);
unsigned int ndctl_region_get_mappings(struct ndctl_region *region);
unsigned long long ndctl_region_get_size(struct ndctl_region *region);
unsigned int ndctl_region_get_type(struct ndctl_region *region);
const char *ndctl_region_get_type_name(struct ndctl_region *region);
struct ndctl_bus *ndctl_region_get_bus(struct ndctl_region *region);
struct ndctl_ctx *ndctl_region_get_ctx(struct ndctl_region *region);
struct ndctl_dimm *ndctl_region_get_first_dimm(struct ndctl_region *region);
struct ndctl_dimm *ndctl_region_get_next_dimm(struct ndctl_region *region,
		struct ndctl_dimm *dimm);
int ndctl_region_wait_probe(struct ndctl_region *region);
#define ndctl_dimm_foreach_in_region(region, dimm) \
        for (dimm = ndctl_region_get_first_dimm(region); \
             dimm != NULL; \
             dimm = ndctl_region_get_next_dimm(region, dimm))

/*
 * ndctl_mapping
 *
 * access the list of mappings
 */
struct ndctl_mapping;
struct ndctl_mapping *ndctl_mapping_get_first(struct ndctl_region *region);
struct ndctl_mapping *ndctl_mapping_get_next(struct ndctl_mapping *mapping);
#define ndctl_mapping_foreach(region, mapping) \
        for (mapping = ndctl_mapping_get_first(region); \
             mapping != NULL; \
             mapping = ndctl_mapping_get_next(mapping))
struct ndctl_dimm *ndctl_mapping_get_dimm(struct ndctl_mapping *mapping);
struct ndctl_ctx *ndctl_mapping_get_ctx(struct ndctl_mapping *mapping);
struct ndctl_bus *ndctl_mapping_get_bus(struct ndctl_mapping *mapping);
struct ndctl_region *ndctl_mapping_get_region(struct ndctl_mapping *mapping);
unsigned long long ndctl_mapping_get_offset(struct ndctl_mapping *mapping);
unsigned long long ndctl_mapping_get_length(struct ndctl_mapping *mapping);

/*
 * ndctl_namespace
 *
 * access the list of namespaces
 */
struct ndctl_namespace;
struct ndctl_namespace *ndctl_namespace_get_first(struct ndctl_region *region);
struct ndctl_namespace *ndctl_namespace_get_next(struct ndctl_namespace *ndns);
#define ndctl_namespace_foreach(region, ndns) \
        for (ndns = ndctl_namespace_get_first(region); \
             ndns != NULL; \
             ndns = ndctl_namespace_get_next(ndns))
struct ndctl_ctx *ndctl_namespace_get_ctx(struct ndctl_namespace *ndns);
struct ndctl_bus *ndctl_namespace_get_bus(struct ndctl_namespace *ndns);
struct ndctl_region *ndctl_namespace_get_region(struct ndctl_namespace *ndns);
unsigned int ndctl_namespace_get_id(struct ndctl_namespace *ndns);
unsigned int ndctl_namespace_get_type(struct ndctl_namespace *ndns);
const char *ndctl_namespace_get_type_name(struct ndctl_namespace *ndns);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
