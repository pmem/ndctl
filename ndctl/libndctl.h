/* SPDX-License-Identifier: LGPL-2.1 */
/* Copyright (C) 2014-2020, Intel Corporation. All rights reserved. */
#ifndef _LIBNDCTL_H_
#define _LIBNDCTL_H_

#include <stdbool.h>
#include <stdarg.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>

#ifdef HAVE_UUID
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

#define ND_EVENT_SPARES_REMAINING	(1 << 0)
#define ND_EVENT_MEDIA_TEMPERATURE	(1 << 1)
#define ND_EVENT_CTRL_TEMPERATURE	(1 << 2)
#define ND_EVENT_HEALTH_STATE		(1 << 3)
#define ND_EVENT_UNCLEAN_SHUTDOWN	(1 << 4)

size_t ndctl_min_namespace_size(void);
size_t ndctl_sizeof_namespace_index(void);
size_t ndctl_sizeof_namespace_label(void);
double ndctl_decode_smart_temperature(unsigned int temp);
unsigned int ndctl_encode_smart_temperature(double temp);

struct ndctl_ctx;
struct ndctl_ctx *ndctl_ref(struct ndctl_ctx *ctx);
struct ndctl_ctx *ndctl_unref(struct ndctl_ctx *ctx);
int ndctl_new(struct ndctl_ctx **ctx);
void ndctl_set_private_data(struct ndctl_ctx *ctx, void *data);
void *ndctl_get_private_data(struct ndctl_ctx *ctx);
struct daxctl_ctx;
struct daxctl_ctx *ndctl_get_daxctl_ctx(struct ndctl_ctx *ctx);
void ndctl_invalidate(struct ndctl_ctx *ctx);
void ndctl_set_log_fn(struct ndctl_ctx *ctx,
                  void (*log_fn)(struct ndctl_ctx *ctx,
                                 int priority, const char *file, int line, const char *fn,
                                 const char *format, va_list args));
int ndctl_get_log_priority(struct ndctl_ctx *ctx);
void ndctl_set_log_priority(struct ndctl_ctx *ctx, int priority);
void ndctl_set_userdata(struct ndctl_ctx *ctx, void *userdata);
void *ndctl_get_userdata(struct ndctl_ctx *ctx);

enum ndctl_persistence_domain {
	PERSISTENCE_NONE = 0,
	PERSISTENCE_MEM_CTRL = 10,
	PERSISTENCE_CPU_CACHE = 20,
	PERSISTENCE_UNKNOWN = INT_MAX,
};

enum ndctl_fwa_state {
	NDCTL_FWA_INVALID,
	NDCTL_FWA_IDLE,
	NDCTL_FWA_ARMED,
	NDCTL_FWA_BUSY,
	NDCTL_FWA_ARM_OVERFLOW,
};

enum ndctl_fwa_method {
	NDCTL_FWA_METHOD_RESET,
	NDCTL_FWA_METHOD_SUSPEND,
	NDCTL_FWA_METHOD_LIVE,
};

struct ndctl_bus;
struct ndctl_bus *ndctl_bus_get_first(struct ndctl_ctx *ctx);
struct ndctl_bus *ndctl_bus_get_next(struct ndctl_bus *bus);
#define ndctl_bus_foreach(ctx, bus) \
        for (bus = ndctl_bus_get_first(ctx); \
             bus != NULL; \
             bus = ndctl_bus_get_next(bus))
struct ndctl_ctx *ndctl_bus_get_ctx(struct ndctl_bus *bus);
int ndctl_bus_has_nfit(struct ndctl_bus *bus);
int ndctl_bus_has_of_node(struct ndctl_bus *bus);
int ndctl_bus_is_papr_scm(struct ndctl_bus *bus);
unsigned int ndctl_bus_get_major(struct ndctl_bus *bus);
unsigned int ndctl_bus_get_minor(struct ndctl_bus *bus);
const char *ndctl_bus_get_devname(struct ndctl_bus *bus);
struct ndctl_bus *ndctl_bus_get_by_provider(struct ndctl_ctx *ctx,
		const char *provider);
const char *ndctl_bus_get_cmd_name(struct ndctl_bus *bus, int cmd);
int ndctl_bus_is_cmd_supported(struct ndctl_bus *bus, int cmd);
unsigned int ndctl_bus_get_revision(struct ndctl_bus *bus);
unsigned int ndctl_bus_get_id(struct ndctl_bus *bus);
const char *ndctl_bus_get_provider(struct ndctl_bus *bus);
enum ndctl_persistence_domain ndctl_bus_get_persistence_domain(
		struct ndctl_bus *bus);
int ndctl_bus_wait_probe(struct ndctl_bus *bus);
int ndctl_bus_wait_for_scrub_completion(struct ndctl_bus *bus);
int ndctl_bus_poll_scrub_completion(struct ndctl_bus *bus,
		unsigned int poll_interval, unsigned int timeout);
