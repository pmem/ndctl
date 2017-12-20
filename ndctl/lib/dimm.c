/*
 * Copyright (c) 2014-2017, Intel Corporation.
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
#include <ndctl/namespace.h>
#include <ndctl/libndctl.h>
#include <util/fletcher.h>
#include <util/bitmap.h>
#include <util/sysfs.h>
#include <stdlib.h>
#include "private.h"

static const char NSINDEX_SIGNATURE[] = "NAMESPACE_INDEX\0";

/*
 * Note, best_seq(), inc_seq(), sizeof_namespace_index()
 * nvdimm_num_label_slots(), label_validate(), and label_write_index()
 * are copied from drivers/nvdimm/label.c in the Linux kernel with the
 * following modifications:
 * 1/ s,nd_,,gc
 * 2/ s,ndd->nsarea.config_size,ndd->config_size,gc
 * 3/ s,dev_dbg(dev,dbg(ctx,gc
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

static struct ndctl_dimm *to_dimm(struct nvdimm_data *ndd)
{
	return container_of(ndd, struct ndctl_dimm, ndd);
}

static unsigned int sizeof_namespace_label(struct nvdimm_data *ndd)
{
	return ndctl_dimm_sizeof_namespace_label(to_dimm(ndd));
}

static int nvdimm_num_label_slots(struct nvdimm_data *ndd)
{
	return ndd->config_size / (sizeof_namespace_label(ndd) + 1);
}

static unsigned int sizeof_namespace_index(struct nvdimm_data *ndd)
{
	u32 nslot, space, size;
	struct ndctl_dimm *dimm = to_dimm(ndd);
	struct ndctl_ctx *ctx = ndctl_dimm_get_ctx(dimm);

	/*
	 * The minimum index space is 512 bytes, with that amount of
	 * index we can describe ~1400 labels which is less than a byte
	 * of overhead per label.  Round up to a byte of overhead per
	 * label and determine the size of the index region.  Yes, this
	 * starts to waste space at larger config_sizes, but it's
	 * unlikely we'll ever see anything but 128K.
	 */
	nslot = nvdimm_num_label_slots(ndd);
	space = ndd->config_size - nslot * sizeof_namespace_label(ndd);
	size = ALIGN(sizeof(struct namespace_index) + DIV_ROUND_UP(nslot, 8),
			NSINDEX_ALIGN) * 2;
	if (size <= space)
		return size / 2;

	err(ctx, "%s: label area (%ld) too small to host (%d byte) labels\n",
			ndctl_dimm_get_devname(dimm), ndd->config_size,
			sizeof_namespace_label(ndd));
	return 0;
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
	struct ndctl_ctx *ctx = ndctl_dimm_get_ctx(to_dimm(ndd));

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
			dbg(ctx, "nsindex%d signature invalid\n", i);
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
			dbg(ctx, "nsindex%d labelsize %d invalid\n",
					i, nsindex[i]->labelsize);
			continue;
		}

		sum_save = le64_to_cpu(nsindex[i]->checksum);
		nsindex[i]->checksum = cpu_to_le64(0);
		sum = fletcher64(nsindex[i], sizeof_namespace_index(ndd), 1);
		nsindex[i]->checksum = cpu_to_le64(sum_save);
		if (sum != sum_save) {
			dbg(ctx, "nsindex%d checksum invalid\n", i);
			continue;
		}

		seq = le32_to_cpu(nsindex[i]->seq);
		if ((seq & NSINDEX_SEQ_MASK) == 0) {
			dbg(ctx, "nsindex%d sequence: %#x invalid\n", i, seq);
			continue;
		}

		/* sanity check the index against expected values */
		if (le64_to_cpu(nsindex[i]->myoff)
				!= i * sizeof_namespace_index(ndd)) {
			dbg(ctx, "nsindex%d myoff: %#llx invalid\n",
					i, (unsigned long long)
					le64_to_cpu(nsindex[i]->myoff));
			continue;
		}
		if (le64_to_cpu(nsindex[i]->otheroff)
				!= (!i) * sizeof_namespace_index(ndd)) {
			dbg(ctx, "nsindex%d otheroff: %#llx invalid\n",
					i, (unsigned long long)
					le64_to_cpu(nsindex[i]->otheroff));
			continue;
		}

		size = le64_to_cpu(nsindex[i]->mysize);
		if (size > sizeof_namespace_index(ndd)
				|| size < sizeof(struct namespace_index)) {
			dbg(ctx, "nsindex%d mysize: %#zx invalid\n", i, size);
			continue;
		}

		nslot = le32_to_cpu(nsindex[i]->nslot);
		if (nslot * sizeof_namespace_label(ndd)
				+ 2 * sizeof_namespace_index(ndd)
				> ndd->config_size) {
			dbg(ctx, "nsindex%d nslot: %u invalid, config_size: %#zx\n",
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
		err(ctx, "unexpected index-block parse error\n");
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

	return -EINVAL;
}

/*
 * If the dimm labels have not been previously validated this routine
 * will make up a default size. Otherwise, it will pick the size based
 * on what version is specified in the index block.
 */
NDCTL_EXPORT unsigned int ndctl_dimm_sizeof_namespace_label(struct ndctl_dimm *dimm)
{
	struct nvdimm_data *ndd = &dimm->ndd;
	struct namespace_index nsindex;
	ssize_t offset, size;
	int v1 = 0, v2 = 0;

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
		size = 256;
	else
		size = 128;
	ndd->nslabel_size = size;
	return size;
}

static int label_validate(struct nvdimm_data *ndd)
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
			return nvdimm_num_label_slots(ndd);
	}

	return -EINVAL;
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

