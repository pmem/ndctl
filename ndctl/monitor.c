/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright(c) 2018, FUJITSU LIMITED. All rights reserved. */

#include <stdio.h>
#include <json-c/json.h>
#include <libgen.h>
#include <dirent.h>
#include <util/log.h>
#include <util/json.h>
#include <util/filter.h>
#include <util/util.h>
#include <util/parse-options.h>
#include <util/strbuf.h>
#include <ndctl/lib/private.h>
#include <ndctl/libndctl.h>
#include <sys/epoll.h>
#define BUF_SIZE 2048


static struct monitor {
	const char *log;
	const char *dimm_event;
	bool daemon;
	bool human;
	unsigned int event_flags;
} monitor;

struct monitor_dimm {
	struct ndctl_dimm *dimm;
	int health_eventfd;
	unsigned int health;
	unsigned int event_flags;
	struct list_node list;
};

struct util_filter_params param;

static int did_fail;

#define fail(fmt, ...) \
do { \
	did_fail = 1; \
	dbg(ctx, "ndctl-%s:%s:%d: " fmt, \
			VERSION, __func__, __LINE__, ##__VA_ARGS__); \
} while (0)

static void log_syslog(struct ndctl_ctx *ctx, int priority, const char *file,
		int line, const char *fn, const char *format, va_list args)
{
	char *buf;

	if (vasprintf(&buf, format, args) < 0) {
		fail("vasprintf error\n");
		return;
	}
	syslog(priority, "%s", buf);

	free(buf);
	return;
}

static void log_standard(struct ndctl_ctx *ctx, int priority, const char *file,
		int line, const char *fn, const char *format, va_list args)
{
	char *buf;

	if (vasprintf(&buf, format, args) < 0) {
		fail("vasprintf error\n");
		return;
	}

	if (priority == 6)
		fprintf(stdout, "%s", buf);
	else
		fprintf(stderr, "%s", buf);

	free(buf);
	return;
}

static void log_file(struct ndctl_ctx *ctx, int priority, const char *file,
		int line, const char *fn, const char *format, va_list args)
{
	FILE *f;
	char *buf;

	if (vasprintf(&buf, format, args) < 0) {
		fail("vasprintf error\n");
		return;
	}

	f = fopen(monitor.log, "a+");
	if (!f) {
		ndctl_set_log_fn(ctx, log_syslog);
		fail("open logfile %s failed\n%s", monitor.log, buf);
		goto end;
	}
	fprintf(f, "%s", buf);
	fflush(f);
	fclose(f);
end:
	free(buf);
	return;
}

static struct json_object *dimm_event_to_json(struct monitor_dimm *mdimm)
{
	struct json_object *jevent, *jobj;
	bool spares_flag, media_temp_flag, ctrl_temp_flag,
			health_state_flag, unclean_shutdown_flag;
	struct ndctl_ctx *ctx = ndctl_dimm_get_ctx(mdimm->dimm);

	jevent = json_object_new_object();
	if (!jevent) {
		fail("\n");
		return NULL;
	}

	if (monitor.event_flags & ND_EVENT_SPARES_REMAINING) {
		spares_flag = !!(mdimm->event_flags
				& ND_EVENT_SPARES_REMAINING);
		jobj = json_object_new_boolean(spares_flag);
		if (jobj)
			json_object_object_add(jevent,
				"dimm-spares-remaining", jobj);
	}

	if (monitor.event_flags & ND_EVENT_MEDIA_TEMPERATURE) {
		media_temp_flag = !!(mdimm->event_flags
				& ND_EVENT_MEDIA_TEMPERATURE);
		jobj = json_object_new_boolean(media_temp_flag);
		if (jobj)
			json_object_object_add(jevent,
				"dimm-media-temperature", jobj);
	}

	if (monitor.event_flags & ND_EVENT_CTRL_TEMPERATURE) {
		ctrl_temp_flag = !!(mdimm->event_flags
				& ND_EVENT_CTRL_TEMPERATURE);
		jobj = json_object_new_boolean(ctrl_temp_flag);
		if (jobj)
			json_object_object_add(jevent,
				"dimm-controller-temperature", jobj);
	}

	if (monitor.event_flags & ND_EVENT_HEALTH_STATE) {
		health_state_flag = !!(mdimm->event_flags
				& ND_EVENT_HEALTH_STATE);
		jobj = json_object_new_boolean(health_state_flag);
		if (jobj)
			json_object_object_add(jevent,
				"dimm-health-state", jobj);
	}

	if (monitor.event_flags & ND_EVENT_UNCLEAN_SHUTDOWN) {
		unclean_shutdown_flag = !!(mdimm->event_flags
				& ND_EVENT_UNCLEAN_SHUTDOWN);
		jobj = json_object_new_boolean(unclean_shutdown_flag);
		if (jobj)
			json_object_object_add(jevent,
				"dimm-unclean-shutdown", jobj);
	}

	return jevent;
}

static int notify_dimm_event(struct monitor_dimm *mdimm)
{
	struct json_object *jmsg, *jdimm, *jobj;
	struct timespec ts;
	char timestamp[32];
	struct ndctl_ctx *ctx = ndctl_dimm_get_ctx(mdimm->dimm);

	jmsg = json_object_new_object();
	if (!jmsg) {
		fail("\n");
		return -1;
	}

	clock_gettime(CLOCK_REALTIME, &ts);
	sprintf(timestamp, "%10ld.%09ld", ts.tv_sec, ts.tv_nsec);
	jobj = json_object_new_string(timestamp);
	if (!jobj) {
		fail("\n");
		return -1;
	}
	json_object_object_add(jmsg, "timestamp", jobj);

	jobj = json_object_new_int(getpid());
	if (!jobj) {
		fail("\n");
		return -1;
	}
	json_object_object_add(jmsg, "pid", jobj);

	jobj = dimm_event_to_json(mdimm);
	if (!jobj) {
		fail("\n");
		return -1;
	}
	json_object_object_add(jmsg, "event", jobj);

	jdimm = util_dimm_to_json(mdimm->dimm, 0);
	if (!jdimm) {
		fail("\n");
		return -1;
	}
	json_object_object_add(jmsg, "dimm", jdimm);

	jobj = util_dimm_health_to_json(mdimm->dimm);
	if (!jobj) {
		fail("\n");
		return -1;
	}
	json_object_object_add(jdimm, "health", jobj);

	if (monitor.human)
		notice(ctx, "%s\n", json_object_to_json_string_ext(jmsg,
						JSON_C_TO_STRING_PRETTY));
	else
		notice(ctx, "%s\n", json_object_to_json_string_ext(jmsg,
						JSON_C_TO_STRING_PLAIN));

	free(jobj);
	free(jdimm);
	free(jmsg);
	return 0;
}

static struct monitor_dimm *util_dimm_event_filter(struct monitor_dimm *mdimm,
		unsigned int event_flags)
{
	unsigned int health;

	mdimm->event_flags = ndctl_dimm_get_event_flags(mdimm->dimm);
	if (mdimm->event_flags == UINT_MAX)
		return NULL;

	health = ndctl_dimm_get_health(mdimm->dimm);
	if (health == UINT_MAX)
		return NULL;
	if (mdimm->health != health)
		mdimm->event_flags |= ND_EVENT_HEALTH_STATE;

	if (mdimm->event_flags & event_flags)
		return mdimm;
	return NULL;
}

static int enable_dimm_supported_threshold_alarms(struct ndctl_dimm *dimm)
{
	unsigned int alarm;
	int rc = -EOPNOTSUPP;
	struct ndctl_cmd *st_cmd = NULL, *sst_cmd = NULL;
	const char *name = ndctl_dimm_get_devname(dimm);
	struct ndctl_ctx *ctx = ndctl_dimm_get_ctx(dimm);

	st_cmd = ndctl_dimm_cmd_new_smart_threshold(dimm);
	if (!st_cmd) {
		err(ctx, "%s: no smart threshold command support\n", name);
		goto out;
	}
	if (ndctl_cmd_submit(st_cmd)) {
		err(ctx, "%s: smart threshold command failed\n", name);
		goto out;
	}

	sst_cmd = ndctl_dimm_cmd_new_smart_set_threshold(st_cmd);
	if (!sst_cmd) {
		err(ctx, "%s: no smart set threshold command support\n", name);
		goto out;
	}

	alarm = ndctl_cmd_smart_threshold_get_alarm_control(st_cmd);
	if (monitor.event_flags & ND_EVENT_SPARES_REMAINING)
		alarm |= ND_SMART_SPARE_TRIP;
	if (monitor.event_flags & ND_EVENT_MEDIA_TEMPERATURE)
		alarm |= ND_SMART_TEMP_TRIP;
	if (monitor.event_flags & ND_EVENT_CTRL_TEMPERATURE)
		alarm |= ND_SMART_CTEMP_TRIP;
	ndctl_cmd_smart_threshold_set_alarm_control(sst_cmd, alarm);

	rc = ndctl_cmd_submit(sst_cmd);
	if (rc) {
		err(ctx, "%s: smart set threshold command failed\n", name);
		goto out;
	}

out:
	ndctl_cmd_unref(sst_cmd);
	ndctl_cmd_unref(st_cmd);
	return rc;
}

static bool filter_region(struct ndctl_region *region,
		struct util_filter_ctx *fctx)
{
	return true;
}

static void filter_dimm(struct ndctl_dimm *dimm, struct util_filter_ctx *fctx)
{
	struct monitor_dimm *mdimm;
	struct monitor_filter_arg *mfa = fctx->monitor;
	struct ndctl_ctx *ctx = ndctl_dimm_get_ctx(dimm);
	const char *name = ndctl_dimm_get_devname(dimm);

	if (!ndctl_dimm_is_cmd_supported(dimm, ND_CMD_SMART)) {
		err(ctx, "%s: no smart support\n", name);
		return;
	}
	if (!ndctl_dimm_is_cmd_supported(dimm, ND_CMD_SMART_THRESHOLD)) {
		err(ctx, "%s: no smart threshold support\n", name);
		return;
	}

	if (!ndctl_dimm_is_flag_supported(dimm, ND_SMART_ALARM_VALID)) {
		err(ctx, "%s: smart alarm invalid\n", name);
		return;
	}

	if (enable_dimm_supported_threshold_alarms(dimm)) {
		err(ctx, "%s: enable supported threshold alarms failed\n", name);
		return;
	}

	mdimm = calloc(1, sizeof(struct monitor_dimm));
	if (!mdimm) {
		err(ctx, "%s: calloc for monitor dimm failed\n", name);
		return;
	}

	mdimm->dimm = dimm;
	mdimm->health_eventfd = ndctl_dimm_get_health_eventfd(dimm);
	mdimm->health = ndctl_dimm_get_health(dimm);
	mdimm->event_flags = ndctl_dimm_get_event_flags(dimm);

	if (mdimm->event_flags
			&& util_dimm_event_filter(mdimm, monitor.event_flags)) {
		if (notify_dimm_event(mdimm)) {
			err(ctx, "%s: notify dimm event failed\n", name);
			free(mdimm);
			return;
		}
	}

	list_add_tail(&mfa->dimms, &mdimm->list);
	if (mdimm->health_eventfd > mfa->maxfd_dimm)
		mfa->maxfd_dimm = mdimm->health_eventfd;
	mfa->num_dimm++;
	return;
}

static bool filter_bus(struct ndctl_bus *bus, struct util_filter_ctx *fctx)
{
	return true;
}

static int monitor_event(struct ndctl_ctx *ctx,
		struct monitor_filter_arg *mfa)
{
	struct epoll_event ev, *events;
	int nfds, epollfd, i, rc;
	struct monitor_dimm *mdimm;
	char buf;

	events = calloc(mfa->num_dimm, sizeof(struct epoll_event));
	if (!events) {
		err(ctx, "malloc for events error\n");
		return 1;
	}
	epollfd = epoll_create1(0);
	if (epollfd == -1) {
		err(ctx, "epoll_create1 error\n");
		return 1;
	}
	list_for_each(&mfa->dimms, mdimm, list) {
		memset(&ev, 0, sizeof(ev));
		rc = pread(mdimm->health_eventfd, &buf, sizeof(buf), 0);
		if (rc < 0) {
			err(ctx, "pread error\n");
			return 1;
		}
		ev.data.ptr = mdimm;
		if (epoll_ctl(epollfd, EPOLL_CTL_ADD,
				mdimm->health_eventfd, &ev) != 0) {
			err(ctx, "epoll_ctl error\n");
			return 1;
		}
	}

	while (1) {
		did_fail = 0;
		nfds = epoll_wait(epollfd, events, mfa->num_dimm, -1);
		if (nfds <= 0) {
			err(ctx, "epoll_wait error\n");
			return 1;
		}
		for (i = 0; i < nfds; i++) {
			mdimm = events[i].data.ptr;
			if (util_dimm_event_filter(mdimm, monitor.event_flags)) {
				if (notify_dimm_event(mdimm))
					fail("%s: notify dimm event failed\n",
						ndctl_dimm_get_devname(mdimm->dimm));
			}
			rc = pread(mdimm->health_eventfd, &buf, sizeof(buf), 0);
			if (rc < 0)
				fail("pread error\n");
		}
		if (did_fail)
			return 1;
	}
	return 0;
}

static int parse_monitor_event(struct monitor *_monitor)
{
	char *dimm_event, *save;
	const char *event;

	if (!_monitor->dimm_event)
		goto dimm_event_all;
	dimm_event = strdup(_monitor->dimm_event);
	if (!dimm_event)
		return 1;

	for (event = strtok_r(dimm_event, " ", &save); event;
			event = strtok_r(NULL, " ", &save)) {
		if (strcmp(event, "all") == 0) {
			free(dimm_event);
			goto dimm_event_all;
		}
		if (strcmp(event, "dimm-spares-remaining") == 0)
			_monitor->event_flags |= ND_EVENT_SPARES_REMAINING;
		if (strcmp(event, "dimm-media-temperature") == 0)
			_monitor->event_flags |= ND_EVENT_MEDIA_TEMPERATURE;
		if (strcmp(event, "dimm-controller-temperature") == 0)
			_monitor->event_flags |= ND_EVENT_CTRL_TEMPERATURE;
		if (strcmp(event, "dimm-health-state") == 0)
			_monitor->event_flags |= ND_EVENT_HEALTH_STATE;
		if (strcmp(event, "dimm-unclean-shutdown") == 0)
			_monitor->event_flags |= ND_EVENT_UNCLEAN_SHUTDOWN;
	}

	free(dimm_event);
	return 0;

dimm_event_all:
	_monitor->event_flags = ND_EVENT_SPARES_REMAINING
			| ND_EVENT_MEDIA_TEMPERATURE
			| ND_EVENT_CTRL_TEMPERATURE
			| ND_EVENT_HEALTH_STATE
			| ND_EVENT_UNCLEAN_SHUTDOWN;
	return 0;
}

int cmd_monitor(int argc, const char **argv, void *ctx)
{
	const struct option options[] = {
		OPT_STRING('b', "bus", &param.bus, "bus-id", "filter by bus"),
		OPT_STRING('r', "region", &param.region, "region-id",
				"filter by region"),
		OPT_STRING('d', "dimm", &param.dimm, "dimm-id",
				"filter by dimm"),
		OPT_STRING('n', "namespace", &param.namespace,
				"namespace-id", "filter by namespace id"),
		OPT_STRING('D', "dimm-event", &monitor.dimm_event,
			"name of event type", "filter by DIMM event type"),
		OPT_FILENAME('l', "log", &monitor.log,
				"<file> | syslog | standard",
				"where to output the monitor's notification"),
		OPT_BOOLEAN('x', "daemon", &monitor.daemon,
				"run ndctl monitor as a daemon"),
		OPT_BOOLEAN('u', "human", &monitor.human,
				"use human friendly output formats"),
		OPT_END(),
	};
	const char * const u[] = {
		"ndctl monitor [<options>]",
		NULL
	};
	const char *prefix = "./";
	struct util_filter_ctx fctx = { 0 };
	struct monitor_filter_arg mfa = { 0 };
	int i;

	argc = parse_options_prefix(argc, argv, prefix, options, u, 0);
	for (i = 0; i < argc; i++) {
		error("unknown parameter \"%s\"\n", argv[i]);
	}
	if (argc)
		usage_with_options(u, options);

	/* default to log_standard */
	ndctl_set_log_fn((struct ndctl_ctx *)ctx, log_standard);
	ndctl_set_log_priority((struct ndctl_ctx *)ctx, LOG_NOTICE);

	if (monitor.log) {
		if (strncmp(monitor.log, "./syslog", 8) == 0)
			ndctl_set_log_fn((struct ndctl_ctx *)ctx, log_syslog);
		else if (strncmp(monitor.log, "./standard", 10) == 0)
			; /*default, already set */
		else
			ndctl_set_log_fn((struct ndctl_ctx *)ctx, log_file);
	}

	if (monitor.daemon) {
		if (daemon(0, 0) != 0) {
			err((struct ndctl_ctx *)ctx, "daemon start failed\n");
			goto out;
		}
		notice((struct ndctl_ctx *)ctx, "ndctl monitor daemon started\n");
	}

	if (parse_monitor_event(&monitor))
		goto out;

	fctx.filter_bus = filter_bus;
	fctx.filter_dimm = filter_dimm;
	fctx.filter_region = filter_region;
	fctx.filter_namespace = NULL;
	fctx.arg = &mfa;
	list_head_init(&mfa.dimms);
	mfa.num_dimm = 0;
	mfa.maxfd_dimm = -1;
	mfa.flags = 0;

	if (util_filter_walk(ctx, &fctx, &param))
		goto out;

	if (!mfa.num_dimm) {
		err((struct ndctl_ctx *)ctx, "no dimms to monitor\n");
		goto out;
	}

	if (monitor_event(ctx, &mfa))
		goto out;

	return 0;
out:
	return 1;
}