unsigned int ndctl_bus_get_scrub_count(struct ndctl_bus *bus);
int ndctl_bus_get_scrub_state(struct ndctl_bus *bus);
int ndctl_bus_start_scrub(struct ndctl_bus *bus);
int ndctl_bus_has_error_injection(struct ndctl_bus *bus);
enum ndctl_fwa_state ndctl_bus_get_fw_activate_state(struct ndctl_bus *bus);
enum ndctl_fwa_method ndctl_bus_get_fw_activate_method(struct ndctl_bus *bus);
int ndctl_bus_set_fw_activate_noidle(struct ndctl_bus *bus);
int ndctl_bus_clear_fw_activate_noidle(struct ndctl_bus *bus);
int ndctl_bus_set_fw_activate_nosuspend(struct ndctl_bus *bus);
int ndctl_bus_clear_fw_activate_nosuspend(struct ndctl_bus *bus);
int ndctl_bus_activate_firmware(struct ndctl_bus *bus, enum ndctl_fwa_method method);

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
long long ndctl_dimm_get_dirty_shutdown(struct ndctl_dimm *dimm);
unsigned short ndctl_dimm_get_subsystem_vendor(struct ndctl_dimm *dimm);
unsigned short ndctl_dimm_get_subsystem_device(struct ndctl_dimm *dimm);
unsigned short ndctl_dimm_get_manufacturing_date(struct ndctl_dimm *dimm);
unsigned char ndctl_dimm_get_manufacturing_location(struct ndctl_dimm *dimm);
unsigned short ndctl_dimm_get_subsystem_revision(struct ndctl_dimm *dimm);
unsigned short ndctl_dimm_get_format(struct ndctl_dimm *dimm);
int ndctl_dimm_get_formats(struct ndctl_dimm *dimm);
int ndctl_dimm_get_formatN(struct ndctl_dimm *dimm, int i);
unsigned int ndctl_dimm_get_major(struct ndctl_dimm *dimm);
unsigned int ndctl_dimm_get_minor(struct ndctl_dimm *dimm);
unsigned int ndctl_dimm_get_id(struct ndctl_dimm *dimm);
const char *ndctl_dimm_get_unique_id(struct ndctl_dimm *dimm);
unsigned int ndctl_dimm_get_serial(struct ndctl_dimm *dimm);
const char *ndctl_dimm_get_cmd_name(struct ndctl_dimm *dimm, int cmd);
int ndctl_dimm_is_cmd_supported(struct ndctl_dimm *dimm, int cmd);
int ndctl_dimm_locked(struct ndctl_dimm *dimm);
int ndctl_dimm_aliased(struct ndctl_dimm *dimm);
int ndctl_dimm_has_errors(struct ndctl_dimm *dimm);
int ndctl_dimm_has_notifications(struct ndctl_dimm *dimm);
int ndctl_dimm_failed_save(struct ndctl_dimm *dimm);
int ndctl_dimm_failed_arm(struct ndctl_dimm *dimm);
int ndctl_dimm_failed_restore(struct ndctl_dimm *dimm);
int ndctl_dimm_failed_map(struct ndctl_dimm *dimm);
int ndctl_dimm_smart_pending(struct ndctl_dimm *dimm);
int ndctl_dimm_failed_flush(struct ndctl_dimm *dimm);
int ndctl_dimm_get_health_eventfd(struct ndctl_dimm *dimm);
unsigned int ndctl_dimm_get_health(struct ndctl_dimm *dimm);
unsigned int ndctl_dimm_get_flags(struct ndctl_dimm *dimm);
unsigned int ndctl_dimm_get_event_flags(struct ndctl_dimm *dimm);
int ndctl_dimm_is_flag_supported(struct ndctl_dimm *dimm, unsigned int flag);
unsigned int ndctl_dimm_handle_get_node(struct ndctl_dimm *dimm);
unsigned int ndctl_dimm_handle_get_socket(struct ndctl_dimm *dimm);
unsigned int ndctl_dimm_handle_get_imc(struct ndctl_dimm *dimm);
unsigned int ndctl_dimm_handle_get_channel(struct ndctl_dimm *dimm);
unsigned int ndctl_dimm_handle_get_dimm(struct ndctl_dimm *dimm);
const char *ndctl_dimm_get_devname(struct ndctl_dimm *dimm);
struct ndctl_bus *ndctl_dimm_get_bus(struct ndctl_dimm *dimm);
struct ndctl_ctx *ndctl_dimm_get_ctx(struct ndctl_dimm *dimm);
struct ndctl_dimm *ndctl_dimm_get_by_handle(struct ndctl_bus *bus,
		unsigned int handle);
struct ndctl_dimm *ndctl_bus_get_dimm_by_physical_address(struct ndctl_bus *bus,
		unsigned long long address);
int ndctl_dimm_is_active(struct ndctl_dimm *dimm);
int ndctl_dimm_is_enabled(struct ndctl_dimm *dimm);
int ndctl_dimm_disable(struct ndctl_dimm *dimm);
int ndctl_dimm_enable(struct ndctl_dimm *dimm);

struct ndctl_cmd;
struct ndctl_cmd *ndctl_bus_cmd_new_ars_cap(struct ndctl_bus *bus,
		unsigned long long address, unsigned long long len);
struct ndctl_cmd *ndctl_bus_cmd_new_ars_start(struct ndctl_cmd *ars_cap, int type);
struct ndctl_cmd *ndctl_bus_cmd_new_ars_status(struct ndctl_cmd *ars_cap);
struct ndctl_range {
	unsigned long long address;
	unsigned long long length;
};
unsigned int ndctl_cmd_ars_cap_get_size(struct ndctl_cmd *ars_cap);
int ndctl_cmd_ars_cap_get_range(struct ndctl_cmd *ars_cap,
		struct ndctl_range *range);
int ndctl_cmd_ars_in_progress(struct ndctl_cmd *ars_status);
unsigned int ndctl_cmd_ars_num_records(struct ndctl_cmd *ars_stat);
unsigned long long ndctl_cmd_ars_get_record_addr(struct ndctl_cmd *ars_stat,
		unsigned int rec_index);
unsigned long long ndctl_cmd_ars_get_record_len(struct ndctl_cmd *ars_stat,
		unsigned int rec_index);
struct ndctl_cmd *ndctl_bus_cmd_new_clear_error(unsigned long long address,
		unsigned long long len, struct ndctl_cmd *ars_cap);
