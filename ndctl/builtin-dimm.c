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
#include <util/log.h>
#include <uuid/uuid.h>
#include <util/json.h>
#include <util/filter.h>
#include <json-c/json.h>
#include <ndctl/libndctl.h>
#include <util/parse-options.h>
#include <ccan/minmax/minmax.h>
#include <ccan/short_types/short_types.h>
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

static const char NSINDEX_SIGNATURE[] = "NAMESPACE_INDEX\0";

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

struct nvdimm_data {
	struct ndctl_dimm *dimm;
	struct ndctl_cmd *cmd_read;
	unsigned long config_size;
	struct log_ctx ctx;
	void *data;
	int nsindex_size;
	int ns_current, ns_next;
};

/*
 * Note, best_seq(), inc_seq(), fletcher64(), sizeof_namespace_index()
 * nvdimm_num_label_slots(), label_validate(), and label_write_index()
 * are copied from drivers/nvdimm/label.c in the Linux kernel with the
 * following modifications:
 * 1/ s,nd_,,gc
 * 2/ s,ndd->nsarea.config_size,ndd->config_size,gc
 * 3/ s,dev_dbg(dev,dbg(ndd,gc
 * 4/ s,__le,le,gc
 * 5/ s,__cpu_to,cpu_to,gc
 * 6/ remove flags argument to label_write_index
 * 7/ dropped clear_bit_le() usage in label_write_index
 */

static u64 fletcher64(void *addr, size_t len, bool le)
{
	u32 *buf = addr;
	u32 lo32 = 0;
	u64 hi32 = 0;
	size_t i;

	for (i = 0; i < len / sizeof(u32); i++) {
		lo32 += le ? le32_to_cpu((le32) buf[i]) : buf[i];
		hi32 += lo32;
	}

	return hi32 << 32 | lo32;
}

static unsigned inc_seq(unsigned seq)
{
	static const unsigned next[] = { 0, 2, 3, 1 };

	return next[seq & 3];
}

static u32 best_seq(u32 a, u32 b)
{
	a &= NSINDEX_SEQ_MASK;
	b &= NSINDEX_SEQ_MASK;

	if (a == 0 || a == b)
		return b;
	else if (b == 0)
		return a;
	else if (inc_seq(a) == b)
		return b;
	else
		return a;
}

static size_t sizeof_namespace_index(struct nvdimm_data *ndd)
{
	u32 index_span;

	if (ndd->nsindex_size)
		return ndd->nsindex_size;

	/*
	 * The minimum index space is 512 bytes, with that amount of
	 * index we can describe ~1400 labels which is less than a byte
	 * of overhead per label.  Round up to a byte of overhead per
	 * label and determine the size of the index region.  Yes, this
	 * starts to waste space at larger config_sizes, but it's
	 * unlikely we'll ever see anything but 128K.
	 */
	index_span = ndd->config_size / 129;
	index_span /= NSINDEX_ALIGN * 2;
	ndd->nsindex_size = index_span * NSINDEX_ALIGN;

	return ndd->nsindex_size;
}

static int nvdimm_num_label_slots(struct nvdimm_data *ndd)
{
	return ndd->config_size / 129;
}

static struct namespace_index *to_namespace_index(struct nvdimm_data *ndd,
		int i)
{
	char *index;

	if (i < 0)
		return NULL;

	index = (char *) ndd->data + sizeof_namespace_index(ndd) * i;
	return (struct namespace_index *) index;
}

