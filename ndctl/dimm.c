/*
 * Copyright (c) 2016, Intel Corporation.
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
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>
#include <syslog.h>
#include <util/log.h>
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
	bool crypto_erase;
	bool overwrite;
	bool zero_key;
	bool master_pass;
	bool human;
	bool force;
	bool json;
	bool verbose;
} param = {
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
	return ndctl_dimm_zero_labels(dimm);
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
	jindex = dump_index_json(cmd_read, size);
	if (!jindex)
		goto err_jindex;
	jlabel = dump_label_json(dimm, cmd_read, size, flags);
	if (!jlabel)
		goto err_jlabel;

	jobj = json_object_new_string(ndctl_dimm_get_devname(dimm));
	if (!jobj)
		goto err_jobj;

	json_object_object_add(jdimm, "dev", jobj);
	json_object_object_add(jdimm, "index", jindex);
	json_object_object_add(jdimm, "label", jlabel);
	return jdimm;

 err_jobj:
	json_object_put(jlabel);
 err_jlabel:
	json_object_put(jindex);
 err_jindex:
	json_object_put(jdimm);
	return NULL;
}

static int rw_bin(FILE *f, struct ndctl_cmd *cmd, ssize_t size, int rw)
{
	char buf[4096];
	ssize_t offset, write = 0;

	for (offset = 0; offset < size; offset += sizeof(buf)) {
		ssize_t len = min_t(ssize_t, sizeof(buf), size - offset), rc;

		if (rw) {
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

	cmd_read = ndctl_dimm_read_labels(dimm);
	if (!cmd_read)
		return -ENXIO;

	cmd_write = ndctl_dimm_cmd_new_cfg_write(cmd_read);
	if (!cmd_write) {
		ndctl_cmd_unref(cmd_read);
		return -ENXIO;
	}

	size = ndctl_cmd_cfg_read_get_size(cmd_read);
	rc = rw_bin(actx->f_in, cmd_write, size, 1);

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

	cmd_read = ndctl_dimm_read_labels(dimm);
	if (!cmd_read)
		return -ENXIO;

	size = ndctl_cmd_cfg_read_get_size(cmd_read);
	if (actx->jdimms) {
		struct json_object *jdimm = dump_json(dimm, cmd_read, size);

		if (jdimm)
			json_object_array_add(actx->jdimms, jdimm);
		else
			rc = -ENOMEM;
	} else
		rc = rw_bin(actx->f_out, cmd_read, size, 0);

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
		fprintf(stderr, "GET FIRMWARE INFO on DIMM %s failed: %#x\n",
				ndctl_dimm_get_devname(dimm), status);
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

static int submit_start_firmware_upload(struct ndctl_dimm *dimm,
		struct action_context *actx)
{
	struct update_context *uctx = &actx->update;
	struct fw_info *fw = &uctx->dimm_fw;
	struct ndctl_cmd *cmd;
	int rc;
	enum ND_FW_STATUS status;

	cmd = ndctl_dimm_cmd_new_fw_start_update(dimm);
	if (!cmd)
		return -ENXIO;

	rc = ndctl_cmd_submit(cmd);
	if (rc < 0)
		return rc;

	status = ndctl_cmd_fw_xlat_firmware_status(cmd);
	if (status != FW_SUCCESS) {
		fprintf(stderr,
			"START FIRMWARE UPDATE on DIMM %s failed: %#x\n",
			ndctl_dimm_get_devname(dimm), status);
		if (status == FW_EBUSY)
			fprintf(stderr, "Another firmware upload in progress"
					" or firmware already updated.\n");
		return -ENXIO;
	}

	fw->context = ndctl_cmd_fw_start_get_context(cmd);
	if (fw->context == UINT_MAX) {
		fprintf(stderr,
			"Retrieved firmware context invalid on DIMM %s\n",
			ndctl_dimm_get_devname(dimm));
		return -ENXIO;
	}

	uctx->start = cmd;

	return 0;
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

static int send_firmware(struct ndctl_dimm *dimm,
		struct action_context *actx)
{
	struct update_context *uctx = &actx->update;
	struct fw_info *fw = &uctx->dimm_fw;
	struct ndctl_cmd *cmd = NULL;
	ssize_t read;
	int rc = -ENXIO;
	enum ND_FW_STATUS status;
	uint32_t copied = 0, len, remain;
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
			rc = -ENXIO;
			goto cleanup;
		}

		rc = ndctl_cmd_submit(cmd);
		if (rc < 0)
			goto cleanup;

		status = ndctl_cmd_fw_xlat_firmware_status(cmd);
		if (status != FW_SUCCESS) {
			error("SEND FIRMWARE failed: %#x\n", status);
			rc = -ENXIO;
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
	struct update_context *uctx = &actx->update;
	struct ndctl_cmd *cmd;
	int rc;
	enum ND_FW_STATUS status;

	cmd = ndctl_dimm_cmd_new_fw_finish(uctx->start);
	if (!cmd)
		return -ENXIO;

	rc = ndctl_cmd_submit(cmd);
	if (rc < 0)
		goto out;

	status = ndctl_cmd_fw_xlat_firmware_status(cmd);
	if (status != FW_SUCCESS) {
		fprintf(stderr,
			"FINISH FIRMWARE UPDATE on DIMM %s failed: %#x\n",
			ndctl_dimm_get_devname(dimm), status);
		rc = -ENXIO;
		goto out;
	}

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
	if (!(status & ND_CMD_STATUS_FIN_ABORTED)) {
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

static int query_fw_finish_status(struct ndctl_dimm *dimm,
		struct action_context *actx)
{
	struct update_context *uctx = &actx->update;
	struct fw_info *fw = &uctx->dimm_fw;
	struct ndctl_cmd *cmd;
	int rc;
	enum ND_FW_STATUS status;
	bool done = false;
	struct timespec now, before, after;
	uint64_t ver;

	cmd = ndctl_dimm_cmd_new_fw_finish_query(uctx->start);
	if (!cmd)
		return -ENXIO;

	rc = clock_gettime(CLOCK_MONOTONIC, &before);
	if (rc < 0)
		goto out;

	now.tv_nsec = fw->query_interval / 1000;
	now.tv_sec = 0;

	do {
		rc = ndctl_cmd_submit(cmd);
		if (rc < 0)
			break;

		status = ndctl_cmd_fw_xlat_firmware_status(cmd);
		switch (status) {
		case FW_SUCCESS:
			ver = ndctl_cmd_fw_fquery_get_fw_rev(cmd);
			if (ver == 0) {
				fprintf(stderr, "No firmware updated.\n");
				rc = -ENXIO;
				goto out;
			}

			printf("Image updated successfully to DIMM %s.\n",
					ndctl_dimm_get_devname(dimm));
			printf("Firmware version %#lx.\n", ver);
			printf("Cold reboot to activate.\n");
			done = true;
			rc = 0;
			break;
		case FW_EBUSY:
			/* Still on going, continue */
			rc = clock_gettime(CLOCK_MONOTONIC, &after);
			if (rc < 0) {
				rc = -errno;
				goto out;
			}

			/*
			 * If we expire max query time,
			 * we timed out
			 */
			if (after.tv_sec - before.tv_sec >
					fw->max_query / 1000000) {
				rc = -ETIMEDOUT;
				goto out;
			}

			/*
			 * Sleep the interval dictated by firmware
			 * before query again.
			 */
			rc = nanosleep(&now, NULL);
			if (rc < 0) {
				rc = -errno;
				goto out;
			}
			break;
		case FW_EBADFW:
			fprintf(stderr,
				"Firmware failed to verify by DIMM %s.\n",
				ndctl_dimm_get_devname(dimm));
		case FW_EINVAL_CTX:
		case FW_ESEQUENCE:
			done = true;
			rc = -ENXIO;
			goto out;
		case FW_ENORES:
			fprintf(stderr,
				"Firmware update sequence timed out: %s\n",
				ndctl_dimm_get_devname(dimm));
			rc = -ETIMEDOUT;
			done = true;
			goto out;
		default:
			fprintf(stderr,
				"Unknown update status: %#x on DIMM %s\n",
				status, ndctl_dimm_get_devname(dimm));
			rc = -EINVAL;
			done = true;
			goto out;
		}
	} while (!done);