unsigned long long ndctl_cmd_clear_error_get_cleared(
		struct ndctl_cmd *clear_err);
unsigned int ndctl_cmd_ars_cap_get_clear_unit(struct ndctl_cmd *ars_cap);
int ndctl_cmd_ars_stat_get_flag_overflow(struct ndctl_cmd *ars_stat);

/*
 * Note: ndctl_cmd_smart_get_temperature is an alias for
 * ndctl_cmd_smart_get_temperature
 */

/*
 * the ndctl.h definition of these are deprecated, libndctl.h is the
 * authoritative defintion.
 */
#define ND_SMART_HEALTH_VALID	(1 << 0)
#define ND_SMART_SPARES_VALID	(1 << 1)
#define ND_SMART_USED_VALID	(1 << 2)
#define ND_SMART_MTEMP_VALID 	(1 << 3)
#define ND_SMART_TEMP_VALID 	ND_SMART_MTEMP_VALID
#define ND_SMART_CTEMP_VALID 	(1 << 4)
#define ND_SMART_SHUTDOWN_COUNT_VALID	(1 << 5)
#define ND_SMART_AIT_STATUS_VALID (1 << 6)
#define ND_SMART_PTEMP_VALID	(1 << 7)
#define ND_SMART_ALARM_VALID	(1 << 9)
#define ND_SMART_SHUTDOWN_VALID	(1 << 10)
#define ND_SMART_VENDOR_VALID	(1 << 11)
#define ND_SMART_SPARE_TRIP	(1 << 0)
#define ND_SMART_MTEMP_TRIP	(1 << 1)
#define ND_SMART_TEMP_TRIP	ND_SMART_MTEMP_TRIP
#define ND_SMART_CTEMP_TRIP	(1 << 2)
#define ND_SMART_NON_CRITICAL_HEALTH	(1 << 0)
#define ND_SMART_CRITICAL_HEALTH	(1 << 1)
#define ND_SMART_FATAL_HEALTH		(1 << 2)

struct ndctl_cmd *ndctl_dimm_cmd_new_smart(struct ndctl_dimm *dimm);
unsigned int ndctl_cmd_smart_get_flags(struct ndctl_cmd *cmd);
unsigned int ndctl_cmd_smart_get_health(struct ndctl_cmd *cmd);
unsigned int ndctl_cmd_smart_get_temperature(struct ndctl_cmd *cmd);
unsigned int ndctl_cmd_smart_get_media_temperature(struct ndctl_cmd *cmd);
unsigned int ndctl_cmd_smart_get_ctrl_temperature(struct ndctl_cmd *cmd);
unsigned int ndctl_cmd_smart_get_spares(struct ndctl_cmd *cmd);
unsigned int ndctl_cmd_smart_get_alarm_flags(struct ndctl_cmd *cmd);
unsigned int ndctl_cmd_smart_get_life_used(struct ndctl_cmd *cmd);
unsigned int ndctl_cmd_smart_get_shutdown_state(struct ndctl_cmd *cmd);
unsigned int ndctl_cmd_smart_get_shutdown_count(struct ndctl_cmd *cmd);
unsigned int ndctl_cmd_smart_get_vendor_size(struct ndctl_cmd *cmd);
unsigned char *ndctl_cmd_smart_get_vendor_data(struct ndctl_cmd *cmd);
struct ndctl_cmd *ndctl_dimm_cmd_new_smart_threshold(struct ndctl_dimm *dimm);
unsigned int ndctl_cmd_smart_threshold_get_alarm_control(struct ndctl_cmd *cmd);
unsigned int ndctl_cmd_smart_threshold_get_temperature(struct ndctl_cmd *cmd);
unsigned int ndctl_cmd_smart_threshold_get_media_temperature(struct ndctl_cmd *cmd);
unsigned int ndctl_cmd_smart_threshold_get_ctrl_temperature(struct ndctl_cmd *cmd);
unsigned int ndctl_cmd_smart_threshold_get_spares(struct ndctl_cmd *cmd);
struct ndctl_cmd *ndctl_dimm_cmd_new_smart_set_threshold(struct ndctl_cmd *cmd);
unsigned int ndctl_cmd_smart_threshold_get_supported_alarms(struct ndctl_cmd *cmd);
int ndctl_cmd_smart_threshold_set_alarm_control(struct ndctl_cmd *cmd,
		unsigned int val);
int ndctl_cmd_smart_threshold_set_temperature(struct ndctl_cmd *cmd,
		unsigned int val);
int ndctl_cmd_smart_threshold_set_media_temperature(struct ndctl_cmd *cmd,
		unsigned int val);
int ndctl_cmd_smart_threshold_set_ctrl_temperature(struct ndctl_cmd *cmd,
		unsigned int val);
int ndctl_cmd_smart_threshold_set_spares(struct ndctl_cmd *cmd,
		unsigned int val);
struct ndctl_cmd *ndctl_dimm_cmd_new_smart_inject(struct ndctl_dimm *dimm);
int ndctl_cmd_smart_inject_media_temperature(struct ndctl_cmd *cmd, bool enable,
		unsigned int mtemp);
int ndctl_cmd_smart_inject_ctrl_temperature(struct ndctl_cmd *cmd, bool enable,
		unsigned int ctemp);
int ndctl_cmd_smart_inject_spares(struct ndctl_cmd *cmd, bool enable,
		unsigned int spares);
int ndctl_cmd_smart_inject_fatal(struct ndctl_cmd *cmd, bool enable);
int ndctl_cmd_smart_inject_unsafe_shutdown(struct ndctl_cmd *cmd, bool enable);
int ndctl_dimm_smart_inject_supported(struct ndctl_dimm *dimm);