static void init_ndd(struct nvdimm_data *ndd, struct ndctl_cmd *cmd_read)
{
	ndctl_cmd_unref(ndd->cmd_read);
	memset(ndd, 0, sizeof(*ndd));
	ndd->cmd_read = cmd_read;
	ndctl_cmd_ref(cmd_read);
	ndd->data = cmd_read->iter.total_buf;
	ndd->config_size = cmd_read->iter.total_xfer;
	ndd->ns_current = -1;
	ndd->ns_next = -1;
}

static int write_label_index(struct ndctl_dimm *dimm,
		enum ndctl_namespace_version ver, unsigned index, unsigned seq)
{
	struct nvdimm_data *ndd = &dimm->ndd;
	struct namespace_index *nsindex;
	unsigned long offset;
	u64 checksum;
	u32 nslot;

	/*
	 * We may have initialized ndd to whatever labelsize is
	 * currently on the dimm during label_validate(), so we reset it
	 * to the desired version here.
	 */
	switch (ver) {
	case NDCTL_NS_VERSION_1_1:
		ndd->nslabel_size = 128;
		break;
	case NDCTL_NS_VERSION_1_2:
		ndd->nslabel_size = 256;
		break;
	default:
		return -EINVAL;
	}

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

NDCTL_EXPORT int ndctl_dimm_init_labels(struct ndctl_dimm *dimm,
		enum ndctl_namespace_version v)
{
	struct ndctl_bus *bus = ndctl_dimm_get_bus(dimm);
	struct ndctl_ctx *ctx = ndctl_dimm_get_ctx(dimm);
	struct nvdimm_data *ndd = &dimm->ndd;
	struct ndctl_region *region;
	int i;

	if (!ndd->cmd_read) {
		err(ctx, "%s: needs to be initialized by ndctl_dimm_read_labels\n",
				ndctl_dimm_get_devname(dimm));
		return -EINVAL;
	}

	ndctl_region_foreach(bus, region) {
		struct ndctl_dimm *match;

		ndctl_dimm_foreach_in_region(region, match)
			if (match == dimm) {
				region_flag_refresh(region);
				break;
			}
	}

	for (i = 0; i < 2; i++) {
		int rc;

		rc = write_label_index(dimm, v, i, 3 - i);
		if (rc < 0)
			return rc;
	}

	return nvdimm_num_label_slots(ndd);
}

NDCTL_EXPORT int ndctl_dimm_validate_labels(struct ndctl_dimm *dimm)
{
	struct ndctl_ctx *ctx = ndctl_dimm_get_ctx(dimm);
	struct nvdimm_data *ndd = &dimm->ndd;

	if (!ndd->cmd_read) {
		err(ctx, "%s: needs to be initialized by ndctl_dimm_read_labels\n",
				ndctl_dimm_get_devname(dimm));
		return -EINVAL;
	}

	return label_validate(&dimm->ndd);
}

NDCTL_EXPORT struct ndctl_cmd *ndctl_dimm_read_labels(struct ndctl_dimm *dimm)
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

	init_ndd(&dimm->ndd, cmd_read);

	return cmd_read;

 out_read:
        ndctl_cmd_unref(cmd_read);
 out_size:
        ndctl_cmd_unref(cmd_size);
        return NULL;
}

NDCTL_EXPORT int ndctl_dimm_zero_labels(struct ndctl_dimm *dimm)
{
	struct ndctl_ctx *ctx = ndctl_dimm_get_ctx(dimm);
	struct ndctl_cmd *cmd_read, *cmd_write;
	int rc;

	cmd_read = ndctl_dimm_read_labels(dimm);
	if (!cmd_read)
		return -ENXIO;

	if (ndctl_dimm_is_active(dimm)) {
		dbg(ctx, "%s: regions active, abort label write\n",
			ndctl_dimm_get_devname(dimm));
		rc = -EBUSY;
		goto out_read;
	}

	cmd_write = ndctl_dimm_cmd_new_cfg_write(cmd_read);
	if (!cmd_write) {
		rc = -ENOTTY;
		goto out_read;
	}
	if (ndctl_cmd_cfg_write_zero_data(cmd_write) < 0) {
		rc = -ENXIO;
		goto out_write;
	}
	rc = ndctl_cmd_submit(cmd_write);
	if (rc || ndctl_cmd_get_firmware_status(cmd_write))
		goto out_write;

	/*
	 * If the dimm is already disabled the kernel is not holding a cached
	 * copy of the label space.
	 */
	if (!ndctl_dimm_is_enabled(dimm))
		goto out_write;

	rc = ndctl_dimm_disable(dimm);
	if (rc)
		goto out_write;
	rc = ndctl_dimm_enable(dimm);

 out_write:
	ndctl_cmd_unref(cmd_write);
 out_read:
	ndctl_cmd_unref(cmd_read);

	return rc;
}

NDCTL_EXPORT unsigned long ndctl_dimm_get_available_labels(
		struct ndctl_dimm *dimm)
{
	struct ndctl_ctx *ctx = ndctl_dimm_get_ctx(dimm);
	char *path = dimm->dimm_buf;
	int len = dimm->buf_len;
	char buf[20];

	if (snprintf(path, len, "%s/available_slots", dimm->dimm_path) >= len) {
		err(ctx, "%s: buffer too small!\n",
				ndctl_dimm_get_devname(dimm));
		return ULONG_MAX;
	}

	if (sysfs_read_attr(ctx, path, buf) < 0)
		return ULONG_MAX;

	return strtoul(buf, NULL, 0);
}
