// SPDX-License-Identifier: LGPL-2.1
// Copyright (C) 2016-2020, Intel Corporation. All rights reserved.
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>
#include <syslog.h>
#include <util/size.h>
#include <uuid/uuid.h>
#include <util/json.h>
#include <util/filter.h>
#include <json-c/json.h>
#include <util/fletcher.h>
#include <ndctl/libndctl.h>
#include <ndctl/namespace.h>
#include <util/parse-options.h>
#include <ccan/minmax/minmax.h>
#include <ccan/array_size/array_size.h>
#include <ndctl/firmware-update.h>
#include <util/keys.h>

static const char *cmd_name = "dimm";
static int err_count;
#define err(fmt, ...) \
	({ err_count++; error("%s: " fmt, cmd_name, ##__VA_ARGS__); })

struct action_context {
	struct json_object *jdimms;
	enum ndctl_namespace_version labelversion;
	FILE *f_out;
	FILE *f_in;
	struct update_context update;
};

static struct parameters {
	const char *bus;
	const char *outfile;
	const char *infile;
	const char *labelversion;
	const char *kek;
	unsigned len;
	unsigned offset;
	bool crypto_erase;
	bool overwrite;
	bool zero_key;
	bool master_pass;
	bool human;
	bool force;
	bool arm;
	bool arm_set;
	bool disarm;
	bool disarm_set;
	bool index;
	bool json;
	bool verbose;
} param = {
	.arm = true,
	.labelversion = "1.1",
};

static int action_disable(struct ndctl_dimm *dimm, struct action_context *actx)
{
	if (ndctl_dimm_is_active(dimm)) {
		fprintf(stderr, "%s is active, skipping...\n",
				ndctl_dimm_get_devname(dimm));
		return -EBUSY;
	}

	return ndctl_dimm_disable(dimm);
}

static int action_enable(struct ndctl_dimm *dimm, struct action_context *actx)
{
	return ndctl_dimm_enable(dimm);
}

static int action_zero(struct ndctl_dimm *dimm, struct action_context *actx)
{
	if (ndctl_dimm_is_active(dimm)) {
		fprintf(stderr, "%s: regions active, abort label write\n",
			ndctl_dimm_get_devname(dimm));
		return -EBUSY;
	}

	return ndctl_dimm_zero_label_extent(dimm, param.len, param.offset);
}

static struct json_object *dump_label_json(struct ndctl_dimm *dimm,
		struct ndctl_cmd *cmd_read, ssize_t size, unsigned long flags)
{
	struct json_object *jarray = json_object_new_array();
	struct json_object *jlabel = NULL;
	struct namespace_label nslabel;
	unsigned int slot = -1;
	ssize_t offset;

	if (!jarray)
		return NULL;

	for (offset = NSINDEX_ALIGN * 2; offset < size;
			offset += ndctl_dimm_sizeof_namespace_label(dimm)) {
		ssize_t len = min_t(ssize_t,
				ndctl_dimm_sizeof_namespace_label(dimm),
				size - offset);
		struct json_object *jobj;
		char uuid[40];

		slot++;
		jlabel = json_object_new_object();
		if (!jlabel)
			break;

		if (len < (ssize_t) ndctl_dimm_sizeof_namespace_label(dimm))
			break;

		len = ndctl_cmd_cfg_read_get_data(cmd_read, &nslabel, len, offset);
		if (len < 0)
			break;

		if (le32_to_cpu(nslabel.slot) != slot)
			continue;

		uuid_unparse((void *) nslabel.uuid, uuid);
		jobj = json_object_new_string(uuid);
		if (!jobj)
			break;
		json_object_object_add(jlabel, "uuid", jobj);

		nslabel.name[NSLABEL_NAME_LEN - 1] = 0;
		jobj = json_object_new_string(nslabel.name);
		if (!jobj)
			break;
		json_object_object_add(jlabel, "name", jobj);

		jobj = json_object_new_int(le32_to_cpu(nslabel.slot));
		if (!jobj)
			break;
		json_object_object_add(jlabel, "slot", jobj);

		jobj = json_object_new_int(le16_to_cpu(nslabel.position));
		if (!jobj)
			break;
		json_object_object_add(jlabel, "position", jobj);

		jobj = json_object_new_int(le16_to_cpu(nslabel.nlabel));
		if (!jobj)
			break;
		json_object_object_add(jlabel, "nlabel", jobj);

		jobj = util_json_object_hex(le32_to_cpu(nslabel.flags), flags);
		if (!jobj)
			break;
		json_object_object_add(jlabel, "flags", jobj);

		jobj = util_json_object_hex(le64_to_cpu(nslabel.isetcookie),
				flags);
		if (!jobj)
			break;
		json_object_object_add(jlabel, "isetcookie", jobj);

		jobj = json_object_new_int64(le64_to_cpu(nslabel.lbasize));
		if (!jobj)
			break;
		json_object_object_add(jlabel, "lbasize", jobj);

		jobj = util_json_object_hex(le64_to_cpu(nslabel.dpa), flags);
		if (!jobj)
			break;
		json_object_object_add(jlabel, "dpa", jobj);

		jobj = util_json_object_size(le64_to_cpu(nslabel.rawsize), flags);
		if (!jobj)
			break;
		json_object_object_add(jlabel, "rawsize", jobj);

		json_object_array_add(jarray, jlabel);

		if (ndctl_dimm_sizeof_namespace_label(dimm) < 256)
			continue;

		uuid_unparse((void *) nslabel.type_guid, uuid);
		jobj = json_object_new_string(uuid);
		if (!jobj)
			break;
		json_object_object_add(jlabel, "type_guid", jobj);

		uuid_unparse((void *) nslabel.abstraction_guid, uuid);
		jobj = json_object_new_string(uuid);
		if (!jobj)
			break;
		json_object_object_add(jlabel, "abstraction_guid", jobj);
	}

	if (json_object_array_length(jarray) < 1) {
		json_object_put(jarray);
		if (jlabel)
			json_object_put(jlabel);
		jarray = NULL;
	}

	return jarray;
}

static struct json_object *dump_index_json(struct ndctl_cmd *cmd_read, ssize_t size)
{
	struct json_object *jarray = json_object_new_array();
	struct json_object *jindex = NULL;
	struct namespace_index nsindex;
	ssize_t offset;

	if (!jarray)
		return NULL;

	for (offset = 0; offset < NSINDEX_ALIGN * 2; offset += NSINDEX_ALIGN) {
		ssize_t len = min_t(ssize_t, sizeof(nsindex), size - offset);
		struct json_object *jobj;

		jindex = json_object_new_object();
		if (!jindex)
			break;

		if (len < (ssize_t) sizeof(nsindex))
			break;

		len = ndctl_cmd_cfg_read_get_data(cmd_read, &nsindex, len, offset);
		if (len < 0)
			break;

		nsindex.sig[NSINDEX_SIG_LEN - 1] = 0;
		jobj = json_object_new_string(nsindex.sig);
		if (!jobj)
			break;
		json_object_object_add(jindex, "signature", jobj);

		jobj = json_object_new_int(le16_to_cpu(nsindex.major));
		if (!jobj)
			break;
		json_object_object_add(jindex, "major", jobj);

		jobj = json_object_new_int(le16_to_cpu(nsindex.minor));
		if (!jobj)
			break;
		json_object_object_add(jindex, "minor", jobj);

		jobj = json_object_new_int(1 << (7 + nsindex.labelsize));
		if (!jobj)
			break;
		json_object_object_add(jindex, "labelsize", jobj);

		jobj = json_object_new_int(le32_to_cpu(nsindex.seq));
		if (!jobj)
			break;
		json_object_object_add(jindex, "seq", jobj);

		jobj = json_object_new_int(le32_to_cpu(nsindex.nslot));
		if (!jobj)
			break;
		json_object_object_add(jindex, "nslot", jobj);

		json_object_array_add(jarray, jindex);
	}

	if (json_object_array_length(jarray) < 1) {
		json_object_put(jarray);
		if (jindex)
			json_object_put(jindex);
		jarray = NULL;
	}

	return jarray;
}

static struct json_object *dump_json(struct ndctl_dimm *dimm,
		struct ndctl_cmd *cmd_read, ssize_t size)
{
	unsigned long flags = param.human ? UTIL_JSON_HUMAN : 0;
	struct json_object *jdimm = json_object_new_object();
	struct json_object *jlabel, *jobj, *jindex;

	if (!jdimm)
		return NULL;

	jobj = json_object_new_string(ndctl_dimm_get_devname(dimm));
	if (!jobj)
		goto err;
	json_object_object_add(jdimm, "dev", jobj);

	jindex = dump_index_json(cmd_read, size);
	if (!jindex)
		goto err;
	json_object_object_add(jdimm, "index", jindex);
	if (param.index)
		return jdimm;

	jlabel = dump_label_json(dimm, cmd_read, size, flags);
	if (!jlabel)
		goto err;
	json_object_object_add(jdimm, "label", jlabel);

	return jdimm;
err:
	json_object_put(jdimm);
	return NULL;
}

static int rw_bin(FILE *f, struct ndctl_cmd *cmd, ssize_t size,
		unsigned int start_offset, int rw)
{
	char buf[4096];
	ssize_t offset, write = 0;

	for (offset = start_offset; offset < start_offset + size;
			offset += sizeof(buf)) {
		ssize_t len = min_t(ssize_t, sizeof(buf), size - offset), rc;

		if (rw == WRITE) {
			len = fread(buf, 1, len, f);
			if (len == 0)
				break;
			rc = ndctl_cmd_cfg_write_set_data(cmd, buf, len, offset);
			if (rc < 0)
				return -ENXIO;
			write += len;
		} else {
			len = ndctl_cmd_cfg_read_get_data(cmd, buf, len, offset);
			if (len < 0)
				return len;
			rc = fwrite(buf, 1, len, f);
			if (rc != len)
				return -ENXIO;
			fflush(f);
		}
	}

	if (write)
		return ndctl_cmd_submit(cmd);

	return 0;
}

static int action_write(struct ndctl_dimm *dimm, struct action_context *actx)
{
	struct ndctl_cmd *cmd_read, *cmd_write;
	ssize_t size;
	int rc = 0;

	if (ndctl_dimm_is_active(dimm)) {
		fprintf(stderr, "dimm is active, abort label write\n");
		return -EBUSY;
	}

	cmd_read = ndctl_dimm_read_label_extent(dimm, param.len, param.offset);
	if (!cmd_read)
		return -EINVAL;

	cmd_write = ndctl_dimm_cmd_new_cfg_write(cmd_read);
	if (!cmd_write) {
		ndctl_cmd_unref(cmd_read);
		return -ENXIO;
	}

	size = ndctl_cmd_cfg_read_get_size(cmd_read);
	rc = rw_bin(actx->f_in, cmd_write, size, param.offset, WRITE);

	/*
	 * If the dimm is already disabled the kernel is not holding a cached
	 * copy of the label space.
	 */
	if (!ndctl_dimm_is_enabled(dimm))
		goto out;

	rc = ndctl_dimm_disable(dimm);
	if (rc)
		goto out;
	rc = ndctl_dimm_enable(dimm);

 out:
	ndctl_cmd_unref(cmd_read);
	ndctl_cmd_unref(cmd_write);

	return rc;
}

static int action_read(struct ndctl_dimm *dimm, struct action_context *actx)
{
	struct ndctl_cmd *cmd_read;
	ssize_t size;
	int rc = 0;

	if (param.index)
		cmd_read = ndctl_dimm_read_label_index(dimm);
	else
		cmd_read = ndctl_dimm_read_label_extent(dimm, param.len,
				param.offset);
	if (!cmd_read)
		return -EINVAL;

	size = ndctl_cmd_cfg_read_get_size(cmd_read);
	if (actx->jdimms) {
		struct json_object *jdimm = dump_json(dimm, cmd_read, size);

		if (jdimm)
			json_object_array_add(actx->jdimms, jdimm);
		else
			rc = -ENOMEM;
	} else
		rc = rw_bin(actx->f_out, cmd_read, size, param.offset, READ);

	ndctl_cmd_unref(cmd_read);

	return rc;
}

static int update_verify_input(struct action_context *actx)
{
	int rc;
	struct stat st;

	rc = fstat(fileno(actx->f_in), &st);
	if (rc == -1) {
		rc = -errno;
		fprintf(stderr, "fstat failed: %s\n", strerror(errno));
		return rc;
	}

	if (!S_ISREG(st.st_mode)) {
		fprintf(stderr, "Input not a regular file.\n");
		return -EINVAL;
	}

	if (st.st_size == 0) {
		fprintf(stderr, "Input file size is 0.\n");
		return -EINVAL;
	}

	actx->update.fw_size = st.st_size;
	return 0;
}

static int verify_fw_size(struct update_context *uctx)
{
	struct fw_info *fw = &uctx->dimm_fw;

	if (uctx->fw_size > fw->store_size) {
		error("Firmware file size greater than DIMM store\n");
		return -ENOSPC;
	}

	return 0;
}

static int submit_get_firmware_info(struct ndctl_dimm *dimm,
		struct action_context *actx)
{
	const char *devname = ndctl_dimm_get_devname(dimm);
	struct update_context *uctx = &actx->update;
	struct fw_info *fw = &uctx->dimm_fw;
	struct ndctl_cmd *cmd;
	int rc;
	enum ND_FW_STATUS status;

	cmd = ndctl_dimm_cmd_new_fw_get_info(dimm);
	if (!cmd)
		return -ENXIO;

	rc = ndctl_cmd_submit(cmd);
	if (rc < 0)
		goto out;

	rc = -ENXIO;
	status = ndctl_cmd_fw_xlat_firmware_status(cmd);
	if (status != FW_SUCCESS) {
		err("%s: failed to retrieve firmware info", devname);
		goto out;
	}

	fw->store_size = ndctl_cmd_fw_info_get_storage_size(cmd);
	if (fw->store_size == UINT_MAX)
		goto out;

	fw->update_size = ndctl_cmd_fw_info_get_max_send_len(cmd);
	if (fw->update_size == UINT_MAX)
		goto out;

	fw->query_interval = ndctl_cmd_fw_info_get_query_interval(cmd);
	if (fw->query_interval == UINT_MAX)
		goto out;

	fw->max_query = ndctl_cmd_fw_info_get_max_query_time(cmd);
	if (fw->max_query == UINT_MAX)
		goto out;

	fw->run_version = ndctl_cmd_fw_info_get_run_version(cmd);
	if (fw->run_version == ULLONG_MAX)
		goto out;

	rc = verify_fw_size(uctx);

out:
	ndctl_cmd_unref(cmd);
	return rc;
}

static int submit_abort_firmware(struct ndctl_dimm *dimm,
		struct action_context *actx)
{
	struct update_context *uctx = &actx->update;
	struct ndctl_cmd *cmd;
	int rc;
	enum ND_FW_STATUS status;

	cmd = ndctl_dimm_cmd_new_fw_abort(uctx->start);
	if (!cmd)
		return -ENXIO;

	rc = ndctl_cmd_submit(cmd);
	if (rc < 0)
		goto out;

	status = ndctl_cmd_fw_xlat_firmware_status(cmd);
	if (status != FW_ABORTED) {
		fprintf(stderr,
			"Firmware update abort on DIMM %s failed: %#x\n",
			ndctl_dimm_get_devname(dimm), status);
		rc = -ENXIO;
		goto out;
	}

out:
	ndctl_cmd_unref(cmd);
	return rc;
}

static int submit_start_firmware_upload(struct ndctl_dimm *dimm,
		struct action_context *actx)
{
	const char *devname = ndctl_dimm_get_devname(dimm);
	struct update_context *uctx = &actx->update;
	struct fw_info *fw = &uctx->dimm_fw;
	struct ndctl_cmd *cmd;
	enum ND_FW_STATUS status;
	int rc;

	cmd = ndctl_dimm_cmd_new_fw_start_update(dimm);
	if (!cmd)
		return -ENXIO;

	rc = ndctl_cmd_submit(cmd);
	if (rc < 0)
		goto err;

	uctx->start = cmd;
	status = ndctl_cmd_fw_xlat_firmware_status(cmd);
	if (status == FW_EBUSY) {
		if (param.force) {
			rc = submit_abort_firmware(dimm, actx);
			if (rc < 0) {
				err("%s: busy with another firmware update, "
				    "abort failed", devname);
				rc = -EBUSY;
				goto err;
			}
			rc = -EAGAIN;
			goto err;
		} else {
			err("%s: busy with another firmware update", devname);
			rc = -EBUSY;
			goto err;
		}
	}
	if (status != FW_SUCCESS) {
		err("%s: failed to create start context", devname);
		rc = -ENXIO;
		goto err;
	}

	fw->context = ndctl_cmd_fw_start_get_context(cmd);
	if (fw->context == UINT_MAX) {
		err("%s: failed to retrieve start context", devname);
		rc = -ENXIO;
		goto err;
	}

	return 0;

err:
	uctx->start = NULL;
	ndctl_cmd_unref(cmd);
	return rc;
}

static int get_fw_data_from_file(FILE *file, void *buf, uint32_t len)
{
	size_t rc;

	rc = fread(buf, len, 1, file);
	if (rc != 1) {
		if (feof(file))
			fprintf(stderr,
				"Firmware file shorter than expected\n");
		else if (ferror(file))
			fprintf(stderr, "Firmware file read error\n");
		return -EBADF;
	}

	return len;
}

static int send_firmware(struct ndctl_dimm *dimm, struct action_context *actx)
{
	const char *devname = ndctl_dimm_get_devname(dimm);
	struct update_context *uctx = &actx->update;
	struct fw_info *fw = &uctx->dimm_fw;
	uint32_t copied = 0, len, remain;
	struct ndctl_cmd *cmd = NULL;
	enum ND_FW_STATUS status;
	int rc = -ENXIO;
	ssize_t read;
	void *buf;

	buf = malloc(fw->update_size);
	if (!buf)
		return -ENOMEM;

	remain = uctx->fw_size;

	while (remain) {
		len = min(fw->update_size, remain);
		read = get_fw_data_from_file(actx->f_in, buf, len);
		if (read < 0) {
			rc = read;
			goto cleanup;
		}

		cmd = ndctl_dimm_cmd_new_fw_send(uctx->start, copied, read,
				buf);
		if (!cmd) {
			rc = -ENOMEM;
			goto cleanup;
		}

		rc = ndctl_cmd_submit(cmd);
		if (rc < 0) {
			err("%s: failed to issue firmware transmit: %d",
					devname, rc);
			goto cleanup;
		}

		status = ndctl_cmd_fw_xlat_firmware_status(cmd);
		if (status != FW_SUCCESS) {
			err("%s: failed to transmit firmware: %d",
					devname, status);
			rc = -EIO;
			goto cleanup;
		}

		copied += read;
		remain -= read;

		ndctl_cmd_unref(cmd);
		cmd = NULL;
	}

cleanup:
	ndctl_cmd_unref(cmd);
	free(buf);
	return rc;
}

static int submit_finish_firmware(struct ndctl_dimm *dimm,
		struct action_context *actx)
{
	const char *devname = ndctl_dimm_get_devname(dimm);
	struct update_context *uctx = &actx->update;
	enum ND_FW_STATUS status;
	struct ndctl_cmd *cmd;
	int rc = -ENXIO;

	cmd = ndctl_dimm_cmd_new_fw_finish(uctx->start);
	if (!cmd)
		return -ENXIO;

	rc = ndctl_cmd_submit(cmd);
	if (rc < 0)
		goto out;

	status = ndctl_cmd_fw_xlat_firmware_status(cmd);
	switch (status) {
	case FW_SUCCESS:
		rc = 0;
		break;
	case FW_ERETRY:
		err("%s: device busy with other operation (ARS?)", devname);
		break;
	case FW_EBADFW:
		err("%s: firmware image rejected", devname);
		break;
	default:
		err("%s: update failed: error code: %d", devname, status);
		break;
	}

out:
	ndctl_cmd_unref(cmd);
	return rc;
}

static enum ndctl_fwa_state fw_update_arm(struct ndctl_dimm *dimm)
{
	struct ndctl_bus *bus = ndctl_dimm_get_bus(dimm);
	const char *devname = ndctl_dimm_get_devname(dimm);
	enum ndctl_fwa_state state = ndctl_bus_get_fw_activate_state(bus);

	if (state == NDCTL_FWA_INVALID) {
		if (param.verbose)
			err("%s: firmware activate capability not found\n",
					devname);
		return NDCTL_FWA_INVALID;
	}

	if (state == NDCTL_FWA_ARM_OVERFLOW && !param.force) {
		err("%s: overflow detected skip arm\n", devname);
		return NDCTL_FWA_INVALID;
	}

	state = ndctl_dimm_fw_activate_arm(dimm);
	if (state != NDCTL_FWA_ARMED) {
		err("%s: failed to arm\n", devname);
		return NDCTL_FWA_INVALID;
	}

	if (param.force)
		return state;

	state = ndctl_bus_get_fw_activate_state(bus);
	if (state == NDCTL_FWA_ARM_OVERFLOW) {
		err("%s: arm aborted, tripped overflow\n", devname);
		ndctl_dimm_fw_activate_disarm(dimm);
		return NDCTL_FWA_INVALID;
	}
	return NDCTL_FWA_ARMED;
}

#define ARM_FAILURE_FATAL (1)
#define ARM_FAILURE_OK (0)

static int fw_update_toggle_arm(struct ndctl_dimm *dimm,
		struct json_object *jdimms, bool arm_fatal)
{
	enum ndctl_fwa_state state;
	struct json_object *jobj;
	unsigned long flags;

	if (param.disarm)
		state = ndctl_dimm_fw_activate_disarm(dimm);
	else if (param.arm)
		state = fw_update_arm(dimm);
	else
		state = NDCTL_FWA_INVALID;

	if (state == NDCTL_FWA_INVALID && arm_fatal)
		return -ENXIO;

	flags = UTIL_JSON_FIRMWARE;
	if (isatty(1))
		flags |= UTIL_JSON_HUMAN;
	jobj = util_dimm_to_json(dimm, flags);
	if (jobj)
		json_object_array_add(jdimms, jobj);

	return 0;
}

static int query_fw_finish_status(struct ndctl_dimm *dimm,
		struct action_context *actx)
{
	const char *devname = ndctl_dimm_get_devname(dimm);
	struct update_context *uctx = &actx->update;
	struct fw_info *fw = &uctx->dimm_fw;
	struct timespec now, before, after;
	enum ND_FW_STATUS status;
	struct ndctl_cmd *cmd;
	uint64_t ver;
	int rc;

	cmd = ndctl_dimm_cmd_new_fw_finish_query(uctx->start);
	if (!cmd)
		return -ENXIO;

	rc = clock_gettime(CLOCK_MONOTONIC, &before);
	if (rc < 0)
		goto unref;

	now.tv_nsec = fw->query_interval / 1000;
	now.tv_sec = 0;

again:
	rc = ndctl_cmd_submit(cmd);
	if (rc < 0)
		goto unref;

	status = ndctl_cmd_fw_xlat_firmware_status(cmd);
	if (status == FW_EBUSY) {
		/* Still on going, continue */
		rc = clock_gettime(CLOCK_MONOTONIC, &after);
		if (rc < 0) {
			rc = -errno;
			goto unref;
		}

		/*
		 * If we expire max query time,
		 * we timed out
		 */
		if (after.tv_sec - before.tv_sec >
				fw->max_query / 1000000) {
			rc = -ETIMEDOUT;
			goto unref;
		}

		/*
		 * Sleep the interval dictated by firmware
		 * before query again.
		 */
		rc = nanosleep(&now, NULL);
		if (rc < 0) {
			rc = -errno;
			goto unref;
		}
		goto again;
	}

	/* We are done determine error code */
	switch (status) {
	case FW_SUCCESS:
		ver = ndctl_cmd_fw_fquery_get_fw_rev(cmd);
		if (ver == 0) {
			err("%s: new firmware not found after update", devname);
			rc = -EIO;
			goto unref;
		}

		/*
		 * Now try to arm/disarm firmware activation if
		 * requested.  Failure to toggle the arm state is not
		 * fatal, the success / failure will be inferred from
		 * the emitted json state.
		 */
		fw_update_toggle_arm(dimm, actx->jdimms, ARM_FAILURE_OK);
		rc = 0;
		break;
	case FW_EBADFW:
		err("%s: firmware verification failure", devname);
		rc = -EINVAL;
		break;
	case FW_ENORES:
		err("%s: timeout awaiting update", devname);
		rc = -ETIMEDOUT;
		break;
	default:
		err("%s: unhandled error %d", devname, status);
		rc = -EIO;
		break;
	}

unref:
	ndctl_cmd_unref(cmd);
	return rc;
}

static int update_firmware(struct ndctl_dimm *dimm,
		struct action_context *actx)
{
	const char *devname = ndctl_dimm_get_devname(dimm);
	int rc, i;

	rc = submit_get_firmware_info(dimm, actx);
	if (rc < 0)
		return rc;

	/* try a few times in the --force and state busy case */
	for (i = 0; i < 3; i++) {
		rc = submit_start_firmware_upload(dimm, actx);
		if (rc == -EAGAIN)
			continue;
		if (rc < 0)
			return rc;
		break;
	}

	if (param.verbose)
		fprintf(stderr, "%s: uploading firmware\n", devname);

	rc = send_firmware(dimm, actx);
	if (rc < 0) {
		err("%s: firmware send failed", devname);
		rc = submit_abort_firmware(dimm, actx);
		if (rc < 0)
			err("%s: abort failed", devname);
		return rc;
	}

	/*
	 * Done reading file, reset firmware file back to beginning for
	 * next update.
	 */
	rewind(actx->f_in);

	rc = submit_finish_firmware(dimm, actx);
	if (rc < 0) {
		err("%s: failed to finish update sequence", devname);
		rc = submit_abort_firmware(dimm, actx);
		if (rc < 0)
			err("%s: failed to abort update", devname);
		return rc;
	}

	rc = query_fw_finish_status(dimm, actx);
	if (rc < 0)
		return rc;

	return 0;
}

static int action_update(struct ndctl_dimm *dimm, struct action_context *actx)
{
	struct ndctl_bus *bus = ndctl_dimm_get_bus(dimm);
	const char *devname = ndctl_dimm_get_devname(dimm);
	int rc;

	if (!param.infile)
		return fw_update_toggle_arm(dimm, actx->jdimms,
				ARM_FAILURE_FATAL);

	rc = ndctl_dimm_fw_update_supported(dimm);
	switch (rc) {
	case -ENOTTY:
		err("%s: firmware update not supported by ndctl.", devname);
		return rc;
	case -EOPNOTSUPP:
		err("%s: firmware update not supported by the kernel", devname);
		return rc;
	case -EIO:
		err("%s: firmware update not supported by either platform firmware or the kernel.",
				devname);
		return rc;
	}

	if (ndctl_bus_get_scrub_state(bus) > 0 && !param.force) {
		err("%s: scrub active, retry after 'ndctl wait-scrub'",
				devname);
		return -EBUSY;
	}

	rc = update_verify_input(actx);
	if (rc < 0)
		return rc;

	rc = update_firmware(dimm, actx);
	if (rc < 0)
		return rc;

	ndctl_cmd_unref(actx->update.start);

	return rc;
}

static int action_setup_passphrase(struct ndctl_dimm *dimm,
		struct action_context *actx)
{
	if (ndctl_dimm_get_security(dimm) < 0) {
		error("%s: security operation not supported\n",
				ndctl_dimm_get_devname(dimm));
		return -EOPNOTSUPP;
	}

	if (!param.kek)
		return -EINVAL;

	return ndctl_dimm_setup_key(dimm, param.kek,
			param.master_pass ? ND_MASTER_KEY : ND_USER_KEY);
}

static int action_update_passphrase(struct ndctl_dimm *dimm,
		struct action_context *actx)
{
	if (ndctl_dimm_get_security(dimm) < 0) {
		error("%s: security operation not supported\n",
				ndctl_dimm_get_devname(dimm));
		return -EOPNOTSUPP;
	}

	return ndctl_dimm_update_key(dimm, param.kek,
			param.master_pass ? ND_MASTER_KEY : ND_USER_KEY);
}

static int action_remove_passphrase(struct ndctl_dimm *dimm,
		struct action_context *actx)
{
	if (ndctl_dimm_get_security(dimm) < 0) {
		error("%s: security operation not supported\n",
				ndctl_dimm_get_devname(dimm));
		return -EOPNOTSUPP;
	}

	return ndctl_dimm_remove_key(dimm);
}

static int action_security_freeze(struct ndctl_dimm *dimm,
		struct action_context *actx)
{
	int rc;

	if (ndctl_dimm_get_security(dimm) < 0) {
		error("%s: security operation not supported\n",
				ndctl_dimm_get_devname(dimm));
		return -EOPNOTSUPP;
	}

	rc = ndctl_dimm_freeze_security(dimm);
	if (rc < 0)
		error("Failed to freeze security for %s\n",
				ndctl_dimm_get_devname(dimm));
	return rc;
}

static int action_sanitize_dimm(struct ndctl_dimm *dimm,
		struct action_context *actx)
{
	int rc;
	enum ndctl_key_type key_type;

	if (ndctl_dimm_get_security(dimm) < 0) {
		error("%s: security operation not supported\n",
				ndctl_dimm_get_devname(dimm));
		return -EOPNOTSUPP;
	}

	if (param.overwrite && param.master_pass) {
		error("%s: overwrite does not support master passphrase\n",
				ndctl_dimm_get_devname(dimm));
		return -EINVAL;
	}

	/*
	 * Setting crypto erase to be default. The other method will be
	 * overwrite.
	 */
	if (!param.crypto_erase && !param.overwrite) {
		param.crypto_erase = true;
		if (param.verbose)
			fprintf(stderr, "No santize method passed in, default to crypto-erase\n");
	}

	if (param.crypto_erase) {
		if (param.zero_key)
			key_type = ND_ZERO_KEY;
		else if (param.master_pass)
			key_type = ND_MASTER_KEY;
		else
			key_type = ND_USER_KEY;

		rc = ndctl_dimm_secure_erase_key(dimm, key_type);
		if (rc < 0)
			return rc;
	}

	if (param.overwrite) {
		rc = ndctl_dimm_overwrite_key(dimm);
		if (rc < 0)
			return rc;
	}

	return 0;
}

static int action_wait_overwrite(struct ndctl_dimm *dimm,
		struct action_context *actx)
{
	int rc;

	if (ndctl_dimm_get_security(dimm) < 0) {
		error("%s: security operation not supported\n",
				ndctl_dimm_get_devname(dimm));
		return -EOPNOTSUPP;
	}

	rc = ndctl_dimm_wait_overwrite(dimm);
	if (rc == 1 && param.verbose)
		fprintf(stderr, "%s: overwrite completed.\n",
				ndctl_dimm_get_devname(dimm));
	return rc;
}

static int __action_init(struct ndctl_dimm *dimm,
		enum ndctl_namespace_version version, int chk_only)
{
	struct ndctl_cmd *cmd_read;
	int rc;

	cmd_read = ndctl_dimm_read_label_index(dimm);
	if (!cmd_read)
		return -ENXIO;

	/*
	 * If the region goes active after this point, i.e. we're racing
	 * another administrative action, the kernel will fail writes to
	 * the label area.
	 */
	if (!chk_only && ndctl_dimm_is_active(dimm)) {
		fprintf(stderr, "%s: regions active, abort label write\n",
				ndctl_dimm_get_devname(dimm));
		rc = -EBUSY;
		goto out;
	}

	rc = ndctl_dimm_validate_labels(dimm);
	if (chk_only)
		goto out;

	if (rc >= 0 && !param.force) {
		fprintf(stderr, "%s: error: labels already initialized\n",
				ndctl_dimm_get_devname(dimm));
		rc = -EBUSY;
		goto out;
	}

	rc = ndctl_dimm_init_labels(dimm, version);
	if (rc < 0)
		goto out;

	/*
	 * If the dimm is already disabled the kernel is not holding a cached
	 * copy of the label space.
	 */
	if (!ndctl_dimm_is_enabled(dimm))
		goto out;

	rc = ndctl_dimm_disable(dimm);
	if (rc)
		goto out;
	rc = ndctl_dimm_enable(dimm);

 out:
	ndctl_cmd_unref(cmd_read);
	return rc >= 0 ? 0 : rc;
}

static int action_init(struct ndctl_dimm *dimm, struct action_context *actx)
{
	return __action_init(dimm, actx->labelversion, 0);
}

static int action_check(struct ndctl_dimm *dimm, struct action_context *actx)
{
	return __action_init(dimm, 0, 1);
}


#define BASE_OPTIONS() \
OPT_STRING('b', "bus", &param.bus, "bus-id", \
	"<nmem> must be on a bus with an id/provider of <bus-id>"), \
OPT_BOOLEAN('v',"verbose", &param.verbose, "turn on debug")

#define READ_OPTIONS() \
OPT_STRING('o', "output", &param.outfile, "output-file", \
	"filename to write label area contents"), \
OPT_BOOLEAN('j', "json", &param.json, "parse label data into json"), \
OPT_BOOLEAN('u', "human", &param.human, "use human friendly number formats (implies --json)"), \
OPT_BOOLEAN('I', "index", &param.index, "limit read to the index block area")

#define WRITE_OPTIONS() \
OPT_STRING('i', "input", &param.infile, "input-file", \
	"filename to read label area data")

#define UPDATE_OPTIONS() \
OPT_STRING('f', "firmware", &param.infile, "firmware-file", \
	"firmware filename for update"), \
OPT_BOOLEAN('i', "force", &param.force, "ignore ARS / arm status, try to force update"), \
OPT_BOOLEAN_SET('A', "arm", &param.arm, &param.arm_set, \
	"arm device for firmware activation (default)"), \
OPT_BOOLEAN_SET('D', "disarm", &param.disarm, &param.disarm_set, \
	"disarm device for firmware activation")

#define INIT_OPTIONS() \
OPT_BOOLEAN('f', "force", &param.force, \
		"force initialization even if existing index-block present"), \
OPT_STRING('V', "label-version", &param.labelversion, "version-number", \
	"namespace label specification version (default: 1.1)")

#define KEY_OPTIONS() \
OPT_STRING('k', "key-handle", &param.kek, "key-handle", \
		"master encryption key handle")

#define SANITIZE_OPTIONS() \
OPT_BOOLEAN('c', "crypto-erase", &param.crypto_erase, \
		"crypto erase a dimm"), \
OPT_BOOLEAN('o', "overwrite", &param.overwrite, \
		"overwrite a dimm"), \
OPT_BOOLEAN('z', "zero-key", &param.zero_key, \
		"pass in a zero key")

#define MASTER_OPTIONS() \
OPT_BOOLEAN('m', "master-passphrase", &param.master_pass, \
		"use master passphrase")

#define LABEL_OPTIONS() \
OPT_UINTEGER('s', "size", &param.len, "number of label bytes to operate"), \
OPT_UINTEGER('O', "offset", &param.offset, \
	"offset into the label area to start operation")

static const struct option read_options[] = {
	BASE_OPTIONS(),
	LABEL_OPTIONS(),
	READ_OPTIONS(),
	OPT_END(),
};

static const struct option write_options[] = {
	BASE_OPTIONS(),
	LABEL_OPTIONS(),
	WRITE_OPTIONS(),
	OPT_END(),
};

static const struct option zero_options[] = {
	BASE_OPTIONS(),
	LABEL_OPTIONS(),
	OPT_END(),
};

static const struct option update_options[] = {
	BASE_OPTIONS(),
	UPDATE_OPTIONS(),
	OPT_END(),
};

static const struct option base_options[] = {
	BASE_OPTIONS(),
	OPT_END(),
};

static const struct option init_options[] = {
	BASE_OPTIONS(),
	INIT_OPTIONS(),
	OPT_END(),
};

static const struct option key_options[] = {
	BASE_OPTIONS(),
	KEY_OPTIONS(),
	MASTER_OPTIONS(),
};

static const struct option sanitize_options[] = {
	BASE_OPTIONS(),
	SANITIZE_OPTIONS(),
	MASTER_OPTIONS(),
	OPT_END(),
};

static int dimm_action(int argc, const char **argv, struct ndctl_ctx *ctx,
		int (*action)(struct ndctl_dimm *dimm, struct action_context *actx),
		const struct option *options, const char *usage)
{
	struct action_context actx = { 0 };
	int i, rc = 0, count = 0, err = 0;
	struct ndctl_dimm *single = NULL;
	const char * const u[] = {
		usage,
		NULL
	};
	unsigned long id;
	bool json = false;

        argc = parse_options(argc, argv, options, u, 0);

	if (argc == 0)
		usage_with_options(u, options);
	for (i = 0; i < argc; i++) {
		if (strcmp(argv[i], "all") == 0) {
			argv[0] = "all";
			argc = 1;
			break;
		}

		if (sscanf(argv[i], "nmem%lu", &id) != 1) {
			fprintf(stderr, "'%s' is not a valid dimm name\n",
					argv[i]);
			err++;
		}
	}

	if (err == argc) {
		usage_with_options(u, options);
		return -EINVAL;
	}

	json = param.json || param.human || action == action_update;
	if (action == action_read && json && (param.len || param.offset)) {
		fprintf(stderr, "--size and --offset are incompatible with --json\n");
		usage_with_options(u, options);
		return -EINVAL;
	}

	if (param.index && param.len) {
		fprintf(stderr, "pick either --size, or --index, not both\n");
		usage_with_options(u, options);
		return -EINVAL;
	}

	if (json) {
		actx.jdimms = json_object_new_array();
		if (!actx.jdimms)
			return -ENOMEM;
	}

	if (!param.outfile)
		actx.f_out = stdout;
	else {
		actx.f_out = fopen(param.outfile, "w+");
		if (!actx.f_out) {
			fprintf(stderr, "failed to open: %s: (%s)\n",
					param.outfile, strerror(errno));
			rc = -errno;
			goto out;
		}
	}

	if (param.arm_set && param.disarm_set) {
		fprintf(stderr, "set either --arm, or --disarm, not both\n");
		usage_with_options(u, options);
	}

	if (param.disarm_set && !param.disarm) {
		fprintf(stderr, "--no-disarm syntax not supported\n");
		usage_with_options(u, options);
		return -EINVAL;
	}

	if (!param.infile) {
		/*
		 * Update needs an infile unless we are only being
		 * called to toggle the arm state. Other actions either
		 * do no need an input file, or are prepared for stdin.
		 */
		if (action == action_update) {
			if (!param.arm_set && !param.disarm_set) {
				fprintf(stderr, "require --arm, or --disarm\n");
				usage_with_options(u, options);
				return -EINVAL;
			}

			if (param.arm_set && !param.arm) {
				fprintf(stderr, "--no-arm syntax not supported\n");
				usage_with_options(u, options);
				return -EINVAL;
			}
		}
		actx.f_in = stdin;
	} else {
		actx.f_in = fopen(param.infile, "r");
		if (!actx.f_in) {
			fprintf(stderr, "failed to open: %s: (%s)\n",
					param.infile, strerror(errno));
			rc = -errno;
			goto out_close_fout;
		}
	}

	if (param.verbose)
		ndctl_set_log_priority(ctx, LOG_DEBUG);

	if (strcmp(param.labelversion, "1.1") == 0)
		actx.labelversion = NDCTL_NS_VERSION_1_1;
	else if (strcmp(param.labelversion, "v1.1") == 0)
		actx.labelversion = NDCTL_NS_VERSION_1_1;
	else if (strcmp(param.labelversion, "1.2") == 0)
		actx.labelversion = NDCTL_NS_VERSION_1_2;
	else if (strcmp(param.labelversion, "v1.2") == 0)
		actx.labelversion = NDCTL_NS_VERSION_1_2;
	else {
		fprintf(stderr, "'%s' is not a valid label version\n",
				param.labelversion);
		rc = -EINVAL;
		goto out_close_fin_fout;
	}

	rc = 0;
	err = 0;
	count = 0;
	for (i = 0; i < argc; i++) {
		struct ndctl_dimm *dimm;
		struct ndctl_bus *bus;

		if (sscanf(argv[i], "nmem%lu", &id) != 1
				&& strcmp(argv[i], "all") != 0)
			continue;

		ndctl_bus_foreach(ctx, bus) {
			if (!util_bus_filter(bus, param.bus))
				continue;
			ndctl_dimm_foreach(bus, dimm) {
				if (!util_dimm_filter(dimm, argv[i]))
					continue;
				if (action == action_write) {
					single = dimm;
					rc = 0;
				} else
					rc = action(dimm, &actx);

				if (rc == 0)
					count++;
				else if (rc && !err)
					err = rc;
			}
		}
	}
	rc = err;

	if (action == action_write) {
		if (count > 1) {
			error("write-labels only supports writing a single dimm\n");
			usage_with_options(u, options);
			return -EINVAL;
		} else if (single)
			rc = action(single, &actx);
	}

	if (actx.jdimms && json_object_array_length(actx.jdimms) > 0) {
		unsigned long flags = 0;

		if (actx.f_out == stdout && isatty(1))
			flags |= UTIL_JSON_HUMAN;
		util_display_json_array(actx.f_out, actx.jdimms, flags);
	}

 out_close_fin_fout:
	if (actx.f_in != stdin)
		fclose(actx.f_in);

 out_close_fout:
	if (actx.f_out != stdout)
		fclose(actx.f_out);

 out:
	/*
	 * count if some actions succeeded, 0 if none were attempted,
	 * negative error code otherwise.
	 */
	if (count > 0)
		return count;
	return rc;
}

int cmd_write_labels(int argc, const char **argv, struct ndctl_ctx *ctx)
{
	int count = dimm_action(argc, argv, ctx, action_write, write_options,
			"ndctl write-labels <nmem> [-i <filename>]");

	fprintf(stderr, "wrote %d nmem%s\n", count >= 0 ? count : 0,
			count > 1 ? "s" : "");
	return count >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_read_labels(int argc, const char **argv, struct ndctl_ctx *ctx)
{
	int count = dimm_action(argc, argv, ctx, action_read, read_options,
			"ndctl read-labels <nmem0> [<nmem1>..<nmemN>] [-o <filename>]");

	fprintf(stderr, "read %d nmem%s\n", count >= 0 ? count : 0,
			count > 1 ? "s" : "");
	return count >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_zero_labels(int argc, const char **argv, struct ndctl_ctx *ctx)
{
	int count = dimm_action(argc, argv, ctx, action_zero, zero_options,
			"ndctl zero-labels <nmem0> [<nmem1>..<nmemN>] [<options>]");

	fprintf(stderr, "zeroed %d nmem%s\n", count >= 0 ? count : 0,
			count > 1 ? "s" : "");
	return count >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_init_labels(int argc, const char **argv, struct ndctl_ctx *ctx)
{
	int count = dimm_action(argc, argv, ctx, action_init, init_options,
			"ndctl init-labels <nmem0> [<nmem1>..<nmemN>] [<options>]");

	fprintf(stderr, "initialized %d nmem%s\n", count >= 0 ? count : 0,
			count > 1 ? "s" : "");
	return count >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_check_labels(int argc, const char **argv, struct ndctl_ctx *ctx)
{
	int count = dimm_action(argc, argv, ctx, action_check, base_options,
			"ndctl check-labels <nmem0> [<nmem1>..<nmemN>] [<options>]");

	fprintf(stderr, "successfully verified %d nmem label%s\n",
			count >= 0 ? count : 0, count > 1 ? "s" : "");
	return count >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_disable_dimm(int argc, const char **argv, struct ndctl_ctx *ctx)
{
	int count = dimm_action(argc, argv, ctx, action_disable, base_options,
			"ndctl disable-dimm <nmem0> [<nmem1>..<nmemN>] [<options>]");

	fprintf(stderr, "disabled %d nmem%s\n", count >= 0 ? count : 0,
			count > 1 ? "s" : "");
	return count >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_enable_dimm(int argc, const char **argv, struct ndctl_ctx *ctx)
{
	int count = dimm_action(argc, argv, ctx, action_enable, base_options,
			"ndctl enable-dimm <nmem0> [<nmem1>..<nmemN>] [<options>]");

	fprintf(stderr, "enabled %d nmem%s\n", count >= 0 ? count : 0,
			count > 1 ? "s" : "");
	return count >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_update_firmware(int argc, const char **argv, struct ndctl_ctx *ctx)
{
	int count;

	cmd_name = "update firmware";
	count = dimm_action(argc, argv, ctx, action_update, update_options,
			"ndctl update-firmware <nmem0> [<nmem1>..<nmemN>] [<options>]");

	fprintf(stderr, "updated %d nmem%s.\n", count >= 0 ? count : 0,
			count > 1 ? "s" : "");
	return count >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_update_passphrase(int argc, const char **argv, struct ndctl_ctx *ctx)
{
	int count = dimm_action(argc, argv, ctx, action_update_passphrase,
			key_options,
			"ndctl update-passphrase <nmem0> [<nmem1>..<nmemN>] [<options>]");

	fprintf(stderr, "passphrase updated for %d nmem%s.\n", count >= 0 ? count : 0,
			count > 1 ? "s" : "");
	return count >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_setup_passphrase(int argc, const char **argv, struct ndctl_ctx *ctx)
{
	int count = dimm_action(argc, argv, ctx, action_setup_passphrase,
			key_options,
			"ndctl setup-passphrase <nmem0> [<nmem1>..<nmemN>] [<options>]");

	fprintf(stderr, "passphrase enabled for %d nmem%s.\n", count >= 0 ? count : 0,
			count > 1 ? "s" : "");
	return count >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_remove_passphrase(int argc, const char **argv, void *ctx)
{
	int count = dimm_action(argc, argv, ctx, action_remove_passphrase,
			base_options,
			"ndctl remove-passphrase <nmem0> [<nmem1>..<nmemN>] [<options>]");

	fprintf(stderr, "passphrase removed for %d nmem%s.\n", count >= 0 ? count : 0,
			count > 1 ? "s" : "");
	return count >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_freeze_security(int argc, const char **argv, void *ctx)
{
	int count = dimm_action(argc, argv, ctx, action_security_freeze, base_options,
			"ndctl freeze-security <nmem0> [<nmem1>..<nmemN>] [<options>]");

	fprintf(stderr, "security froze %d nmem%s.\n", count >= 0 ? count : 0,
			count > 1 ? "s" : "");
	return count >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_sanitize_dimm(int argc, const char **argv, void *ctx)
{
	int count = dimm_action(argc, argv, ctx, action_sanitize_dimm,
			sanitize_options,
			"ndctl sanitize-dimm <nmem0> [<nmem1>..<nmemN>] [<options>]");

	if (param.overwrite)
		fprintf(stderr, "overwrite issued for %d nmem%s.\n",
				count >= 0 ? count : 0, count > 1 ? "s" : "");
	else
		fprintf(stderr, "sanitized %d nmem%s.\n",
				count >= 0 ? count : 0, count > 1 ? "s" : "");
	return count >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_wait_overwrite(int argc, const char **argv, void *ctx)
{
	int count = dimm_action(argc, argv, ctx, action_wait_overwrite,
			base_options,
			"ndctl wait-overwrite <nmem0> [<nmem1>..<nmemN>] [<options>]");

	return count >= 0 ? 0 : EXIT_FAILURE;
}