static int label_validate(struct nvdimm_data *ndd)
{
	/*
	 * On media label format consists of two index blocks followed
	 * by an array of labels.  None of these structures are ever
	 * updated in place.  A sequence number tracks the current
	 * active index and the next one to write, while labels are
	 * written to free slots.
	 *
	 *     +------------+
	 *     |            |
	 *     |  nsindex0  |
	 *     |            |
	 *     +------------+
	 *     |            |
	 *     |  nsindex1  |
	 *     |            |
	 *     +------------+
	 *     |   label0   |
	 *     +------------+
	 *     |   label1   |
	 *     +------------+
	 *     |            |
	 *      ....nslot...
	 *     |            |
	 *     +------------+
	 *     |   labelN   |
	 *     +------------+
	 */
	struct namespace_index *nsindex[] = {
		to_namespace_index(ndd, 0),
		to_namespace_index(ndd, 1),
	};
	const int num_index = ARRAY_SIZE(nsindex);
	bool valid[2] = { 0 };
	int i, num_valid = 0;
	u32 seq;

	for (i = 0; i < num_index; i++) {
		u32 nslot;
		u8 sig[NSINDEX_SIG_LEN];
		u64 sum_save, sum, size;

		memcpy(sig, nsindex[i]->sig, NSINDEX_SIG_LEN);
		if (memcmp(sig, NSINDEX_SIGNATURE, NSINDEX_SIG_LEN) != 0) {
			dbg(ndd, "nsindex%d signature invalid\n", i);
			continue;
		}
		sum_save = le64_to_cpu(nsindex[i]->checksum);
		nsindex[i]->checksum = cpu_to_le64(0);
		sum = fletcher64(nsindex[i], sizeof_namespace_index(ndd), 1);
		nsindex[i]->checksum = cpu_to_le64(sum_save);
		if (sum != sum_save) {
			dbg(ndd, "nsindex%d checksum invalid\n", i);
			continue;
		}

		seq = le32_to_cpu(nsindex[i]->seq);
		if ((seq & NSINDEX_SEQ_MASK) == 0) {
			dbg(ndd, "nsindex%d sequence: %#x invalid\n", i, seq);
			continue;
		}

		/* sanity check the index against expected values */
		if (le64_to_cpu(nsindex[i]->myoff)
				!= i * sizeof_namespace_index(ndd)) {
			dbg(ndd, "nsindex%d myoff: %#llx invalid\n",
					i, (unsigned long long)
					le64_to_cpu(nsindex[i]->myoff));
			continue;
		}
		if (le64_to_cpu(nsindex[i]->otheroff)
				!= (!i) * sizeof_namespace_index(ndd)) {
			dbg(ndd, "nsindex%d otheroff: %#llx invalid\n",
					i, (unsigned long long)
					le64_to_cpu(nsindex[i]->otheroff));
			continue;
		}

		size = le64_to_cpu(nsindex[i]->mysize);
		if (size > sizeof_namespace_index(ndd)
				|| size < sizeof(struct namespace_index)) {
			dbg(ndd, "nsindex%d mysize: %#zx invalid\n", i, size);
			continue;
		}

		nslot = le32_to_cpu(nsindex[i]->nslot);
		if (nslot * sizeof(struct namespace_label)
				+ 2 * sizeof_namespace_index(ndd)
				> ndd->config_size) {
			dbg(ndd, "nsindex%d nslot: %u invalid, config_size: %#zx\n",
					i, nslot, ndd->config_size);
			continue;
		}
		valid[i] = true;
		num_valid++;
	}

	switch (num_valid) {
	case 0:
		break;
	case 1:
		for (i = 0; i < num_index; i++)
			if (valid[i])
				return i;
		/* can't have num_valid > 0 but valid[] = { false, false } */
		err(ndd, "unexpected index-block parse error\n");
		break;
	default:
		/* pick the best index... */
		seq = best_seq(le32_to_cpu(nsindex[0]->seq),
				le32_to_cpu(nsindex[1]->seq));
		if (seq == (le32_to_cpu(nsindex[1]->seq) & NSINDEX_SEQ_MASK))
			return 1;
		else
			return 0;
		break;
	}

	return -1;
}

static int nvdimm_set_config_data(struct nvdimm_data *ndd, size_t offset,
		void *buf, size_t len)
{
	struct ndctl_cmd *cmd_write;
	int rc;

	cmd_write = ndctl_dimm_cmd_new_cfg_write(ndd->cmd_read);
	if (!cmd_write)
		return -ENXIO;

	rc = ndctl_cmd_cfg_write_set_data(cmd_write, buf, len, offset);
	if (rc < 0)
		goto out;

	rc = ndctl_cmd_submit(cmd_write);
	if (rc || ndctl_cmd_get_firmware_status(cmd_write))
		rc = -ENXIO;
 out:
	ndctl_cmd_unref(cmd_write);
	return rc;
}

static int label_next_nsindex(int index)
{
	if (index < 0)
		return -1;
	return (index + 1) % 2;
}

static struct namespace_label *label_base(struct nvdimm_data *ndd)
{
	char *base = (char *) to_namespace_index(ndd, 0);

	base += 2 * sizeof_namespace_index(ndd);
	return (struct namespace_label *) base;
}