struct ndctl_cmd *ndctl_dimm_cmd_new_vendor_specific(struct ndctl_dimm *dimm,
		unsigned int opcode, size_t input_size, size_t output_size);
ssize_t ndctl_cmd_vendor_set_input(struct ndctl_cmd *cmd, void *buf,
		unsigned int len);
ssize_t ndctl_cmd_vendor_get_output_size(struct ndctl_cmd *cmd);
ssize_t ndctl_cmd_vendor_get_output(struct ndctl_cmd *cmd, void *buf,
		unsigned int len);
struct ndctl_cmd *ndctl_dimm_cmd_new_cfg_size(struct ndctl_dimm *dimm);
struct ndctl_cmd *ndctl_dimm_cmd_new_cfg_read(struct ndctl_cmd *cfg_size);
struct ndctl_cmd *ndctl_dimm_cmd_new_cfg_write(struct ndctl_cmd *cfg_read);
int ndctl_dimm_zero_labels(struct ndctl_dimm *dimm);
int ndctl_dimm_zero_label_extent(struct ndctl_dimm *dimm,
		unsigned int len, unsigned int offset);
struct ndctl_cmd *ndctl_dimm_read_labels(struct ndctl_dimm *dimm);
struct ndctl_cmd *ndctl_dimm_read_label_index(struct ndctl_dimm *dimm);
struct ndctl_cmd *ndctl_dimm_read_label_extent(struct ndctl_dimm *dimm,
		unsigned int len, unsigned int offset);
int ndctl_dimm_validate_labels(struct ndctl_dimm *dimm);
enum ndctl_namespace_version {
	NDCTL_NS_VERSION_1_1,
	NDCTL_NS_VERSION_1_2,
};
int ndctl_dimm_init_labels(struct ndctl_dimm *dimm,
		enum ndctl_namespace_version v);
unsigned long ndctl_dimm_get_available_labels(struct ndctl_dimm *dimm);
unsigned int ndctl_dimm_sizeof_namespace_label(struct ndctl_dimm *dimm);
unsigned int ndctl_cmd_cfg_size_get_size(struct ndctl_cmd *cfg_size);
ssize_t ndctl_cmd_cfg_read_get_data(struct ndctl_cmd *cfg_read, void *buf,
		unsigned int len, unsigned int offset);
ssize_t ndctl_cmd_cfg_read_get_size(struct ndctl_cmd *cfg_read);
int ndctl_cmd_cfg_read_set_extent(struct ndctl_cmd *cfg_read,
		unsigned int len, unsigned int offset);
int ndctl_cmd_cfg_write_set_extent(struct ndctl_cmd *cfg_write,
		unsigned int len, unsigned int offset);
ssize_t ndctl_cmd_cfg_write_set_data(struct ndctl_cmd *cfg_write, void *buf,
		unsigned int len, unsigned int offset);
ssize_t ndctl_cmd_cfg_write_zero_data(struct ndctl_cmd *cfg_write);
void ndctl_cmd_unref(struct ndctl_cmd *cmd);
void ndctl_cmd_ref(struct ndctl_cmd *cmd);
int ndctl_cmd_get_type(struct ndctl_cmd *cmd);
int ndctl_cmd_get_status(struct ndctl_cmd *cmd);
unsigned int ndctl_cmd_get_firmware_status(struct ndctl_cmd *cmd);
int ndctl_cmd_submit(struct ndctl_cmd *cmd);

struct badblock {
	unsigned long long offset;
	unsigned int len;
};

struct ndctl_region;
struct ndctl_region *ndctl_region_get_first(struct ndctl_bus *bus);
struct ndctl_region *ndctl_region_get_next(struct ndctl_region *region);
#define ndctl_region_foreach(bus, region) \
        for (region = ndctl_region_get_first(bus); \
             region != NULL; \
             region = ndctl_region_get_next(region))
struct badblock *ndctl_region_get_first_badblock(struct ndctl_region *region);
struct badblock *ndctl_region_get_next_badblock(struct ndctl_region *region);
#define ndctl_region_badblock_foreach(region, badblock) \
        for (badblock = ndctl_region_get_first_badblock(region); \
             badblock != NULL; \
             badblock = ndctl_region_get_next_badblock(region))
unsigned int ndctl_region_get_id(struct ndctl_region *region);
const char *ndctl_region_get_devname(struct ndctl_region *region);
unsigned int ndctl_region_get_interleave_ways(struct ndctl_region *region);
unsigned int ndctl_region_get_mappings(struct ndctl_region *region);
unsigned long long ndctl_region_get_size(struct ndctl_region *region);
unsigned long long ndctl_region_get_available_size(struct ndctl_region *region);
unsigned long long ndctl_region_get_max_available_extent(
		struct ndctl_region *region);
unsigned int ndctl_region_get_range_index(struct ndctl_region *region);
unsigned int ndctl_region_get_type(struct ndctl_region *region);
struct ndctl_namespace *ndctl_region_get_namespace_seed(
		struct ndctl_region *region);
int ndctl_region_get_ro(struct ndctl_region *region);
int ndctl_region_set_ro(struct ndctl_region *region, int ro);
unsigned long ndctl_region_get_align(struct ndctl_region *region);
int ndctl_region_set_align(struct ndctl_region *region, unsigned long align);
unsigned long long ndctl_region_get_resource(struct ndctl_region *region);
struct ndctl_btt *ndctl_region_get_btt_seed(struct ndctl_region *region);
struct ndctl_pfn *ndctl_region_get_pfn_seed(struct ndctl_region *region);
unsigned int ndctl_region_get_nstype(struct ndctl_region *region);
const char *ndctl_region_get_type_name(struct ndctl_region *region);
struct ndctl_bus *ndctl_region_get_bus(struct ndctl_region *region);
struct ndctl_ctx *ndctl_region_get_ctx(struct ndctl_region *region);
struct ndctl_dimm *ndctl_region_get_first_dimm(struct ndctl_region *region);
struct ndctl_dimm *ndctl_region_get_next_dimm(struct ndctl_region *region,
		struct ndctl_dimm *dimm);
