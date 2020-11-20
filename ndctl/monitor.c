// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2018, FUJITSU LIMITED. All rights reserved.

#include <stdio.h>
#include <json-c/json.h>
#include <libgen.h>
#include <time.h>
#include <dirent.h>
#include <util/json.h>
#include <util/filter.h>
#include <util/util.h>
#include <util/parse-options.h>
#include <util/strbuf.h>
#include <ndctl/config.h>
#include <ndctl/ndctl.h>
#include <ndctl/libndctl.h>
#include <sys/epoll.h>
#define BUF_SIZE 2048

/* reuse the core log helpers for the monitor logger */
#ifndef ENABLE_LOGGING
#define ENABLE_LOGGING
#endif
#ifndef ENABLE_DEBUG
#define ENABLE_DEBUG
#endif
#include <util/log.h>

static struct monitor {
	const char *log;
	const char *config_file;
	const char *dimm_event;
	FILE *log_file;
	bool daemon;
	bool human;
	bool verbose;
	unsigned int poll_timeout;
	unsigned int event_flags;
	struct log_ctx ctx;
} monitor;

struct monitor_dimm {
	struct ndctl_dimm *dimm;
	int health_eventfd;
	unsigned int health;
	unsigned int event_flags;
	struct list_node list;
};

static struct util_filter_params param;

static int did_fail;

