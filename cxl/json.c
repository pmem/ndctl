// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2015-2021 Intel Corporation. All rights reserved.
#include <limits.h>
#include <util/json.h>
#include <uuid/uuid.h>
#include <cxl/libcxl.h>
#include <json-c/json.h>
#include <json-c/printbuf.h>
#include <ccan/short_types/short_types.h>

#include "filter.h"
#include "json.h"

static struct json_object *util_cxl_memdev_health_to_json(
		struct cxl_memdev *memdev, unsigned long flags)
{
	struct json_object *jhealth;
	struct json_object *jobj;
	struct cxl_cmd *cmd;
	u32 field;
	int rc;

	jhealth = json_object_new_object();
	if (!jhealth)
		return NULL;
	if (!memdev)
		goto err_jobj;

	cmd = cxl_cmd_new_get_health_info(memdev);
	if (!cmd)
		goto err_jobj;

	rc = cxl_cmd_submit(cmd);
	if (rc < 0)
		goto err_cmd;
	rc = cxl_cmd_get_mbox_status(cmd);
	if (rc != 0)
		goto err_cmd;

	/* health_status fields */
	rc = cxl_cmd_health_info_get_maintenance_needed(cmd);
	jobj = json_object_new_boolean(rc);
	if (jobj)
		json_object_object_add(jhealth, "maintenance_needed", jobj);

	rc = cxl_cmd_health_info_get_performance_degraded(cmd);
	jobj = json_object_new_boolean(rc);
	if (jobj)
		json_object_object_add(jhealth, "performance_degraded", jobj);

	rc = cxl_cmd_health_info_get_hw_replacement_needed(cmd);
	jobj = json_object_new_boolean(rc);
	if (jobj)
		json_object_object_add(jhealth, "hw_replacement_needed", jobj);

	/* media_status fields */
	rc = cxl_cmd_health_info_get_media_normal(cmd);
	jobj = json_object_new_boolean(rc);
	if (jobj)
		json_object_object_add(jhealth, "media_normal", jobj);

	rc = cxl_cmd_health_info_get_media_not_ready(cmd);
	jobj = json_object_new_boolean(rc);
	if (jobj)
		json_object_object_add(jhealth, "media_not_ready", jobj);

	rc = cxl_cmd_health_info_get_media_persistence_lost(cmd);
	jobj = json_object_new_boolean(rc);
	if (jobj)
		json_object_object_add(jhealth, "media_persistence_lost", jobj);

	rc = cxl_cmd_health_info_get_media_data_lost(cmd);
	jobj = json_object_new_boolean(rc);
	if (jobj)
		json_object_object_add(jhealth, "media_data_lost", jobj);

	rc = cxl_cmd_health_info_get_media_powerloss_persistence_loss(cmd);
	jobj = json_object_new_boolean(rc);
	if (jobj)
		json_object_object_add(jhealth, "media_powerloss_persistence_loss", jobj);

	rc = cxl_cmd_health_info_get_media_shutdown_persistence_loss(cmd);
	jobj = json_object_new_boolean(rc);
	if (jobj)
		json_object_object_add(jhealth, "media_shutdown_persistence_loss", jobj);

	rc = cxl_cmd_health_info_get_media_persistence_loss_imminent(cmd);
	jobj = json_object_new_boolean(rc);
	if (jobj)
		json_object_object_add(jhealth, "media_persistence_loss_imminent", jobj);

	rc = cxl_cmd_health_info_get_media_powerloss_data_loss(cmd);
	jobj = json_object_new_boolean(rc);
	if (jobj)
		json_object_object_add(jhealth, "media_powerloss_data_loss", jobj);

	rc = cxl_cmd_health_info_get_media_shutdown_data_loss(cmd);
	jobj = json_object_new_boolean(rc);
	if (jobj)
		json_object_object_add(jhealth, "media_shutdown_data_loss", jobj);

	rc = cxl_cmd_health_info_get_media_data_loss_imminent(cmd);
	jobj = json_object_new_boolean(rc);
	if (jobj)
		json_object_object_add(jhealth, "media_data_loss_imminent", jobj);

