
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
#include <unistd.h>
#include <limits.h>
#include <syslog.h>
#include <uuid/uuid.h>
#include <util/filter.h>
#include <util/json.h>
#include <json-c/json.h>
#include <ndctl/libndctl.h>
#include <util/parse-options.h>
#include <ccan/minmax/minmax.h>
#define CCAN_SHORT_TYPES_H
#include <ccan/endian/endian.h>

enum {
	NSINDEX_SIG_LEN = 16,
	NSINDEX_ALIGN = 256,
	NSINDEX_SEQ_MASK = 0x3,
	NSLABEL_UUID_LEN = 16,
	NSLABEL_NAME_LEN = 64,
};

struct namespace_index {
        char sig[NSINDEX_SIG_LEN];
        le32 flags;
        le32 seq;
        le64 myoff;
        le64 mysize;
        le64 otheroff;
        le64 labeloff;
        le32 nslot;
        le16 major;
        le16 minor;
        le64 checksum;
        char free[0];
};

struct namespace_label {
	char uuid[NSLABEL_UUID_LEN];
	char name[NSLABEL_NAME_LEN];
	le32 flags;
	le16 nlabel;
	le16 position;
	le64 isetcookie;
	le64 lbasize;
	le64 dpa;
	le64 rawsize;
	le32 slot;
	le32 unused;
};

static struct json_object *dump_label_json(struct ndctl_cmd *cmd_read, ssize_t size)
{
	struct json_object *jarray = json_object_new_array();
	struct json_object *jlabel = NULL;
	struct namespace_label nslabel;
	unsigned int slot = -1;
	ssize_t offset;

	if (!jarray)
		return NULL;

