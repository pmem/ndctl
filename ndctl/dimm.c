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

struct nvdimm_data {
	struct ndctl_dimm *dimm;
	struct ndctl_cmd *cmd_read;
	unsigned long config_size;
	size_t nslabel_size;
	struct log_ctx ctx;
	void *data;
	int nsindex_size;
	int ns_current, ns_next;
};

static size_t sizeof_namespace_label(struct nvdimm_data *ndd)
{
	struct namespace_index nsindex;
	int v1 = 0, v2 = 0;
	ssize_t offset;

	if (ndd->nslabel_size)
		return ndd->nslabel_size;

	for (offset = 0; offset < NSINDEX_ALIGN * 2; offset += NSINDEX_ALIGN) {
		ssize_t len = (ssize_t) sizeof(nsindex);

		len = ndctl_cmd_cfg_read_get_data(ndd->cmd_read, &nsindex,
				len, offset);
		if (len < 0)
			break;

		/*
		 * Since we're doing a best effort parsing we don't
		 * fully validate the index block. Instead just assume
		 * v1.1 unless there's 2 index blocks that say v1.2.
		 */
		if (le16_to_cpu(nsindex.major) == 1) {
			if (le16_to_cpu(nsindex.minor) == 1)
				v1++;
			else if (le16_to_cpu(nsindex.minor) == 2)
				v2++;
		}
	}

	if (v2 > v1)
		ndd->nslabel_size = 256;
	else
		ndd->nslabel_size = 128;
	return ndd->nslabel_size;
}

#define namespace_label_has(ndd, field) \
	(offsetof(struct namespace_label, field) \
	 < sizeof_namespace_label(ndd))

static const char NSINDEX_SIGNATURE[] = "NAMESPACE_INDEX\0";

struct action_context {
	struct json_object *jdimms;
	int labelversion;
	FILE *f_out;
	FILE *f_in;
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
	struct nvdimm_data __ndd = {
		.nslabel_size = 0,
		.cmd_read = cmd_read
	}, *ndd = &__ndd;
	struct json_object *jarray = json_object_new_array();
	struct json_object *jlabel = NULL;
	struct namespace_label nslabel;
	unsigned int slot = -1;
	ssize_t offset;

	if (!jarray)
		return NULL;

