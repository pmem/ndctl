// SPDX-License-Identifier: GPL-2.0
/* Copyright (C) 2020-2021 Intel Corporation. All rights reserved. */
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <sys/types.h>
#include <util/log.h>
#include <util/filter.h>
#include <util/parse-options.h>
#include <ccan/list/list.h>
#include <ccan/minmax/minmax.h>
#include <ccan/array_size/array_size.h>
#include <ccan/endian/endian.h>
#include <ccan/short_types/short_types.h>
#include <cxl/libcxl.h>

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
} param;

#define fail(fmt, ...) \
do { \
	fprintf(stderr, "cxl-%s:%s:%d: " fmt, \
			VERSION, __func__, __LINE__, ##__VA_ARGS__); \
} while (0)

#define BASE_OPTIONS() \
OPT_BOOLEAN('v',"verbose", &param.verbose, "turn on debug")

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

static const struct option cmd_identify_options[] = {
	BASE_OPTIONS(),
	OPT_END(),
};

static const struct option cmd_get_supported_logs_options[] = {
	BASE_OPTIONS(),
	OPT_END(),
};

static const struct option cmd_get_cel_log_options[] = {
	BASE_OPTIONS(),
	OPT_END(),
};

static const struct option cmd_get_event_interrupt_policy_options[] = {
	BASE_OPTIONS(),
	OPT_END(),
};

static struct _interrupt_policy_params {
	u32 policy;
	bool verbose;
} interrupt_policy_params;

#define INT_POLICY_BASE_OPTIONS() \
OPT_BOOLEAN('v',"verbose", &interrupt_policy_params.verbose, "turn on debug")

#define SET_INTERRUPT_POLICY_OPTIONS() \
OPT_UINTEGER('i', "int_policy", &interrupt_policy_params.policy, "Set event interrupt policy. Fields: Informational Event Log Interrupt Settings (1B), Warning Event Log Interrupt Settings (1B), Failure Event Log Interrupt Settings (1B), Fatal Event Log Interrupt Settings (1B)")

static const struct option cmd_set_event_interrupt_policy_options[] = {
	INT_POLICY_BASE_OPTIONS(),
	SET_INTERRUPT_POLICY_OPTIONS(),
	OPT_END(),
};

static const struct option cmd_get_timestamp_options[] = {
	BASE_OPTIONS(),
	OPT_END(),
};

static struct _ts_params {
	u64 timestamp;
	bool verbose;
} ts_params;

#define TS_BASE_OPTIONS() \
OPT_BOOLEAN('v',"verbose", &ts_params.verbose, "turn on debug")

#define SET_TIMESTAMP_OPTIONS() \
OPT_U64('t', "timestamp", &ts_params.timestamp, "Set the timestamp on the device")

static const struct option cmd_set_timestamp_options[] = {
	TS_BASE_OPTIONS(),
	SET_TIMESTAMP_OPTIONS(),
	OPT_END(),
};

static const struct option cmd_get_alert_config_options[] = {
	BASE_OPTIONS(),
	OPT_END(),
};

static struct _alert_config_params {
	u32 alert_prog_threshold;
	u32 device_temp_threshold;
	u32 mem_error_threshold;
	bool verbose;
} alert_config_params;

#define SET_ALERT_CONFIG_BASE_OPTIONS() \
OPT_BOOLEAN('v',"verbose", &interrupt_policy_params.verbose, "turn on debug")

#define SET_ALERT_CONFIG_OPTIONS() \
OPT_UINTEGER('a', "alert_prog_threshold", &alert_config_params.alert_prog_threshold, "Set valid, enable alert actions and life used programmable threshold. Fields: Valid Alert Actions (1B), Enable Alert Actions (1B), Life Used Programmable Warning Threshold (1B)"), \
OPT_UINTEGER('d', "device_temp_threshold", &alert_config_params.device_temp_threshold, "Set device over/under temp thresholds. Fields: Device Over-Temperature Programmable Warning Threshold (2B), Device Under-Temperature Programmable Warning Threshold (2B)"), \
OPT_UINTEGER('m', "mem_error_threshold", &alert_config_params.mem_error_threshold, "Set memory corrected thresholds. Fields: Corrected Volatile Memory Error Programmable Warning Threshold (2B), Corrected Persistent Memory Error Programmable Warning Threshold (2B)")

static const struct option cmd_set_alert_config_options[] = {
	SET_ALERT_CONFIG_BASE_OPTIONS(),
	SET_ALERT_CONFIG_OPTIONS(),
	OPT_END(),
};

static const struct option cmd_get_health_info_options[] = {
	BASE_OPTIONS(),
	OPT_END(),
};

static struct _get_ld_info_params {
	bool verbose;
} get_ld_info_params;

#define GET_LD_INFO_BASE_OPTIONS() \
OPT_BOOLEAN('v',"verbose", &get_ld_info_params.verbose, "turn on debug")

static const struct option cmd_get_ld_info_options[] = {
	GET_LD_INFO_BASE_OPTIONS(),
	OPT_END(),
};

static struct _ddr_info_params {
	bool verbose;
	int ddr_id;
} ddr_info_params;

#define DDR_INFO_BASE_OPTIONS() \
OPT_BOOLEAN('v',"verbose", &ddr_info_params.verbose, "turn on debug")

#define DDR_INFO_OPTIONS() \
OPT_INTEGER('i', "ddr_id", &ddr_info_params.ddr_id, "DDR instance id")


static const struct option cmd_ddr_info_options[] = {
	DDR_INFO_BASE_OPTIONS(),
	DDR_INFO_OPTIONS(),
	OPT_END(),
};

static struct _get_event_records_params {
	int event_log_type; /* 00 - information, 01 - warning, 02 - failure, 03 - fatal */
	bool verbose;
} get_event_records_params;

#define GET_EVENT_RECORDS_BASE_OPTIONS() \
OPT_BOOLEAN('v',"verbose", &get_event_records_params.verbose, "turn on debug")

#define GET_EVENT_RECORDS_OPTIONS() \
OPT_INTEGER('t', "log_type", &get_event_records_params.event_log_type, "Event log type (00 - information (default), 01 - warning, 02 - failure, 03 - fatal)")

static const struct option cmd_get_event_records_options[] = {
	GET_EVENT_RECORDS_BASE_OPTIONS(),
	GET_EVENT_RECORDS_OPTIONS(),
	OPT_END(),
};

static struct _clear_event_records_params {
	int event_log_type; /* 00 - information, 01 - warning, 02 - failure, 03 - fatal */
	int clear_event_flags; /* bit 0 - when set, clears all events */
	unsigned event_record_handle; /* only one is supported */
	bool verbose;
} clear_event_records_params;

#define CLEAR_EVENT_RECORDS_BASE_OPTIONS() \
OPT_BOOLEAN('v',"verbose", &clear_event_records_params.verbose, "turn on debug")

#define CLEAR_EVENT_RECORDS_OPTIONS() \
OPT_INTEGER('t', "log_type", &clear_event_records_params.event_log_type, "Event log type (00 - information (default), 01 - warning, 02 - failure, 03 - fatal)"), \
OPT_INTEGER('f', "event_flag", &clear_event_records_params.clear_event_flags, "Clear Event Flags: 1 - clear all events, 0 (default) - clear specific event record"), \
OPT_UINTEGER('i', "event_record_handle", &clear_event_records_params.event_record_handle, "Clear Specific Event specific by Event Record Handle")

static const struct option cmd_clear_event_records_options[] = {
	CLEAR_EVENT_RECORDS_BASE_OPTIONS(),
	CLEAR_EVENT_RECORDS_OPTIONS(),
	OPT_END(),
};

/* insert here params options */

static int action_cmd_clear_event_records(struct cxl_memdev *memdev, struct action_context *actx)
{
	u16 record_handle;
	if (cxl_memdev_is_active(memdev)) {
		fprintf(stderr, "%s: memdev active, abort clear_event_records\n",
			cxl_memdev_get_devname(memdev));
		return -EBUSY;
	}
	if (clear_event_records_params.clear_event_flags) {
		record_handle = 0;
		return cxl_memdev_clear_event_records(memdev, clear_event_records_params.event_log_type,
			clear_event_records_params.clear_event_flags, 0, &record_handle);
	}
	else {
		record_handle = (u16) clear_event_records_params.event_record_handle;
		return cxl_memdev_clear_event_records(memdev, clear_event_records_params.event_log_type,
			clear_event_records_params.clear_event_flags, 1, &record_handle);
	}
}

static int action_cmd_get_event_records(struct cxl_memdev *memdev, struct action_context *actx)
{
	if (cxl_memdev_is_active(memdev)) {
		fprintf(stderr, "%s: memdev active, abort get_event_records\n",
			cxl_memdev_get_devname(memdev));
		return -EBUSY;
	}
#if 0
	if (get_event_records_params.event_log_type < 0 || get_event_records_params.event_log_type > 3) {
		fprintf(stderr, "%s: Invalid Event Log type: %d, Allowed values Event log type "
			"(00 - information (default), 01 - warning, 02 - failure, 03 - fatal)\n",
			cxl_memdev_get_devname(memdev), get_event_records_params.event_log_type);
		return -EINVAL;
	}
#endif

	return cxl_memdev_get_event_records(memdev, get_event_records_params.event_log_type);
}

static int action_cmd_get_ld_info(struct cxl_memdev *memdev, struct action_context *actx)
{
	if (cxl_memdev_is_active(memdev)) {
		fprintf(stderr, "%s: memdev active, abort get_ld_info\n",
			cxl_memdev_get_devname(memdev));
		return -EBUSY;
	}

	return cxl_memdev_get_ld_info(memdev);
}

static int action_cmd_ddr_info(struct cxl_memdev *memdev, struct action_context *actx)
{
	if (cxl_memdev_is_active(memdev)) {
		fprintf(stderr, "%s: memdev active, abort ddr_info\n",
			cxl_memdev_get_devname(memdev));
		return -EBUSY;
	}
	fprintf(stdout, "memdev id: %d", cxl_memdev_get_id(memdev));
	return cxl_memdev_ddr_info(memdev, ddr_info_params.ddr_id);
}

static int action_cmd_get_health_info(struct cxl_memdev *memdev, struct action_context *actx)
{
	if (cxl_memdev_is_active(memdev)) {
		fprintf(stderr, "%s: memdev active, abort get_health_info\n",
			cxl_memdev_get_devname(memdev));
		return -EBUSY;
	}

	return cxl_memdev_get_health_info(memdev);
}

static int action_cmd_get_alert_config(struct cxl_memdev *memdev, struct action_context *actx)
{
	if (cxl_memdev_is_active(memdev)) {
		fprintf(stderr, "%s: memdev active, abort get_alert_config\n",
			cxl_memdev_get_devname(memdev));
		return -EBUSY;
	}

	return cxl_memdev_get_alert_config(memdev);
}

static int action_cmd_set_alert_config(struct cxl_memdev *memdev, struct action_context *actx)
{
	if (cxl_memdev_is_active(memdev)) {
		fprintf(stderr, "%s: memdev active, abort set_alert_config\n",
			cxl_memdev_get_devname(memdev));
		return -EBUSY;
	}

	return cxl_memdev_set_alert_config(memdev, alert_config_params.alert_prog_threshold,
		alert_config_params.device_temp_threshold, alert_config_params.mem_error_threshold);
}

static int action_cmd_get_timestamp(struct cxl_memdev *memdev, struct action_context *actx)
{
	if (cxl_memdev_is_active(memdev)) {
		fprintf(stderr, "%s: memdev active, get_timestamp\n",
			cxl_memdev_get_devname(memdev));
		return -EBUSY;
	}

	return cxl_memdev_get_timestamp(memdev);
}

static int action_cmd_set_timestamp(struct cxl_memdev *memdev, struct action_context *actx)
{
	if (cxl_memdev_is_active(memdev)) {
		fprintf(stderr, "%s: memdev active, set_timestamp\n",
			cxl_memdev_get_devname(memdev));
		return -EBUSY;
	}

	printf("timestamp: 0x%lx (%ld)\n", ts_params.timestamp, ts_params.timestamp);
	return cxl_memdev_set_timestamp(memdev, ts_params.timestamp);
}

static int action_cmd_get_event_interrupt_policy(struct cxl_memdev *memdev, struct action_context *actx)
{
	if (cxl_memdev_is_active(memdev)) {
		fprintf(stderr, "%s: memdev active, get_event_interrupt_policy\n",
			cxl_memdev_get_devname(memdev));
		return -EBUSY;
	}

	return cxl_memdev_get_event_interrupt_policy(memdev);
}

static int action_cmd_set_event_interrupt_policy(struct cxl_memdev *memdev, struct action_context *actx)
{
	if (cxl_memdev_is_active(memdev)) {
		fprintf(stderr, "%s: memdev active, set_event_interrupt_policy\n",
			cxl_memdev_get_devname(memdev));
		return -EBUSY;
	}

	return cxl_memdev_set_event_interrupt_policy(memdev, interrupt_policy_params.policy);
}

static int action_cmd_get_cel_log(struct cxl_memdev *memdev, struct action_context *actx)
{
	if (cxl_memdev_is_active(memdev)) {
		fprintf(stderr, "%s: memdev active, get_cel_log\n",
			cxl_memdev_get_devname(memdev));
		return -EBUSY;
	}

	return cxl_memdev_get_cel_log(memdev);
}

static int action_cmd_get_supported_logs(struct cxl_memdev *memdev, struct action_context *actx)
{
	if (cxl_memdev_is_active(memdev)) {
		fprintf(stderr, "%s: memdev active, get_supported_logs\n",
			cxl_memdev_get_devname(memdev));
		return -EBUSY;
	}

	return cxl_memdev_get_supported_logs(memdev);
}

static int action_cmd_identify(struct cxl_memdev *memdev, struct action_context *actx)
{
	if (cxl_memdev_is_active(memdev)) {
		fprintf(stderr, "%s: memdev active, cmd_identify\n",
			cxl_memdev_get_devname(memdev));
		return -EBUSY;
	}

	return cxl_memdev_cmd_identify(memdev);
}

/* insert here action */

static int action_zero(struct cxl_memdev *memdev, struct action_context *actx)
{
	int rc;

	if (cxl_memdev_is_active(memdev)) {
		fprintf(stderr, "%s: memdev active, abort label write\n",
			cxl_memdev_get_devname(memdev));
		return -EBUSY;
	}

	rc = cxl_memdev_zero_lsa(memdev);
	if (rc < 0)
		fprintf(stderr, "%s: label zeroing failed: %s\n",
			cxl_memdev_get_devname(memdev), strerror(-rc));

	return rc;
}

static int action_write(struct cxl_memdev *memdev, struct action_context *actx)
{
	size_t size = param.len, read_len;
	unsigned char *buf;
	int rc;

	if (cxl_memdev_is_active(memdev)) {
		fprintf(stderr, "%s is active, abort label write\n",
			cxl_memdev_get_devname(memdev));
		return -EBUSY;
	}

	if (!size) {
		size_t lsa_size = cxl_memdev_get_lsa_size(memdev);

		fseek(actx->f_in, 0L, SEEK_END);
		size = ftell(actx->f_in);
		fseek(actx->f_in, 0L, SEEK_SET);

		if (size > lsa_size) {
			fprintf(stderr,
				"File size (%zu) greater than LSA size (%zu), aborting\n",
				size, lsa_size);
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

	rc = cxl_memdev_set_lsa(memdev, buf, size, param.offset);
	if (rc < 0)
		fprintf(stderr, "%s: label write failed: %s\n",
			cxl_memdev_get_devname(memdev), strerror(-rc));

out:
	free(buf);
	return rc;
}

static int action_read(struct cxl_memdev *memdev, struct action_context *actx)
{
	size_t size = param.len, write_len;
	char *buf;
	int rc;

	if (!size)
		size = cxl_memdev_get_lsa_size(memdev);

	buf = calloc(1, size);
	if (!buf)
		return -ENOMEM;

	rc = cxl_memdev_get_lsa(memdev, buf, size, param.offset);
	if (rc < 0) {
		fprintf(stderr, "%s: label read failed: %s\n",
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

static int memdev_action(int argc, const char **argv, struct cxl_ctx *ctx,
		int (*action)(struct cxl_memdev *memdev, struct action_context *actx),
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

	argc = parse_options(argc, argv, options, u, 0);

	if (argc == 0)
		usage_with_options(u, options);
	for (i = 0; i < argc; i++) {
		if (strcmp(argv[i], "all") == 0) {
			argv[0] = "all";
			argc = 1;
			break;
		}

		if (sscanf(argv[i], "mem%lu", &id) != 1) {
			fprintf(stderr, "'%s' is not a valid memdev name\n",
					argv[i]);
			err++;
		}
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
			fprintf(stderr, "failed to open: %s: (%s)\n",
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
			fprintf(stderr, "failed to open: %s: (%s)\n",
					param.infile, strerror(errno));
			rc = -errno;
			goto out_close_fout;
		}
	}

	if (param.verbose)
		cxl_set_log_priority(ctx, LOG_DEBUG);

	rc = 0;
	err = 0;
	count = 0;

	for (i = 0; i < argc; i++) {
		if (sscanf(argv[i], "mem%lu", &id) != 1
				&& strcmp(argv[i], "all") != 0)
			continue;

		cxl_memdev_foreach (ctx, memdev) {
			if (!util_cxl_memdev_filter(memdev, argv[i]))
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

	fprintf(stderr, "wrote %d mem%s\n", count >= 0 ? count : 0,
			count > 1 ? "s" : "");
	return count >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_read_labels(int argc, const char **argv, struct cxl_ctx *ctx)
{
	int count = memdev_action(argc, argv, ctx, action_read, read_options,
			"cxl read-labels <mem0> [<mem1>..<memN>] [-o <filename>]");

	fprintf(stderr, "read %d mem%s\n", count >= 0 ? count : 0,
			count > 1 ? "s" : "");
	return count >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_zero_labels(int argc, const char **argv, struct cxl_ctx *ctx)
{
	int count = memdev_action(argc, argv, ctx, action_zero, zero_options,
			"cxl zero-labels <mem0> [<mem1>..<memN>] [<options>]");

	fprintf(stderr, "zeroed %d mem%s\n", count >= 0 ? count : 0,
			count > 1 ? "s" : "");
	return count >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_identify(int argc, const char **argv, struct cxl_ctx *ctx)
{
	int rc = memdev_action(argc, argv, ctx, action_cmd_identify, cmd_identify_options,
			"cxl id-cmd <mem0> [<mem1>..<memN>] [<options>]");

	return rc >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_get_supported_logs(int argc, const char **argv, struct cxl_ctx *ctx)
{
	int rc = memdev_action(argc, argv, ctx, action_cmd_get_supported_logs, cmd_get_supported_logs_options,
			"cxl get-supported-logs <mem0> [<mem1>..<memN>] [<options>]");

	return rc >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_get_cel_log(int argc, const char **argv, struct cxl_ctx *ctx)
{
	int rc = memdev_action(argc, argv, ctx, action_cmd_get_cel_log, cmd_get_cel_log_options,
			"cxl get-cel-log <mem0> [<mem1>..<memN>] [<options>]");

	return rc >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_get_event_interrupt_policy(int argc, const char **argv, struct cxl_ctx *ctx)
{
	int rc = memdev_action(argc, argv, ctx, action_cmd_get_event_interrupt_policy, cmd_get_event_interrupt_policy_options,
			"cxl get-event-interrupt-policy <mem0> [<mem1>..<memN>] [<options>]");

	return rc >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_set_event_interrupt_policy(int argc, const char **argv, struct cxl_ctx *ctx)
{
	int rc = memdev_action(argc, argv, ctx, action_cmd_set_event_interrupt_policy, cmd_set_event_interrupt_policy_options,
			"cxl set-event-interrupt-policy <mem0> [<mem1>..<memN>] [<options>]");

	return rc >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_get_timestamp(int argc, const char **argv, struct cxl_ctx *ctx)
{
	int rc = memdev_action(argc, argv, ctx, action_cmd_get_timestamp, cmd_get_timestamp_options,
			"cxl get-timestamp <mem0> [<mem1>..<memN>] [<options>]");

	return rc >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_set_timestamp(int argc, const char **argv, struct cxl_ctx *ctx)
{
	int rc = memdev_action(argc, argv, ctx, action_cmd_set_timestamp, cmd_set_timestamp_options,
			"cxl set-timestamp <mem0> [<mem1>..<memN>] [<options>]");

	return rc >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_get_alert_config(int argc, const char **argv, struct cxl_ctx *ctx)
{
	int rc = memdev_action(argc, argv, ctx, action_cmd_get_alert_config, cmd_get_alert_config_options,
			"cxl get-alert-config <mem0> [<mem1>..<memN>] [<options>]");

	return rc >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_set_alert_config(int argc, const char **argv, struct cxl_ctx *ctx)
{
	int rc = memdev_action(argc, argv, ctx, action_cmd_set_alert_config, cmd_set_alert_config_options,
			"cxl set-alert-config <mem0> [<mem1>..<memN>] [<options>]");

	return rc >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_get_health_info(int argc, const char **argv, struct cxl_ctx *ctx)
{
	int rc = memdev_action(argc, argv, ctx, action_cmd_get_health_info, cmd_get_health_info_options,
			"cxl get-health-info <mem0> [<mem1>..<memN>] [<options>]");

	return rc >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_get_event_records(int argc, const char **argv, struct cxl_ctx *ctx)
{
	int rc = memdev_action(argc, argv, ctx, action_cmd_get_event_records, cmd_get_event_records_options,
			"cxl get-event-records <mem0> [<mem1>..<memN>] [<options>]");

	return rc >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_get_ld_info(int argc, const char **argv, struct cxl_ctx *ctx)
{
	int rc = memdev_action(argc, argv, ctx, action_cmd_get_ld_info, cmd_get_ld_info_options,
			"cxl get-ld-info <mem0> [<mem1>..<memN>] [<options>]");

	return rc >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_ddr_info(int argc, const char **argv, struct cxl_ctx *ctx)
{
	int rc = memdev_action(argc, argv, ctx, action_cmd_ddr_info, cmd_ddr_info_options,
			"cxl ddr-info <mem0> [<mem1>..<memN>] [-i <ddr_instance_id>]");

	return rc >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_clear_event_records(int argc, const char **argv, struct cxl_ctx *ctx)
{
	int rc = memdev_action(argc, argv, ctx, action_cmd_clear_event_records, cmd_clear_event_records_options,
			"cxl clear-event-records <mem0> [<mem1>..<memN>] [<options>]");

	return rc >= 0 ? 0 : EXIT_FAILURE;
}

/* insert here cmd */
