/* SPDX-License-Identifier: LGPL-2.1 */
/* Copyright (C) 2020-2021, Intel Corporation. All rights reserved. */
#ifndef _LIBCXL_H_
#define _LIBCXL_H_

#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>

#ifdef HAVE_UUID
#include <uuid/uuid.h>
#else
typedef unsigned char uuid_t[16];
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct cxl_ctx;
struct cxl_ctx *cxl_ref(struct cxl_ctx *ctx);
void cxl_unref(struct cxl_ctx *ctx);
int cxl_new(struct cxl_ctx **ctx);
void cxl_set_log_fn(struct cxl_ctx *ctx,
		void (*log_fn)(struct cxl_ctx *ctx, int priority,
			const char *file, int line, const char *fn,
			const char *format, va_list args));
int cxl_get_log_priority(struct cxl_ctx *ctx);
void cxl_set_log_priority(struct cxl_ctx *ctx, int priority);
void cxl_set_userdata(struct cxl_ctx *ctx, void *userdata);
void *cxl_get_userdata(struct cxl_ctx *ctx);
void cxl_set_private_data(struct cxl_ctx *ctx, void *data);
void *cxl_get_private_data(struct cxl_ctx *ctx);

enum cxl_fwl_status {
	CXL_FWL_STATUS_UNKNOWN,
	CXL_FWL_STATUS_IDLE,
	CXL_FWL_STATUS_RECEIVING,
	CXL_FWL_STATUS_PREPARING,
	CXL_FWL_STATUS_TRANSFERRING,
	CXL_FWL_STATUS_PROGRAMMING,
};

static inline enum cxl_fwl_status cxl_fwl_status_from_ident(char *status)
{
	if (strcmp(status, "idle") == 0)
		return CXL_FWL_STATUS_IDLE;
	if (strcmp(status, "receiving") == 0)
		return CXL_FWL_STATUS_RECEIVING;
	if (strcmp(status, "preparing") == 0)
		return CXL_FWL_STATUS_PREPARING;
	if (strcmp(status, "transferring") == 0)
		return CXL_FWL_STATUS_TRANSFERRING;
	if (strcmp(status, "programming") == 0)
		return CXL_FWL_STATUS_PROGRAMMING;

	return CXL_FWL_STATUS_UNKNOWN;
}

struct cxl_memdev;
struct cxl_memdev *cxl_memdev_get_first(struct cxl_ctx *ctx);
struct cxl_memdev *cxl_memdev_get_next(struct cxl_memdev *memdev);
int cxl_memdev_get_id(struct cxl_memdev *memdev);
unsigned long long cxl_memdev_get_serial(struct cxl_memdev *memdev);
int cxl_memdev_get_numa_node(struct cxl_memdev *memdev);
const char *cxl_memdev_get_devname(struct cxl_memdev *memdev);
const char *cxl_memdev_get_host(struct cxl_memdev *memdev);
struct cxl_bus *cxl_memdev_get_bus(struct cxl_memdev *memdev);
int cxl_memdev_get_major(struct cxl_memdev *memdev);
int cxl_memdev_get_minor(struct cxl_memdev *memdev);
struct cxl_ctx *cxl_memdev_get_ctx(struct cxl_memdev *memdev);
unsigned long long cxl_memdev_get_pmem_size(struct cxl_memdev *memdev);
unsigned long long cxl_memdev_get_ram_size(struct cxl_memdev *memdev);
int cxl_memdev_get_pmem_qos_class(struct cxl_memdev *memdev);
int cxl_memdev_get_ram_qos_class(struct cxl_memdev *memdev);
const char *cxl_memdev_get_firmware_verison(struct cxl_memdev *memdev);
bool cxl_memdev_fw_update_in_progress(struct cxl_memdev *memdev);
size_t cxl_memdev_fw_update_get_remaining(struct cxl_memdev *memdev);
int cxl_memdev_update_fw(struct cxl_memdev *memdev, const char *fw_path);
int cxl_memdev_cancel_fw_update(struct cxl_memdev *memdev);
int cxl_memdev_wait_sanitize(struct cxl_memdev *memdev, int timeout_ms);

/* ABI spelling mistakes are forever */
static inline const char *cxl_memdev_get_firmware_version(
		struct cxl_memdev *memdev)
{
	return cxl_memdev_get_firmware_verison(memdev);
}

