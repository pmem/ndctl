// SPDX-License-Identifier: GPL-2.0
/* Copyright (C) 2020-2021 Intel Corporation. All rights reserved. */
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <util/log.h>
#include <util/json.h>
#include <util/size.h>
#include <cxl/libcxl.h>
#include <util/parse-options.h>
#include <ccan/minmax/minmax.h>
#include <ccan/array_size/array_size.h>

#include "json.h"
#include "filter.h"

struct action_context {
	FILE *f_out;
	FILE *f_in;
};

static struct parameters {
	const char *outfile;
	const char *infile;
	unsigned len;
	unsigned offset;
	bool verbose;
	bool serial;
	bool force;
	bool align;
	const char *type;
	const char *size;
} param;

static struct log_ctx ml;

enum cxl_setpart_type {
	CXL_SETPART_PMEM,
	CXL_SETPART_VOLATILE,
};

#define BASE_OPTIONS() \
OPT_BOOLEAN('v',"verbose", &param.verbose, "turn on debug"), \
OPT_BOOLEAN('S', "serial", &param.serial, "use serial numbers to id memdevs")

#define READ_OPTIONS() \
OPT_STRING('o', "output", &param.outfile, "output-file", \
	"filename to write label area contents")

#define WRITE_OPTIONS() \
OPT_STRING('i', "input", &param.infile, "input-file", \
	"filename to read label area data")

#define LABEL_OPTIONS() \
OPT_UINTEGER('s', "size", &param.len, "number of label bytes to operate"), \
OPT_UINTEGER('O', "offset", &param.offset, \
	"offset into the label area to start operation")

#define DISABLE_OPTIONS()                                              \
OPT_BOOLEAN('f', "force", &param.force,                                \
	    "DANGEROUS: override active memdev safety checks")

#define SET_PARTITION_OPTIONS() \
OPT_STRING('t', "type",  &param.type, "type",			\
	"'pmem' or 'volatile' (Default: 'pmem')"),		\
OPT_STRING('s', "size",  &param.size, "size",			\
	"size in bytes (Default: all available capacity)"),	\
OPT_BOOLEAN('a', "align",  &param.align,			\
	"auto-align --size per device's requirement")

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

static const struct option disable_options[] = {
	BASE_OPTIONS(),
	DISABLE_OPTIONS(),
	OPT_END(),
};

static const struct option enable_options[] = {
	BASE_OPTIONS(),
	OPT_END(),
};

static const struct option set_partition_options[] = {
	BASE_OPTIONS(),
	SET_PARTITION_OPTIONS(),
	OPT_END(),
};

static int action_disable(struct cxl_memdev *memdev, struct action_context *actx)
{
	if (!cxl_memdev_is_enabled(memdev))
		return 0;

	if (!param.force) {
		/* TODO: actually detect rather than assume active */
		log_err(&ml, "%s is part of an active region\n",
			cxl_memdev_get_devname(memdev));
		return -EBUSY;
	}

	return cxl_memdev_disable_invalidate(memdev);
}

static int action_enable(struct cxl_memdev *memdev, struct action_context *actx)
{
	return cxl_memdev_enable(memdev);
}

static int action_zero(struct cxl_memdev *memdev, struct action_context *actx)
{
	size_t size;
	int rc;

	if (param.len)
		size = param.len;
	else
		size = cxl_memdev_get_label_size(memdev);

	if (cxl_memdev_nvdimm_bridge_active(memdev)) {
		log_err(&ml,
			"%s: has active nvdimm bridge, abort label write\n",
			cxl_memdev_get_devname(memdev));
		return -EBUSY;
	}

	rc = cxl_memdev_zero_label(memdev, size, param.offset);
	if (rc < 0)
		log_err(&ml, "%s: label zeroing failed: %s\n",
			cxl_memdev_get_devname(memdev), strerror(-rc));

	return rc;
}