out:
	ndctl_cmd_unref(cmd);
	return rc;
}

static int update_firmware(struct ndctl_dimm *dimm,
		struct action_context *actx)
{
	int rc;

	rc = submit_get_firmware_info(dimm, actx);
	if (rc < 0)
		return rc;

	rc = submit_start_firmware_upload(dimm, actx);
	if (rc < 0)
		return rc;

	printf("Uploading firmware to DIMM %s.\n",
			ndctl_dimm_get_devname(dimm));

	rc = send_firmware(dimm, actx);
	if (rc < 0) {
		fprintf(stderr, "Firmware send failed. Aborting!\n");
		rc = submit_abort_firmware(dimm, actx);
		if (rc < 0)
			fprintf(stderr, "Aborting update sequence failed.\n");
		return rc;
	}

	/*
	 * Done reading file, reset firmware file back to beginning for
	 * next update.
	 */
	rewind(actx->f_in);

	rc = submit_finish_firmware(dimm, actx);
	if (rc < 0) {
		fprintf(stderr, "Unable to end update sequence.\n");
		rc = submit_abort_firmware(dimm, actx);
		if (rc < 0)
			fprintf(stderr, "Aborting update sequence failed.\n");
		return rc;
	}

	rc = query_fw_finish_status(dimm, actx);
	if (rc < 0)
		return rc;