size_t cxl_memdev_get_label_size(struct cxl_memdev *memdev);
int cxl_memdev_disable_invalidate(struct cxl_memdev *memdev);
int cxl_memdev_enable(struct cxl_memdev *memdev);
struct cxl_endpoint;
struct cxl_endpoint *cxl_memdev_get_endpoint(struct cxl_memdev *memdev);
int cxl_memdev_nvdimm_bridge_active(struct cxl_memdev *memdev);
int cxl_memdev_zero_label(struct cxl_memdev *memdev, size_t length,
		size_t offset);
int cxl_memdev_read_label(struct cxl_memdev *memdev, void *buf, size_t length,
		size_t offset);
int cxl_memdev_write_label(struct cxl_memdev *memdev, void *buf, size_t length,
		size_t offset);
struct cxl_cmd *cxl_cmd_new_get_fw_info(struct cxl_memdev *memdev);
unsigned int cxl_cmd_fw_info_get_num_slots(struct cxl_cmd *cmd);
unsigned int cxl_cmd_fw_info_get_active_slot(struct cxl_cmd *cmd);
unsigned int cxl_cmd_fw_info_get_staged_slot(struct cxl_cmd *cmd);
bool cxl_cmd_fw_info_get_online_activate_capable(struct cxl_cmd *cmd);
int cxl_cmd_fw_info_get_fw_ver(struct cxl_cmd *cmd, int slot, char *buf,
			       unsigned int len);

#define cxl_memdev_foreach(ctx, memdev) \
        for (memdev = cxl_memdev_get_first(ctx); \
             memdev != NULL; \
             memdev = cxl_memdev_get_next(memdev))

struct cxl_bus;
struct cxl_bus *cxl_bus_get_first(struct cxl_ctx *ctx);
struct cxl_bus *cxl_bus_get_next(struct cxl_bus *bus);
const char *cxl_bus_get_provider(struct cxl_bus *bus);
const char *cxl_bus_get_devname(struct cxl_bus *bus);
int cxl_bus_get_id(struct cxl_bus *bus);
struct cxl_port *cxl_bus_get_port(struct cxl_bus *bus);
struct cxl_ctx *cxl_bus_get_ctx(struct cxl_bus *bus);
int cxl_bus_disable_invalidate(struct cxl_bus *bus);

#define cxl_bus_foreach(ctx, bus)                                              \
	for (bus = cxl_bus_get_first(ctx); bus != NULL;                        \
	     bus = cxl_bus_get_next(bus))

struct cxl_port;
struct cxl_port *cxl_port_get_first(struct cxl_port *parent);
struct cxl_port *cxl_port_get_next(struct cxl_port *port);
const char *cxl_port_get_devname(struct cxl_port *port);
int cxl_port_get_id(struct cxl_port *port);
struct cxl_ctx *cxl_port_get_ctx(struct cxl_port *port);
int cxl_port_is_enabled(struct cxl_port *port);
struct cxl_port *cxl_port_get_parent(struct cxl_port *port);
bool cxl_port_is_root(struct cxl_port *port);
bool cxl_port_is_switch(struct cxl_port *port);
int cxl_port_get_depth(struct cxl_port *port);
struct cxl_bus *cxl_port_to_bus(struct cxl_port *port);
bool cxl_port_is_endpoint(struct cxl_port *port);
struct cxl_endpoint *cxl_port_to_endpoint(struct cxl_port *port);
struct cxl_bus *cxl_port_get_bus(struct cxl_port *port);
const char *cxl_port_get_host(struct cxl_port *port);
struct cxl_dport *cxl_port_get_parent_dport(struct cxl_port *port);
bool cxl_port_hosts_memdev(struct cxl_port *port, struct cxl_memdev *memdev);
int cxl_port_get_nr_dports(struct cxl_port *port);
int cxl_port_disable_invalidate(struct cxl_port *port);
int cxl_port_enable(struct cxl_port *port);
int cxl_port_decoders_committed(struct cxl_port *port);
struct cxl_port *cxl_port_get_next_all(struct cxl_port *port,
				       const struct cxl_port *top);

#define cxl_port_foreach(parent, port)                                         \
	for (port = cxl_port_get_first(parent); port != NULL;                  \
	     port = cxl_port_get_next(port))