#define fail(fmt, ...) \
do { \
	did_fail = 1; \
	dbg(&monitor, "ndctl-%s:%s:%d: " fmt, \
			VERSION, __func__, __LINE__, ##__VA_ARGS__); \
} while (0)

static void log_syslog(struct log_ctx *ctx, int priority, const char *file,
		int line, const char *fn, const char *format, va_list args)
{
	vsyslog(priority, format, args);
}

static void log_standard(struct log_ctx *ctx, int priority, const char *file,
		int line, const char *fn, const char *format, va_list args)
{
	if (priority == 6)
		vfprintf(stdout, format, args);
	else
		vfprintf(stderr, format, args);
}

static void log_file(struct log_ctx *ctx, int priority, const char *file,
		int line, const char *fn, const char *format, va_list args)
{
	FILE *f = monitor.log_file;

	if (priority != LOG_NOTICE) {
		struct timespec ts;

		clock_gettime(CLOCK_REALTIME, &ts);
		fprintf(f, "[%10ld.%09ld] [%d] ", ts.tv_sec, ts.tv_nsec, getpid());
		vfprintf(f, format, args);
	} else
		vfprintf(f, format, args);

	fflush(f);
}

static struct json_object *dimm_event_to_json(struct monitor_dimm *mdimm)
{
	struct json_object *jevent, *jobj;
	bool spares_flag, media_temp_flag, ctrl_temp_flag,
			health_state_flag, unclean_shutdown_flag;

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

	jmsg = json_object_new_object();
	if (!jmsg) {
		fail("\n");
		return -ENOMEM;
	}

	clock_gettime(CLOCK_REALTIME, &ts);
	sprintf(timestamp, "%10ld.%09ld", ts.tv_sec, ts.tv_nsec);
	jobj = json_object_new_string(timestamp);
	if (jobj)
		json_object_object_add(jmsg, "timestamp", jobj);

	jobj = json_object_new_int(getpid());
	if (jobj)
		json_object_object_add(jmsg, "pid", jobj);

	jobj = dimm_event_to_json(mdimm);
	if (jobj)
		json_object_object_add(jmsg, "event", jobj);

	jdimm = util_dimm_to_json(mdimm->dimm, 0);
	if (jdimm)
		json_object_object_add(jmsg, "dimm", jdimm);

	jobj = util_dimm_health_to_json(mdimm->dimm);
	if (jobj)
		json_object_object_add(jdimm, "health", jobj);

	if (monitor.human)
		notice(&monitor, "%s\n", json_object_to_json_string_ext(jmsg,
						JSON_C_TO_STRING_PRETTY));
	else
		notice(&monitor, "%s\n", json_object_to_json_string_ext(jmsg,
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

	st_cmd = ndctl_dimm_cmd_new_smart_threshold(dimm);
	if (!st_cmd) {
		err(&monitor, "%s: no smart threshold command support\n", name);
		goto out;
	}
	if (ndctl_cmd_submit_xlat(st_cmd) < 0) {
		err(&monitor, "%s: smart threshold command failed\n", name);
		goto out;
	}

	sst_cmd = ndctl_dimm_cmd_new_smart_set_threshold(st_cmd);
	if (!sst_cmd) {
		err(&monitor, "%s: no smart set threshold command support\n", name);
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

	rc = ndctl_cmd_submit_xlat(sst_cmd);
	if (rc < 0) {
		err(&monitor, "%s: smart set threshold command failed\n", name);
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
	const char *name = ndctl_dimm_get_devname(dimm);

	if (!ndctl_dimm_is_cmd_supported(dimm, ND_CMD_SMART)) {
		err(&monitor, "%s: no smart support\n", name);
		return;
	}

	if (!ndctl_dimm_is_cmd_supported(dimm, ND_CMD_SMART_THRESHOLD)) {
		dbg(&monitor, "%s: no smart threshold support\n", name);
	} else if (!ndctl_dimm_is_flag_supported(dimm, ND_SMART_ALARM_VALID)) {
		err(&monitor, "%s: smart alarm invalid\n", name);
		return;
	} else if (enable_dimm_supported_threshold_alarms(dimm)) {
		err(&monitor, "%s: enable supported threshold alarms failed\n", name);
		return;
	}

	mdimm = calloc(1, sizeof(struct monitor_dimm));
	if (!mdimm) {
		err(&monitor, "%s: calloc for monitor dimm failed\n", name);
		return;
	}

	mdimm->dimm = dimm;
	mdimm->health_eventfd = ndctl_dimm_get_health_eventfd(dimm);
	mdimm->health = ndctl_dimm_get_health(dimm);
	mdimm->event_flags = ndctl_dimm_get_event_flags(dimm);

	if (mdimm->event_flags
			&& util_dimm_event_filter(mdimm, monitor.event_flags)) {
		if (notify_dimm_event(mdimm)) {
			err(&monitor, "%s: notify dimm event failed\n", name);
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
	int nfds, epollfd, i, rc = 0, polltimeout = -1;
	struct monitor_dimm *mdimm;
	char buf;
	/* last time a full poll happened */
	struct timespec fullpoll_ts, ts;

	if (monitor.poll_timeout)
		polltimeout = monitor.poll_timeout * 1000;

	events = calloc(mfa->num_dimm, sizeof(struct epoll_event));
	if (!events) {
		err(&monitor, "malloc for events error\n");
		return -ENOMEM;
	}
	epollfd = epoll_create1(0);
	if (epollfd == -1) {
		err(&monitor, "epoll_create1 error\n");
		rc = -errno;
		goto out;
	}
	list_for_each(&mfa->dimms, mdimm, list) {
		memset(&ev, 0, sizeof(ev));
		rc = pread(mdimm->health_eventfd, &buf, sizeof(buf), 0);
		if (rc < 0) {
			err(&monitor, "pread error\n");
			rc = -errno;
			goto out;
		}
		ev.data.ptr = mdimm;
		if (epoll_ctl(epollfd, EPOLL_CTL_ADD,
				mdimm->health_eventfd, &ev) != 0) {
			err(&monitor, "epoll_ctl error\n");
			rc = -errno;
			goto out;
		}
	}

	clock_gettime(CLOCK_BOOTTIME, &fullpoll_ts);
	while (1) {
		did_fail = 0;
		nfds = epoll_wait(epollfd, events, mfa->num_dimm, polltimeout);
		if (nfds < 0 && errno != EINTR) {
			err(&monitor, "epoll_wait error: (%s)\n", strerror(errno));
			rc = -errno;
			goto out;
		}

		/* If needed force a full poll of dimm health */
		clock_gettime(CLOCK_BOOTTIME, &ts);
		if ((fullpoll_ts.tv_sec - ts.tv_sec) > monitor.poll_timeout) {
			nfds = 0;
			dbg(&monitor, "forcing a full poll\n");
		}

		/* If we timed out then fill events array with all dimms */
		if (nfds == 0) {
			list_for_each(&mfa->dimms, mdimm, list)
				events[nfds++].data.ptr = mdimm;
			fullpoll_ts = ts;
		}

		for (i = 0; i < nfds; i++) {
			mdimm = events[i].data.ptr;
			if (util_dimm_event_filter(mdimm, monitor.event_flags)) {
				rc = notify_dimm_event(mdimm);
				if (rc) {
					err(&monitor, "%s: notify dimm event failed\n",
						ndctl_dimm_get_devname(mdimm->dimm));
					did_fail = 1;
					goto out;
				}
			}
			rc = pread(mdimm->health_eventfd, &buf, sizeof(buf), 0);
			if (rc < 0) {
				err(&monitor, "pread error\n");
				rc = -errno;
				goto out;
			}
		}
		if (did_fail)
			return 1;
	}
 out:
	free(events);
	return rc;
}

static void monitor_enable_all_events(struct monitor *_monitor)
{
	_monitor->event_flags = ND_EVENT_SPARES_REMAINING
			| ND_EVENT_MEDIA_TEMPERATURE
			| ND_EVENT_CTRL_TEMPERATURE
			| ND_EVENT_HEALTH_STATE
			| ND_EVENT_UNCLEAN_SHUTDOWN;
}

static int parse_monitor_event(struct monitor *_monitor, struct ndctl_ctx *ctx)
{
	char *dimm_event, *save;
	const char *event;
	int rc = 0;

	if (!_monitor->dimm_event) {
		monitor_enable_all_events(_monitor);
		return 0;;
	}

	dimm_event = strdup(_monitor->dimm_event);
	if (!dimm_event)
		return -ENOMEM;

	for (event = strtok_r(dimm_event, " ", &save); event;
			event = strtok_r(NULL, " ", &save)) {
		if (strcmp(event, "all") == 0) {
			monitor_enable_all_events(_monitor);
			goto out;
		}
		if (strcmp(event, "dimm-spares-remaining") == 0)
			_monitor->event_flags |= ND_EVENT_SPARES_REMAINING;
		else if (strcmp(event, "dimm-media-temperature") == 0)
			_monitor->event_flags |= ND_EVENT_MEDIA_TEMPERATURE;
		else if (strcmp(event, "dimm-controller-temperature") == 0)
			_monitor->event_flags |= ND_EVENT_CTRL_TEMPERATURE;
		else if (strcmp(event, "dimm-health-state") == 0)
			_monitor->event_flags |= ND_EVENT_HEALTH_STATE;
		else if (strcmp(event, "dimm-unclean-shutdown") == 0)
			_monitor->event_flags |= ND_EVENT_UNCLEAN_SHUTDOWN;
		else {
			err(&monitor, "no dimm-event named %s\n", event);
			rc = -EINVAL;
			goto out;
		}
	}

out:
	free(dimm_event);
	return rc;
}

static void parse_config(const char **arg, char *key, char *val, char *ident)
{
	struct strbuf value = STRBUF_INIT;
	size_t arg_len = *arg ? strlen(*arg) : 0;

	if (!ident || !key || (strcmp(ident, key) != 0))
		return;

	if (arg_len) {
		strbuf_add(&value, *arg, arg_len);
		strbuf_addstr(&value, " ");
	}
	strbuf_addstr(&value, val);
	*arg = strbuf_detach(&value, NULL);
}

static int read_config_file(struct ndctl_ctx *ctx, struct monitor *_monitor,
		struct util_filter_params *_param)
{
	FILE *f;
	size_t len = 0;
	int line = 0, rc = 0;
	char *buf = NULL, *seek, *value, *config_file;

	if (_monitor->config_file)
		config_file = strdup(_monitor->config_file);
	else
		config_file = strdup(NDCTL_CONF_FILE);
	if (!config_file) {
		fail("strdup default config file failed\n");
		rc = -ENOMEM;
		goto out;
	}

	buf = malloc(BUF_SIZE);
	if (!buf) {
		fail("malloc read config-file buf error\n");
		rc = -ENOMEM;
		goto out;
	}
	seek = buf;

	f = fopen(config_file, "r");
	if (!f) {
		if (_monitor->config_file) {
			err(&monitor, "config-file: %s cannot be opened\n",
				config_file);
			rc = -errno;
		}
		goto out;
	}

	while (fgets(seek, BUF_SIZE, f)) {
		value = NULL;
		line++;

		while (isspace(*seek))
			seek++;

		if (*seek == '#' || *seek == '\0')
			continue;

		value = strchr(seek, '=');
		if (!value) {
			fail("config-file syntax error, skip line[%i]\n", line);
			continue;
		}

		value[0] = '\0';
		value++;

		while (isspace(value[0]))
			value++;

		len = strlen(seek);
		if (len == 0)
			continue;
		while (isspace(seek[len-1]))
			len--;
		seek[len] = '\0';

		len = strlen(value);
		if (len == 0)
			continue;
		while (isspace(value[len-1]))
			len--;
		value[len] = '\0';

		if (len == 0)
			continue;

		parse_config(&_param->bus, "bus", value, seek);
		parse_config(&_param->dimm, "dimm", value, seek);
		parse_config(&_param->region, "region", value, seek);
		parse_config(&_param->namespace, "namespace", value, seek);
		parse_config(&_monitor->dimm_event, "dimm-event", value, seek);

		if (!_monitor->log)
			parse_config(&_monitor->log, "log", value, seek);
	}
	fclose(f);
out:
	free(buf);
	free(config_file);
	return rc;
}

int cmd_monitor(int argc, const char **argv, struct ndctl_ctx *ctx)
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
		OPT_FILENAME('c', "config-file", &monitor.config_file,
				"config-file", "override the default config"),
		OPT_BOOLEAN('\0', "daemon", &monitor.daemon,
				"run ndctl monitor as a daemon"),
		OPT_BOOLEAN('u', "human", &monitor.human,
				"use human friendly output formats"),
		OPT_BOOLEAN('v', "verbose", &monitor.verbose,
				"emit extra debug messages to log"),
		OPT_UINTEGER('p', "poll", &monitor.poll_timeout,
			     "poll and report events/status every <n> seconds"),
		OPT_END(),
	};
	const char * const u[] = {
		"ndctl monitor [<options>]",
		NULL
	};
	const char *prefix = "./";
	struct util_filter_ctx fctx = { 0 };
	struct monitor_filter_arg mfa = { 0 };
	int i, rc;

	argc = parse_options_prefix(argc, argv, prefix, options, u, 0);
	for (i = 0; i < argc; i++) {
		error("unknown parameter \"%s\"\n", argv[i]);
	}
	if (argc)
		usage_with_options(u, options);

	log_init(&monitor.ctx, "ndctl/monitor", "NDCTL_MONITOR_LOG");
	monitor.ctx.log_fn = log_standard;

	if (monitor.verbose)
		monitor.ctx.log_priority = LOG_DEBUG;
	else
		monitor.ctx.log_priority = LOG_INFO;

	rc = read_config_file(ctx, &monitor, &param);
	if (rc)
		goto out;

	if (monitor.log) {
		if (strncmp(monitor.log, "./", 2) != 0)
			fix_filename(prefix, (const char **)&monitor.log);
		if (strncmp(monitor.log, "./syslog", 8) == 0)
			monitor.ctx.log_fn = log_syslog;
		else if (strncmp(monitor.log, "./standard", 10) == 0)
			monitor.ctx.log_fn = log_standard;
		else {
			monitor.log_file = fopen(monitor.log, "a+");
			if (!monitor.log_file) {
				error("open %s failed\n", monitor.log);
				rc = -errno;
				goto out;
			}
			monitor.ctx.log_fn = log_file;
		}
	}

	if (monitor.daemon) {
		if (!monitor.log || strncmp(monitor.log, "./", 2) == 0)
			monitor.ctx.log_fn = log_syslog;
		if (daemon(0, 0) != 0) {
			err(&monitor, "daemon start failed\n");
			goto out;
		}
		info(&monitor, "ndctl monitor daemon started\n");
	}

	if (parse_monitor_event(&monitor, ctx))
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

	rc = util_filter_walk(ctx, &fctx, &param);
	if (rc)
		goto out;

	if (!mfa.num_dimm) {
		info(&monitor, "no dimms to monitor, exiting\n");
		if (!monitor.daemon)
			rc = -ENXIO;
		goto out;
	}

	rc = monitor_event(ctx, &mfa);
out:
	if (monitor.log_file)
		fclose(monitor.log_file);
	return rc;
}