int ndctl_region_get_numa_node(struct ndctl_region *region);
int ndctl_region_has_numa(struct ndctl_region *region);
int ndctl_region_get_target_node(struct ndctl_region *region);
struct ndctl_region *ndctl_bus_get_region_by_physical_address(struct ndctl_bus *bus,
		unsigned long long address);
#define ndctl_dimm_foreach_in_region(region, dimm) \
        for (dimm = ndctl_region_get_first_dimm(region); \
             dimm != NULL; \
             dimm = ndctl_region_get_next_dimm(region, dimm))
enum ndctl_persistence_domain ndctl_region_get_persistence_domain(
		struct ndctl_region *region);
int ndctl_region_is_enabled(struct ndctl_region *region);
int ndctl_region_enable(struct ndctl_region *region);
int ndctl_region_disable_invalidate(struct ndctl_region *region);
int ndctl_region_disable_preserve(struct ndctl_region *region);
void ndctl_region_cleanup(struct ndctl_region *region);
int ndctl_region_deep_flush(struct ndctl_region *region);

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
int ndctl_mapping_get_position(struct ndctl_mapping *mapping);

struct ndctl_namespace;
struct ndctl_namespace *ndctl_namespace_get_first(struct ndctl_region *region);
struct ndctl_namespace *ndctl_namespace_get_next(struct ndctl_namespace *ndns);
#define ndctl_namespace_foreach(region, ndns) \
        for (ndns = ndctl_namespace_get_first(region); \
             ndns != NULL; \
             ndns = ndctl_namespace_get_next(ndns))
#define ndctl_namespace_foreach_safe(region, ndns, _ndns) \
	for (ndns = ndctl_namespace_get_first(region), \
	     _ndns = ndns ? ndctl_namespace_get_next(ndns) : NULL; \
	     ndns != NULL; \
	     ndns = _ndns, \
	     _ndns = _ndns ? ndctl_namespace_get_next(_ndns) : NULL)
struct badblock *ndctl_namespace_get_first_badblock(struct ndctl_namespace *ndns);
struct badblock *ndctl_namespace_get_next_badblock(struct ndctl_namespace *ndns);
#define ndctl_namespace_badblock_foreach(ndns, badblock) \
        for (badblock = ndctl_namespace_get_first_badblock(ndns); \
             badblock != NULL; \
             badblock = ndctl_namespace_get_next_badblock(ndns))
struct ndctl_ctx *ndctl_namespace_get_ctx(struct ndctl_namespace *ndns);
struct ndctl_bus *ndctl_namespace_get_bus(struct ndctl_namespace *ndns);
struct ndctl_region *ndctl_namespace_get_region(struct ndctl_namespace *ndns);
struct ndctl_btt *ndctl_namespace_get_btt(struct ndctl_namespace *ndns);
struct ndctl_pfn *ndctl_namespace_get_pfn(struct ndctl_namespace *ndns);
struct ndctl_dax *ndctl_namespace_get_dax(struct ndctl_namespace *ndns);
unsigned int ndctl_namespace_get_id(struct ndctl_namespace *ndns);
const char *ndctl_namespace_get_devname(struct ndctl_namespace *ndns);
unsigned int ndctl_namespace_get_type(struct ndctl_namespace *ndns);
const char *ndctl_namespace_get_type_name(struct ndctl_namespace *ndns);
const char *ndctl_namespace_get_block_device(struct ndctl_namespace *ndns);
enum ndctl_namespace_mode {
	NDCTL_NS_MODE_MEMORY,
	NDCTL_NS_MODE_FSDAX = NDCTL_NS_MODE_MEMORY,
	NDCTL_NS_MODE_SAFE,
	NDCTL_NS_MODE_SECTOR = NDCTL_NS_MODE_SAFE,
	NDCTL_NS_MODE_RAW,
	NDCTL_NS_MODE_DAX,
	NDCTL_NS_MODE_DEVDAX = NDCTL_NS_MODE_DAX,
	NDCTL_NS_MODE_UNKNOWN, /* must be last entry */
};
enum ndctl_namespace_mode ndctl_namespace_get_mode(
		struct ndctl_namespace *ndns);
enum ndctl_namespace_mode ndctl_namespace_get_enforce_mode(
		struct ndctl_namespace *ndns);
int ndctl_namespace_set_enforce_mode(struct ndctl_namespace *ndns,
		enum ndctl_namespace_mode mode);
int ndctl_namespace_is_enabled(struct ndctl_namespace *ndns);
int ndctl_namespace_enable(struct ndctl_namespace *ndns);
int ndctl_namespace_disable(struct ndctl_namespace *ndns);
int ndctl_namespace_disable_invalidate(struct ndctl_namespace *ndns);
int ndctl_namespace_disable_safe(struct ndctl_namespace *ndns);
bool ndctl_namespace_is_active(struct ndctl_namespace *ndns);
int ndctl_namespace_is_valid(struct ndctl_namespace *ndns);
int ndctl_namespace_is_configured(struct ndctl_namespace *ndns);
int ndctl_namespace_is_configuration_idle(struct ndctl_namespace *ndns);
int ndctl_namespace_delete(struct ndctl_namespace *ndns);
int ndctl_namespace_set_uuid(struct ndctl_namespace *ndns, uuid_t uu);
void ndctl_namespace_get_uuid(struct ndctl_namespace *ndns, uuid_t uu);
const char *ndctl_namespace_get_alt_name(struct ndctl_namespace *ndns);
int ndctl_namespace_set_alt_name(struct ndctl_namespace *ndns,
		const char *alt_name);
