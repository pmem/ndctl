// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2015-2020 Intel Corporation. All rights reserved.
#include <limits.h>
#include <string.h>
#include <util/json.h>
#include <util/filter.h>
#include <uuid/uuid.h>
#include <json-c/json.h>
#include <json-c/printbuf.h>
#include <ndctl/libndctl.h>
#include <daxctl/libdaxctl.h>
#include <ccan/array_size/array_size.h>
#include <ccan/short_types/short_types.h>
#include <ndctl.h>

/* adapted from mdadm::human_size_brief() */
static int display_size(struct json_object *jobj, struct printbuf *pbuf,
		int level, int flags)
{
	unsigned long long bytes = json_object_get_int64(jobj);
	static char buf[128];
	int c;

	/*
	 * We convert bytes to either centi-M{ega,ibi}bytes or
	 * centi-G{igi,ibi}bytes, with appropriate rounding, and then print
	 * 1/100th of those as a decimal.  We allow upto 2048Megabytes before
	 * converting to gigabytes, as that shows more precision and isn't too
	 * large a number.  Terabytes are not yet handled.
	 *
	 * If prefix == IEC, we mean prefixes like kibi,mebi,gibi etc.
	 * If prefix == JEDEC, we mean prefixes like kilo,mega,giga etc.
	 */

	if (bytes < 5000*1024)
		snprintf(buf, sizeof(buf), "%lld", bytes);
	else {
		/* IEC */
		if (bytes < 2*1024LL*1024LL*1024LL) {
			long cMiB = (bytes * 200LL / (1LL<<20) +1) /2;

			c = snprintf(buf, sizeof(buf), "\"%ld.%02ld MiB",
					cMiB/100 , cMiB % 100);
		} else {
			long cGiB = (bytes * 200LL / (1LL<<30) +1) /2;

			c = snprintf(buf, sizeof(buf), "\"%ld.%02ld GiB",
					cGiB/100 , cGiB % 100);
		}

		/* JEDEC */
		if (bytes < 2*1024LL*1024LL*1024LL) {
			long cMB  = (bytes / (1000000LL / 200LL) + 1) / 2;

			snprintf(buf + c, sizeof(buf) - c, " (%ld.%02ld MB)\"",
					cMB/100, cMB % 100);
		} else {
			long cGB  = (bytes / (1000000000LL/200LL) + 1) / 2;

			snprintf(buf + c, sizeof(buf) - c, " (%ld.%02ld GB)\"",
					cGB/100 , cGB % 100);
		}
	}

	return printbuf_memappend(pbuf, buf, strlen(buf));
}

static int display_hex(struct json_object *jobj, struct printbuf *pbuf,
		int level, int flags)
{
	unsigned long long val = json_object_get_int64(jobj);
	static char buf[32];

	snprintf(buf, sizeof(buf), "\"%#llx\"", val);
	return printbuf_memappend(pbuf, buf, strlen(buf));
}

struct json_object *util_json_object_size(unsigned long long size,
		unsigned long flags)
{
	struct json_object *jobj = json_object_new_int64(size);

	if (jobj && (flags & UTIL_JSON_HUMAN))
		json_object_set_serializer(jobj, display_size, NULL, NULL);
	return jobj;
}

struct json_object *util_json_object_hex(unsigned long long val,
		unsigned long flags)
{
	struct json_object *jobj = json_object_new_int64(val);

	if (jobj && (flags & UTIL_JSON_HUMAN))
		json_object_set_serializer(jobj, display_hex, NULL, NULL);
	return jobj;
}

void util_display_json_array(FILE *f_out, struct json_object *jarray,
		unsigned long flags)
{
	int len = json_object_array_length(jarray);
	int jflag = JSON_C_TO_STRING_PRETTY;

	if (json_object_array_length(jarray) > 1 || !(flags & UTIL_JSON_HUMAN))
		fprintf(f_out, "%s\n", json_object_to_json_string_ext(jarray, jflag));
	else if (len) {
		struct json_object *jobj;

		jobj = json_object_array_get_idx(jarray, 0);
		fprintf(f_out, "%s\n", json_object_to_json_string_ext(jobj, jflag));
	}
	json_object_put(jarray);
}

struct json_object *util_bus_to_json(struct ndctl_bus *bus, unsigned long flags)
{
	struct json_object *jbus = json_object_new_object();
	struct json_object *jobj, *fw_obj = NULL;
	int scrub;

	if (!jbus)
		return NULL;

	jobj = json_object_new_string(ndctl_bus_get_provider(bus));
	if (!jobj)
		goto err;
	json_object_object_add(jbus, "provider", jobj);

	jobj = json_object_new_string(ndctl_bus_get_devname(bus));
	if (!jobj)
		goto err;
	json_object_object_add(jbus, "dev", jobj);

	scrub = ndctl_bus_get_scrub_state(bus);
	if (scrub < 0)
		return jbus;

	jobj = json_object_new_string(scrub ? "active" : "idle");
	if (!jobj)
		goto err;
	json_object_object_add(jbus, "scrub_state", jobj);

	if (flags & UTIL_JSON_FIRMWARE) {
		struct ndctl_dimm *dimm;

		/*
		 * Skip displaying firmware activation capability if no
		 * DIMMs support firmware update.
		 */
		ndctl_dimm_foreach(bus, dimm)
			if (ndctl_dimm_fw_update_supported(dimm) == 0) {
				fw_obj = json_object_new_object();
				break;
			}
	}