static int action_write(struct cxl_memdev *memdev, struct action_context *actx)
{
	size_t size = param.len, read_len;
	unsigned char *buf;
	int rc;

	if (cxl_memdev_nvdimm_bridge_active(memdev)) {
		log_err(&ml,
			"%s: has active nvdimm bridge, abort label write\n",
			cxl_memdev_get_devname(memdev));
		return -EBUSY;
	}

	if (!size) {
		size_t label_size = cxl_memdev_get_label_size(memdev);

		fseek(actx->f_in, 0L, SEEK_END);
		size = ftell(actx->f_in);
		fseek(actx->f_in, 0L, SEEK_SET);

		if (size > label_size) {
			log_err(&ml,
				"File size (%zu) greater than label area size (%zu), aborting\n",
				size, label_size);
			return -EINVAL;
		}
	}

	buf = calloc(1, size);
	if (!buf)
		return -ENOMEM;

	read_len = fread(buf, 1, size, actx->f_in);
	if (read_len != size) {
		rc = -ENXIO;
		goto out;
	}

	rc = cxl_memdev_write_label(memdev, buf, size, param.offset);
	if (rc < 0)
		log_err(&ml, "%s: label write failed: %s\n",
			cxl_memdev_get_devname(memdev), strerror(-rc));

out:
	free(buf);
	return rc;
}

static int action_read(struct cxl_memdev *memdev, struct action_context *actx)
{
	size_t size, write_len;
	char *buf;
	int rc;

	if (param.len)
		size = param.len;
	else
		size = cxl_memdev_get_label_size(memdev);

	buf = calloc(1, size);
	if (!buf)
		return -ENOMEM;

	rc = cxl_memdev_read_label(memdev, buf, size, param.offset);
	if (rc < 0) {
		log_err(&ml, "%s: label read failed: %s\n",
			cxl_memdev_get_devname(memdev), strerror(-rc));
		goto out;
	}

	write_len = fwrite(buf, 1, size, actx->f_out);
	if (write_len != size) {
		rc = -ENXIO;
		goto out;
	}
	fflush(actx->f_out);

out:
	free(buf);
	return rc;
}

static unsigned long long
partition_align(const char *devname, enum cxl_setpart_type type,
		unsigned long long volatile_size, unsigned long long alignment,
		unsigned long long available)
{
	if (IS_ALIGNED(volatile_size, alignment))
		return volatile_size;

	if (!param.align) {
		log_err(&ml, "%s: size %lld is not partition aligned %lld\n",
			devname, volatile_size, alignment);
		return ULLONG_MAX;
	}

	/* Align based on partition type to fulfill users size request */
	if (type == CXL_SETPART_PMEM)
		volatile_size = ALIGN_DOWN(volatile_size, alignment);
	else
		volatile_size = ALIGN(volatile_size, alignment);

	/* Fail if the align pushes size over the available limit. */
	if (volatile_size > available) {
		log_err(&ml, "%s: aligned partition size %lld exceeds available size %lld\n",
			devname, volatile_size, available);
		volatile_size = ULLONG_MAX;
	}

	return volatile_size;
}

static unsigned long long
param_size_to_volatile_size(const char *devname, enum cxl_setpart_type type,
		unsigned long long size, unsigned long long available)
{
	/* User omits size option. Apply all available capacity to type. */
	if (size == ULLONG_MAX) {
		if (type == CXL_SETPART_PMEM)
			return 0;
		return available;
	}

	/* User includes a size option. Apply it to type */
	if (size > available) {
		log_err(&ml, "%s: %lld exceeds available capacity %lld\n",
			devname, size, available);
			return ULLONG_MAX;
	}
	if (type == CXL_SETPART_PMEM)
		return available - size;
	return size;
}

/*
 * Return the volatile_size to use in the CXL set paritition
 * command, or ULLONG_MAX if unable to validate the partition
 * request.
 */