#define cxl_port_foreach_all(top, port)                                        \
	for (port = cxl_port_get_first(top); port != NULL;                     \
	     port = cxl_port_get_next_all(port, top))

struct cxl_dport;
struct cxl_dport *cxl_dport_get_first(struct cxl_port *port);
struct cxl_dport *cxl_dport_get_next(struct cxl_dport *dport);
const char *cxl_dport_get_devname(struct cxl_dport *dport);
const char *cxl_dport_get_physical_node(struct cxl_dport *dport);
const char *cxl_dport_get_firmware_node(struct cxl_dport *dport);
struct cxl_port *cxl_dport_get_port(struct cxl_dport *dport);
int cxl_dport_get_id(struct cxl_dport *dport);
bool cxl_dport_maps_memdev(struct cxl_dport *dport, struct cxl_memdev *memdev);
struct cxl_dport *cxl_port_get_dport_by_memdev(struct cxl_port *port,
					       struct cxl_memdev *memdev);

#define cxl_dport_foreach(port, dport)                                         \
	for (dport = cxl_dport_get_first(port); dport != NULL;                 \
	     dport = cxl_dport_get_next(dport))

#define CXL_QOS_CLASS_NONE		-1

struct cxl_decoder;
struct cxl_decoder *cxl_decoder_get_first(struct cxl_port *port);
struct cxl_decoder *cxl_decoder_get_next(struct cxl_decoder *decoder);
struct cxl_decoder *cxl_decoder_get_last(struct cxl_port *port);
struct cxl_decoder *cxl_decoder_get_prev(struct cxl_decoder *decoder);
unsigned long long cxl_decoder_get_resource(struct cxl_decoder *decoder);
unsigned long long cxl_decoder_get_size(struct cxl_decoder *decoder);
unsigned long long cxl_decoder_get_dpa_resource(struct cxl_decoder *decoder);
unsigned long long cxl_decoder_get_dpa_size(struct cxl_decoder *decoder);
unsigned long long
cxl_decoder_get_max_available_extent(struct cxl_decoder *decoder);
int cxl_root_decoder_get_qos_class(struct cxl_decoder *decoder);

enum cxl_decoder_mode {
	CXL_DECODER_MODE_NONE,
	CXL_DECODER_MODE_MIXED,
	CXL_DECODER_MODE_PMEM,
	CXL_DECODER_MODE_RAM,
};

static inline const char *cxl_decoder_mode_name(enum cxl_decoder_mode mode)
{
	static const char *names[] = {
		[CXL_DECODER_MODE_NONE] = "none",
		[CXL_DECODER_MODE_MIXED] = "mixed",
		[CXL_DECODER_MODE_PMEM] = "pmem",
		[CXL_DECODER_MODE_RAM] = "ram",
	};

	if (mode < CXL_DECODER_MODE_NONE || mode > CXL_DECODER_MODE_RAM)
		mode = CXL_DECODER_MODE_NONE;
	return names[mode];
}

static inline enum cxl_decoder_mode
cxl_decoder_mode_from_ident(const char *ident)
{
	if (strcmp(ident, "ram") == 0)
		return CXL_DECODER_MODE_RAM;
	else if (strcmp(ident, "volatile") == 0)
		return CXL_DECODER_MODE_RAM;
	else if (strcmp(ident, "pmem") == 0)
		return CXL_DECODER_MODE_PMEM;
	return CXL_DECODER_MODE_NONE;
}

enum cxl_decoder_mode cxl_decoder_get_mode(struct cxl_decoder *decoder);
int cxl_decoder_set_mode(struct cxl_decoder *decoder,
			 enum cxl_decoder_mode mode);
int cxl_decoder_set_dpa_size(struct cxl_decoder *decoder,
			     unsigned long long size);
const char *cxl_decoder_get_devname(struct cxl_decoder *decoder);
struct cxl_target *cxl_decoder_get_target_by_memdev(struct cxl_decoder *decoder,
						    struct cxl_memdev *memdev);
struct cxl_target *
cxl_decoder_get_target_by_position(struct cxl_decoder *decoder, int position);
int cxl_decoder_get_nr_targets(struct cxl_decoder *decoder);
struct cxl_ctx *cxl_decoder_get_ctx(struct cxl_decoder *decoder);
int cxl_decoder_get_id(struct cxl_decoder *decoder);
struct cxl_port *cxl_decoder_get_port(struct cxl_decoder *decoder);