	if (fw_obj) {
		enum ndctl_fwa_state state;
		enum ndctl_fwa_method method;

		jobj = NULL;
		method = ndctl_bus_get_fw_activate_method(bus);
		if (method == NDCTL_FWA_METHOD_RESET)
			jobj = json_object_new_string("reset");
		if (method == NDCTL_FWA_METHOD_SUSPEND)
			jobj = json_object_new_string("suspend");
		if (method == NDCTL_FWA_METHOD_LIVE)
			jobj = json_object_new_string("live");
		if (jobj)
			json_object_object_add(fw_obj, "activate_method", jobj);

		jobj = NULL;
		state = ndctl_bus_get_fw_activate_state(bus);
		if (state == NDCTL_FWA_ARMED)
			jobj = json_object_new_string("armed");
		if (state == NDCTL_FWA_IDLE)
			jobj = json_object_new_string("idle");
		if (state == NDCTL_FWA_ARM_OVERFLOW)
			jobj = json_object_new_string("overflow");
		if (jobj)
			json_object_object_add(fw_obj, "activate_state", jobj);

		json_object_object_add(jbus, "firmware", fw_obj);
	}

	return jbus;
 err:
	json_object_put(jbus);
	return NULL;
}

struct json_object *util_dimm_firmware_to_json(struct ndctl_dimm *dimm,
		unsigned long flags)
{
	struct json_object *jfirmware = json_object_new_object();
	bool can_update, need_powercycle;
	enum ndctl_fwa_result result;
	enum ndctl_fwa_state state;
	struct json_object *jobj;
	struct ndctl_cmd *cmd;
	uint64_t run, next;
	int rc;

	if (!jfirmware)
		return NULL;

	cmd = ndctl_dimm_cmd_new_fw_get_info(dimm);
	if (!cmd)
		goto err;

	rc = ndctl_cmd_submit(cmd);
	if ((rc < 0) || ndctl_cmd_fw_xlat_firmware_status(cmd) != FW_SUCCESS) {
		jobj = util_json_object_hex(-1, flags);
		if (jobj)
			json_object_object_add(jfirmware, "current_version",
					jobj);
		goto out;
	}

	run = ndctl_cmd_fw_info_get_run_version(cmd);
	if (run == ULLONG_MAX) {
		jobj = util_json_object_hex(-1, flags);
		if (jobj)
			json_object_object_add(jfirmware, "current_version",
					jobj);
		goto out;
	}

	jobj = util_json_object_hex(run, flags);
	if (jobj)
		json_object_object_add(jfirmware, "current_version", jobj);

	rc = ndctl_dimm_fw_update_supported(dimm);
	can_update = rc == 0;
	jobj = json_object_new_boolean(can_update);
	if (jobj)
		json_object_object_add(jfirmware, "can_update", jobj);


	next = ndctl_cmd_fw_info_get_updated_version(cmd);
	if (next == ULLONG_MAX) {
		jobj = util_json_object_hex(-1, flags);
		if (jobj)
			json_object_object_add(jfirmware, "next_version",
					jobj);
		goto out;
	}

	if (!next)
		goto out;

	jobj = util_json_object_hex(next, flags);
	if (jobj)
		json_object_object_add(jfirmware,
				"next_version", jobj);

	state = ndctl_dimm_get_fw_activate_state(dimm);
	switch (state) {
	case NDCTL_FWA_IDLE:
		jobj = json_object_new_string("idle");
		break;
	case NDCTL_FWA_ARMED:
		jobj = json_object_new_string("armed");
		break;
	case NDCTL_FWA_BUSY:
		jobj = json_object_new_string("busy");
		break;
	default:
		jobj = NULL;
		break;
	}
	if (jobj)
		json_object_object_add(jfirmware, "activate_state", jobj);

	result = ndctl_dimm_get_fw_activate_result(dimm);
	switch (result) {
	case NDCTL_FWA_RESULT_NONE:
	case NDCTL_FWA_RESULT_SUCCESS:
	case NDCTL_FWA_RESULT_NOTSTAGED:
		/*
		 * If a 'next' firmware version is staged then this
		 * result is stale, if the activation succeeds that is
		 * indicated by not finding a 'next' entry.
		 */
		need_powercycle = false;
		break;
	case NDCTL_FWA_RESULT_NEEDRESET:
	case NDCTL_FWA_RESULT_FAIL:
	default:
		/*
		 * If the last activation failed, or if the activation
		 * result is unavailable it is always the case that the
		 * only remediation is powercycle.
		 */
		need_powercycle = true;
		break;
	}

	if (need_powercycle) {
		jobj = json_object_new_boolean(true);
		if (!jobj)
			goto out;
		json_object_object_add(jfirmware, "need_powercycle", jobj);
	}

	ndctl_cmd_unref(cmd);
	return jfirmware;

err:
	json_object_put(jfirmware);
	jfirmware = NULL;
out:
	if (cmd)
		ndctl_cmd_unref(cmd);
	return jfirmware;
}