static unsigned long long
validate_partition(struct cxl_memdev *memdev, enum cxl_setpart_type type,
		unsigned long long size)
{
	unsigned long long total_cap, volatile_only, persistent_only;
	const char *devname = cxl_memdev_get_devname(memdev);
	unsigned long long volatile_size = ULLONG_MAX;
	unsigned long long available, alignment;
	struct cxl_cmd *cmd;
	int rc;

	cmd = cxl_cmd_new_identify(memdev);
	if (!cmd)
		return ULLONG_MAX;
	rc = cxl_cmd_submit(cmd);
	if (rc < 0)
		goto out;
	rc = cxl_cmd_get_mbox_status(cmd);
	if (rc != 0)
		goto out;

	alignment = cxl_cmd_identify_get_partition_align(cmd);
	if (alignment == 0) {
		log_err(&ml, "%s: no available capacity\n", devname);
		goto out;
	}

	/* Calculate the actual available capacity */
	total_cap = cxl_cmd_identify_get_total_size(cmd);
	volatile_only = cxl_cmd_identify_get_volatile_only_size(cmd);
	persistent_only = cxl_cmd_identify_get_persistent_only_size(cmd);
	available = total_cap - volatile_only - persistent_only;

	/* Translate the users size request into an aligned volatile_size */
	volatile_size = param_size_to_volatile_size(devname, type, size,
				available);
	if (volatile_size == ULLONG_MAX)
		goto out;

	volatile_size = partition_align(devname, type, volatile_size, alignment,
				available);

out:
	cxl_cmd_unref(cmd);
	return volatile_size;
}

static int action_setpartition(struct cxl_memdev *memdev,
		struct action_context *actx)
{
	const char *devname = cxl_memdev_get_devname(memdev);
	enum cxl_setpart_type type = CXL_SETPART_PMEM;
	unsigned long long size = ULLONG_MAX;
	struct json_object *jmemdev;
	struct cxl_cmd *cmd;
	int rc;

	if (param.type) {
		if (strcmp(param.type, "pmem") == 0)
			/* default */;
		else if (strcmp(param.type, "volatile") == 0)
			type = CXL_SETPART_VOLATILE;
		else {
			log_err(&ml, "invalid type '%s'\n", param.type);
			return -EINVAL;
		}
	}

	if (param.size) {
		size = parse_size64(param.size);
		if (size == ULLONG_MAX) {
			log_err(&ml, "%s: failed to parse size option '%s'\n",
			devname, param.size);
			return -EINVAL;
		}
	}

	size = validate_partition(memdev, type, size);
	if (size == ULLONG_MAX)
		return -EINVAL;

	cmd = cxl_cmd_new_set_partition(memdev, size);
	if (!cmd) {
		rc = -ENXIO;
		goto out_err;
	}

	rc = cxl_cmd_submit(cmd);
	if (rc < 0) {
		log_err(&ml, "cmd submission failed: %s\n", strerror(-rc));
		goto out_cmd;
	}

	rc = cxl_cmd_get_mbox_status(cmd);
	if (rc != 0) {
		log_err(&ml, "%s: mbox status: %d\n", __func__, rc);
		rc = -ENXIO;
	}

out_cmd:
	cxl_cmd_unref(cmd);
out_err:
	if (rc)
		log_err(&ml, "%s error: %s\n", devname, strerror(-rc));

	jmemdev = util_cxl_memdev_to_json(memdev, UTIL_JSON_PARTITION);
	if (jmemdev)
		printf("%s\n", json_object_to_json_string_ext(jmemdev,
		       JSON_C_TO_STRING_PRETTY));

	return rc;
}