	for (offset = NSINDEX_ALIGN * 2; offset < size; offset += sizeof(nslabel)) {
		ssize_t len = min_t(ssize_t, sizeof(nslabel), size - offset);
		struct json_object *jobj;
		char uuid[40];

		slot++;
		jlabel = json_object_new_object();
		if (!jlabel)
			break;

		if (len < (ssize_t) sizeof(nslabel))
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

		jobj = json_object_new_int64(le64_to_cpu(nslabel.isetcookie));
		if (!jobj)
			break;
		json_object_object_add(jlabel, "isetcookie", jobj);

		jobj = json_object_new_int64(le64_to_cpu(nslabel.dpa));
		if (!jobj)
			break;
		json_object_object_add(jlabel, "dpa", jobj);

		jobj = json_object_new_int64(le64_to_cpu(nslabel.rawsize));
		if (!jobj)
			break;
		json_object_object_add(jlabel, "rawsize", jobj);

		json_object_array_add(jarray, jlabel);
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
	struct json_object *jdimm = json_object_new_object();
	struct json_object *jlabel, *jobj, *jindex;

	if (!jdimm)
		return NULL;
	jindex = dump_index_json(cmd_read, size);
	if (!jindex)
		goto err_jindex;
	jlabel = dump_label_json(cmd_read, size);
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

static int dump_bin(FILE *f_out, struct ndctl_cmd *cmd_read, ssize_t size)
{
	char buf[4096];
	ssize_t offset;

	for (offset = 0; offset < size; offset += sizeof(buf)) {
		ssize_t len = min_t(ssize_t, sizeof(buf), size - offset), rc;

		len = ndctl_cmd_cfg_read_get_data(cmd_read, buf, len, offset);
		if (len < 0)
			return len;
		rc = fwrite(buf, 1, len, f_out);
		if (rc != len)
			return -ENXIO;
		fflush(f_out);
	}

	return 0;
}

static int do_read_dimm(FILE *f_out, struct ndctl_dimm *dimm, const char **argv,
		int argc, bool verbose, struct json_object *jdimms)
{
	struct ndctl_ctx *ctx = ndctl_dimm_get_ctx(dimm);
	struct ndctl_bus *bus = ndctl_dimm_get_bus(dimm);
	struct ndctl_cmd *cmd_size, *cmd_read;
	ssize_t size;
	int i, rc, log;

	for (i = 0; i < argc; i++)
		if (util_dimm_filter(dimm, argv[i]))
			break;
	if (i >= argc)
		return -ENODEV;

	log = ndctl_get_log_priority(ctx);
	if (verbose)
		ndctl_set_log_priority(ctx, LOG_DEBUG);

	rc = ndctl_bus_wait_probe(bus);
	if (rc < 0)
		goto out;

	cmd_size = ndctl_dimm_cmd_new_cfg_size(dimm);
	if (!cmd_size)
		return -ENOTTY;
	rc = ndctl_cmd_submit(cmd_size);
	if (rc || ndctl_cmd_get_firmware_status(cmd_size))
		goto out_size;

	cmd_read = ndctl_dimm_cmd_new_cfg_read(cmd_size);
	if (!cmd_read) {
		rc = -ENOTTY;
		goto out_size;
	}
	rc = ndctl_cmd_submit(cmd_read);
	if (rc || ndctl_cmd_get_firmware_status(cmd_read))
		goto out_read;

	size = ndctl_cmd_cfg_size_get_size(cmd_size);
	if (jdimms) {
		struct json_object *jdimm = dump_json(dimm, cmd_read, size);
		if (!jdimm)
			return -ENOMEM;
		json_object_array_add(jdimms, jdimm);
	} else
		rc = dump_bin(f_out, cmd_read, size);

 out_read:
	ndctl_cmd_unref(cmd_read);
 out_size:
	ndctl_cmd_unref(cmd_size);
 out:
	ndctl_set_log_priority(ctx, log);

	return rc;
}

int cmd_read_labels(int argc, const char **argv)
{
	const char *nmem_bus = NULL, *output = NULL;
	bool verbose = false, json = false;
	const struct option nmem_options[] = {
		OPT_STRING('b', "bus", &nmem_bus, "bus-id",
				"<nmem> must be on a bus with an id/provider of <bus-id>"),
		OPT_STRING('o', NULL, &output, "output-file",
				"filename to write label area contents"),
		OPT_BOOLEAN('j', "json", &json, "parse label data into json"),
		OPT_BOOLEAN('v',"verbose", &verbose, "turn on debug"),
		OPT_END(),
	};
	const char * const u[] = {
		"ndctl read-labels <nmem0> [<nmem1>..<nmemN>] [-o <filename>]",
		NULL
	};
	struct json_object *jdimms = NULL;
	struct ndctl_dimm *dimm;
	struct ndctl_ctx *ctx;
	struct ndctl_bus *bus;
	int i, rc, count, err = 0;
	FILE *f_out = NULL;

        argc = parse_options(argc, argv, nmem_options, u, 0);

	if (argc == 0)
		usage_with_options(u, nmem_options);
	for (i = 0; i < argc; i++) {
		unsigned long id;

		if (strcmp(argv[i], "all") == 0)
			continue;
		if (sscanf(argv[i], "nmem%lu", &id) != 1) {
			fprintf(stderr, "unknown extra parameter \"%s\"\n",
					argv[i]);
			usage_with_options(u, nmem_options);
		}
	}

	if (json) {
		jdimms = json_object_new_array();
		if (!jdimms)
			return -ENOMEM;
	}

	if (!output)
		f_out = stdout;
	else {
		f_out = fopen(output, "w+");
		if (!f_out) {
			fprintf(stderr, "failed to open: %s: (%s)\n",
					output, strerror(errno));
			rc = -errno;
			goto out;
		}
	}

	rc = ndctl_new(&ctx);
	if (rc < 0)
		goto out;

	count = 0;
        ndctl_bus_foreach(ctx, bus) {
		if (!util_bus_filter(bus, nmem_bus))
			continue;

		ndctl_dimm_foreach(bus, dimm) {
			rc = do_read_dimm(f_out, dimm, argv, argc, verbose,
					jdimms);
			if (rc == 0)
				count++;
			else if (rc && !err)
				err = rc;
		}
	}
	rc = err;

	if (jdimms)

	fprintf(stderr, "read %d nmem%s\n", count, count > 1 ? "s" : "");

	ndctl_unref(ctx);

 out:
	if (jdimms) {
		util_display_json_array(f_out, jdimms, JSON_C_TO_STRING_PRETTY);
		json_object_put(jdimms);
	}

	if (f_out != stdout)
		fclose(f_out);

	/*
	 * 0 if all dimms zeroed, count if at least 1 dimm zeroed, < 0
	 * if all errors
	 */
	if (rc == 0)
		return 0;
	if (count)
		return count;
	return rc;
}
