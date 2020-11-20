// SPDX-License-Identifier: GPL-2.0
/* Copyright (C) 2018-2020 Intel Corporation. All rights reserved. */
#include <math.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/ioctl.h>

#include <ndctl.h>
#include <util/log.h>
#include <util/size.h>
#include <util/json.h>
#include <json-c/json.h>
#include <util/filter.h>
#include <ndctl/libndctl.h>
#include <util/parse-options.h>
#include <ccan/array_size/array_size.h>
#include <ccan/short_types/short_types.h>

#include <builtin.h>
#include <test.h>

static struct parameters {
	const char *bus;
	const char *dimm;
	bool verbose;
	bool human;
	const char *media_temperature;
	const char *ctrl_temperature;
	const char *spares;
	const char *media_temperature_threshold;
	const char *ctrl_temperature_threshold;
	const char *spares_threshold;
	const char *media_temperature_alarm;
	const char *ctrl_temperature_alarm;
	const char *spares_alarm;
	bool fatal;
	bool unsafe_shutdown;
	bool media_temperature_uninject;
	bool ctrl_temperature_uninject;
	bool spares_uninject;
	bool fatal_uninject;
	bool unsafe_shutdown_uninject;
	bool uninject_all;
} param;

static struct smart_ctx {
	bool alarms_present;
	bool err_continue;
	unsigned long op_mask;
	unsigned long flags;
	unsigned int media_temperature;
	unsigned int ctrl_temperature;
	unsigned long spares;
	unsigned int media_temperature_threshold;
	unsigned int ctrl_temperature_threshold;
	unsigned long spares_threshold;
	unsigned int media_temperature_alarm;
	unsigned int ctrl_temperature_alarm;
	unsigned long spares_alarm;
} sctx;

#define SMART_OPTIONS() \
OPT_STRING('b', "bus", &param.bus, "bus-id", \
	"limit dimm to a bus with an id or provider of <bus-id>"), \
OPT_BOOLEAN('v', "verbose", &param.verbose, "emit extra debug messages to stderr"), \
OPT_BOOLEAN('u', "human", &param.human, "use human friendly number formats"), \
OPT_STRING('m', "media-temperature", &param.media_temperature, \
	"smart media temperature attribute", \
	"inject a value for smart media temperature"), \
OPT_STRING('M', "media-temperature-threshold", \
	&param.media_temperature_threshold, \
	"set smart media temperature threshold", \
	"set threshold value for smart media temperature"), \
OPT_STRING('\0', "media-temperature-alarm", &param.media_temperature_alarm, \
	"smart media temperature alarm", \
	"enable or disable the smart media temperature alarm"), \
OPT_BOOLEAN('\0', "media-temperature-uninject", \
	&param.media_temperature_uninject, "uninject media temperature"), \
OPT_STRING('c', "ctrl-temperature", &param.ctrl_temperature, \
	"smart controller temperature attribute", \
	"inject a value for smart controller temperature"), \
OPT_STRING('C', "ctrl-temperature-threshold", \
	&param.ctrl_temperature_threshold, \
	"set smart controller temperature threshold", \
	"set threshold value for smart controller temperature"), \
OPT_STRING('\0', "ctrl-temperature-alarm", &param.ctrl_temperature_alarm, \
	"smart controller temperature alarm", \
	"enable or disable the smart controller temperature alarm"), \
OPT_BOOLEAN('\0', "ctrl-temperature-uninject", \
	&param.ctrl_temperature_uninject, "uninject controller temperature"), \
OPT_STRING('s', "spares", &param.spares, \
	"smart spares attribute", \
	"inject a value for smart spares"), \
OPT_STRING('S', "spares-threshold", &param.spares_threshold, \
	"set smart spares threshold", \
	"set a threshold value for smart spares"), \
OPT_STRING('\0', "spares-alarm", &param.spares_alarm, \
	"smart spares alarm", \
	"enable or disable the smart spares alarm"), \
OPT_BOOLEAN('\0', "spares-uninject", \
	&param.spares_uninject, "uninject spare percentage"), \
OPT_BOOLEAN('f', "fatal", &param.fatal, "inject fatal smart health status"), \
OPT_BOOLEAN('F', "fatal-uninject", \
	&param.fatal_uninject, "uninject fatal health condition"), \
OPT_BOOLEAN('U', "unsafe-shutdown", &param.unsafe_shutdown, \
	"inject smart unsafe shutdown status"), \
