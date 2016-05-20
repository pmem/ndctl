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
#include <syslog.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <util/log.h>

void do_log(struct log_ctx *ctx, int priority, const char *file,
		int line, const char *fn, const char *format, ...)
{
	va_list args;

	va_start(args, format);
	ctx->log_fn(ctx, priority, file, line, fn, format, args);
	va_end(args);
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
