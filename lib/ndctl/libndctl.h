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

#ifdef HAVE_LIBUUID
#include <uuid/uuid.h>
#else
typedef unsigned char uuid_t[16];
#endif

/*
 *          "nd/ndctl" device/object hierarchy and kernel modules
 *
 * +-----------+-----------+-----------+------------------+-----------+
 * | DEVICE    |    BUS    |  REGION   |    NAMESPACE     |   BLOCK   |
 * | CLASSES:  | PROVIDERS |  DEVICES  |     DEVICES      |  DEVICES  |
 * +-----------+-----------+-----------+------------------+-----------+
 * | MODULES:  |  nd_core  |  nd_core  |    nd_region     |  nd_pmem  |
 * |           |  nd_acpi  | nd_region |                  |  nd_blk   |
 * |           | nfit_test |           |                  |    btt    |
 * +-----------v-----------v-----------v------------------v-----------v
 *               +-----+
 *               | CTX |
 *               +--+--+    +---------+   +--------------+   +-------+
 *                  |     +-> REGION0 +---> NAMESPACE0.0 +---> PMEM3 |
 * +-------+     +--+---+ | +---------+   +--------------+   +-------+
 * | DIMM0 <-----+ BUS0 +---> REGION1 +---> NAMESPACE1.0 +---> PMEM2 |
 * +-------+     +--+---+ | +---------+   +--------------+   +-------+
 *                  |     +-> REGION2 +---> NAMESPACE2.0 +---> PMEM1 |
 *                  |       +---------+   + ------------ +   +-------+
 *                  |
 * +-------+        |       +---------+   +--------------+   +-------+
 * | DIMM1 <---+ +--+---+ +-> REGION3 +---> NAMESPACE3.0 +---> PMEM0 |
 * +-------+   +-+ BUS1 +-+ +---------+   +--------------+   +-------+
 * | DIMM2 <---+ +--+---+ +-> REGION4 +---> NAMESPACE4.0 +--->  ND0  |
 * +-------+        |       + ------- +   +--------------+   +-------+
 *                  |
 * +-------+        |                     +--------------+   +-------+
 * | DIMM3 <---+    |                   +-> NAMESPACE5.0 +--->  ND2  |
 * +-------+   | +--+---+   +---------+ | +--------------+   +---------------+
 * | DIMM4 <-----+ BUS2 +---> REGION5 +---> NAMESPACE5.1 +--->  BTT1 |  ND1  |
 * +-------+   | +------+   +---------+ | +--------------+   +---------------+
 * | DIMM5 <---+                        +-> NAMESPACE5.2 +--->  BTT0 |  ND0  |
 * +-------+                              +--------------+   +-------+-------+
 *
 * Notes:
 * 1/ The object ids are not guaranteed to be stable from boot to boot
 * 2/ While regions and busses are numbered in sequential/bus-discovery
 *    order, the resulting block devices may appear to have random ids.
 *    Use static attributes of the devices/device-path to generate a
 *    persistent name.
 */