enum cxl_decoder_target_type {
	CXL_DECODER_TTYPE_UNKNOWN,
	CXL_DECODER_TTYPE_EXPANDER,
	CXL_DECODER_TTYPE_ACCELERATOR,
};

enum cxl_decoder_target_type
cxl_decoder_get_target_type(struct cxl_decoder *decoder);
bool cxl_decoder_is_pmem_capable(struct cxl_decoder *decoder);
bool cxl_decoder_is_volatile_capable(struct cxl_decoder *decoder);
bool cxl_decoder_is_mem_capable(struct cxl_decoder *decoder);
bool cxl_decoder_is_accelmem_capable(struct cxl_decoder *decoder);
bool cxl_decoder_is_locked(struct cxl_decoder *decoder);
unsigned int
cxl_decoder_get_interleave_granularity(struct cxl_decoder *decoder);
unsigned int cxl_decoder_get_interleave_ways(struct cxl_decoder *decoder);
struct cxl_region *cxl_decoder_get_region(struct cxl_decoder *decoder);
struct cxl_region *cxl_decoder_create_pmem_region(struct cxl_decoder *decoder);
struct cxl_region *cxl_decoder_create_ram_region(struct cxl_decoder *decoder);
struct cxl_decoder *cxl_decoder_get_by_name(struct cxl_ctx *ctx,
					    const char *ident);
struct cxl_memdev *cxl_decoder_get_memdev(struct cxl_decoder *decoder);
#define cxl_decoder_foreach(port, decoder)                                     \
	for (decoder = cxl_decoder_get_first(port); decoder != NULL;           \
	     decoder = cxl_decoder_get_next(decoder))

#define cxl_decoder_foreach_reverse(port, decoder)                             \
	for (decoder = cxl_decoder_get_last(port); decoder != NULL;           \
	     decoder = cxl_decoder_get_prev(decoder))

struct cxl_target;
struct cxl_target *cxl_target_get_first(struct cxl_decoder *decoder);
struct cxl_target *cxl_target_get_next(struct cxl_target *target);
struct cxl_decoder *cxl_target_get_decoder(struct cxl_target *target);
int cxl_target_get_position(struct cxl_target *target);
unsigned long cxl_target_get_id(struct cxl_target *target);
const char *cxl_target_get_devname(struct cxl_target *target);
bool cxl_target_maps_memdev(struct cxl_target *target,
			    struct cxl_memdev *memdev);
const char *cxl_target_get_physical_node(struct cxl_target *target);
const char *cxl_target_get_firmware_node(struct cxl_target *target);

#define cxl_target_foreach(decoder, target)                                    \
	for (target = cxl_target_get_first(decoder); target != NULL;           \
	     target = cxl_target_get_next(target))

struct cxl_endpoint;
struct cxl_endpoint *cxl_endpoint_get_first(struct cxl_port *parent);
struct cxl_endpoint *cxl_endpoint_get_next(struct cxl_endpoint *endpoint);
const char *cxl_endpoint_get_devname(struct cxl_endpoint *endpoint);
int cxl_endpoint_get_id(struct cxl_endpoint *endpoint);
struct cxl_ctx *cxl_endpoint_get_ctx(struct cxl_endpoint *endpoint);
int cxl_endpoint_is_enabled(struct cxl_endpoint *endpoint);
struct cxl_port *cxl_endpoint_get_parent(struct cxl_endpoint *endpoint);
struct cxl_port *cxl_endpoint_get_port(struct cxl_endpoint *endpoint);
const char *cxl_endpoint_get_host(struct cxl_endpoint *endpoint);
struct cxl_bus *cxl_endpoint_get_bus(struct cxl_endpoint *endpoint);
struct cxl_memdev *cxl_endpoint_get_memdev(struct cxl_endpoint *endpoint);
int cxl_memdev_is_enabled(struct cxl_memdev *memdev);

#define cxl_endpoint_foreach(port, endpoint)                                   \
	for (endpoint = cxl_endpoint_get_first(port); endpoint != NULL;        \
	     endpoint = cxl_endpoint_get_next(endpoint))