static int memdev_action(int argc, const char **argv, struct cxl_ctx *ctx,
			 int (*action)(struct cxl_memdev *memdev,
				       struct action_context *actx),
			 const struct option *options, const char *usage)
{
	struct cxl_memdev *memdev, *single = NULL;
	struct action_context actx = { 0 };
	int i, rc = 0, count = 0, err = 0;
	const char * const u[] = {
		usage,
		NULL
	};
	unsigned long id;

	log_init(&ml, "cxl memdev", "CXL_MEMDEV_LOG");
	argc = parse_options(argc, argv, options, u, 0);

	if (argc == 0)
		usage_with_options(u, options);
	for (i = 0; i < argc; i++) {
		if (param.serial) {
			char *end;

			strtoull(argv[i], &end, 0);
			if (end[0] == 0)
				continue;
		} else {
			if (strcmp(argv[i], "all") == 0) {
				argc = 1;
				break;
			}
			if (sscanf(argv[i], "mem%lu", &id) == 1)
				continue;
			if (sscanf(argv[i], "%lu", &id) == 1)
				continue;
		}

		log_err(&ml, "'%s' is not a valid memdev %s\n", argv[i],
			param.serial ? "serial number" : "name");
		err++;
	}

	if (err == argc) {
		usage_with_options(u, options);
		return -EINVAL;
	}

	if (!param.outfile)
		actx.f_out = stdout;
	else {
		actx.f_out = fopen(param.outfile, "w+");
		if (!actx.f_out) {
			log_err(&ml, "failed to open: %s: (%s)\n",
				param.outfile, strerror(errno));
			rc = -errno;
			goto out;
		}
	}

	if (!param.infile) {
		actx.f_in = stdin;
	} else {
		actx.f_in = fopen(param.infile, "r");
		if (!actx.f_in) {
			log_err(&ml, "failed to open: %s: (%s)\n", param.infile,
				strerror(errno));
			rc = -errno;
			goto out_close_fout;
		}
	}

	if (param.verbose) {
		cxl_set_log_priority(ctx, LOG_DEBUG);
		ml.log_priority = LOG_DEBUG;
	} else
		ml.log_priority = LOG_INFO;

	rc = 0;
	err = 0;
	count = 0;

	for (i = 0; i < argc; i++) {
		cxl_memdev_foreach(ctx, memdev) {
			const char *memdev_filter = NULL;
			const char *serial_filter = NULL;

			if (param.serial)
				serial_filter = argv[i];
			else
				memdev_filter = argv[i];

			if (!util_cxl_memdev_filter(memdev, memdev_filter,
						    serial_filter))
				continue;

			if (action == action_write) {
				single = memdev;
				rc = 0;
			} else
				rc = action(memdev, &actx);

			if (rc == 0)
				count++;
			else if (rc && !err)
				err = rc;
		}
	}
	rc = err;

	if (action == action_write) {
		if (count > 1) {
			error("write-labels only supports writing a single memdev\n");
			usage_with_options(u, options);
			return -EINVAL;
		} else if (single) {
			rc = action(single, &actx);
			if (rc)
				count = 0;
		}
	}

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

int cmd_write_labels(int argc, const char **argv, struct cxl_ctx *ctx)
{
	int count = memdev_action(argc, argv, ctx, action_write, write_options,
			"cxl write-labels <memdev> [-i <filename>]");

	log_info(&ml, "wrote %d mem%s\n", count >= 0 ? count : 0,
		 count > 1 ? "s" : "");
	return count >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_read_labels(int argc, const char **argv, struct cxl_ctx *ctx)
{
	int count = memdev_action(argc, argv, ctx, action_read, read_options,
			"cxl read-labels <mem0> [<mem1>..<memN>] [-o <filename>]");

	log_info(&ml, "read %d mem%s\n", count >= 0 ? count : 0,
		 count > 1 ? "s" : "");
	return count >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_zero_labels(int argc, const char **argv, struct cxl_ctx *ctx)
{
	int count = memdev_action(argc, argv, ctx, action_zero, zero_options,
			"cxl zero-labels <mem0> [<mem1>..<memN>] [<options>]");

	log_info(&ml, "zeroed %d mem%s\n", count >= 0 ? count : 0,
		 count > 1 ? "s" : "");
	return count >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_disable_memdev(int argc, const char **argv, struct cxl_ctx *ctx)
{
	int count = memdev_action(
		argc, argv, ctx, action_disable, disable_options,
		"cxl disable-memdev <mem0> [<mem1>..<memN>] [<options>]");

	log_info(&ml, "disabled %d mem%s\n", count >= 0 ? count : 0,
		 count > 1 ? "s" : "");
	return count >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_enable_memdev(int argc, const char **argv, struct cxl_ctx *ctx)
{
	int count = memdev_action(
		argc, argv, ctx, action_enable, enable_options,
		"cxl enable-memdev <mem0> [<mem1>..<memN>] [<options>]");

	log_info(&ml, "enabled %d mem%s\n", count >= 0 ? count : 0,
		 count > 1 ? "s" : "");
	return count >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_set_partition(int argc, const char **argv, struct cxl_ctx *ctx)
{
	int count = memdev_action(argc, argv, ctx, action_setpartition,
			set_partition_options,
			"cxl set-partition <mem0> [<mem1>..<memN>] [<options>]");
	log_info(&ml, "set_partition %d mem%s\n", count >= 0 ? count : 0,
		 count > 1 ? "s" : "");

	return count >= 0 ? 0 : EXIT_FAILURE;
}