	/* ext_status fields */
	if (cxl_cmd_health_info_get_ext_life_used_normal(cmd))
		jobj = json_object_new_string("normal");
	else if (cxl_cmd_health_info_get_ext_life_used_warning(cmd))
		jobj = json_object_new_string("warning");
	else if (cxl_cmd_health_info_get_ext_life_used_critical(cmd))
		jobj = json_object_new_string("critical");
	else
		jobj = json_object_new_string("unknown");
	if (jobj)
		json_object_object_add(jhealth, "ext_life_used", jobj);

	if (cxl_cmd_health_info_get_ext_temperature_normal(cmd))
		jobj = json_object_new_string("normal");
	else if (cxl_cmd_health_info_get_ext_temperature_warning(cmd))
		jobj = json_object_new_string("warning");
	else if (cxl_cmd_health_info_get_ext_temperature_critical(cmd))
		jobj = json_object_new_string("critical");
	else
		jobj = json_object_new_string("unknown");
	if (jobj)
		json_object_object_add(jhealth, "ext_temperature", jobj);

	if (cxl_cmd_health_info_get_ext_corrected_volatile_normal(cmd))
		jobj = json_object_new_string("normal");
	else if (cxl_cmd_health_info_get_ext_corrected_volatile_warning(cmd))
		jobj = json_object_new_string("warning");
	else
		jobj = json_object_new_string("unknown");
	if (jobj)
		json_object_object_add(jhealth, "ext_corrected_volatile", jobj);

	if (cxl_cmd_health_info_get_ext_corrected_persistent_normal(cmd))
		jobj = json_object_new_string("normal");
	else if (cxl_cmd_health_info_get_ext_corrected_persistent_warning(cmd))
		jobj = json_object_new_string("warning");
	else
		jobj = json_object_new_string("unknown");
	if (jobj)
		json_object_object_add(jhealth, "ext_corrected_persistent", jobj);

	/* other fields */
	field = cxl_cmd_health_info_get_life_used(cmd);
	if (field != 0xff) {
		jobj = json_object_new_int(field);
		if (jobj)
			json_object_object_add(jhealth, "life_used_percent", jobj);
	}

	field = cxl_cmd_health_info_get_temperature(cmd);
	if (field != 0xffff) {
		jobj = json_object_new_int(field);
		if (jobj)
			json_object_object_add(jhealth, "temperature", jobj);
	}

	field = cxl_cmd_health_info_get_dirty_shutdowns(cmd);
	jobj = util_json_new_u64(field);
	if (jobj)
		json_object_object_add(jhealth, "dirty_shutdowns", jobj);

	field = cxl_cmd_health_info_get_volatile_errors(cmd);
	jobj = util_json_new_u64(field);
	if (jobj)
		json_object_object_add(jhealth, "volatile_errors", jobj);

	field = cxl_cmd_health_info_get_pmem_errors(cmd);
	jobj = util_json_new_u64(field);
	if (jobj)
		json_object_object_add(jhealth, "pmem_errors", jobj);

	cxl_cmd_unref(cmd);
	return jhealth;

err_cmd:
	cxl_cmd_unref(cmd);
err_jobj:
	json_object_put(jhealth);
	return NULL;
}

/*
 * Present complete view of memdev partition by presenting fields from
 * both GET_PARTITION_INFO and IDENTIFY mailbox commands.
 */