struct json_object *util_dimm_to_json(struct ndctl_dimm *dimm,
		unsigned long flags)
{
	struct json_object *jdimm = json_object_new_object();
	const char *id = ndctl_dimm_get_unique_id(dimm);
	unsigned int handle = ndctl_dimm_get_handle(dimm);
	unsigned short phys_id = ndctl_dimm_get_phys_id(dimm);
	struct json_object *jobj;
	enum ndctl_security_state sstate;

	if (!jdimm)
		return NULL;

	jobj = json_object_new_string(ndctl_dimm_get_devname(dimm));
	if (!jobj)
		goto err;
	json_object_object_add(jdimm, "dev", jobj);

	if (id) {
		jobj = json_object_new_string(id);
		if (!jobj)
			goto err;
		json_object_object_add(jdimm, "id", jobj);
	}

	if (handle < UINT_MAX) {
		jobj = util_json_object_hex(handle, flags);
		if (!jobj)
			goto err;
		json_object_object_add(jdimm, "handle", jobj);
	}

	if (phys_id < USHRT_MAX) {
		jobj = util_json_object_hex(phys_id, flags);
		if (!jobj)
			goto err;
		json_object_object_add(jdimm, "phys_id", jobj);
	}

	if (!ndctl_dimm_is_enabled(dimm)) {
		jobj = json_object_new_string("disabled");
		if (!jobj)
			goto err;
		json_object_object_add(jdimm, "state", jobj);
	}

	if (ndctl_dimm_failed_map(dimm)) {
		jobj = json_object_new_boolean(true);
		if (!jobj)
			goto err;
		json_object_object_add(jdimm, "flag_failed_map", jobj);
	}

	if (ndctl_dimm_failed_save(dimm)) {
		jobj = json_object_new_boolean(true);
		if (!jobj)
			goto err;
		json_object_object_add(jdimm, "flag_failed_save", jobj);
	}

	if (ndctl_dimm_failed_arm(dimm)) {
		jobj = json_object_new_boolean(true);
		if (!jobj)
			goto err;
		json_object_object_add(jdimm, "flag_failed_arm", jobj);
	}

	if (ndctl_dimm_failed_restore(dimm)) {
		jobj = json_object_new_boolean(true);
		if (!jobj)
			goto err;
		json_object_object_add(jdimm, "flag_failed_restore", jobj);
	}

	if (ndctl_dimm_failed_flush(dimm)) {
		jobj = json_object_new_boolean(true);
		if (!jobj)
			goto err;
		json_object_object_add(jdimm, "flag_failed_flush", jobj);
	}

	if (ndctl_dimm_smart_pending(dimm)) {
		jobj = json_object_new_boolean(true);
		if (!jobj)
			goto err;
		json_object_object_add(jdimm, "flag_smart_event", jobj);
	}

	sstate = ndctl_dimm_get_security(dimm);
	if (sstate == NDCTL_SECURITY_DISABLED)
		jobj = json_object_new_string("disabled");
	else if (sstate == NDCTL_SECURITY_UNLOCKED)
		jobj = json_object_new_string("unlocked");
	else if (sstate == NDCTL_SECURITY_LOCKED)
		jobj = json_object_new_string("locked");
	else if (sstate == NDCTL_SECURITY_FROZEN)
		jobj = json_object_new_string("frozen");
	else if (sstate == NDCTL_SECURITY_OVERWRITE)
		jobj = json_object_new_string("overwrite");
	else
		jobj = NULL;
	if (jobj)
		json_object_object_add(jdimm, "security", jobj);

	if (ndctl_dimm_security_is_frozen(dimm)) {
		jobj = json_object_new_boolean(true);
		if (jobj)
			json_object_object_add(jdimm, "security_frozen", jobj);
	}

	if (flags & UTIL_JSON_FIRMWARE) {
		struct json_object *jfirmware;

		jfirmware = util_dimm_firmware_to_json(dimm, flags);
		if (jfirmware)
			json_object_object_add(jdimm, "firmware", jfirmware);
	}

	return jdimm;
 err:
	json_object_put(jdimm);
	return NULL;
}

struct json_object *util_daxctl_dev_to_json(struct daxctl_dev *dev,
		unsigned long flags)
{
	struct daxctl_memory *mem = daxctl_dev_get_memory(dev);
	const char *devname = daxctl_dev_get_devname(dev);
	struct json_object *jdev, *jobj, *jmappings = NULL;
	struct daxctl_mapping *mapping = NULL;
	int node, movable, align;

	jdev = json_object_new_object();
	if (!devname || !jdev)
		return NULL;

	jobj = json_object_new_string(devname);
	if (jobj)
		json_object_object_add(jdev, "chardev", jobj);

	jobj = util_json_object_size(daxctl_dev_get_size(dev), flags);
	if (jobj)
		json_object_object_add(jdev, "size", jobj);

	node = daxctl_dev_get_target_node(dev);
	if (node >= 0) {
		jobj = json_object_new_int(node);
		if (jobj)
			json_object_object_add(jdev, "target_node", jobj);
	}

	align = daxctl_dev_get_align(dev);
	if (align > 0) {
		jobj = util_json_object_size(daxctl_dev_get_align(dev), flags);
		if (jobj)
			json_object_object_add(jdev, "align", jobj);
	}

	if (mem)
		jobj = json_object_new_string("system-ram");
	else
		jobj = json_object_new_string("devdax");
	if (jobj)
		json_object_object_add(jdev, "mode", jobj);

	if (mem && daxctl_dev_get_resource(dev) != 0) {
		movable = daxctl_memory_is_movable(mem);
		if (movable == 1)
			jobj = json_object_new_boolean(true);
		else if (movable == 0)
			jobj = json_object_new_boolean(false);
		else
			jobj = NULL;
		if (jobj)
			json_object_object_add(jdev, "movable", jobj);
	}

	if (!daxctl_dev_is_enabled(dev)) {
		jobj = json_object_new_string("disabled");
		if (jobj)
			json_object_object_add(jdev, "state", jobj);
	}

	if (!(flags & UTIL_JSON_DAX_MAPPINGS))
		return jdev;

	daxctl_mapping_foreach(dev, mapping) {
		struct json_object *jmapping;

		if (!jmappings) {
			jmappings = json_object_new_array();
			if (!jmappings)
				continue;

			json_object_object_add(jdev, "mappings", jmappings);
		}

		jmapping = util_daxctl_mapping_to_json(mapping, flags);
		if (!jmapping)
			continue;
		json_object_array_add(jmappings, jmapping);
	}
	return jdev;
}