unsigned long long ndctl_namespace_get_size(struct ndctl_namespace *ndns);
unsigned long long ndctl_namespace_get_resource(struct ndctl_namespace *ndns);
int ndctl_namespace_set_size(struct ndctl_namespace *ndns,
		unsigned long long size);
unsigned int ndctl_namespace_get_supported_sector_size(
		struct ndctl_namespace *ndns, int i);
unsigned int ndctl_namespace_get_sector_size(struct ndctl_namespace *ndns);
int ndctl_namespace_get_num_sector_sizes(struct ndctl_namespace *ndns);
int ndctl_namespace_set_sector_size(struct ndctl_namespace *ndns,
		unsigned int sector_size);
int ndctl_namespace_get_raw_mode(struct ndctl_namespace *ndns);
int ndctl_namespace_set_raw_mode(struct ndctl_namespace *ndns, int raw_mode);
int ndctl_namespace_get_numa_node(struct ndctl_namespace *ndns);
int ndctl_namespace_get_target_node(struct ndctl_namespace *ndns);
int ndctl_namespace_inject_error(struct ndctl_namespace *ndns,
		unsigned long long block, unsigned long long count,
		bool notify);
int ndctl_namespace_inject_error2(struct ndctl_namespace *ndns,
		unsigned long long block, unsigned long long count,
		unsigned int flags);
int ndctl_namespace_uninject_error(struct ndctl_namespace *ndns,
		unsigned long long block, unsigned long long count);
int ndctl_namespace_uninject_error2(struct ndctl_namespace *ndns,
		unsigned long long block, unsigned long long count,
		unsigned int flags);
int ndctl_namespace_injection_status(struct ndctl_namespace *ndns);
enum ndctl_namespace_inject_flags {
	NDCTL_NS_INJECT_NOTIFY = 0,
	NDCTL_NS_INJECT_SATURATE,
};

struct ndctl_bb;
unsigned long long ndctl_bb_get_block(struct ndctl_bb *bb);
unsigned long long ndctl_bb_get_count(struct ndctl_bb *bb);
struct ndctl_bb *ndctl_namespace_injection_get_first_bb(
		struct ndctl_namespace *ndns);
struct ndctl_bb *ndctl_namespace_injection_get_next_bb(
		struct ndctl_namespace *ndns, struct ndctl_bb *bb);
#define ndctl_namespace_bb_foreach(ndns, bb) \
        for (bb = ndctl_namespace_injection_get_first_bb(ndns); \
             bb != NULL; \
             bb = ndctl_namespace_injection_get_next_bb(ndns, bb))
int ndctl_namespace_write_cache_is_enabled(struct ndctl_namespace *ndns);
int ndctl_namespace_enable_write_cache(struct ndctl_namespace *ndns);
int ndctl_namespace_disable_write_cache(struct ndctl_namespace *ndns);

struct ndctl_btt;
struct ndctl_btt *ndctl_btt_get_first(struct ndctl_region *region);
struct ndctl_btt *ndctl_btt_get_next(struct ndctl_btt *btt);
#define ndctl_btt_foreach(region, btt) \
        for (btt = ndctl_btt_get_first(region); \
             btt != NULL; \
             btt = ndctl_btt_get_next(btt))
#define ndctl_btt_foreach_safe(region, btt, _btt) \
	for (btt = ndctl_btt_get_first(region), \
	     _btt = btt ? ndctl_btt_get_next(btt) : NULL; \
	     btt != NULL; \
	     btt = _btt, \
	     _btt = _btt ? ndctl_btt_get_next(_btt) : NULL)
struct ndctl_ctx *ndctl_btt_get_ctx(struct ndctl_btt *btt);
struct ndctl_bus *ndctl_btt_get_bus(struct ndctl_btt *btt);
struct ndctl_region *ndctl_btt_get_region(struct ndctl_btt *btt);
unsigned int ndctl_btt_get_id(struct ndctl_btt *btt);
unsigned int ndctl_btt_get_supported_sector_size(struct ndctl_btt *btt, int i);
unsigned int ndctl_btt_get_sector_size(struct ndctl_btt *btt);
int ndctl_btt_get_num_sector_sizes(struct ndctl_btt *btt);
struct ndctl_namespace *ndctl_btt_get_namespace(struct ndctl_btt *btt);
void ndctl_btt_get_uuid(struct ndctl_btt *btt, uuid_t uu);
unsigned long long ndctl_btt_get_size(struct ndctl_btt *btt);
int ndctl_btt_is_enabled(struct ndctl_btt *btt);
int ndctl_btt_is_valid(struct ndctl_btt *btt);
const char *ndctl_btt_get_devname(struct ndctl_btt *btt);
const char *ndctl_btt_get_block_device(struct ndctl_btt *btt);
int ndctl_btt_set_uuid(struct ndctl_btt *btt, uuid_t uu);
int ndctl_btt_set_sector_size(struct ndctl_btt *btt, unsigned int sector_size);
int ndctl_btt_set_namespace(struct ndctl_btt *btt, struct ndctl_namespace *ndns);
int ndctl_btt_enable(struct ndctl_btt *btt);
int ndctl_btt_delete(struct ndctl_btt *btt);
int ndctl_btt_is_configured(struct ndctl_btt *btt);

