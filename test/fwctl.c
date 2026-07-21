// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2024-2025 Intel Corporation. All rights reserved.
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <endian.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <syslog.h>
#include <string.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <sys/ioctl.h>
#include <cxl/libcxl.h>
#include <linux/uuid.h>
#include <uuid/uuid.h>
#include <util/bitmap.h>
#include <cxl/fwctl/features.h>
#include <cxl/fwctl/fwctl.h>
#include <cxl/fwctl/cxl.h>

static const char provider[] = "cxl_test";

/* Running kernel version parsed once in main(). */
static unsigned int kver_major;
static unsigned int kver_minor;

/*
 * kver_ge - Whether the running kernel at least major.minor
 *
 * Test cases for fixes tied to a specific kver gate here so that they quietly
 * skip rather than fail on kernels that predate the fix.
 */
static bool kver_ge(unsigned int major, unsigned int minor)
{
	if (kver_major != major)
		return kver_major > major;
	return kver_minor >= minor;
}

static void parse_kver(void)
{
	struct utsname uts;

	if (uname(&uts) == 0 &&
	    sscanf(uts.release, "%u.%u", &kver_major, &kver_minor) == 2)
		return;

	kver_major = 0;
	kver_minor = 0;
}

UUID_DEFINE(test_uuid,
	    0xff, 0xff, 0xff, 0xff,
	    0xff, 0xff,
	    0xff, 0xff,
	    0xff, 0xff,
	    0xff, 0xff, 0xff, 0xff, 0xff, 0xff
);

#define CXL_MBOX_OPCODE_GET_SUPPORTED_FEATURES	0x0500
#define CXL_MBOX_OPCODE_GET_FEATURE		0x0501
#define CXL_MBOX_OPCODE_SET_FEATURE		0x0502

#define GET_FEAT_SIZE	4
#define SET_FEAT_SIZE	4
#define EFFECTS_MASK	(BIT(0) | BIT(9))

#define MAX_TEST_FEATURES	1
#define DEFAULT_TEST_DATA	0xdeadbeef
#define DEFAULT_TEST_DATA2	0xabcdabcd

struct test_feature {
	uuid_t uuid;
	size_t get_size;
	size_t set_size;
};

static int send_command(int fd, struct fwctl_rpc *rpc, struct fwctl_rpc_cxl_out *out)
{
	if (ioctl(fd, FWCTL_RPC, rpc) == -1) {
		fprintf(stderr, "RPC ioctl error: %s\n", strerror(errno));
		return -errno;
	}

	if (out->retval) {
		fprintf(stderr, "operation returned failure: %d\n", out->retval);
		return -ENXIO;
	}

	return 0;
}

static int get_scope(u16 opcode)
{
	switch (opcode) {
	case CXL_MBOX_OPCODE_GET_SUPPORTED_FEATURES:
	case CXL_MBOX_OPCODE_GET_FEATURE:
		return FWCTL_RPC_CONFIGURATION;
	case CXL_MBOX_OPCODE_SET_FEATURE:
		return FWCTL_RPC_DEBUG_WRITE_FULL;
	default:
		return -EINVAL;
	}
}

static size_t hw_op_size(u16 opcode)
{
	switch (opcode) {
	case CXL_MBOX_OPCODE_GET_SUPPORTED_FEATURES:
		return sizeof(struct cxl_mbox_get_sup_feats_in);
	case CXL_MBOX_OPCODE_GET_FEATURE:
		return sizeof(struct cxl_mbox_get_feat_in);
	case CXL_MBOX_OPCODE_SET_FEATURE:
		return sizeof(struct cxl_mbox_set_feat_in) + sizeof(u32);
	default:
		return SIZE_MAX;
	}
}

static void free_rpc(struct fwctl_rpc *rpc)
{
	void *in, *out;

	in = (void *)rpc->in;
	out = (void *)rpc->out;
	free(in);
	free(out);
	free(rpc);
}

static void *zmalloc_aligned(size_t align, size_t size)
{
	void *ptr;
	int rc;

	rc = posix_memalign((void **)&ptr, align, size);
	if (rc)
		return NULL;
	memset(ptr, 0, size);

	return ptr;
}

