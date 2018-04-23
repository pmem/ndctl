/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright(c) 2018 Intel Corporation. All rights reserved. */
#include <limits.h>
#include <util/json.h>
#include <uuid/uuid.h>
#include <json-c/json.h>
#include <ndctl/libndctl.h>
#include <ccan/array_size/array_size.h>
#include <ndctl.h>

struct json_object *util_dimm_firmware_to_json(struct ndctl_dimm *dimm,
		unsigned long flags)
{
	struct json_object *jfirmware = json_object_new_object();
	struct json_object *jobj;
	struct ndctl_cmd *cmd;
	int rc;
	uint64_t run, next;

	if (!jfirmware)
		return NULL;

	cmd = ndctl_dimm_cmd_new_fw_get_info(dimm);
	if (!cmd)
		goto err;

	rc = ndctl_cmd_submit(cmd);
	if (rc || ndctl_cmd_fw_xlat_firmware_status(cmd) != FW_SUCCESS) {
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

	next = ndctl_cmd_fw_info_get_updated_version(cmd);
	if (next == ULLONG_MAX) {
		jobj = util_json_object_hex(-1, flags);
		if (jobj)
			json_object_object_add(jfirmware, "next_version",
					jobj);
		goto out;
	}

	if (next != 0) {
		jobj = util_json_object_hex(next, flags);
		if (jobj)
			json_object_object_add(jfirmware,
					"next_version", jobj);

		jobj = json_object_new_boolean(true);
		if (jobj)
			json_object_object_add(jfirmware,
					"need_powercycle", jobj);
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