struct ndctl_pfn;
struct ndctl_pfn *ndctl_pfn_get_first(struct ndctl_region *region);
struct ndctl_pfn *ndctl_pfn_get_next(struct ndctl_pfn *pfn);
#define ndctl_pfn_foreach(region, pfn) \
        for (pfn = ndctl_pfn_get_first(region); \
             pfn != NULL; \
             pfn = ndctl_pfn_get_next(pfn))
#define ndctl_pfn_foreach_safe(region, pfn, _pfn) \
	for (pfn = ndctl_pfn_get_first(region), \
	     _pfn = ndctl_pfn_get_next(pfn); \
	     pfn != NULL; \
	     pfn = _pfn, \
	     _pfn = _pfn ? ndctl_pfn_get_next(_pfn) : NULL)
struct ndctl_ctx *ndctl_pfn_get_ctx(struct ndctl_pfn *pfn);
struct ndctl_bus *ndctl_pfn_get_bus(struct ndctl_pfn *pfn);
struct ndctl_region *ndctl_pfn_get_region(struct ndctl_pfn *pfn);
unsigned int ndctl_pfn_get_id(struct ndctl_pfn *pfn);
int ndctl_pfn_is_enabled(struct ndctl_pfn *pfn);
int ndctl_pfn_is_valid(struct ndctl_pfn *pfn);
const char *ndctl_pfn_get_devname(struct ndctl_pfn *pfn);
const char *ndctl_pfn_get_block_device(struct ndctl_pfn *pfn);
enum ndctl_pfn_loc {
	NDCTL_PFN_LOC_NONE,
	NDCTL_PFN_LOC_RAM,
	NDCTL_PFN_LOC_PMEM,
};
int ndctl_pfn_set_location(struct ndctl_pfn *pfn, enum ndctl_pfn_loc loc);
enum ndctl_pfn_loc ndctl_pfn_get_location(struct ndctl_pfn *pfn);
int ndctl_pfn_set_uuid(struct ndctl_pfn *pfn, uuid_t uu);
void ndctl_pfn_get_uuid(struct ndctl_pfn *pfn, uuid_t uu);
int ndctl_pfn_has_align(struct ndctl_pfn *pfn);
int ndctl_pfn_set_align(struct ndctl_pfn *pfn, unsigned long align);
int ndctl_pfn_get_num_alignments(struct ndctl_pfn *pfn);
unsigned long ndctl_pfn_get_align(struct ndctl_pfn *pfn);
unsigned long ndctl_pfn_get_supported_alignment(struct ndctl_pfn *pfn, int i);
unsigned long long ndctl_pfn_get_resource(struct ndctl_pfn *pfn);
unsigned long long ndctl_pfn_get_size(struct ndctl_pfn *pfn);
int ndctl_pfn_set_namespace(struct ndctl_pfn *pfn, struct ndctl_namespace *ndns);
struct ndctl_namespace *ndctl_pfn_get_namespace(struct ndctl_pfn *pfn);
int ndctl_pfn_enable(struct ndctl_pfn *pfn);
int ndctl_pfn_delete(struct ndctl_pfn *pfn);
int ndctl_pfn_is_configured(struct ndctl_pfn *pfn);

#define ndctl_dax_foreach(region, dax) \
        for (dax = ndctl_dax_get_first(region); \
             dax != NULL; \
             dax = ndctl_dax_get_next(dax))
#define ndctl_dax_foreach_safe(region, dax, _dax) \
	for (dax = ndctl_dax_get_first(region), \
	     _dax = ndctl_dax_get_next(dax); \
	     dax != NULL; \
	     dax = _dax, \
	     _dax = _dax ? ndctl_dax_get_next(_dax) : NULL)
struct ndctl_dax *ndctl_region_get_dax_seed(struct ndctl_region *region);
struct ndctl_dax *ndctl_namespace_get_dax(struct ndctl_namespace *ndns);
struct ndctl_dax *ndctl_dax_get_first(struct ndctl_region *region);
struct ndctl_dax *ndctl_dax_get_next(struct ndctl_dax *dax);
unsigned int ndctl_dax_get_id(struct ndctl_dax *dax);
struct ndctl_namespace *ndctl_dax_get_namespace(struct ndctl_dax *dax);
void ndctl_dax_get_uuid(struct ndctl_dax *dax, uuid_t uu);
unsigned long long ndctl_dax_get_size(struct ndctl_dax *dax);
unsigned long long ndctl_dax_get_resource(struct ndctl_dax *dax);
int ndctl_dax_set_uuid(struct ndctl_dax *dax, uuid_t uu);
enum ndctl_pfn_loc ndctl_dax_get_location(struct ndctl_dax *dax);
int ndctl_dax_set_location(struct ndctl_dax *dax, enum ndctl_pfn_loc loc);
int ndctl_dax_get_num_alignments(struct ndctl_dax *dax);
unsigned long ndctl_dax_get_align(struct ndctl_dax *dax);
unsigned long ndctl_dax_get_supported_alignment(struct ndctl_dax *dax, int i);
int ndctl_dax_has_align(struct ndctl_dax *dax);
int ndctl_dax_set_align(struct ndctl_dax *dax, unsigned long align);
int ndctl_dax_set_namespace(struct ndctl_dax *dax,
		struct ndctl_namespace *ndns);
struct ndctl_bus *ndctl_dax_get_bus(struct ndctl_dax *dax);
struct ndctl_ctx *ndctl_dax_get_ctx(struct ndctl_dax *dax);
const char *ndctl_dax_get_devname(struct ndctl_dax *dax);
int ndctl_dax_is_valid(struct ndctl_dax *dax);
int ndctl_dax_is_enabled(struct ndctl_dax *dax);
struct ndctl_region *ndctl_dax_get_region(struct ndctl_dax *dax);
int ndctl_dax_enable(struct ndctl_dax *dax);
int ndctl_dax_delete(struct ndctl_dax *dax);
int ndctl_dax_is_configured(struct ndctl_dax *dax);
struct daxctl_region *ndctl_dax_get_daxctl_region(struct ndctl_dax *dax);