struct json_object *util_daxctl_devs_to_list(struct daxctl_region *region,
		struct json_object *jdevs, const char *ident,
		unsigned long flags)
{
	struct daxctl_dev *dev;

	daxctl_dev_foreach(region, dev) {
		struct json_object *jdev;

		if (!util_daxctl_dev_filter(dev, ident))
			continue;

		if (!(flags & (UTIL_JSON_IDLE|UTIL_JSON_CONFIGURED))
				&& !daxctl_dev_get_size(dev))
			continue;

		if (!jdevs) {
			jdevs = json_object_new_array();
			if (!jdevs)
				return NULL;
		}

		jdev = util_daxctl_dev_to_json(dev, flags);
		if (!jdev) {
			json_object_put(jdevs);
			return NULL;
		}

		json_object_array_add(jdevs, jdev);
	}

	return jdevs;
}

#define _SZ(get_max, get_elem, type) \
static struct json_object *util_##type##_build_size_array(struct ndctl_##type *arg)	\
{								\
	struct json_object *arr = json_object_new_array();	\
	int i;							\
								\
	if (!arr)						\
		return NULL;					\
								\
	for (i = 0; i < get_max(arg); i++) {			\
		struct json_object *jobj;			\
		int64_t align;					\
								\
		align = get_elem(arg, i);			\
		jobj = json_object_new_int64(align);		\
		if (!jobj)					\
			goto err;				\
		json_object_array_add(arr, jobj);		\
	}							\
								\
	return arr;						\
err:								\
	json_object_put(arr);					\
	return NULL;						\
}
#define SZ(type, kind) _SZ(ndctl_##type##_get_num_##kind##s, \
			   ndctl_##type##_get_supported_##kind, type)
SZ(pfn, alignment)
SZ(dax, alignment)
SZ(btt, sector_size)

struct json_object *util_region_capabilities_to_json(struct ndctl_region *region)
{
	struct json_object *jcaps, *jcap, *jobj;
	struct ndctl_btt *btt = ndctl_region_get_btt_seed(region);
	struct ndctl_pfn *pfn = ndctl_region_get_pfn_seed(region);
	struct ndctl_dax *dax = ndctl_region_get_dax_seed(region);

	if (!btt || !pfn || !dax)
		return NULL;

	jcaps = json_object_new_array();
	if (!jcaps)
		return NULL;

	if (btt) {
		jcap = json_object_new_object();
		if (!jcap)
			goto err;
		json_object_array_add(jcaps, jcap);

		jobj = json_object_new_string("sector");
		if (!jobj)
			goto err;
		json_object_object_add(jcap, "mode", jobj);
		jobj = util_btt_build_size_array(btt);
		if (!jobj)
			goto err;
		json_object_object_add(jcap, "sector_sizes", jobj);
	}

	if (pfn) {
		jcap = json_object_new_object();
		if (!jcap)
			goto err;
		json_object_array_add(jcaps, jcap);

		jobj = json_object_new_string("fsdax");
		if (!jobj)
			goto err;
		json_object_object_add(jcap, "mode", jobj);
		jobj = util_pfn_build_size_array(pfn);
		if (!jobj)
			goto err;
		json_object_object_add(jcap, "alignments", jobj);
	}

	if (dax) {
		jcap = json_object_new_object();
		if (!jcap)
			goto err;
		json_object_array_add(jcaps, jcap);

		jobj = json_object_new_string("devdax");
		if (!jobj)
			goto err;
		json_object_object_add(jcap, "mode", jobj);
		jobj = util_dax_build_size_array(dax);
		if (!jobj)
			goto err;
		json_object_object_add(jcap, "alignments", jobj);
	}

	return jcaps;
err:
	json_object_put(jcaps);
	return NULL;
}

struct json_object *util_daxctl_region_to_json(struct daxctl_region *region,
		const char *ident, unsigned long flags)
{
	unsigned long align;
	struct json_object *jregion, *jobj;
	unsigned long long available_size, size;

	jregion = json_object_new_object();
	if (!jregion)
		return NULL;

	/*
	 * The flag indicates when we are being called by an agent that
	 * already knows about the parent device information.
	 */
	if (!(flags & UTIL_JSON_DAX)) {
		/* trim off the redundant /sys/devices prefix */
		const char *path = daxctl_region_get_path(region);
		int len = strlen("/sys/devices");
		const char *trim = &path[len];

		if (strncmp(path, "/sys/devices", len) != 0)
			goto err;
		jobj = json_object_new_string(trim);
		if (!jobj)
			goto err;
		json_object_object_add(jregion, "path", jobj);
	}

	jobj = json_object_new_int(daxctl_region_get_id(region));
	if (!jobj)
		goto err;
	json_object_object_add(jregion, "id", jobj);

	size = daxctl_region_get_size(region);
	if (size < ULLONG_MAX) {
		jobj = util_json_object_size(size, flags);
		if (!jobj)
			goto err;
		json_object_object_add(jregion, "size", jobj);
	}

	available_size = daxctl_region_get_available_size(region);
	if (available_size) {
		jobj = util_json_object_size(available_size, flags);
		if (!jobj)
			goto err;
		json_object_object_add(jregion, "available_size", jobj);
	}

	align = daxctl_region_get_align(region);
	if (align < ULONG_MAX) {
		jobj = json_object_new_int64(align);
		if (!jobj)
			goto err;
		json_object_object_add(jregion, "align", jobj);
	}

	if (!(flags & UTIL_JSON_DAX_DEVS))
		return jregion;

	jobj = util_daxctl_devs_to_list(region, NULL, ident, flags);
	if (jobj)
		json_object_object_add(jregion, "devices", jobj);

	return jregion;
 err:
	json_object_put(jregion);
	return NULL;
}