	return 0;
}

static int action_update(struct ndctl_dimm *dimm, struct action_context *actx)
{
	int rc;

	rc = ndctl_dimm_fw_update_supported(dimm);
	switch (rc) {
	case -ENOTTY:
		error("%s: firmware update not supported by ndctl.",
			ndctl_dimm_get_devname(dimm));
		return rc;
	case -EOPNOTSUPP:
		error("%s: firmware update not supported by the kernel",
			ndctl_dimm_get_devname(dimm));
		return rc;
	case -EIO:
		error("%s: firmware update not supported by either platform firmware or the kernel.",
			ndctl_dimm_get_devname(dimm));
		return rc;
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
		printf("No santize method passed in, default to crypto-erase\n");
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
	if (rc == 1)
		printf("%s: overwrite completed.\n",
				ndctl_dimm_get_devname(dimm));
	return rc;
}

static int __action_init(struct ndctl_dimm *dimm,
		enum ndctl_namespace_version version, int chk_only)
{
	struct ndctl_cmd *cmd_read;
	int rc;

	cmd_read = ndctl_dimm_read_labels(dimm);
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
OPT_BOOLEAN('u', "human", &param.human, "use human friendly number formats (implies --json)")

#define WRITE_OPTIONS() \
OPT_STRING('i', "input", &param.infile, "input-file", \
	"filename to read label area data")

#define UPDATE_OPTIONS() \
OPT_STRING('f', "firmware", &param.infile, "firmware-file", \
	"firmware filename for update")

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

static const struct option read_options[] = {
	BASE_OPTIONS(),
	READ_OPTIONS(),
	OPT_END(),
};

static const struct option write_options[] = {
	BASE_OPTIONS(),
	WRITE_OPTIONS(),
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

	if (param.json || param.human) {
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

	if (!param.infile) {
		if (action == action_update) {
			usage_with_options(u, options);
			return -EINVAL;
		}
		actx.f_in = stdin;
	} else {
		actx.f_in = fopen(param.infile, "r");
		if (!actx.f_in) {
			fprintf(stderr, "failed to open: %s: (%s)\n",
					param.infile, strerror(errno));
			rc = -errno;
			goto out;
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
		goto out;
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

	if (actx.jdimms)
		util_display_json_array(actx.f_out, actx.jdimms, 0);

	if (actx.f_out != stdout)
		fclose(actx.f_out);

	if (actx.f_in != stdin)
		fclose(actx.f_in);

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
	int count = dimm_action(argc, argv, ctx, action_zero, base_options,
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
	int count = dimm_action(argc, argv, ctx, action_update, update_options,
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

	fprintf(stderr, "security freezed %d nmem%s.\n", count >= 0 ? count : 0,
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