static struct fwctl_rpc *get_prepped_command(size_t in_size, size_t out_size,
					     u16 opcode)
{
	struct fwctl_rpc_cxl_out *out;
	struct fwctl_rpc_cxl *in;
	struct fwctl_rpc *rpc;
	size_t op_size;
	int scope;

	rpc = zmalloc_aligned(16, sizeof(*rpc));
	if (!rpc)
		return NULL;

	in = zmalloc_aligned(16, in_size);
	if (!in)
		goto free_rpc;

	out = zmalloc_aligned(16, out_size);
	if (!out)
		goto free_in;

	in->opcode = opcode;

	op_size = hw_op_size(opcode);
	if (op_size == SIZE_MAX)
		goto free_all;

	in->op_size = op_size;

	rpc->size = sizeof(*rpc);
	scope = get_scope(opcode);
	if (scope < 0)
		goto free_all;

	rpc->scope = scope;

	rpc->in_len = in_size;
	rpc->out_len = out_size;
	rpc->in = (uint64_t)(uint64_t *)in;
	rpc->out = (uint64_t)(uint64_t *)out;

	return rpc;

free_all:
	free(out);
free_in:
	free(in);
free_rpc:
	free(rpc);
	return NULL;
}

static int cxl_fwctl_rpc_get_test_feature(int fd, struct test_feature *feat_ctx,
					  const uint32_t expected_data)
{
	struct cxl_mbox_get_feat_in *feat_in;
	struct fwctl_rpc_cxl_out *out;
	size_t out_size, in_size;
	struct fwctl_rpc_cxl *in;
	struct fwctl_rpc *rpc;
	uint32_t val;
	void *data;
	int rc;

	in_size = sizeof(*in) + sizeof(*feat_in);
	out_size = sizeof(*out) + feat_ctx->get_size;

	rpc = get_prepped_command(in_size, out_size,
				  CXL_MBOX_OPCODE_GET_FEATURE);
	if (!rpc)
		return -ENXIO;

	in = (struct fwctl_rpc_cxl *)rpc->in;
	out = (struct fwctl_rpc_cxl_out *)rpc->out;

	feat_in = &in->get_feat_in;
	uuid_copy(feat_in->uuid, feat_ctx->uuid);
	feat_in->count = feat_ctx->get_size;

	rc = send_command(fd, rpc, out);
	if (rc)
		goto out;

	data = out->payload;
	val = le32toh(*(__le32 *)data);
	if (memcmp(&val, &expected_data, sizeof(val)) != 0) {
		rc = -ENXIO;
		goto out;
	}

out:
	free_rpc(rpc);
	return rc;
}

static int cxl_fwctl_rpc_get_feature_oob(int fd, struct test_feature *feat_ctx)
{
	struct cxl_mbox_get_feat_in *feat_in;
	struct fwctl_rpc_cxl_out *out;
	size_t out_size, in_size;
	struct fwctl_rpc_cxl *in;
	struct fwctl_rpc *rpc;
	int rc;

	in_size = sizeof(*in) + sizeof(*feat_in);
	/* header only => zero payload room */
	out_size = offsetof(struct fwctl_rpc_cxl_out, payload);

	rpc = get_prepped_command(in_size, out_size,
				  CXL_MBOX_OPCODE_GET_FEATURE);
	if (!rpc)
		return -ENXIO;

	in = (struct fwctl_rpc_cxl *)rpc->in;
	out = (struct fwctl_rpc_cxl_out *)rpc->out;

	feat_in = &in->get_feat_in;
	uuid_copy(feat_in->uuid, feat_ctx->uuid);
	/* non-zero count that exceeds the zero payload room */
	feat_in->count = feat_ctx->get_size;

	rc = send_command(fd, rpc, out);
	free_rpc(rpc);

	if (rc == -EINVAL)
		return 0;
	if (rc == 0) {
		fprintf(stderr, "Get Feature with undersized out_len was not rejected\n");
		return -ENXIO;
	}
	fprintf(stderr, "Get Feature OOB rejection test: unexpected rc %d\n", rc);
	return rc;
}

static int cxl_fwctl_rpc_set_test_feature(int fd, struct test_feature *feat_ctx)
{
	struct cxl_mbox_set_feat_in *feat_in;
	struct fwctl_rpc_cxl_out *out;
	size_t in_size, out_size;
	struct fwctl_rpc_cxl *in;
	struct fwctl_rpc *rpc;
	uint32_t val;
	void *data;
	int rc;

	in_size = sizeof(*in) + sizeof(*feat_in) + sizeof(val);
	out_size = sizeof(*out) + sizeof(val);
	rpc = get_prepped_command(in_size, out_size,
				  CXL_MBOX_OPCODE_SET_FEATURE);
	if (!rpc)
		return -ENXIO;

	in = (struct fwctl_rpc_cxl *)rpc->in;
	out = (struct fwctl_rpc_cxl_out *)rpc->out;
	feat_in = &in->set_feat_in;
	uuid_copy(feat_in->uuid, feat_ctx->uuid);
	data = feat_in->feat_data;
	val = DEFAULT_TEST_DATA2;
	*(uint32_t *)data = htole32(val);
	feat_in->flags = CXL_SET_FEAT_FLAG_FULL_DATA_TRANSFER;

	rc = send_command(fd, rpc, out);
	if (rc)
		goto out;

	rc = cxl_fwctl_rpc_get_test_feature(fd, feat_ctx, DEFAULT_TEST_DATA2);
	if (rc) {
		fprintf(stderr, "Failed ioctl to get feature verify: %d\n", rc);
		goto out;
	}

out:
	free_rpc(rpc);
	return rc;
}

