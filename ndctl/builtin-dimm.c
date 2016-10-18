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
#include <glob.h>
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
#include <ccan/array_size/array_size.h>

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

struct action_context {
	struct json_object *jdimms;
	FILE *f_out;
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

static struct ndctl_cmd *read_labels(struct ndctl_dimm *dimm)
{
	struct ndctl_bus *bus = ndctl_dimm_get_bus(dimm);
	struct ndctl_cmd *cmd_size, *cmd_read;
	int rc;

	rc = ndctl_bus_wait_probe(bus);
	if (rc < 0)
		return NULL;

	cmd_size = ndctl_dimm_cmd_new_cfg_size(dimm);
	if (!cmd_size)
		return NULL;
	rc = ndctl_cmd_submit(cmd_size);
	if (rc || ndctl_cmd_get_firmware_status(cmd_size))
		goto out_size;

	cmd_read = ndctl_dimm_cmd_new_cfg_read(cmd_size);
	if (!cmd_read)
		goto out_size;
	rc = ndctl_cmd_submit(cmd_read);
	if (rc || ndctl_cmd_get_firmware_status(cmd_read))
		goto out_read;

	ndctl_cmd_unref(cmd_size);
	return cmd_read;

 out_read:
	ndctl_cmd_unref(cmd_read);
 out_size:
	ndctl_cmd_unref(cmd_size);
	return NULL;
}

static int action_read(struct ndctl_dimm *dimm, struct action_context *actx)
{
	struct ndctl_cmd *cmd_read;
	ssize_t size;
	int rc = 0;

	cmd_read = read_labels(dimm);
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
		rc = dump_bin(actx->f_out, cmd_read, size);

	ndctl_cmd_unref(cmd_read);

	return rc;
}

static struct parameters {
	const char *bus;
	const char *outfile;
	bool json;
	bool verbose;
} param;

#define BASE_OPTIONS() \
OPT_STRING('b', "bus", &param.bus, "bus-id", \
	"<nmem> must be on a bus with an id/provider of <bus-id>"), \
OPT_BOOLEAN('v',"verbose", &param.verbose, "turn on debug")

#define READ_OPTIONS() \
OPT_STRING('o', NULL, &param.outfile, "output-file", \
	"filename to write label area contents"), \
OPT_BOOLEAN('j', "json", &param.json, "parse label data into json")

static const struct option read_options[] = {
	BASE_OPTIONS(),
	READ_OPTIONS(),
	OPT_END(),
};

static const struct option base_options[] = {
	BASE_OPTIONS(),
	OPT_END(),
};

static int dimm_action(int argc, const char **argv, struct ndctl_ctx *ctx,
		int (*action)(struct ndctl_dimm *dimm, struct action_context *actx),
		const struct option *options, const char *usage)
{
	int rc = 0, count, err = 0, glob_cnt = 0;
	struct action_context actx = { NULL, NULL };
	const char * const u[] = {
		usage,
		NULL
	};
	char *all[] = { "all " };
	glob_t glob_buf;
	size_t i;

        argc = parse_options(argc, argv, options, u, 0);

	if (argc == 0)
		usage_with_options(u, options);
	for (i = 0; i < (size_t) argc; i++) {
		char *path;

		if (strcmp(argv[i], "all") == 0) {
			argv[0] = "all";
			argc = 1;
			glob_cnt = 0;
			break;
		}
		rc = asprintf(&path, "/sys/bus/nd/devices/%s", argv[i]);
		if (rc < 0) {
			fprintf(stderr, "failed to parse %s\n", argv[i]);
			usage_with_options(u, options);
		}

		rc = glob(path, glob_cnt++ ? GLOB_APPEND : 0, NULL, &glob_buf);
		switch (rc) {
		case GLOB_NOSPACE:
		case GLOB_ABORTED:
			fprintf(stderr, "failed to parse %s\n", argv[i]);
			usage_with_options(u, options);
			break;
		case GLOB_NOMATCH:
		case 0:
			break;
		}
		free(path);
	}

	if (!glob_cnt)
		glob_buf.gl_pathc = 0;
	count = 0;
	for (i = 0; i < glob_buf.gl_pathc; i++) {
		char *dimm_name	= strrchr(glob_buf.gl_pathv[i], '/');
		unsigned long id;

		if (!dimm_name++)
			continue;
		if (sscanf(dimm_name, "nmem%lu", &id) == 1)
			count++;
	}

	if (strcmp(argv[0], "all") == 0) {
		glob_buf.gl_pathc = ARRAY_SIZE(all);
		glob_buf.gl_pathv = all;
	} else if (!count) {
		fprintf(stderr, "Error: ' ");
		for (i = 0; i < (size_t) argc; i++)
			fprintf(stderr, "%s ", argv[i]);
		fprintf(stderr, "' does not specify any present devices\n");
		fprintf(stderr, "See 'ndctl list -D'\n");
		usage_with_options(u, options);
	}

	if (param.json) {
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

	if (param.verbose)
		ndctl_set_log_priority(ctx, LOG_DEBUG);

	rc = 0;
	count = 0;
	for (i = 0; i < glob_buf.gl_pathc; i++) {
		char *dimm_name = strrchr(glob_buf.gl_pathv[i], '/');
		struct ndctl_dimm *dimm;
		struct ndctl_bus *bus;
		unsigned long id;

		if (!dimm_name++)
			continue;
		if (sscanf(dimm_name, "nmem%lu", &id) != 1)
			continue;

		ndctl_bus_foreach(ctx, bus) {
			if (!util_bus_filter(bus, param.bus))
				continue;
			ndctl_dimm_foreach(bus, dimm) {
				if (!util_dimm_filter(dimm, dimm_name))
					continue;
				rc = action(dimm, &actx);
				if (rc == 0)
					count++;
				else if (rc && !err)
					err = rc;
			}
		}
	}
	rc = err;

	if (actx.jdimms) {
		util_display_json_array(actx.f_out, actx.jdimms,
				JSON_C_TO_STRING_PRETTY);
		json_object_put(actx.jdimms);
	}

	if (actx.f_out != stdout)
		fclose(actx.f_out);

 out:
	if (glob_cnt)
		globfree(&glob_buf);

	/*
	 * count if some actions succeeded, 0 if none were attempted,
	 * negative error code otherwise.
	 */
	if (rc < 0)
		return rc;
	return count;
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