static int compare_dimm_number(const void *p1, const void *p2)
{
	struct ndctl_dimm *dimm1 = *(struct ndctl_dimm **)p1;
	struct ndctl_dimm *dimm2 = *(struct ndctl_dimm **)p2;
	const char *dimm1_name = ndctl_dimm_get_devname(dimm1);
	const char *dimm2_name = ndctl_dimm_get_devname(dimm2);
	int num1, num2;

	if (sscanf(dimm1_name, "nmem%d", &num1) != 1)
		num1 = 0;
	if (sscanf(dimm2_name, "nmem%d", &num2) != 1)
		num2 = 0;

	return num1 - num2;
}

static struct json_object *badblocks_to_jdimms(struct ndctl_region *region,
		unsigned long long addr, unsigned long len)
{
	struct ndctl_bus *bus = ndctl_region_get_bus(region);
	int count = ndctl_region_get_interleave_ways(region);
	unsigned long long end = addr + len;
	struct json_object *jdimms, *jobj;
	struct ndctl_dimm **dimms, *dimm;
	int found, i;

	jdimms = json_object_new_array();
	if (!jdimms)
		return NULL;

	dimms = calloc(count, sizeof(struct ndctl_dimm *));
	if (!dimms)
		goto err_dimms;

	for (found = 0; found < count && addr < end; addr += 512) {
		dimm = ndctl_bus_get_dimm_by_physical_address(bus, addr);
		if (!dimm)
			continue;

		for (i = 0; i < count; i++)
			if (dimms[i] == dimm)
				break;
		if (i >= count)
			dimms[found++] = dimm;
	}

	if (!found)
		goto err_found;

	qsort(dimms, found, sizeof(dimm), compare_dimm_number);

	for (i = 0; i < found; i++) {
		const char *devname = ndctl_dimm_get_devname(dimms[i]);

		jobj = json_object_new_string(devname);
		if (!jobj)
			break;
		json_object_array_add(jdimms, jobj);
	}

	if (!i)
		goto err_found;
	free(dimms);
	return jdimms;

err_found:
	free(dimms);
err_dimms:
	json_object_put(jdimms);
	return NULL;
}

struct json_object *util_region_badblocks_to_json(struct ndctl_region *region,
		unsigned int *bb_count, unsigned long flags)
{
	struct json_object *jbb = NULL, *jbbs = NULL, *jobj;
	struct badblock *bb;
	int bbs = 0;

	if (flags & UTIL_JSON_MEDIA_ERRORS) {
		jbbs = json_object_new_array();
		if (!jbbs)
			return NULL;
	}

	ndctl_region_badblock_foreach(region, bb) {
		struct json_object *jdimms;
		unsigned long long addr;

		bbs += bb->len;

		/* recheck so we can still get the badblocks_count from above */
		if (!(flags & UTIL_JSON_MEDIA_ERRORS))
			continue;

		/* get start address of region */
		addr = ndctl_region_get_resource(region);
		if (addr == ULLONG_MAX)
			goto err_array;

		/* get address of bad block */
		addr += bb->offset << 9;

		jbb = json_object_new_object();
		if (!jbb)
			goto err_array;

		jobj = json_object_new_int64(bb->offset);
		if (!jobj)
			goto err;
		json_object_object_add(jbb, "offset", jobj);

		jobj = json_object_new_int(bb->len);
		if (!jobj)
			goto err;
		json_object_object_add(jbb, "length", jobj);

		jdimms = badblocks_to_jdimms(region, addr, bb->len << 9);
		if (jdimms)
			json_object_object_add(jbb, "dimms", jdimms);
		json_object_array_add(jbbs, jbb);
	}

	*bb_count = bbs;

	if (bbs)
		return jbbs;

 err:
	json_object_put(jbb);
 err_array:
	json_object_put(jbbs);
	return NULL;
}

static struct json_object *util_namespace_badblocks_to_json(
			struct ndctl_namespace *ndns,
			unsigned int *bb_count, unsigned long flags)
{
	struct json_object *jbb = NULL, *jbbs = NULL, *jobj;
	struct badblock *bb;
	int bbs = 0;

	if (flags & UTIL_JSON_MEDIA_ERRORS) {
		jbbs = json_object_new_array();
		if (!jbbs)
			return NULL;
	} else
		return NULL;

	ndctl_namespace_badblock_foreach(ndns, bb) {
		bbs += bb->len;

		/* recheck so we can still get the badblocks_count from above */
		if (!(flags & UTIL_JSON_MEDIA_ERRORS))
			continue;

		jbb = json_object_new_object();
		if (!jbb)
			goto err_array;

		jobj = json_object_new_int64(bb->offset);
		if (!jobj)
			goto err;
		json_object_object_add(jbb, "offset", jobj);

		jobj = json_object_new_int(bb->len);
		if (!jobj)
			goto err;
		json_object_object_add(jbb, "length", jobj);
		json_object_array_add(jbbs, jbb);
	}

	*bb_count = bbs;

	if (bbs)
		return jbbs;

 err:
	json_object_put(jbb);
 err_array:
	json_object_put(jbbs);
	return NULL;
}