static struct json_object *util_cxl_memdev_partition_to_json(struct cxl_memdev *memdev,
		unsigned long flags)
{
	struct json_object *jobj = NULL;
	struct json_object *jpart;
	unsigned long long cap;
	struct cxl_cmd *cmd;
	int rc;

	jpart = json_object_new_object();
	if (!jpart)
		return NULL;
	if (!memdev)
		goto err_jobj;

	/* Retrieve partition info in the IDENTIFY mbox cmd */
	cmd = cxl_cmd_new_identify(memdev);
	if (!cmd)
		goto err_jobj;

	rc = cxl_cmd_submit(cmd);
	if (rc < 0)
		goto err_identify;
	rc = cxl_cmd_get_mbox_status(cmd);
	if (rc != 0)
		goto err_identify;

	cap = cxl_cmd_identify_get_total_size(cmd);
	if (cap != ULLONG_MAX) {
		jobj = util_json_object_size(cap, flags);
		if (jobj)
			json_object_object_add(jpart, "total_size", jobj);
	}
	cap = cxl_cmd_identify_get_volatile_only_size(cmd);
	if (cap != ULLONG_MAX) {
		jobj = util_json_object_size(cap, flags);
		if (jobj)
			json_object_object_add(jpart,
					"volatile_only_size", jobj);
	}
	cap = cxl_cmd_identify_get_persistent_only_size(cmd);
	if (cap != ULLONG_MAX) {
		jobj = util_json_object_size(cap, flags);
		if (jobj)
			json_object_object_add(jpart,
					"persistent_only_size", jobj);
	}
	cap = cxl_cmd_identify_get_partition_align(cmd);
	jobj = util_json_object_size(cap, flags);
	if (jobj)
		json_object_object_add(jpart, "partition_alignment_size", jobj);

	cxl_cmd_unref(cmd);

	/* Return now if there is no partition info to get. */
	if (!cap)
		return jpart;

	/* Retrieve partition info in GET_PARTITION_INFO mbox cmd */
	cmd = cxl_cmd_new_get_partition(memdev);
	if (!cmd)
		return jpart;

	rc = cxl_cmd_submit(cmd);
	if (rc < 0)
		goto err_get;
	rc = cxl_cmd_get_mbox_status(cmd);
	if (rc != 0)
		goto err_get;

	cap = cxl_cmd_partition_get_active_volatile_size(cmd);
	if (cap != ULLONG_MAX) {
		jobj = util_json_object_size(cap, flags);
		if (jobj)
			json_object_object_add(jpart,
					"active_volatile_size", jobj);
	}
	cap = cxl_cmd_partition_get_active_persistent_size(cmd);
	if (cap != ULLONG_MAX) {
		jobj = util_json_object_size(cap, flags);
		if (jobj)
			json_object_object_add(jpart,
					"active_persistent_size", jobj);
	}
	cap = cxl_cmd_partition_get_next_volatile_size(cmd);
	if (cap != ULLONG_MAX) {
		jobj = util_json_object_size(cap, flags);
		if (jobj)
			json_object_object_add(jpart,
					"next_volatile_size", jobj);
	}
	cap = cxl_cmd_partition_get_next_persistent_size(cmd);
	if (cap != ULLONG_MAX) {
		jobj = util_json_object_size(cap, flags);
		if (jobj)
			json_object_object_add(jpart,
					"next_persistent_size", jobj);
	}

err_get:
	cxl_cmd_unref(cmd);
	return jpart;

err_identify:
	cxl_cmd_unref(cmd);

err_jobj:
	json_object_put(jpart);
	return NULL;
}

struct json_object *util_cxl_memdev_to_json(struct cxl_memdev *memdev,
		unsigned long flags)
{
	const char *devname = cxl_memdev_get_devname(memdev);
	struct json_object *jdev, *jobj;
	unsigned long long serial;
	int numa_node;

	jdev = json_object_new_object();
	if (!jdev)
		return NULL;

	jobj = json_object_new_string(devname);
	if (jobj)
		json_object_object_add(jdev, "memdev", jobj);

	jobj = util_json_object_size(cxl_memdev_get_pmem_size(memdev), flags);
	if (jobj)
		json_object_object_add(jdev, "pmem_size", jobj);

	jobj = util_json_object_size(cxl_memdev_get_ram_size(memdev), flags);
	if (jobj)
		json_object_object_add(jdev, "ram_size", jobj);

	if (flags & UTIL_JSON_HEALTH) {
		jobj = util_cxl_memdev_health_to_json(memdev, flags);
		if (jobj)
			json_object_object_add(jdev, "health", jobj);
	}

	serial = cxl_memdev_get_serial(memdev);
	if (serial < ULLONG_MAX) {
		jobj = util_json_object_hex(serial, flags);
		if (jobj)
			json_object_object_add(jdev, "serial", jobj);
	}

	numa_node = cxl_memdev_get_numa_node(memdev);
	if (numa_node >= 0) {
		jobj = json_object_new_int(numa_node);
		if (jobj)
			json_object_object_add(jdev, "numa_node", jobj);
	}

	jobj = json_object_new_string(cxl_memdev_get_host(memdev));
	if (jobj)
		json_object_object_add(jdev, "host", jobj);

	if (!cxl_memdev_is_enabled(memdev)) {
		jobj = json_object_new_string("disabled");
		if (jobj)
			json_object_object_add(jdev, "state", jobj);
	}

	if (flags & UTIL_JSON_PARTITION) {
		jobj = util_cxl_memdev_partition_to_json(memdev, flags);
		if (jobj)
			json_object_object_add(jdev, "partition_info", jobj);
	}
	return jdev;
}