OPT_BOOLEAN('\0', "unsafe-shutdown-uninject", \
	&param.unsafe_shutdown_uninject, "uninject unsafe shutdown status"), \
OPT_BOOLEAN('N', "uninject-all", \
	&param.uninject_all, "uninject all possible fields")


static const struct option smart_opts[] = {
	SMART_OPTIONS(),
	OPT_END(),
};

enum smart_ops {
	OP_SET = 0,
	OP_INJECT,
};

enum alarms {
	ALARM_ON = 1,
	ALARM_OFF,
};

static inline void enable_set(void)
{
	sctx.op_mask |= 1 << OP_SET;
}

static inline void enable_inject(void)
{
	sctx.op_mask |= 1 << OP_INJECT;
}

#define smart_param_setup_uint(arg) \
{ \
	if (param.arg) { \
		sctx.arg = strtoul(param.arg, NULL, 0); \
		if (sctx.arg == ULONG_MAX || sctx.arg > UINT_MAX) { \
			error("Invalid argument: %s: %s\n", #arg, param.arg); \
			return -EINVAL; \
		} \
		enable_inject(); \
	} \
	if (param.arg##_threshold) { \
		sctx.arg##_threshold = \
			strtoul(param.arg##_threshold, NULL, 0); \
		if (sctx.arg##_threshold == ULONG_MAX \
				|| sctx.arg##_threshold > UINT_MAX) { \
			error("Invalid argument: %s\n", \
				param.arg##_threshold); \
			return -EINVAL; \
		} \
		enable_set(); \
	} \
}

#define smart_param_setup_temps(arg) \
{ \
	double temp; \
	if (param.arg) { \
		temp = strtod(param.arg, NULL); \
		if (temp == HUGE_VAL || temp == -HUGE_VAL) { \
			error("Invalid argument: %s: %s\n", #arg, param.arg); \
			return -EINVAL; \
		} \
		sctx.arg = ndctl_encode_smart_temperature(temp); \
		enable_inject(); \
	} \
	if (param.arg##_threshold) { \
		temp = strtod(param.arg##_threshold, NULL); \
		if (temp == HUGE_VAL || temp == -HUGE_VAL) { \
			error("Invalid argument: %s\n", \
				param.arg##_threshold); \
			return -EINVAL; \
		} \
		sctx.arg##_threshold = ndctl_encode_smart_temperature(temp); \
		enable_set(); \
	} \
}

#define smart_param_setup_alarm(arg) \
{ \
	if (param.arg##_alarm) { \
		if (strncmp(param.arg##_alarm, "on", 2) == 0) \
			sctx.arg##_alarm = ALARM_ON; \
		else if (strncmp(param.arg##_alarm, "off", 3) == 0) \
			sctx.arg##_alarm = ALARM_OFF; \
		sctx.alarms_present = true; \
	} \
}

#define smart_param_setup_uninj(arg) \
{ \
	if (param.arg##_uninject) { \
		/* Ensure user didn't set inject and uninject together */ \
		if (param.arg) { \
			error("Cannot use %s inject and uninject together\n", \
				#arg); \
			return -EINVAL; \
		} \
		/* Then set the inject flag so this can be accounted for */ \
		param.arg = "0"; \
		enable_inject(); \
	} \
}

static int smart_init(void)
{
	if (param.human)
		sctx.flags |= UTIL_JSON_HUMAN;
	sctx.err_continue = false;

	/* setup attributes and thresholds except alarm_control */
	smart_param_setup_temps(media_temperature)
	smart_param_setup_temps(ctrl_temperature)
	smart_param_setup_uint(spares)

	/* set up alarm_control */
	smart_param_setup_alarm(media_temperature)
	smart_param_setup_alarm(ctrl_temperature)
	smart_param_setup_alarm(spares)
	if (sctx.alarms_present)
		enable_set();

	/* setup remaining injection attributes */
	if (param.fatal || param.unsafe_shutdown)
		enable_inject();

	/* setup uninjections */
	if (param.uninject_all) {
		param.media_temperature_uninject = true;
		param.ctrl_temperature_uninject = true;
		param.spares_uninject = true;
		param.fatal_uninject = true;
		param.unsafe_shutdown_uninject = true;
		sctx.err_continue = true;
	}
	smart_param_setup_uninj(media_temperature)
	smart_param_setup_uninj(ctrl_temperature)
	smart_param_setup_uninj(spares)
	smart_param_setup_uninj(fatal)
	smart_param_setup_uninj(unsafe_shutdown)

	if (sctx.op_mask == 0) {
		error("No valid operation specified\n");
		return -EINVAL;
	}

	return 0;
}

#define setup_thresh_field(arg) \
{ \
	if (param.arg##_threshold) \
		ndctl_cmd_smart_threshold_set_##arg(sst_cmd, \
					sctx.arg##_threshold); \
}

static int smart_set_thresh(struct ndctl_dimm *dimm)
{
	const char *name = ndctl_dimm_get_devname(dimm);
	struct ndctl_cmd *st_cmd = NULL, *sst_cmd = NULL;
	int rc = -EOPNOTSUPP;

	st_cmd = ndctl_dimm_cmd_new_smart_threshold(dimm);
	if (!st_cmd) {
		error("%s: no smart threshold command support\n", name);
		goto out;
	}

	rc = ndctl_cmd_submit_xlat(st_cmd);
	if (rc < 0) {
		error("%s: smart threshold command failed: %s (%d)\n",
			name, strerror(abs(rc)), rc);
		goto out;
	}

	sst_cmd = ndctl_dimm_cmd_new_smart_set_threshold(st_cmd);
	if (!sst_cmd) {
		error("%s: no smart set threshold command support\n", name);
		rc = -EOPNOTSUPP;
		goto out;
	}

	/* setup all thresholds except alarm_control */
	setup_thresh_field(media_temperature)
	setup_thresh_field(ctrl_temperature)
	setup_thresh_field(spares)

	/* setup alarm_control manually */
	if (sctx.alarms_present) {
		unsigned int alarm;

		alarm = ndctl_cmd_smart_threshold_get_alarm_control(st_cmd);
		if (sctx.media_temperature_alarm == ALARM_ON)
			alarm |= ND_SMART_TEMP_TRIP;
		else if (sctx.media_temperature_alarm == ALARM_OFF)
			alarm &= ~ND_SMART_TEMP_TRIP;
		if (sctx.ctrl_temperature_alarm == ALARM_ON)
			alarm |= ND_SMART_CTEMP_TRIP;
		else if (sctx.ctrl_temperature_alarm == ALARM_OFF)
			alarm &= ~ND_SMART_CTEMP_TRIP;
		if (sctx.spares_alarm == ALARM_ON)
			alarm |= ND_SMART_SPARE_TRIP;
		else if (sctx.spares_alarm == ALARM_OFF)
			alarm &= ~ND_SMART_SPARE_TRIP;

		ndctl_cmd_smart_threshold_set_alarm_control(sst_cmd, alarm);
	}

	rc = ndctl_cmd_submit_xlat(sst_cmd);
	if (rc < 0)
		error("%s: smart set threshold command failed: %s (%d)\n",
			name, strerror(abs(rc)), rc);

out:
	ndctl_cmd_unref(sst_cmd);
	ndctl_cmd_unref(st_cmd);
	return rc;
}

#define send_inject_val(arg) \
{ \
	if (param.arg) { \
		bool enable = true; \
		\
		si_cmd = ndctl_dimm_cmd_new_smart_inject(dimm); \
		if (!si_cmd) { \
			error("%s: no smart inject command support\n", name); \
			if (sctx.err_continue == false) \
				goto out; \
		} \
		if (param.arg##_uninject) \
			enable = false; \
		rc = ndctl_cmd_smart_inject_##arg(si_cmd, enable, sctx.arg); \
		if (rc) { \
			error("%s: smart inject %s cmd invalid: %s (%d)\n", \
				name, #arg, strerror(abs(rc)), rc); \
			if (sctx.err_continue == false) \
				goto out; \
		} \
		rc = ndctl_cmd_submit_xlat(si_cmd); \
		if (rc < 0) { \
			error("%s: smart inject %s command failed: %s (%d)\n", \
				name, #arg, strerror(abs(rc)), rc); \
			if (sctx.err_continue == false) \
				goto out; \
		} \
		ndctl_cmd_unref(si_cmd); \
	} \
}

#define send_inject_bool(arg) \
{ \
	if (param.arg) { \
		bool enable = true; \
		\
		si_cmd = ndctl_dimm_cmd_new_smart_inject(dimm); \
		if (!si_cmd) { \
			error("%s: no smart inject command support\n", name); \
			if (sctx.err_continue == false) \
				goto out; \
		} \
		if (param.arg##_uninject) \
			enable = false; \
		rc = ndctl_cmd_smart_inject_##arg(si_cmd, enable); \
		if (rc) { \
			error("%s: smart inject %s cmd invalid: %s (%d)\n", \
				name, #arg, strerror(abs(rc)), rc); \
			if (sctx.err_continue == false) \
				goto out; \
		} \
		rc = ndctl_cmd_submit_xlat(si_cmd); \
		if (rc < 0) { \
			error("%s: smart inject %s command failed: %s (%d)\n", \
				name, #arg, strerror(abs(rc)), rc); \
			if (sctx.err_continue == false) \
				goto out; \
		} \
		ndctl_cmd_unref(si_cmd); \
	} \
}

static int smart_inject(struct ndctl_dimm *dimm)
{
	const char *name = ndctl_dimm_get_devname(dimm);
	struct ndctl_cmd *si_cmd = NULL;
	int rc = -EOPNOTSUPP;

	send_inject_val(media_temperature)
	send_inject_val(ctrl_temperature)
	send_inject_val(spares)
	send_inject_bool(fatal)
	send_inject_bool(unsafe_shutdown)

out:
	ndctl_cmd_unref(si_cmd);
	return rc;
}

static int dimm_inject_smart(struct ndctl_dimm *dimm)
{
	struct json_object *jhealth;
	struct json_object *jdimms;
	struct json_object *jdimm;
	int rc;

	rc = ndctl_dimm_smart_inject_supported(dimm);
	switch (rc) {
	case -ENOTTY:
		error("%s: smart injection not supported by ndctl.",
			ndctl_dimm_get_devname(dimm));
		return rc;
	case -EOPNOTSUPP:
		error("%s: smart injection not supported by the kernel",
			ndctl_dimm_get_devname(dimm));
		return rc;
	case -EIO:
		error("%s: smart injection not supported by either platform firmware or the kernel.",
			ndctl_dimm_get_devname(dimm));
		return rc;
	}

	if (sctx.op_mask & (1 << OP_SET)) {
		rc = smart_set_thresh(dimm);
		if (rc)
			goto out;
	}
	if (sctx.op_mask & (1 << OP_INJECT)) {
		rc = smart_inject(dimm);
		if (rc)
			goto out;
	}

	if (rc == 0) {
		jdimms = json_object_new_array();
		if (!jdimms)
			goto out;
		jdimm = util_dimm_to_json(dimm, sctx.flags);
		if (!jdimm)
			goto out;
		json_object_array_add(jdimms, jdimm);

		jhealth = util_dimm_health_to_json(dimm);
		if (jhealth) {
			json_object_object_add(jdimm, "health", jhealth);
			util_display_json_array(stdout, jdimms, sctx.flags);
		}
	}
out:
	return rc;
}

static int do_smart(const char *dimm_arg, struct ndctl_ctx *ctx)
{
	struct ndctl_dimm *dimm;
	struct ndctl_bus *bus;
	int rc = -ENXIO;

	if (dimm_arg == NULL)
		return rc;

	if (param.verbose)
		ndctl_set_log_priority(ctx, LOG_DEBUG);

        ndctl_bus_foreach(ctx, bus) {
		if (!util_bus_filter(bus, param.bus))
			continue;

		ndctl_dimm_foreach(bus, dimm) {
			if (!util_dimm_filter(dimm, dimm_arg))
				continue;
			return dimm_inject_smart(dimm);
		}
	}
	error("%s: no such dimm\n", dimm_arg);

	return rc;
}

int cmd_inject_smart(int argc, const char **argv, struct ndctl_ctx *ctx)
{
	const char * const u[] = {
		"ndctl inject-smart <dimm> [<options>]",
		NULL
	};
	int i, rc;

        argc = parse_options(argc, argv, smart_opts, u, 0);
	rc = smart_init();
	if (rc)
		return rc;

	if (argc == 0)
		error("specify a dimm for the smart operation\n");
	for (i = 1; i < argc; i++)
		error("unknown extra parameter \"%s\"\n", argv[i]);
	if (argc == 0 || argc > 1) {
		usage_with_options(u, smart_opts);
		return -ENODEV; /* we won't return from usage_with_options() */
	}

	return do_smart(argv[0], ctx);
}