static int cxl_fwctl_rpc_set_feature_oob(int fd, struct test_feature *feat_ctx)
{
	struct cxl_mbox_set_feat_in *feat_in;
	struct fwctl_rpc_cxl_out *out;
	size_t in_size, out_size;
	struct fwctl_rpc_cxl *in;
	struct fwctl_rpc *rpc;
	uint32_t val;
	void *data;
	int rc;

	in_size = sizeof(*in) + sizeof(*feat_in) + sizeof(val);
	out_size = sizeof(*out) + sizeof(val);
	rpc = get_prepped_command(in_size, out_size,
				  CXL_MBOX_OPCODE_SET_FEATURE);
	if (!rpc)
		return -ENXIO;

	in = (struct fwctl_rpc_cxl *)rpc->in;
	out = (struct fwctl_rpc_cxl_out *)rpc->out;
	feat_in = &in->set_feat_in;
	uuid_copy(feat_in->uuid, feat_ctx->uuid);
	data = feat_in->feat_data;
	val = DEFAULT_TEST_DATA2;
	*(uint32_t *)data = htole32(val);
	feat_in->flags = CXL_SET_FEAT_FLAG_FULL_DATA_TRANSFER;

	/* A valid Set Feature request with no room for the output header */
	rpc->out_len = 0;
	rc = send_command(fd, rpc, out);
	free_rpc(rpc);

	if (rc == -EINVAL)
		return 0;
	if (rc == 0) {
		fprintf(stderr,
			"Set Feature with zero out_len was not rejected\n");
		return -ENXIO;
	}
	fprintf(stderr,
		"Set Feature OOB rejection test: unexpected rc %d\n", rc);
	return rc;
}

static int cxl_fwctl_rpc_feature_oob_tests(int fd,
					   struct test_feature *feat_ctx)
{
	int rc;

	if (!kver_ge(7, 3)) {
		fprintf(stderr,
			"skip: Feature OOB rejection tests need kernel >= 7.3\n");
		return 0;
	}

	rc = cxl_fwctl_rpc_get_feature_oob(fd, feat_ctx);
	if (rc) {
		fprintf(stderr, "Failed Get Feature OOB rejection test: %d\n", rc);
		return rc;
	}

	rc = cxl_fwctl_rpc_set_feature_oob(fd, feat_ctx);
	if (rc)
		fprintf(stderr, "Failed Set Feature OOB rejection test: %d\n", rc);
	return rc;
}