	for (offset = NSINDEX_ALIGN * 2; offset < size;
			offset += sizeof_namespace_label(ndd)) {
		ssize_t len = min_t(ssize_t, sizeof_namespace_label(ndd),
				size - offset);
		struct json_object *jobj;
		char uuid[40];

		slot++;
		jlabel = json_object_new_object();
		if (!jlabel)
			break;

		if (len < (ssize_t) sizeof_namespace_label(ndd))
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

		jobj = json_object_new_int64(le64_to_cpu(nslabel.lbasize));
		if (!jobj)
			break;
		json_object_object_add(jlabel, "lbasize", jobj);

		jobj = json_object_new_int64(le64_to_cpu(nslabel.dpa));
		if (!jobj)
			break;
		json_object_object_add(jlabel, "dpa", jobj);

		jobj = json_object_new_int64(le64_to_cpu(nslabel.rawsize));
		if (!jobj)
			break;
		json_object_object_add(jlabel, "rawsize", jobj);

		json_object_array_add(jarray, jlabel);

		if (sizeof_namespace_label(ndd) < 256)
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

static int action_write(struct ndctl_dimm *dimm, struct action_context *actx)
{
	struct ndctl_cmd *cmd_read, *cmd_write;
	ssize_t size;
	int rc = 0;

	if (ndctl_dimm_is_active(dimm)) {
		fprintf(stderr, "dimm is active, abort label write\n");
		return -EBUSY;
	}

	cmd_read = read_labels(dimm);
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
		rc = rw_bin(actx->f_out, cmd_read, size, 0);

	ndctl_cmd_unref(cmd_read);

	return rc;
}

/*
 * Note, best_seq(), inc_seq(), sizeof_namespace_index()
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
 * 8/ s,nvdimm_drvdata,nvdimm_data,gc
 */

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

size_t sizeof_namespace_index(struct nvdimm_data *ndd)
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
	index_span = ndd->config_size / (sizeof_namespace_label(ndd) + 1);
	index_span /= NSINDEX_ALIGN * 2;
	ndd->nsindex_size = index_span * NSINDEX_ALIGN;

	return ndd->nsindex_size;
}

int nvdimm_num_label_slots(struct nvdimm_data *ndd)
{
	return ndd->config_size / (sizeof_namespace_label(ndd) + 1);
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

static int __label_validate(struct nvdimm_data *ndd)
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
		unsigned int version, labelsize;

		memcpy(sig, nsindex[i]->sig, NSINDEX_SIG_LEN);
		if (memcmp(sig, NSINDEX_SIGNATURE, NSINDEX_SIG_LEN) != 0) {
			dbg(ndd, "nsindex%d signature invalid\n", i);
			continue;
		}

		/* label sizes larger than 128 arrived with v1.2 */
		version = le16_to_cpu(nsindex[i]->major) * 100
			+ le16_to_cpu(nsindex[i]->minor);
		if (version >= 102)
			labelsize = 1 << (7 + nsindex[i]->labelsize);
		else
			labelsize = 128;

		if (labelsize != sizeof_namespace_label(ndd)) {
			dbg(ndd, "nsindex%d labelsize %d invalid\n",
					i, nsindex[i]->labelsize);
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
		if (nslot * sizeof_namespace_label(ndd)
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

int label_validate(struct nvdimm_data *ndd)
{
	/*
	 * In order to probe for and validate namespace index blocks we
	 * need to know the size of the labels, and we can't trust the
	 * size of the labels until we validate the index blocks.
	 * Resolve this dependency loop by probing for known label
	 * sizes, but default to v1.2 256-byte namespace labels if
	 * discovery fails.
	 */
	int label_size[] = { 128, 256 };
	int i, rc;

	for (i = 0; (size_t) i < ARRAY_SIZE(label_size); i++) {
		ndd->nslabel_size = label_size[i];
		rc = __label_validate(ndd);
		if (rc >= 0)
			return rc;
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

static int label_write_index(struct nvdimm_data *ndd, int index, u32 seq)
{
	struct namespace_index *nsindex;
	unsigned long offset;
	u64 checksum;
	u32 nslot;

	nsindex = to_namespace_index(ndd, index);
	nslot = nvdimm_num_label_slots(ndd);

	memcpy(nsindex->sig, NSINDEX_SIGNATURE, NSINDEX_SIG_LEN);
	memset(nsindex->flags, 0, 3);
	nsindex->labelsize = sizeof_namespace_label(ndd) >> 8;
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
	if (sizeof_namespace_label(ndd) < 256)
		nsindex->minor = cpu_to_le16(1);
	else
		nsindex->minor = cpu_to_le16(2);
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
	const char *infile;
	const char *labelversion;
	bool force;
	bool json;
	bool verbose;
} param = {
	.labelversion = "1.1",
};

static int __action_init(struct ndctl_dimm *dimm, int version, int chk_only)
{
	struct nvdimm_data __ndd = { 0 }, *ndd = &__ndd;
	struct ndctl_cmd *cmd_read;
	int rc = 0, i;
	ssize_t size;

	cmd_read = read_labels(dimm);
	if (!cmd_read)
		return -ENXIO;

	size = ndctl_cmd_cfg_read_get_size(cmd_read);
	if (size < 0)
		return size;

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
	if (!chk_only && ndctl_dimm_is_active(dimm)) {
		err(ndd, "regions active, abort label write\n");
		rc = -EBUSY;
		goto out;
	}

	rc = label_validate(ndd);
	if (chk_only) {
		rc = rc >= 0 ? 0 : -ENXIO;
		goto out;
	}

	if (rc >= 0 && !param.force) {
		err(ndd, "error: labels already initialized\n");
		rc = -EBUSY;
		goto out;
	}

	/*
	 * We may have initialized ndd to whatever labelsize is
	 * currently on the dimm during label_validate(), so we reset it
	 * to the desired version here.
	 */
	if (version > 1)
		ndd->nslabel_size = 256;
	else
		ndd->nslabel_size = 128;

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
OPT_BOOLEAN('j', "json", &param.json, "parse label data into json")

#define WRITE_OPTIONS() \
OPT_STRING('i', "input", &param.infile, "input-file", \
	"filename to read label area data")

#define INIT_OPTIONS() \
OPT_BOOLEAN('f', "force", &param.force, \
		"force initialization even if existing index-block present"), \
OPT_STRING('V', "label-version", &param.labelversion, "version-number", \
	"namespace label specification version (default: 1.1)")

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

static const struct option base_options[] = {
	BASE_OPTIONS(),
	OPT_END(),
};

static const struct option init_options[] = {
	BASE_OPTIONS(),
	INIT_OPTIONS(),
	OPT_END(),
};

static int dimm_action(int argc, const char **argv, void *ctx,
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

	if (!param.infile)
		actx.f_in = stdin;
	else {
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
		actx.labelversion = 1;
	else if (strcmp(param.labelversion, "v1.1") == 0)
		actx.labelversion = 1;
	else if (strcmp(param.labelversion, "1.2") == 0)
		actx.labelversion = 2;
	else if (strcmp(param.labelversion, "v1.2") == 0)
		actx.labelversion = 2;
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

	if (actx.jdimms) {
		util_display_json_array(actx.f_out, actx.jdimms,
				JSON_C_TO_STRING_PRETTY);
		json_object_put(actx.jdimms);
	}

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

int cmd_write_labels(int argc, const char **argv, void *ctx)
{
	int count = dimm_action(argc, argv, ctx, action_write, write_options,
			"ndctl write-labels <nmem> [-i <filename>]");

	fprintf(stderr, "wrote %d nmem%s\n", count >= 0 ? count : 0,
			count > 1 ? "s" : "");
	return count >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_read_labels(int argc, const char **argv, void *ctx)
{
	int count = dimm_action(argc, argv, ctx, action_read, read_options,
			"ndctl read-labels <nmem0> [<nmem1>..<nmemN>] [-o <filename>]");

	fprintf(stderr, "read %d nmem%s\n", count >= 0 ? count : 0,
			count > 1 ? "s" : "");
	return count >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_zero_labels(int argc, const char **argv, void *ctx)
{
	int count = dimm_action(argc, argv, ctx, action_zero, base_options,
			"ndctl zero-labels <nmem0> [<nmem1>..<nmemN>] [<options>]");

	fprintf(stderr, "zeroed %d nmem%s\n", count >= 0 ? count : 0,
			count > 1 ? "s" : "");
	return count >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_init_labels(int argc, const char **argv, void *ctx)
{
	int count = dimm_action(argc, argv, ctx, action_init, init_options,
			"ndctl init-labels <nmem0> [<nmem1>..<nmemN>] [<options>]");

	fprintf(stderr, "initialized %d nmem%s\n", count >= 0 ? count : 0,
			count > 1 ? "s" : "");
	return count >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_check_labels(int argc, const char **argv, void *ctx)
{
	int count = dimm_action(argc, argv, ctx, action_check, base_options,
			"ndctl check-labels <nmem0> [<nmem1>..<nmemN>] [<options>]");

	fprintf(stderr, "successfully verified %d nmem%s\n",
			count >= 0 ? count : 0, count > 1 ? "s" : "");
	return count >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_disable_dimm(int argc, const char **argv, void *ctx)
{
	int count = dimm_action(argc, argv, ctx, action_disable, base_options,
			"ndctl disable-dimm <nmem0> [<nmem1>..<nmemN>] [<options>]");

	fprintf(stderr, "disabled %d nmem%s\n", count >= 0 ? count : 0,
			count > 1 ? "s" : "");
	return count >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_enable_dimm(int argc, const char **argv, void *ctx)
{
	int count = dimm_action(argc, argv, ctx, action_enable, base_options,
			"ndctl enable-dimm <nmem0> [<nmem1>..<nmemN>] [<options>]");

	fprintf(stderr, "enabled %d nmem%s\n", count >= 0 ? count : 0,
			count > 1 ? "s" : "");
	return count >= 0 ? 0 : EXIT_FAILURE;
}