void util_cxl_dports_append_json(struct json_object *jport,
				 struct cxl_port *port, const char *ident,
				 const char *serial, unsigned long flags)
{
	struct json_object *jobj, *jdports;
	struct cxl_dport *dport;
	int val;

	val = cxl_port_get_nr_dports(port);
	if (!val || !(flags & UTIL_JSON_TARGETS))
		return;

	jobj = json_object_new_int(val);
	if (jobj)
		json_object_object_add(jport, "nr_dports", jobj);

	jdports = json_object_new_array();
	if (!jdports)
		return;

	cxl_dport_foreach(port, dport) {
		struct json_object *jdport;
		const char *phys_node;

		if (!util_cxl_dport_filter_by_memdev(dport, ident, serial))
			continue;

		jdport = json_object_new_object();
		if (!jdport)
			continue;

		jobj = json_object_new_string(cxl_dport_get_devname(dport));
		if (jobj)
			json_object_object_add(jdport, "dport", jobj);

		phys_node = cxl_dport_get_physical_node(dport);
		if (phys_node) {
			jobj = json_object_new_string(phys_node);
			if (jobj)
				json_object_object_add(jdport, "alias", jobj);
		}

		val = cxl_dport_get_id(dport);
		jobj = util_json_object_hex(val, flags);
		if (jobj)
			json_object_object_add(jdport, "id", jobj);

		json_object_array_add(jdports, jdport);
	}

	json_object_object_add(jport, "dports", jdports);
}

struct json_object *util_cxl_bus_to_json(struct cxl_bus *bus,
					 unsigned long flags)
{
	const char *devname = cxl_bus_get_devname(bus);
	struct json_object *jbus, *jobj;

	jbus = json_object_new_object();
	if (!jbus)
		return NULL;

	jobj = json_object_new_string(devname);
	if (jobj)
		json_object_object_add(jbus, "bus", jobj);

	jobj = json_object_new_string(cxl_bus_get_provider(bus));
	if (jobj)
		json_object_object_add(jbus, "provider", jobj);

	return jbus;
}