static int cxl_fwctl_rpc_get_supported_features(int fd, struct test_feature *feat_ctx)
{
	struct cxl_mbox_get_sup_feats_out *feat_out;
	struct cxl_mbox_get_sup_feats_in *feat_in;
	struct fwctl_rpc_cxl_out *out;
	struct cxl_feat_entry *entry;
	size_t out_size, in_size;
	struct fwctl_rpc_cxl *in;
	struct fwctl_rpc *rpc;
	int feats, rc;

	in_size = sizeof(*in) + sizeof(*feat_in);
	out_size = sizeof(*out) + sizeof(*feat_out);
	/* First query, to get number of features w/o per feature data */
	rpc = get_prepped_command(in_size, out_size,
				  CXL_MBOX_OPCODE_GET_SUPPORTED_FEATURES);
	if (!rpc)
		return -ENXIO;

	/* No need to fill in feat_in first go as we are passing in all 0's */

	out = (struct fwctl_rpc_cxl_out *)rpc->out;
	rc = send_command(fd, rpc, out);
	if (rc)
		goto out;

	feat_out = &out->get_sup_feats_out;
	feats = le16toh(feat_out->supported_feats);
	if (feats != MAX_TEST_FEATURES) {
		fprintf(stderr, "Test device has greater than %d test features.\n",
			MAX_TEST_FEATURES);
		rc = -ENXIO;
		goto out;
	}

	free_rpc(rpc);

	/* Going second round to retrieve each feature details */
	in_size = sizeof(*in) + sizeof(*feat_in);
	out_size = sizeof(*out) + sizeof(*feat_out);
	out_size += feats * sizeof(*entry);
	rpc = get_prepped_command(in_size, out_size,
				  CXL_MBOX_OPCODE_GET_SUPPORTED_FEATURES);
	if (!rpc)
		return -ENXIO;

	in = (struct fwctl_rpc_cxl *)rpc->in;
	out = (struct fwctl_rpc_cxl_out *)rpc->out;
	feat_in = &in->get_sup_feats_in;
	feat_in->count = htole32(feats * sizeof(*entry));

	rc = send_command(fd, rpc, out);
	if (rc)
		goto out;

	feat_out = &out->get_sup_feats_out;
	feats = le16toh(feat_out->supported_feats);
	if (feats != MAX_TEST_FEATURES) {
		fprintf(stderr, "Test device has greater than %u test features.\n",
			MAX_TEST_FEATURES);
		rc = -ENXIO;
		goto out;
	}

	if (le16toh(feat_out->num_entries) != MAX_TEST_FEATURES) {
		fprintf(stderr, "Test device did not return expected entries. %u\n",
			le16toh(feat_out->num_entries));
		rc = -ENXIO;
		goto out;
	}

	entry = &feat_out->ents[0];
	if (uuid_compare(test_uuid, entry->uuid) != 0) {
		fprintf(stderr, "Test device did not export expected test feature.\n");
		rc = -ENXIO;
		goto out;
	}

	if (le16toh(entry->get_feat_size) != GET_FEAT_SIZE ||
	    le16toh(entry->set_feat_size) != SET_FEAT_SIZE) {
		fprintf(stderr, "Test device feature in/out size incorrect.\n");
		rc = -ENXIO;
		goto out;
	}

	if (le16toh(entry->effects) != EFFECTS_MASK) {
		fprintf(stderr, "Test device set effects incorrect\n");
		rc = -ENXIO;
		goto out;
	}

	uuid_copy(feat_ctx->uuid, entry->uuid);
	feat_ctx->get_size = le16toh(entry->get_feat_size);
	feat_ctx->set_size = le16toh(entry->set_feat_size);

out:
	free_rpc(rpc);
	return rc;
}

static int test_fwctl_features(struct cxl_memdev *memdev)
{
	struct test_feature feat_ctx;
	unsigned int major, minor;
	struct cxl_fwctl *fwctl;
	int fd, rc;
	char path[256];

	fwctl = cxl_memdev_get_fwctl(memdev);
	if (!fwctl)
		return -ENODEV;

	major = cxl_fwctl_get_major(fwctl);
	minor = cxl_fwctl_get_minor(fwctl);

	if (!major && !minor)
		return -ENODEV;

	sprintf(path, "/dev/char/%d:%d", major, minor);

	fd = open(path, O_RDONLY, 0644);
	if (fd < 0) {
		fprintf(stderr, "Failed to open: %d\n", -errno);
		return -errno;
	}

	rc = cxl_fwctl_rpc_get_supported_features(fd, &feat_ctx);
	if (rc) {
		fprintf(stderr, "Failed ioctl to get supported features: %d\n", rc);
		goto out;
	}

	rc = cxl_fwctl_rpc_get_test_feature(fd, &feat_ctx, DEFAULT_TEST_DATA);
	if (rc) {
		fprintf(stderr, "Failed ioctl to get feature: %d\n", rc);
		goto out;
	}

	rc = cxl_fwctl_rpc_set_test_feature(fd, &feat_ctx);
	if (rc) {
		fprintf(stderr, "Failed ioctl to set feature: %d\n", rc);
		goto out;
	}

	rc = cxl_fwctl_rpc_feature_oob_tests(fd, &feat_ctx);
	if (rc)
		goto out;

out:
	close(fd);
	return rc;
}

static int test_fwctl(struct cxl_ctx *ctx, struct cxl_bus *bus)
{
	struct cxl_memdev *memdev;

	cxl_memdev_foreach(ctx, memdev) {
		if (cxl_memdev_get_bus(memdev) != bus)
			continue;
		return test_fwctl_features(memdev);
	}

	return 0;
}

int main(int argc, char *argv[])
{
	struct cxl_ctx *ctx;
	struct cxl_bus *bus;
	int rc;

	parse_kver();

	rc = cxl_new(&ctx);
	if (rc < 0)
		return rc;

	cxl_set_log_priority(ctx, LOG_DEBUG);

	bus = cxl_bus_get_by_provider(ctx, provider);
	if (!bus) {
		fprintf(stderr, "%s: unable to find bus (%s)\n",
			argv[0], provider);
		rc = -EINVAL;
		goto out;
	}

	rc = test_fwctl(ctx, bus);

out:
	cxl_unref(ctx);
	return rc;
}