static struct json_object *dev_badblocks_to_json(struct ndctl_region *region,
		unsigned long long dev_begin, unsigned long long dev_size,
		unsigned int *bb_count, unsigned long flags)
{
	struct json_object *jbb = NULL, *jbbs = NULL, *jobj;
	unsigned long long region_begin, dev_end, offset;
	unsigned int len, bbs = 0;
	struct badblock *bb;

	region_begin = ndctl_region_get_resource(region);
	if (region_begin == ULLONG_MAX)
		return NULL;

	dev_end = dev_begin + dev_size - 1;

	if (flags & UTIL_JSON_MEDIA_ERRORS) {
		jbbs = json_object_new_array();
		if (!jbbs)
			return NULL;
	}

	ndctl_region_badblock_foreach(region, bb) {
		unsigned long long bb_begin, bb_end, begin, end;
		struct json_object *jdimms;

		bb_begin = region_begin + (bb->offset << 9);
		bb_end = bb_begin + (bb->len << 9) - 1;

		if (bb_end <= dev_begin || bb_begin >= dev_end)
			continue;

		if (bb_begin < dev_begin)
			begin = dev_begin;
		else
			begin = bb_begin;

		if (bb_end > dev_end)
			end = dev_end;
		else
			end = bb_end;

		offset = (begin - dev_begin) >> 9;
		len = (end - begin + 1) >> 9;

		bbs += len;

		/* recheck so we can still get the badblocks_count from above */
		if (!(flags & UTIL_JSON_MEDIA_ERRORS))
			continue;

		jbb = json_object_new_object();
		if (!jbb)
			goto err_array;

		jobj = json_object_new_int64(offset);
		if (!jobj)
			goto err;
		json_object_object_add(jbb, "offset", jobj);

		jobj = json_object_new_int(len);
		if (!jobj)
			goto err;
		json_object_object_add(jbb, "length", jobj);

		jdimms = badblocks_to_jdimms(region, begin, len << 9);
		if (jdimms)
			json_object_object_add(jbb, "dimms", jdimms);

		json_object_array_add(jbbs, jbb);
	}

	*bb_count = bbs;

	if (bbs)
		return jbbs;

 err:
	json_object_put(jbb);
 err_array:
	json_object_put(jbbs);
	return NULL;
}

static struct json_object *util_pfn_badblocks_to_json(struct ndctl_pfn *pfn,
		unsigned int *bb_count, unsigned long flags)
{
	struct ndctl_region *region = ndctl_pfn_get_region(pfn);
	unsigned long long pfn_begin, pfn_size;

	pfn_begin = ndctl_pfn_get_resource(pfn);
	if (pfn_begin == ULLONG_MAX) {
		struct ndctl_namespace *ndns = ndctl_pfn_get_namespace(pfn);

		return util_namespace_badblocks_to_json(ndns, bb_count, flags);
	}

	pfn_size = ndctl_pfn_get_size(pfn);
	if (pfn_size == ULLONG_MAX)
		return NULL;

	return dev_badblocks_to_json(region, pfn_begin, pfn_size,
			bb_count, flags);
}

static void util_btt_badblocks_to_json(struct ndctl_btt *btt,
		unsigned int *bb_count)
{
	struct ndctl_region *region = ndctl_btt_get_region(btt);
	struct ndctl_namespace *ndns = ndctl_btt_get_namespace(btt);
	unsigned long long begin, size;

	if (!ndns)
		return;

	begin = ndctl_namespace_get_resource(ndns);
	if (begin == ULLONG_MAX)
		return;

	size = ndctl_namespace_get_size(ndns);
	if (size == ULLONG_MAX)
		return;

	/*
	 * The dev_badblocks_to_json() for BTT is not accurate with
	 * respect to data vs metadata badblocks, and is only useful for
	 * a potential bb_count.
	 *
	 * FIXME: switch to native BTT badblocks representation
	 * when / if the kernel provides it.
	 */
	dev_badblocks_to_json(region, begin, size, bb_count, 0);
}

static struct json_object *util_dax_badblocks_to_json(struct ndctl_dax *dax,
		unsigned int *bb_count, unsigned long flags)
{
	struct ndctl_region *region = ndctl_dax_get_region(dax);
	unsigned long long dax_begin, dax_size;

	dax_begin = ndctl_dax_get_resource(dax);
	if (dax_begin == ULLONG_MAX)
		return NULL;

	dax_size = ndctl_dax_get_size(dax);
	if (dax_size == ULLONG_MAX)
		return NULL;

	return dev_badblocks_to_json(region, dax_begin, dax_size,
			bb_count, flags);
}

static struct json_object *util_raw_uuid(struct ndctl_namespace *ndns)
{
	char buf[40];
	uuid_t raw_uuid;

	ndctl_namespace_get_uuid(ndns, raw_uuid);
	if (uuid_is_null(raw_uuid))
		return NULL;
	uuid_unparse(raw_uuid, buf);
	return json_object_new_string(buf);
}

static void util_raw_uuid_to_json(struct ndctl_namespace *ndns,
				  unsigned long flags,
				  struct json_object *jndns)
{
	struct json_object *jobj;

	if (!(flags & UTIL_JSON_VERBOSE))
		return;

	jobj = util_raw_uuid(ndns);
	if (!jobj)
		return;
	json_object_object_add(jndns, "raw_uuid", jobj);
}