struct json_object *util_cxl_decoder_to_json(struct cxl_decoder *decoder,
					     unsigned long flags)
{
	const char *devname = cxl_decoder_get_devname(decoder);
	struct cxl_port *port = cxl_decoder_get_port(decoder);
	struct json_object *jdecoder, *jobj;
	u64 val, size;

	jdecoder = json_object_new_object();
	if (!jdecoder)
		return NULL;

	jobj = json_object_new_string(devname);
	if (jobj)
		json_object_object_add(jdecoder, "decoder", jobj);

	size = cxl_decoder_get_size(decoder);
	val = cxl_decoder_get_resource(decoder);
	if (size && val < ULLONG_MAX) {
		jobj = util_json_object_hex(val, flags);
		if (jobj)
			json_object_object_add(jdecoder, "resource", jobj);
	}

	if (size && size < ULLONG_MAX) {
		jobj = util_json_object_size(size, flags);
		if (jobj)
			json_object_object_add(jdecoder, "size", jobj);
	}

	val = cxl_decoder_get_interleave_ways(decoder);
	if (val < UINT_MAX) {
		jobj = json_object_new_int(val);
		if (jobj)
			json_object_object_add(jdecoder, "interleave_ways",
					       jobj);

		/* granularity is a don't care if not interleaving */
		if (val > 1) {
			val = cxl_decoder_get_interleave_granularity(decoder);
			if (val < UINT_MAX) {
				jobj = json_object_new_int(val);
				if (jobj)
					json_object_object_add(
						jdecoder,
						"interleave_granularity", jobj);
			}
		}
	}

	if (size == 0) {
		jobj = json_object_new_string("disabled");
		if (jobj)
			json_object_object_add(jdecoder, "state", jobj);
	}

	if (cxl_port_is_endpoint(port)) {
		enum cxl_decoder_mode mode = cxl_decoder_get_mode(decoder);

		size = cxl_decoder_get_dpa_size(decoder);
		val = cxl_decoder_get_dpa_resource(decoder);
		if (size && val < ULLONG_MAX) {
			jobj = util_json_object_hex(val, flags);
			if (jobj)
				json_object_object_add(jdecoder, "dpa_resource",
						       jobj);
		}

		if (size && size < ULLONG_MAX) {
			jobj = util_json_object_size(size, flags);
			if (jobj)
				json_object_object_add(jdecoder, "dpa_size",
						       jobj);
		}

		if (mode > CXL_DECODER_MODE_NONE) {
			jobj = json_object_new_string(cxl_decoder_mode_name(mode));
			if (jobj)
				json_object_object_add(jdecoder, "mode", jobj);
		}
	}

	if (cxl_port_is_root(port) && cxl_decoder_is_mem_capable(decoder)) {
		size = cxl_decoder_get_max_available_extent(decoder);
		if (size < ULLONG_MAX) {
			jobj = util_json_object_size(size, flags);
			if (jobj)
				json_object_object_add(jdecoder,
						       "max_available_extent",
						       jobj);
		}
		if (cxl_decoder_is_pmem_capable(decoder)) {
			jobj = json_object_new_boolean(true);
			if (jobj)
				json_object_object_add(jdecoder, "pmem_capable",
						       jobj);
		}
		if (cxl_decoder_is_volatile_capable(decoder)) {
			jobj = json_object_new_boolean(true);
			if (jobj)
				json_object_object_add(
					jdecoder, "volatile_capable", jobj);
		}
	}

	if (cxl_port_is_root(port) &&
	    cxl_decoder_is_accelmem_capable(decoder)) {
		jobj = json_object_new_boolean(true);
		if (jobj)
			json_object_object_add(jdecoder, "accelmem_capable",
					       jobj);
	}

	return jdecoder;
}

void util_cxl_mappings_append_json(struct json_object *jregion,
				  struct cxl_region *region,
				  unsigned long flags)
{
	struct json_object *jobj, *jmappings;
	struct cxl_memdev_mapping *mapping;
	unsigned int val, nr_mappings;
	const char *devname;

	nr_mappings = cxl_region_get_interleave_ways(region);
	if (!nr_mappings || (nr_mappings == UINT_MAX))
		return;

	if (!(flags & UTIL_JSON_TARGETS))
		return;

	jmappings = json_object_new_array();
	if (!jmappings)
		return;

	cxl_mapping_foreach(region, mapping) {
		struct json_object *jmapping;
		struct cxl_decoder *decoder;
		struct cxl_memdev *memdev;

		jmapping = json_object_new_object();
		if (!jmapping)
			continue;

		val = cxl_mapping_get_position(mapping);
		if (val < UINT_MAX) {
			jobj = json_object_new_int(val);
			if (jobj)
				json_object_object_add(jmapping, "position",
						       jobj);
		}

		decoder = cxl_mapping_get_decoder(mapping);
		if (!decoder)
			continue;

		memdev = cxl_decoder_get_memdev(decoder);
		if (memdev) {
			devname = cxl_memdev_get_devname(memdev);
			jobj = json_object_new_string(devname);
			if (jobj)
				json_object_object_add(jmapping, "memdev", jobj);
		}

		devname = cxl_decoder_get_devname(decoder);
		jobj = json_object_new_string(devname);
		if (jobj)
			json_object_object_add(jmapping, "decoder", jobj);

		json_object_array_add(jmappings, jmapping);
	}

	json_object_object_add(jregion, "mappings", jmappings);
}