#ifdef __cplusplus
extern "C" {
#endif

size_t ndctl_min_namespace_size(void);
size_t ndctl_sizeof_namespace_index(void);
size_t ndctl_sizeof_namespace_label(void);

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
void ndctl_set_userdata(struct ndctl_ctx *ctx, void *userdata);
void *ndctl_get_userdata(struct ndctl_ctx *ctx);

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
unsigned short ndctl_bus_get_format(struct ndctl_bus *bus);
const char *ndctl_bus_get_cmd_name(struct ndctl_bus *bus, int cmd);
int ndctl_bus_is_cmd_supported(struct ndctl_bus *bus, int cmd);
unsigned int ndctl_bus_get_revision(struct ndctl_bus *bus);
unsigned int ndctl_bus_get_id(struct ndctl_bus *bus);
const char *ndctl_bus_get_provider(struct ndctl_bus *bus);
int ndctl_bus_wait_probe(struct ndctl_bus *bus);

struct ndctl_dimm;
struct ndctl_dimm *ndctl_dimm_get_first(struct ndctl_bus *bus);
struct ndctl_dimm *ndctl_dimm_get_next(struct ndctl_dimm *dimm);
#define ndctl_dimm_foreach(bus, dimm) \
        for (dimm = ndctl_dimm_get_first(bus); \
             dimm != NULL; \
             dimm = ndctl_dimm_get_next(dimm))
unsigned int ndctl_dimm_get_handle(struct ndctl_dimm *dimm);
unsigned short ndctl_dimm_get_phys_id(struct ndctl_dimm *dimm);
unsigned short ndctl_dimm_get_vendor(struct ndctl_dimm *dimm);
unsigned short ndctl_dimm_get_device(struct ndctl_dimm *dimm);
unsigned short ndctl_dimm_get_revision(struct ndctl_dimm *dimm);
unsigned short ndctl_dimm_get_format(struct ndctl_dimm *dimm);
unsigned int ndctl_dimm_handle_get_node(struct ndctl_dimm *dimm);
unsigned int ndctl_dimm_handle_get_socket(struct ndctl_dimm *dimm);
unsigned int ndctl_dimm_handle_get_imc(struct ndctl_dimm *dimm);
unsigned int ndctl_dimm_handle_get_channel(struct ndctl_dimm *dimm);
unsigned int ndctl_dimm_handle_get_dimm(struct ndctl_dimm *dimm);
struct ndctl_bus *ndctl_dimm_get_bus(struct ndctl_dimm *dimm);
struct ndctl_ctx *ndctl_dimm_get_ctx(struct ndctl_dimm *dimm);
struct ndctl_dimm *ndctl_dimm_get_by_handle(struct ndctl_bus *bus,
		unsigned int handle);

struct ndctl_cmd;
struct ndctl_cmd *ndctl_dimm_cmd_new_cfg_size(struct ndctl_dimm *dimm);
struct ndctl_cmd *ndctl_dimm_cmd_new_cfg_read(struct ndctl_cmd *cfg_size);
struct ndctl_cmd *ndctl_dimm_cmd_new_cfg_write(struct ndctl_cmd *cfg_read);
unsigned int ndctl_cmd_cfg_size_get_size(struct ndctl_cmd *cfg_size);
ssize_t ndctl_cmd_cfg_read_get_data(struct ndctl_cmd *cfg_read, void *buf,
		unsigned int len);
ssize_t ndctl_cmd_cfg_write_set_data(struct ndctl_cmd *cfg_write, void *buf,
		unsigned int len);
void ndctl_cmd_unref(struct ndctl_cmd *cmd);
void ndctl_cmd_ref(struct ndctl_cmd *cmd);
int ndctl_cmd_get_type(struct ndctl_cmd *cmd);
int ndctl_cmd_get_status(struct ndctl_cmd *cmd);
int ndctl_cmd_submit(struct ndctl_cmd *cmd);

struct ndctl_region;
struct ndctl_region *ndctl_region_get_first(struct ndctl_bus *bus);
struct ndctl_region *ndctl_region_get_next(struct ndctl_region *region);
#define ndctl_region_foreach(bus, region) \
        for (region = ndctl_region_get_first(bus); \
             region != NULL; \
             region = ndctl_region_get_next(region))
unsigned int ndctl_region_get_id(struct ndctl_region *region);
const char *ndctl_region_get_devname(struct ndctl_region *region);
unsigned int ndctl_region_get_interleave_ways(struct ndctl_region *region);
unsigned int ndctl_region_get_mappings(struct ndctl_region *region);
unsigned long long ndctl_region_get_size(struct ndctl_region *region);
unsigned long long ndctl_region_get_available_size(struct ndctl_region *region);
unsigned int ndctl_region_get_spa_index(struct ndctl_region *region);
unsigned int ndctl_region_get_type(struct ndctl_region *region);
unsigned int ndctl_region_get_nstype(struct ndctl_region *region);
const char *ndctl_region_get_type_name(struct ndctl_region *region);
struct ndctl_bus *ndctl_region_get_bus(struct ndctl_region *region);
struct ndctl_ctx *ndctl_region_get_ctx(struct ndctl_region *region);
struct ndctl_dimm *ndctl_region_get_first_dimm(struct ndctl_region *region);
struct ndctl_dimm *ndctl_region_get_next_dimm(struct ndctl_region *region,
		struct ndctl_dimm *dimm);
#define ndctl_dimm_foreach_in_region(region, dimm) \
        for (dimm = ndctl_region_get_first_dimm(region); \
             dimm != NULL; \
             dimm = ndctl_region_get_next_dimm(region, dimm))
int ndctl_region_is_enabled(struct ndctl_region *region);
int ndctl_region_enable(struct ndctl_region *region);
int ndctl_region_disable(struct ndctl_region *region, int cleanup);
void ndctl_region_cleanup(struct ndctl_region *region);

struct ndctl_interleave_set;
struct ndctl_interleave_set *ndctl_region_get_interleave_set(
		struct ndctl_region *region);
struct ndctl_interleave_set *ndctl_interleave_set_get_first(
		struct ndctl_bus *bus);
struct ndctl_interleave_set *ndctl_interleave_set_get_next(
		struct ndctl_interleave_set *iset);
#define ndctl_interleave_set_foreach(bus, iset) \
        for (iset = ndctl_interleave_set_get_first(bus); \
             iset != NULL; \
             iset = ndctl_interleave_set_get_next(iset))
#define ndctl_dimm_foreach_in_interleave_set(iset, dimm) \
        for (dimm = ndctl_interleave_set_get_first_dimm(iset); \
             dimm != NULL; \
             dimm = ndctl_interleave_set_get_next_dimm(iset, dimm))
int ndctl_interleave_set_is_active(struct ndctl_interleave_set *iset);
unsigned long long ndctl_interleave_set_get_cookie(
		struct ndctl_interleave_set *iset);
struct ndctl_region *ndctl_interleave_set_get_region(
		struct ndctl_interleave_set *iset);
struct ndctl_dimm *ndctl_interleave_set_get_first_dimm(
	struct ndctl_interleave_set *iset);
struct ndctl_dimm *ndctl_interleave_set_get_next_dimm(
	struct ndctl_interleave_set *iset, struct ndctl_dimm *dimm);

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
const char *ndctl_namespace_get_devname(struct ndctl_namespace *ndns);
unsigned int ndctl_namespace_get_type(struct ndctl_namespace *ndns);
const char *ndctl_namespace_get_type_name(struct ndctl_namespace *ndns);
const char *ndctl_namespace_get_block_device(struct ndctl_namespace *ndns);
int ndctl_namespace_is_enabled(struct ndctl_namespace *ndns);
int ndctl_namespace_enable(struct ndctl_namespace *ndns);
int ndctl_namespace_disable(struct ndctl_namespace *ndns);
int ndctl_namespace_is_valid(struct ndctl_namespace *ndns);
int ndctl_namespace_is_configured(struct ndctl_namespace *ndns);
int ndctl_namespace_set_uuid(struct ndctl_namespace *ndns, uuid_t uu);
void ndctl_namespace_get_uuid(struct ndctl_namespace *ndns, uuid_t uu);
const char *ndctl_namespace_get_alt_name(struct ndctl_namespace *ndns);
int ndctl_namespace_set_alt_name(struct ndctl_namespace *ndns,
		const char *alt_name);
unsigned long long ndctl_namespace_get_size(struct ndctl_namespace *ndns);
int ndctl_namespace_set_size(struct ndctl_namespace *ndns,
		unsigned long long size);

struct ndctl_btt;
struct ndctl_btt *ndctl_btt_get_first(struct ndctl_bus *bus);
struct ndctl_btt *ndctl_btt_get_next(struct ndctl_btt *btt);
#define ndctl_btt_foreach(bus, btt) \
        for (btt = ndctl_btt_get_first(bus); \
             btt != NULL; \
             btt = ndctl_btt_get_next(btt))
struct ndctl_ctx *ndctl_btt_get_ctx(struct ndctl_btt *btt);
struct ndctl_bus *ndctl_btt_get_bus(struct ndctl_btt *btt);
unsigned int ndctl_btt_get_id(struct ndctl_btt *btt);
unsigned int ndctl_btt_get_supported_sector_size(struct ndctl_btt *btt, int i);
unsigned int ndctl_btt_get_sector_size(struct ndctl_btt *btt);
int ndctl_btt_get_num_sector_sizes(struct ndctl_btt *btt);
const char *ndctl_btt_get_backing_dev(struct ndctl_btt *btt);
void ndctl_btt_get_uuid(struct ndctl_btt *btt, uuid_t uu);
int ndctl_btt_is_enabled(struct ndctl_btt *btt);
const char *ndctl_btt_get_devname(struct ndctl_btt *btt);
const char *ndctl_btt_get_block_device(struct ndctl_btt *btt);
int ndctl_btt_set_uuid(struct ndctl_btt *btt, uuid_t uu);
int ndctl_btt_set_sector_size(struct ndctl_btt *btt, unsigned int sector_size);
int ndctl_btt_set_backing_dev(struct ndctl_btt *btt, const char *backing_dev);
int ndctl_btt_enable(struct ndctl_btt *btt);
int ndctl_btt_delete(struct ndctl_btt *btt);
int ndctl_btt_is_configured(struct ndctl_btt *btt);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