struct cxl_region;
struct cxl_region *cxl_region_get_first(struct cxl_decoder *decoder);
struct cxl_region *cxl_region_get_next(struct cxl_region *region);
int cxl_region_decode_is_committed(struct cxl_region *region);
int cxl_region_is_enabled(struct cxl_region *region);
int cxl_region_disable(struct cxl_region *region);
int cxl_region_enable(struct cxl_region *region);
int cxl_region_delete(struct cxl_region *region);
struct cxl_ctx *cxl_region_get_ctx(struct cxl_region *region);
struct cxl_decoder *cxl_region_get_decoder(struct cxl_region *region);
int cxl_region_get_id(struct cxl_region *region);
const char *cxl_region_get_devname(struct cxl_region *region);
void cxl_region_get_uuid(struct cxl_region *region, uuid_t uu);
unsigned long long cxl_region_get_size(struct cxl_region *region);
unsigned long long cxl_region_get_resource(struct cxl_region *region);
enum cxl_decoder_mode cxl_region_get_mode(struct cxl_region *region);
unsigned int cxl_region_get_interleave_ways(struct cxl_region *region);
unsigned int cxl_region_get_interleave_granularity(struct cxl_region *region);
struct cxl_decoder *cxl_region_get_target_decoder(struct cxl_region *region,
						  int position);
struct daxctl_region *cxl_region_get_daxctl_region(struct cxl_region *region);
int cxl_region_set_size(struct cxl_region *region, unsigned long long size);
int cxl_region_set_uuid(struct cxl_region *region, uuid_t uu);
int cxl_region_set_interleave_ways(struct cxl_region *region,
				   unsigned int ways);
int cxl_region_set_interleave_granularity(struct cxl_region *region,
					  unsigned int granularity);
int cxl_region_set_target(struct cxl_region *region, int position,
			  struct cxl_decoder *decoder);
int cxl_region_clear_target(struct cxl_region *region, int position);
int cxl_region_clear_all_targets(struct cxl_region *region);
int cxl_region_decode_commit(struct cxl_region *region);
int cxl_region_decode_reset(struct cxl_region *region);
bool cxl_region_qos_class_mismatch(struct cxl_region *region);

#define cxl_region_foreach(decoder, region)                                    \
	for (region = cxl_region_get_first(decoder); region != NULL;           \
	     region = cxl_region_get_next(region))

#define cxl_region_foreach_safe(decoder, region, _region)                      \
	for (region = cxl_region_get_first(decoder),                           \
	     _region = region ? cxl_region_get_next(region) : NULL;            \
	     region != NULL;                                                   \
	     region = _region,                                                 \
	     _region = _region ? cxl_region_get_next(_region) : NULL)

struct cxl_memdev_mapping;
struct cxl_memdev_mapping *cxl_mapping_get_first(struct cxl_region *region);
struct cxl_memdev_mapping *
cxl_mapping_get_next(struct cxl_memdev_mapping *mapping);
struct cxl_decoder *cxl_mapping_get_decoder(struct cxl_memdev_mapping *mapping);
unsigned int cxl_mapping_get_position(struct cxl_memdev_mapping *mapping);

#define cxl_mapping_foreach(region, mapping) \
        for (mapping = cxl_mapping_get_first(region); \
             mapping != NULL; \
             mapping = cxl_mapping_get_next(mapping))

