// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2022, Intel Corp. All rights reserved.
/* Some bits copied from ndctl monitor code */
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <event-parse.h>
#include <json-c/json.h>
#include <libgen.h>
#include <time.h>
#include <dirent.h>
#include <ccan/list/list.h>
#include <util/json.h>
#include <util/util.h>
#include <util/parse-options.h>
#include <util/parse-configs.h>
#include <util/strbuf.h>
#include <sys/epoll.h>
#include <sys/stat.h>
#include <tracefs.h>
#include <cxl/libcxl.h>

/* reuse the core log helpers for the monitor logger */
#ifndef ENABLE_LOGGING
#define ENABLE_LOGGING
#endif
#ifndef ENABLE_DEBUG
#define ENABLE_DEBUG
#endif
#include <util/log.h>

#include "event_trace.h"

static const char *cxl_system = "cxl";
const char *default_log = "/var/log/cxl-monitor.log";

static struct monitor {
	const char *log;
	struct log_ctx ctx;
	FILE *log_file;
	bool human;
	bool verbose;
	bool daemon;
} monitor;

static int monitor_event(struct cxl_ctx *ctx)
{
	int fd, epollfd, rc = 0, timeout = -1;
	struct epoll_event ev, *events;
	struct tracefs_instance *inst;
	struct event_ctx ectx;
	int jflag;

	events = calloc(1, sizeof(struct epoll_event));
	if (!events) {
		err(&monitor, "alloc for events error\n");
		return -ENOMEM;
	}

	epollfd = epoll_create1(0);
	if (epollfd == -1) {
		rc = -errno;
		err(&monitor, "epoll_create1() error: %d\n", rc);
		goto epoll_err;
	}

	inst = tracefs_instance_create("cxl_monitor");
	if (!inst) {
		rc = -errno;
		err(&monitor, "tracefs_instance_create( failed: %d\n", rc);
		goto inst_err;
	}

	fd = tracefs_instance_file_open(inst, "trace_pipe", -1);
	if (fd < 0) {
		rc = fd;
		err(&monitor, "tracefs_instance_file_open() err: %d\n", rc);
		goto inst_file_err;
	}

	memset(&ev, 0, sizeof(ev));
	ev.events = EPOLLIN;
	ev.data.fd = fd;

	if (epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &ev) != 0) {
		rc = -errno;
		err(&monitor, "epoll_ctl() error: %d\n", rc);
		goto epoll_ctl_err;
	}

	rc = cxl_event_tracing_enable(inst, cxl_system, NULL);
	if (rc < 0) {
		err(&monitor, "cxl_trace_event_enable() failed: %d\n", rc);
		goto event_en_err;
	}

	memset(&ectx, 0, sizeof(ectx));
	ectx.system = cxl_system;
	if (monitor.human)
		jflag = JSON_C_TO_STRING_PRETTY;
	else
		jflag = JSON_C_TO_STRING_PLAIN;

	while (1) {
		struct jlist_node *jnode, *next;

		rc = epoll_wait(epollfd, events, 1, timeout);
		if (rc < 0) {
			rc = -errno;
			if (errno != EINTR)
				err(&monitor, "epoll_wait error: %d\n", -errno);
			break;
		}

		list_head_init(&ectx.jlist_head);
		rc = cxl_parse_events(inst, &ectx);
		if (rc < 0)
			goto parse_err;

		if (list_empty(&ectx.jlist_head))
			continue;

		list_for_each_safe(&ectx.jlist_head, jnode, next, list) {
			notice(&monitor, "%s\n",
				json_object_to_json_string_ext(jnode->jobj, jflag));
			list_del(&jnode->list);
			json_object_put(jnode->jobj);
			free(jnode);
		}
	}

parse_err:
	if (cxl_event_tracing_disable(inst) < 0)
		err(&monitor, "failed to disable tracing\n");
event_en_err:
epoll_ctl_err:
	close(fd);
inst_file_err:
	tracefs_instance_free(inst);
inst_err:
	close(epollfd);
epoll_err:
	free(events);
	return rc;
}

int cmd_monitor(int argc, const char **argv, struct cxl_ctx *ctx)
{
	const struct option options[] = {
		OPT_FILENAME('l', "log", &monitor.log,
				"<file> | standard",
				"where to output the monitor's notification"),
		OPT_BOOLEAN('\0', "daemon", &monitor.daemon,
				"run cxl monitor as a daemon"),
		OPT_BOOLEAN('u', "human", &monitor.human,
				"use human friendly output formats"),
		OPT_BOOLEAN('v', "verbose", &monitor.verbose,
				"emit extra debug messages to log"),
		OPT_END(),
	};
	const char * const u[] = {
		"cxl monitor [<options>]",
		NULL
	};
	const char *prefix ="./";
	int rc = 0, i;
	const char *log;

	argc = parse_options_prefix(argc, argv, prefix, options, u, 0);
	for (i = 0; i < argc; i++)
		error("unknown parameter \"%s\"\n", argv[i]);
	if (argc)
		usage_with_options(u, options);

	/* sanity check */
	if (monitor.daemon && monitor.log && !strncmp(monitor.log, "./", 2)) {
		error("relative path or 'standard' are not compatible with daemon mode\n");
		return -EINVAL;
	}

	log_init(&monitor.ctx, "cxl/monitor", "CXL_MONITOR_LOG");
	if (monitor.log)
		log = monitor.log;
	else
		log = monitor.daemon ? default_log : "./standard";

	if (monitor.verbose)
		monitor.ctx.log_priority = LOG_DEBUG;
	else
		monitor.ctx.log_priority = LOG_INFO;

	if (strncmp(log, "./standard", 10) == 0)
		monitor.ctx.log_fn = log_standard;
	else {
		monitor.log_file = fopen(log, "a+");
		if (!monitor.log_file) {
			rc = -errno;
			error("open %s failed: %d\n", log, rc);
			goto out;
		}
		monitor.ctx.log_fn = log_file;
	}

	if (monitor.daemon) {
		if (daemon(0, 0) != 0) {
			err(&monitor, "daemon start failed\n");
			goto out;
		}
		info(&monitor, "cxl monitor daemon started.\n");
	}

	rc = monitor_event(ctx);

out:
	if (monitor.log_file)
		fclose(monitor.log_file);
	return rc;
}