struct json_object *util_namespace_to_json(struct ndctl_namespace *ndns,
		unsigned long flags)
{
	struct json_object *jndns = json_object_new_object();
	enum ndctl_pfn_loc loc = NDCTL_PFN_LOC_NONE;
	struct json_object *jobj, *jbbs = NULL;
	const char *locations[] = {
		[NDCTL_PFN_LOC_NONE] = "none",
		[NDCTL_PFN_LOC_RAM] = "mem",
		[NDCTL_PFN_LOC_PMEM] = "dev",
	};
	unsigned long long size = ULLONG_MAX;
	unsigned int sector_size = UINT_MAX;
	enum ndctl_namespace_mode mode;
	const char *bdev = NULL, *name;
	unsigned int bb_count = 0;
	struct ndctl_btt *btt;
	struct ndctl_pfn *pfn;
	struct ndctl_dax *dax;
	unsigned long align = 0;
	char buf[40];
	uuid_t uuid;
	int numa, target;

	if (!jndns)
		return NULL;

	jobj = json_object_new_string(ndctl_namespace_get_devname(ndns));
	if (!jobj)
		goto err;
	json_object_object_add(jndns, "dev", jobj);

	btt = ndctl_namespace_get_btt(ndns);
	dax = ndctl_namespace_get_dax(ndns);
	pfn = ndctl_namespace_get_pfn(ndns);
	mode = ndctl_namespace_get_mode(ndns);
	switch (mode) {
	case NDCTL_NS_MODE_MEMORY:
		if (pfn) { /* dynamic memory mode */
			size = ndctl_pfn_get_size(pfn);
			loc = ndctl_pfn_get_location(pfn);
		} else { /* native/static memory mode */
			size = ndctl_namespace_get_size(ndns);
			loc = NDCTL_PFN_LOC_RAM;
		}
		jobj = json_object_new_string("fsdax");
		break;
	case NDCTL_NS_MODE_DAX:
		if (!dax)
			goto err;
		size = ndctl_dax_get_size(dax);
		jobj = json_object_new_string("devdax");
		loc = ndctl_dax_get_location(dax);
		break;
	case NDCTL_NS_MODE_SECTOR:
		if (!btt)
			goto err;
		jobj = json_object_new_string("sector");
		size = ndctl_btt_get_size(btt);
		break;
	case NDCTL_NS_MODE_RAW:
		size = ndctl_namespace_get_size(ndns);
		jobj = json_object_new_string("raw");
		break;
	default:
		jobj = NULL;
	}
	if (jobj)
		json_object_object_add(jndns, "mode", jobj);

	if ((mode != NDCTL_NS_MODE_SECTOR) && (mode != NDCTL_NS_MODE_RAW)) {
		jobj = json_object_new_string(locations[loc]);
		if (jobj)
			json_object_object_add(jndns, "map", jobj);
	}

	if (size < ULLONG_MAX) {
		jobj = util_json_object_size(size, flags);
		if (jobj)
			json_object_object_add(jndns, "size", jobj);
	}

	if (btt) {
		ndctl_btt_get_uuid(btt, uuid);
		uuid_unparse(uuid, buf);
		jobj = json_object_new_string(buf);
		if (!jobj)
			goto err;
		json_object_object_add(jndns, "uuid", jobj);
		util_raw_uuid_to_json(ndns, flags, jndns);
		bdev = ndctl_btt_get_block_device(btt);
	} else if (pfn) {
		align = ndctl_pfn_get_align(pfn);
		ndctl_pfn_get_uuid(pfn, uuid);
		uuid_unparse(uuid, buf);
		jobj = json_object_new_string(buf);
		if (!jobj)
			goto err;
		json_object_object_add(jndns, "uuid", jobj);
		util_raw_uuid_to_json(ndns, flags, jndns);
		bdev = ndctl_pfn_get_block_device(pfn);
	} else if (dax) {
		struct daxctl_region *dax_region;

		dax_region = ndctl_dax_get_daxctl_region(dax);
		align = ndctl_dax_get_align(dax);
		ndctl_dax_get_uuid(dax, uuid);
		uuid_unparse(uuid, buf);
		jobj = json_object_new_string(buf);
		if (!jobj)
			goto err;
		json_object_object_add(jndns, "uuid", jobj);
		util_raw_uuid_to_json(ndns, flags, jndns);
		if ((flags & UTIL_JSON_DAX) && dax_region) {
			jobj = util_daxctl_region_to_json(dax_region, NULL,
					flags);
			if (jobj)
				json_object_object_add(jndns, "daxregion", jobj);
		} else if (dax_region) {
			struct daxctl_dev *dev;

			/*
			 * We can only find/list these device-dax
			 * details when the instance is enabled.
			 */
			dev = daxctl_dev_get_first(dax_region);
			if (dev) {
				name = daxctl_dev_get_devname(dev);
				jobj = json_object_new_string(name);
				if (!jobj)
					goto err;
				json_object_object_add(jndns, "chardev", jobj);
			}
		}
	} else if (ndctl_namespace_get_type(ndns) != ND_DEVICE_NAMESPACE_IO) {
		ndctl_namespace_get_uuid(ndns, uuid);
		uuid_unparse(uuid, buf);
		jobj = json_object_new_string(buf);
		if (!jobj)
			goto err;
		json_object_object_add(jndns, "uuid", jobj);
		bdev = ndctl_namespace_get_block_device(ndns);
	} else
		bdev = ndctl_namespace_get_block_device(ndns);

	if (btt)
		sector_size = ndctl_btt_get_sector_size(btt);
	else if (!dax) {
		sector_size = ndctl_namespace_get_sector_size(ndns);
		if (!sector_size || sector_size == UINT_MAX)
			sector_size = 512;
	}

	/*
	 * The kernel will default to a 512 byte sector size on PMEM
	 * namespaces that don't explicitly have a sector size. This
	 * happens because they use pre-v1.2 labels or because they
	 * don't have a label space (devtype=nd_namespace_io).
	 */
	if (sector_size < UINT_MAX) {
		jobj = json_object_new_int(sector_size);
		if (!jobj)
			goto err;
		json_object_object_add(jndns, "sector_size", jobj);
	}

	if (align) {
		jobj = json_object_new_int64(align);
		if (!jobj)
			goto err;
		json_object_object_add(jndns, "align", jobj);
	}

	if (bdev && bdev[0]) {
		jobj = json_object_new_string(bdev);
		if (!jobj)
			goto err;
		json_object_object_add(jndns, "blockdev", jobj);
	}

	if (!ndctl_namespace_is_active(ndns)) {
		jobj = json_object_new_string("disabled");
		if (!jobj)
			goto err;
		json_object_object_add(jndns, "state", jobj);
	}

	name = ndctl_namespace_get_alt_name(ndns);
	if (name && name[0]) {
		jobj = json_object_new_string(name);
		if (!jobj)
			goto err;
		json_object_object_add(jndns, "name", jobj);
	}

	numa = ndctl_namespace_get_numa_node(ndns);
	if (numa >= 0 && flags & UTIL_JSON_VERBOSE) {
		jobj = json_object_new_int(numa);
		if (jobj)
			json_object_object_add(jndns, "numa_node", jobj);
	}

	target = ndctl_namespace_get_target_node(ndns);
	if (target >= 0 && flags & UTIL_JSON_VERBOSE) {
		jobj = json_object_new_int(target);
		if (jobj)
			json_object_object_add(jndns, "target_node", jobj);
	}

	if (pfn)
		jbbs = util_pfn_badblocks_to_json(pfn, &bb_count, flags);
	else if (dax)
		jbbs = util_dax_badblocks_to_json(dax, &bb_count, flags);
	else if (btt)
		util_btt_badblocks_to_json(btt, &bb_count);
	else {
		jbbs = util_region_badblocks_to_json(
				ndctl_namespace_get_region(ndns), &bb_count,
				flags);
		if (!jbbs)
			jbbs = util_namespace_badblocks_to_json(ndns, &bb_count,
					flags);
	}

	if (bb_count) {
		jobj = json_object_new_int(bb_count);
		if (!jobj) {
			json_object_put(jbbs);
			goto err;
		}
		json_object_object_add(jndns, "badblock_count", jobj);
	}

	if ((flags & UTIL_JSON_MEDIA_ERRORS) && jbbs)
		json_object_object_add(jndns, "badblocks", jbbs);

	return jndns;
 err:
	json_object_put(jndns);
	return NULL;
}