struct cxl_cmd;
const char *cxl_cmd_get_devname(struct cxl_cmd *cmd);
struct cxl_cmd *cxl_cmd_new_raw(struct cxl_memdev *memdev, int opcode);
int cxl_cmd_set_input_payload(struct cxl_cmd *cmd, void *in, int size);
int cxl_cmd_set_output_payload(struct cxl_cmd *cmd, void *out, int size);
void cxl_cmd_ref(struct cxl_cmd *cmd);
void cxl_cmd_unref(struct cxl_cmd *cmd);
int cxl_cmd_submit(struct cxl_cmd *cmd);
int cxl_cmd_get_mbox_status(struct cxl_cmd *cmd);
int cxl_cmd_get_out_size(struct cxl_cmd *cmd);
struct cxl_cmd *cxl_cmd_new_identify(struct cxl_memdev *memdev);
int cxl_cmd_identify_get_fw_rev(struct cxl_cmd *cmd, char *fw_rev, int fw_len);
unsigned long long cxl_cmd_identify_get_total_size(struct cxl_cmd *cmd);
unsigned long long cxl_cmd_identify_get_volatile_only_size(struct cxl_cmd *cmd);
unsigned long long cxl_cmd_identify_get_persistent_only_size(struct cxl_cmd *cmd);
unsigned long long cxl_cmd_identify_get_partition_align(struct cxl_cmd *cmd);
unsigned int cxl_cmd_identify_get_label_size(struct cxl_cmd *cmd);
struct cxl_cmd *cxl_cmd_new_get_health_info(struct cxl_memdev *memdev);
int cxl_cmd_health_info_get_maintenance_needed(struct cxl_cmd *cmd);
int cxl_cmd_health_info_get_performance_degraded(struct cxl_cmd *cmd);
int cxl_cmd_health_info_get_hw_replacement_needed(struct cxl_cmd *cmd);
int cxl_cmd_health_info_get_media_normal(struct cxl_cmd *cmd);
int cxl_cmd_health_info_get_media_not_ready(struct cxl_cmd *cmd);
int cxl_cmd_health_info_get_media_persistence_lost(struct cxl_cmd *cmd);
int cxl_cmd_health_info_get_media_data_lost(struct cxl_cmd *cmd);
int cxl_cmd_health_info_get_media_normal(struct cxl_cmd *cmd);
int cxl_cmd_health_info_get_media_not_ready(struct cxl_cmd *cmd);
int cxl_cmd_health_info_get_media_persistence_lost(struct cxl_cmd *cmd);
int cxl_cmd_health_info_get_media_data_lost(struct cxl_cmd *cmd);
int cxl_cmd_health_info_get_media_powerloss_persistence_loss(struct cxl_cmd *cmd);
int cxl_cmd_health_info_get_media_shutdown_persistence_loss(struct cxl_cmd *cmd);
int cxl_cmd_health_info_get_media_persistence_loss_imminent(struct cxl_cmd *cmd);
int cxl_cmd_health_info_get_media_powerloss_data_loss(struct cxl_cmd *cmd);
int cxl_cmd_health_info_get_media_shutdown_data_loss(struct cxl_cmd *cmd);
int cxl_cmd_health_info_get_media_data_loss_imminent(struct cxl_cmd *cmd);
int cxl_cmd_health_info_get_ext_life_used_normal(struct cxl_cmd *cmd);
int cxl_cmd_health_info_get_ext_life_used_warning(struct cxl_cmd *cmd);
int cxl_cmd_health_info_get_ext_life_used_critical(struct cxl_cmd *cmd);
int cxl_cmd_health_info_get_ext_temperature_normal(struct cxl_cmd *cmd);
int cxl_cmd_health_info_get_ext_temperature_warning(struct cxl_cmd *cmd);
int cxl_cmd_health_info_get_ext_temperature_critical(struct cxl_cmd *cmd);
int cxl_cmd_health_info_get_ext_corrected_volatile_normal(struct cxl_cmd *cmd);
int cxl_cmd_health_info_get_ext_corrected_volatile_warning(struct cxl_cmd *cmd);
int cxl_cmd_health_info_get_ext_corrected_persistent_normal(struct cxl_cmd *cmd);
int cxl_cmd_health_info_get_ext_corrected_persistent_warning(struct cxl_cmd *cmd);
int cxl_cmd_health_info_get_life_used(struct cxl_cmd *cmd);
int cxl_cmd_health_info_get_temperature(struct cxl_cmd *cmd);
int cxl_cmd_health_info_get_dirty_shutdowns(struct cxl_cmd *cmd);
int cxl_cmd_health_info_get_volatile_errors(struct cxl_cmd *cmd);
int cxl_cmd_health_info_get_pmem_errors(struct cxl_cmd *cmd);
struct cxl_cmd *cxl_cmd_new_get_alert_config(struct cxl_memdev *memdev);
int cxl_cmd_alert_config_life_used_prog_warn_threshold_valid(
	struct cxl_cmd *cmd);
int cxl_cmd_alert_config_dev_over_temperature_prog_warn_threshold_valid(
	struct cxl_cmd *cmd);
int cxl_cmd_alert_config_dev_under_temperature_prog_warn_threshold_valid(
	struct cxl_cmd *cmd);
int cxl_cmd_alert_config_corrected_volatile_mem_err_prog_warn_threshold_valid(
	struct cxl_cmd *cmd);
int cxl_cmd_alert_config_corrected_pmem_err_prog_warn_threshold_valid(
	struct cxl_cmd *cmd);