struct json_object *util_cxl_region_to_json(struct cxl_region *region,
					     unsigned long flags)
{
	const char *devname = cxl_region_get_devname(region);
	struct json_object *jregion, *jobj;
	u64 val;

	jregion = json_object_new_object();
	if (!jregion)
		return NULL;

	jobj = json_object_new_string(devname);
	if (jobj)
		json_object_object_add(jregion, "region", jobj);

	val = cxl_region_get_resource(region);
	if (val < ULLONG_MAX) {
		jobj = util_json_object_hex(val, flags);
		if (jobj)
			json_object_object_add(jregion, "resource", jobj);
	}

	val = cxl_region_get_size(region);
	if (val < ULLONG_MAX) {
		jobj = util_json_object_size(val, flags);
		if (jobj)
			json_object_object_add(jregion, "size", jobj);
	}

	val = cxl_region_get_interleave_ways(region);
	if (val < INT_MAX) {
		jobj = json_object_new_int(val);
		if (jobj)
			json_object_object_add(jregion,
					       "interleave_ways", jobj);
	}

	val = cxl_region_get_interleave_granularity(region);
	if (val < INT_MAX) {
		jobj = json_object_new_int(val);
		if (jobj)
			json_object_object_add(jregion,
					       "interleave_granularity", jobj);
	}

	if (cxl_region_decode_is_committed(region))
		jobj = json_object_new_string("commit");
	else
		jobj = json_object_new_string("reset");
	if (jobj)
		json_object_object_add(jregion, "decode_state", jobj);

	if (!cxl_region_is_enabled(region)) {
		jobj = json_object_new_string("disabled");
		if (jobj)
			json_object_object_add(jregion, "state", jobj);
	}

	util_cxl_mappings_append_json(jregion, region, flags);

	return jregion;
}

void util_cxl_targets_append_json(struct json_object *jdecoder,
				  struct cxl_decoder *decoder,
				  const char *ident, const char *serial,
				  unsigned long flags)
{
	struct cxl_port *port = cxl_decoder_get_port(decoder);
	struct json_object *jobj, *jtargets;
	struct cxl_target *target;
	int val;

	/* Endpoints don't have targets, they *are* targets */
	if (cxl_port_is_endpoint(port))
		return;

	val = cxl_decoder_get_nr_targets(decoder);
	jobj = json_object_new_int(val);
	if (jobj)
		json_object_object_add(jdecoder, "nr_targets", jobj);

	if (!(flags & UTIL_JSON_TARGETS) ||
	    !cxl_decoder_get_nr_targets(decoder))
		return;

	jtargets = json_object_new_array();
	if (!jtargets)
		return;

	cxl_target_foreach(decoder, target) {
		struct json_object *jtarget;
		const char *phys_node;
		const char *devname;

		if (!util_cxl_target_filter_by_memdev(target, ident, serial))
			continue;

		jtarget = json_object_new_object();
		if (!jtarget)
			continue;

		devname = cxl_target_get_devname(target);
		jobj = json_object_new_string(devname);
		if (jobj)
			json_object_object_add(jtarget, "target", jobj);

		phys_node = cxl_target_get_physical_node(target);
		if (phys_node) {
			jobj = json_object_new_string(phys_node);
			if (jobj)
				json_object_object_add(jtarget, "alias", jobj);
		}

		val = cxl_target_get_position(target);
		jobj = json_object_new_int(val);
		if (jobj)
			json_object_object_add(jtarget, "position", jobj);

		val = cxl_target_get_id(target);
		jobj = util_json_object_hex(val, flags);
		if (jobj)
			json_object_object_add(jtarget, "id", jobj);

		json_object_array_add(jtargets, jtarget);
	}

	json_object_object_add(jdecoder, "targets", jtargets);
}

static struct json_object *__util_cxl_port_to_json(struct cxl_port *port,
						   const char *name_key,
						   unsigned long flags)
{
	const char *devname = cxl_port_get_devname(port);
	struct json_object *jport, *jobj;

	jport = json_object_new_object();
	if (!jport)
		return NULL;

	jobj = json_object_new_string(devname);
	if (jobj)
		json_object_object_add(jport, name_key, jobj);

	jobj = json_object_new_string(cxl_port_get_host(port));
	if (jobj)
		json_object_object_add(jport, "host", jobj);

	if (!cxl_port_is_enabled(port)) {
		jobj = json_object_new_string("disabled");
		if (jobj)
			json_object_object_add(jport, "state", jobj);
	}

	return jport;
}

struct json_object *util_cxl_port_to_json(struct cxl_port *port,
					  unsigned long flags)
{
	return __util_cxl_port_to_json(port, "port", flags);
}

struct json_object *util_cxl_endpoint_to_json(struct cxl_endpoint *endpoint,
					      unsigned long flags)
{
	return __util_cxl_port_to_json(cxl_endpoint_get_port(endpoint),
				       "endpoint", flags);
}