enum ND_FW_STATUS {
	FW_SUCCESS = 0,		/* success */
	FW_ENOTSUPP,		/* not supported */
	FW_ENOTEXIST,		/* device not exist */
	FW_EINVAL,		/* invalid input */
	FW_EHWERR,		/* hardware error */
	FW_ERETRY,		/* try again */
	FW_EUNKNOWN,		/* unknown reason */
	FW_ENORES,		/* out of resource */
	FW_ENOTREADY,		/* hardware not ready */
	FW_EBUSY,		/* firmware inprogress */
	FW_EINVAL_CTX,		/* invalid context passed in */
	FW_ALREADY_DONE,	/* firmware already updated */
	FW_EBADFW,		/* firmware failed verification */
	FW_ABORTED,		/* update sequence aborted success */
	FW_ESEQUENCE,		/* update sequence incorrect */
};

struct ndctl_cmd *ndctl_dimm_cmd_new_fw_get_info(struct ndctl_dimm *dimm);
struct ndctl_cmd *ndctl_dimm_cmd_new_fw_start_update(struct ndctl_dimm *dimm);
struct ndctl_cmd *ndctl_dimm_cmd_new_fw_send(struct ndctl_cmd *start,
		unsigned int offset, unsigned int len, void *data);
struct ndctl_cmd *ndctl_dimm_cmd_new_fw_finish(struct ndctl_cmd *start);
struct ndctl_cmd *ndctl_dimm_cmd_new_fw_abort(struct ndctl_cmd *start);
struct ndctl_cmd *ndctl_dimm_cmd_new_fw_finish_query(struct ndctl_cmd *start);
unsigned int ndctl_cmd_fw_info_get_storage_size(struct ndctl_cmd *cmd);
unsigned int ndctl_cmd_fw_info_get_max_send_len(struct ndctl_cmd *cmd);
unsigned int ndctl_cmd_fw_info_get_query_interval(struct ndctl_cmd *cmd);
unsigned int ndctl_cmd_fw_info_get_max_query_time(struct ndctl_cmd *cmd);
unsigned long long ndctl_cmd_fw_info_get_run_version(struct ndctl_cmd *cmd);
unsigned long long ndctl_cmd_fw_info_get_updated_version(struct ndctl_cmd *cmd);
unsigned int ndctl_cmd_fw_start_get_context(struct ndctl_cmd *cmd);
unsigned long long ndctl_cmd_fw_fquery_get_fw_rev(struct ndctl_cmd *cmd);
enum ND_FW_STATUS ndctl_cmd_fw_xlat_firmware_status(struct ndctl_cmd *cmd);
struct ndctl_cmd *ndctl_dimm_cmd_new_ack_shutdown_count(struct ndctl_dimm *dimm);
int ndctl_dimm_fw_update_supported(struct ndctl_dimm *dimm);

enum ndctl_fwa_result {
        NDCTL_FWA_RESULT_INVALID,
        NDCTL_FWA_RESULT_NONE,
        NDCTL_FWA_RESULT_SUCCESS,
        NDCTL_FWA_RESULT_NOTSTAGED,
        NDCTL_FWA_RESULT_NEEDRESET,
        NDCTL_FWA_RESULT_FAIL,
};

enum ndctl_fwa_state ndctl_dimm_get_fw_activate_state(struct ndctl_dimm *dimm);
enum ndctl_fwa_result ndctl_dimm_get_fw_activate_result(struct ndctl_dimm *dimm);
enum ndctl_fwa_state ndctl_dimm_fw_activate_disarm(struct ndctl_dimm *dimm);
enum ndctl_fwa_state ndctl_dimm_fw_activate_arm(struct ndctl_dimm *dimm);

int ndctl_cmd_xlat_firmware_status(struct ndctl_cmd *cmd);
int ndctl_cmd_submit_xlat(struct ndctl_cmd *cmd);

#define ND_PASSPHRASE_SIZE	32
#define ND_KEY_DESC_LEN	22
#define ND_KEY_DESC_PREFIX  7

enum ndctl_security_state {
	NDCTL_SECURITY_INVALID = -1,
	NDCTL_SECURITY_DISABLED = 0,
	NDCTL_SECURITY_UNLOCKED,
	NDCTL_SECURITY_LOCKED,
	NDCTL_SECURITY_FROZEN,
	NDCTL_SECURITY_OVERWRITE,
};

enum ndctl_security_state ndctl_dimm_get_security(struct ndctl_dimm *dimm);
bool ndctl_dimm_security_is_frozen(struct ndctl_dimm *dimm);
int ndctl_dimm_update_passphrase(struct ndctl_dimm *dimm,
		long ckey, long nkey);
int ndctl_dimm_disable_passphrase(struct ndctl_dimm *dimm, long key);
int ndctl_dimm_freeze_security(struct ndctl_dimm *dimm);
int ndctl_dimm_secure_erase(struct ndctl_dimm *dimm, long key);
int ndctl_dimm_overwrite(struct ndctl_dimm *dimm, long key);
int ndctl_dimm_wait_overwrite(struct ndctl_dimm *dimm);
int ndctl_dimm_update_master_passphrase(struct ndctl_dimm *dimm,
		long ckey, long nkey);
int ndctl_dimm_master_secure_erase(struct ndctl_dimm *dimm, long key);

#define ND_KEY_DESC_SIZE	128
#define ND_KEY_CMD_SIZE		128

#define NUMA_NO_NODE    (-1)
#define NUMA_NO_ATTR    (-2)

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
