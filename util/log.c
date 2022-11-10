// SPDX-License-Identifier: LGPL-2.1
// Copyright (C) 2016-2020, Intel Corporation. All rights reserved.
#include <syslog.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <util/log.h>

void log_syslog(struct log_ctx *ctx, int priority, const char *file, int line,
		const char *fn, const char *format, va_list args)
{
	vsyslog(priority, format, args);
}

void log_standard(struct log_ctx *ctx, int priority, const char *file, int line,
		  const char *fn, const char *format, va_list args)
{
	if (priority == 6)
		vfprintf(stdout, format, args);
	else
		vfprintf(stderr, format, args);
}

void log_file(struct log_ctx *ctx, int priority, const char *file, int line,
	      const char *fn, const char *format, va_list args)
{
	FILE *f = ctx->log_file;

	if (priority != LOG_NOTICE) {
		struct timespec ts;

		clock_gettime(CLOCK_REALTIME, &ts);
		fprintf(f, "[%10ld.%09ld] [%d] ", ts.tv_sec, ts.tv_nsec,
			getpid());
	}

	vfprintf(f, format, args);
	fflush(f);
}

void do_log(struct log_ctx *ctx, int priority, const char *file,
		int line, const char *fn, const char *format, ...)
{
	va_list args;
	int errno_save = errno;

	va_start(args, format);
	ctx->log_fn(ctx, priority, file, line, fn, format, args);
	va_end(args);
	errno = errno_save;
}

static void log_stderr(struct log_ctx *ctx, int priority, const char *file,
		int line, const char *fn, const char *format, va_list args)
{
	fprintf(stderr, "%s: %s: ", ctx->owner, fn);
	vfprintf(stderr, format, args);
}

static int log_priority(const char *priority)
{
	char *endptr;
	int prio;

	prio = strtol(priority, &endptr, 10);
	if (endptr[0] == '\0' || isspace(endptr[0]))
		return prio;
	if (strncmp(priority, "err", 3) == 0)
		return LOG_ERR;
	if (strncmp(priority, "info", 4) == 0)
		return LOG_INFO;
	if (strncmp(priority, "debug", 5) == 0)
		return LOG_DEBUG;
	if (strncmp(priority, "notice", 6) == 0)
		return LOG_NOTICE;
	return 0;
}

void log_init(struct log_ctx *ctx, const char *owner, const char *log_env)
{
	const char *env;

	ctx->owner = owner;
	ctx->log_fn = log_stderr;
	ctx->log_priority = LOG_ERR;

	/* environment overwrites config */
	env = secure_getenv(log_env);
	if (env != NULL)
		ctx->log_priority = log_priority(env);
}