#define ALIGN(x, a) ((((unsigned long long) x) + (a - 1)) & ~(a - 1))
#define BITS_PER_LONG (sizeof(unsigned long) * 8)
static int label_write_index(struct nvdimm_data *ndd, int index, u32 seq)
{
	struct namespace_index *nsindex;
	unsigned long offset;
	u64 checksum;
	u32 nslot;

	nsindex = to_namespace_index(ndd, index);
	nslot = nvdimm_num_label_slots(ndd);

	memcpy(nsindex->sig, NSINDEX_SIGNATURE, NSINDEX_SIG_LEN);
	nsindex->flags = cpu_to_le32(0);
	nsindex->seq = cpu_to_le32(seq);
	offset = (unsigned long) nsindex
		- (unsigned long) to_namespace_index(ndd, 0);
	nsindex->myoff = cpu_to_le64(offset);
	nsindex->mysize = cpu_to_le64(sizeof_namespace_index(ndd));
	offset = (unsigned long) to_namespace_index(ndd,
			label_next_nsindex(index))
		- (unsigned long) to_namespace_index(ndd, 0);
	nsindex->otheroff = cpu_to_le64(offset);
	offset = (unsigned long) label_base(ndd)
		- (unsigned long) to_namespace_index(ndd, 0);
	nsindex->labeloff = cpu_to_le64(offset);
	nsindex->nslot = cpu_to_le32(nslot);
	nsindex->major = cpu_to_le16(1);
	nsindex->minor = cpu_to_le16(1);
	nsindex->checksum = cpu_to_le64(0);
	/* init label bitmap */
	memset(nsindex->free, 0xff, ALIGN(nslot, BITS_PER_LONG) / 8);
	checksum = fletcher64(nsindex, sizeof_namespace_index(ndd), 1);
	nsindex->checksum = cpu_to_le64(checksum);
	return nvdimm_set_config_data(ndd, le64_to_cpu(nsindex->myoff),
			nsindex, sizeof_namespace_index(ndd));
}

static struct parameters {
	const char *bus;
	const char *outfile;
	bool force;
	bool json;
	bool verbose;
} param;

static int action_init(struct ndctl_dimm *dimm, struct action_context *actx)
{
	struct nvdimm_data __ndd, *ndd = &__ndd;
	struct ndctl_cmd *cmd_read;
	int rc = 0, i;
	ssize_t size;

	cmd_read = read_labels(dimm);
	if (!cmd_read)
		return -ENXIO;

	size = ndctl_cmd_cfg_read_get_size(cmd_read);
	ndd->data = malloc(size);
	if (!ndd->data)
		return -ENOMEM;
	rc = ndctl_cmd_cfg_read_get_data(cmd_read, ndd->data, size, 0);
	if (rc < 0)
		goto out;

	ndd->dimm = dimm;
	ndd->cmd_read = cmd_read;
	ndd->config_size = size;
	ndd->nsindex_size = 0;
	ndd->ns_current = -1;
	ndd->ns_next = -1;
	log_init(&ndd->ctx, ndctl_dimm_get_devname(dimm), "NDCTL_INIT_LABELS");
	if (param.verbose)
		ndd->ctx.log_priority = LOG_DEBUG;

	/*
	 * If the region goes active after this point, i.e. we're racing
	 * another administrative action, the kernel will fail writes to
	 * the label area.
	 */
	if (ndctl_dimm_is_active(dimm)) {
		err(ndd, "regions active, abort label write\n");
		rc = -EBUSY;
		goto out;
	}

	if (label_validate(ndd) >= 0 && !param.force) {
		err(ndd, "error: labels already initialized\n");
		rc = -EBUSY;
		goto out;
	}

	for (i = 0; i < 2; i++) {
		rc = label_write_index(ndd, i, i*2);
		if (rc)
			goto out;
	}

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
	free(ndd->data);
	return rc;
}

#define BASE_OPTIONS() \
OPT_STRING('b', "bus", &param.bus, "bus-id", \
	"<nmem> must be on a bus with an id/provider of <bus-id>"), \
OPT_BOOLEAN('v',"verbose", &param.verbose, "turn on debug")

#define READ_OPTIONS() \
OPT_STRING('o', NULL, &param.outfile, "output-file", \
	"filename to write label area contents"), \
OPT_BOOLEAN('j', "json", &param.json, "parse label data into json")

#define INIT_OPTIONS() \
OPT_BOOLEAN('f', "force", &param.force, \
		"force initialization even if existing index-block present")

static const struct option read_options[] = {
	BASE_OPTIONS(),
	READ_OPTIONS(),
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

int cmd_init_labels(int argc, const char **argv, struct ndctl_ctx *ctx)
{
	int count = dimm_action(argc, argv, ctx, action_init, init_options,
			"ndctl init-labels <nmem0> [<nmem1>..<nmemN>] [<options>]");

	fprintf(stderr, "initialized %d nmem%s\n", count >= 0 ? count : 0,
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