struct json_object *util_mapping_to_json(struct ndctl_mapping *mapping,
		unsigned long flags)
{
	struct json_object *jmapping = json_object_new_object();
	struct ndctl_dimm *dimm = ndctl_mapping_get_dimm(mapping);
	struct json_object *jobj;
	int position;

	if (!jmapping)
		return NULL;

	jobj = json_object_new_string(ndctl_dimm_get_devname(dimm));
	if (!jobj)
		goto err;
	json_object_object_add(jmapping, "dimm", jobj);

	jobj = util_json_object_hex(ndctl_mapping_get_offset(mapping), flags);
	if (!jobj)
		goto err;
	json_object_object_add(jmapping, "offset", jobj);

	jobj = util_json_object_hex(ndctl_mapping_get_length(mapping), flags);
	if (!jobj)
		goto err;
	json_object_object_add(jmapping, "length", jobj);

	position = ndctl_mapping_get_position(mapping);
	if (position >= 0) {
		jobj = json_object_new_int(position);
		if (!jobj)
			goto err;
		json_object_object_add(jmapping, "position", jobj);
	}

	return jmapping;
 err:
	json_object_put(jmapping);
	return NULL;
}

struct json_object *util_daxctl_mapping_to_json(struct daxctl_mapping *mapping,
		unsigned long flags)
{
	struct json_object *jmapping = json_object_new_object();
	struct json_object *jobj;

	if (!jmapping)
		return NULL;

	jobj = util_json_object_hex(daxctl_mapping_get_offset(mapping), flags);
	if (!jobj)
		goto err;
	json_object_object_add(jmapping, "page_offset", jobj);

	jobj = util_json_object_hex(daxctl_mapping_get_start(mapping), flags);
	if (!jobj)
		goto err;
	json_object_object_add(jmapping, "start", jobj);

	jobj = util_json_object_hex(daxctl_mapping_get_end(mapping), flags);
	if (!jobj)
		goto err;
	json_object_object_add(jmapping, "end", jobj);

	jobj = util_json_object_size(daxctl_mapping_get_size(mapping), flags);
	if (!jobj)
		goto err;
	json_object_object_add(jmapping, "size", jobj);

	return jmapping;
 err:
	json_object_put(jmapping);
	return NULL;
}

struct json_object *util_badblock_rec_to_json(u64 block, u64 count,
		unsigned long flags)
{
	struct json_object *jerr = json_object_new_object();
	struct json_object *jobj;

	if (!jerr)
		return NULL;

	jobj = util_json_object_hex(block, flags);
	if (!jobj)
		goto err;
	json_object_object_add(jerr, "block", jobj);

	jobj = util_json_object_hex(count, flags);
	if (!jobj)
		goto err;
	json_object_object_add(jerr, "count", jobj);

	return jerr;
 err:
	json_object_put(jerr);
	return NULL;
}