int cxl_cmd_alert_config_life_used_prog_warn_threshold_writable(
	struct cxl_cmd *cmd);
int cxl_cmd_alert_config_dev_over_temperature_prog_warn_threshold_writable(
	struct cxl_cmd *cmd);
int cxl_cmd_alert_config_dev_under_temperature_prog_warn_threshold_writable(
	struct cxl_cmd *cmd);
int cxl_cmd_alert_config_corrected_volatile_mem_err_prog_warn_threshold_writable(
	struct cxl_cmd *cmd);
int cxl_cmd_alert_config_corrected_pmem_err_prog_warn_threshold_writable(
	struct cxl_cmd *cmd);
int cxl_cmd_alert_config_get_life_used_crit_alert_threshold(struct cxl_cmd *cmd);
int cxl_cmd_alert_config_get_life_used_prog_warn_threshold(struct cxl_cmd *cmd);
int cxl_cmd_alert_config_get_dev_over_temperature_crit_alert_threshold(
	struct cxl_cmd *cmd);
int cxl_cmd_alert_config_get_dev_under_temperature_crit_alert_threshold(
	struct cxl_cmd *cmd);
int cxl_cmd_alert_config_get_dev_over_temperature_prog_warn_threshold(
	struct cxl_cmd *cmd);
int cxl_cmd_alert_config_get_dev_under_temperature_prog_warn_threshold(
	struct cxl_cmd *cmd);
int cxl_cmd_alert_config_get_corrected_volatile_mem_err_prog_warn_threshold(
	struct cxl_cmd *cmd);
int cxl_cmd_alert_config_get_corrected_pmem_err_prog_warn_threshold(
	struct cxl_cmd *cmd);
struct cxl_cmd *cxl_cmd_new_read_label(struct cxl_memdev *memdev,
		unsigned int offset, unsigned int length);
ssize_t cxl_cmd_read_label_get_payload(struct cxl_cmd *cmd, void *buf,
		unsigned int length);
struct cxl_cmd *cxl_cmd_new_write_label(struct cxl_memdev *memdev,
		void *buf, unsigned int offset, unsigned int length);
struct cxl_cmd *cxl_cmd_new_get_partition(struct cxl_memdev *memdev);
unsigned long long cxl_cmd_partition_get_active_volatile_size(struct cxl_cmd *cmd);
unsigned long long cxl_cmd_partition_get_active_persistent_size(struct cxl_cmd *cmd);
unsigned long long cxl_cmd_partition_get_next_volatile_size(struct cxl_cmd *cmd);
unsigned long long cxl_cmd_partition_get_next_persistent_size(struct cxl_cmd *cmd);
struct cxl_cmd *cxl_cmd_new_set_partition(struct cxl_memdev *memdev,
		unsigned long long volatile_size);

enum cxl_setpartition_mode {
	CXL_SETPART_NEXTBOOT,
	CXL_SETPART_IMMEDIATE,
};

int cxl_cmd_partition_set_mode(struct cxl_cmd *cmd,
		enum cxl_setpartition_mode mode);
int cxl_memdev_trigger_poison_list(struct cxl_memdev *memdev);
int cxl_region_trigger_poison_list(struct cxl_region *region);

int cxl_cmd_alert_config_set_life_used_prog_warn_threshold(struct cxl_cmd *cmd,
							   int threshold);
int cxl_cmd_alert_config_set_dev_over_temperature_prog_warn_threshold(
	struct cxl_cmd *cmd, int threshold);
int cxl_cmd_alert_config_set_dev_under_temperature_prog_warn_threshold(
	struct cxl_cmd *cmd, int threshold);
int cxl_cmd_alert_config_set_corrected_volatile_mem_err_prog_warn_threshold(
	struct cxl_cmd *cmd, int threshold);
int cxl_cmd_alert_config_set_corrected_pmem_err_prog_warn_threshold(
	struct cxl_cmd *cmd, int threshold);
int cxl_cmd_alert_config_set_valid_alert_actions(struct cxl_cmd *cmd,
						 int action);
int cxl_cmd_alert_config_set_enable_alert_actions(struct cxl_cmd *cmd,
						  int enable);
struct cxl_cmd *cxl_cmd_new_set_alert_config(struct cxl_memdev *memdev);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
