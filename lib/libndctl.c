/*
 * libndctl: helper library for the nd (nvdimm, nfit-defined, persistent
 *           memory, ...) sub-system.
 *
 * Copyright (c) 2014, Intel Corporation.
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
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>

#include <ndctl/libndctl.h>
#include "libndctl-private.h"

/**
 * SECTION:libndctl
 * @short_description: libndctl context
 *
 * The context contains the default values for the library user,
 * and is passed to all library operations.
 */

/**
 * ndctl_ctx:
 *
 * Opaque object representing the library context.
 */
struct ndctl_ctx {
	int refcount;
	void (*log_fn)(struct ndctl_ctx *ctx,
			int priority, const char *file, int line, const char *fn,
			const char *format, va_list args);
	void *userdata;
	int log_priority;
};

void ndctl_log(struct ndctl_ctx *ctx,
		int priority, const char *file, int line, const char *fn,
		const char *format, ...)
{
	va_list args;

	va_start(args, format);
	ctx->log_fn(ctx, priority, file, line, fn, format, args);
	va_end(args);
}

static void log_stderr(struct ndctl_ctx *ctx,
		int priority, const char *file, int line, const char *fn,
		const char *format, va_list args)
{
	fprintf(stderr, "libndctl: %s: ", fn);
	vfprintf(stderr, format, args);
}

/**
 * ndctl_get_userdata:
 * @ctx: ndctl library context
 *
 * Retrieve stored data pointer from library context. This might be useful
 * to access from callbacks like a custom logging function.
 *
 * Returns: stored userdata
 **/
NDCTL_EXPORT void *ndctl_get_userdata(struct ndctl_ctx *ctx)
{
	if (ctx == NULL)
		return NULL;
	return ctx->userdata;
}

/**
 * ndctl_set_userdata:
 * @ctx: ndctl library context
 * @userdata: data pointer
 *
 * Store custom @userdata in the library context.
 **/
NDCTL_EXPORT void ndctl_set_userdata(struct ndctl_ctx *ctx, void *userdata)
{
	if (ctx == NULL)
		return;
	ctx->userdata = userdata;
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

/**
 * ndctl_new:
 *
 * Create ndctl library context. This reads the ndctl configuration
 * and fills in the default values.
 *
 * The initial refcount is 1, and needs to be decremented to
 * release the resources of the ndctl library context.
 *
 * Returns: a new ndctl library context
 **/
NDCTL_EXPORT int ndctl_new(struct ndctl_ctx **ctx)
{
	const char *env;
	struct ndctl_ctx *c;

	c = calloc(1, sizeof(struct ndctl_ctx));
	if (!c)
		return -ENOMEM;

	c->refcount = 1;
	c->log_fn = log_stderr;
	c->log_priority = LOG_ERR;

	/* environment overwrites config */
	env = secure_getenv("NDCTL_LOG");
	if (env != NULL)
		ndctl_set_log_priority(c, log_priority(env));

	info(c, "ctx %p created\n", c);
	dbg(c, "log_priority=%d\n", c->log_priority);
	*ctx = c;
	return 0;
}

/**
 * ndctl_ref:
 * @ctx: ndctl library context
 *
 * Take a reference of the ndctl library context.
 *
 * Returns: the passed ndctl library context
 **/
NDCTL_EXPORT struct ndctl_ctx *ndctl_ref(struct ndctl_ctx *ctx)
{
	if (ctx == NULL)
		return NULL;
	ctx->refcount++;
	return ctx;
}

/**
 * ndctl_unref:
 * @ctx: ndctl library context
 *
 * Drop a reference of the ndctl library context.
 *
 **/
NDCTL_EXPORT struct ndctl_ctx *ndctl_unref(struct ndctl_ctx *ctx)
{
	if (ctx == NULL)
		return NULL;
	ctx->refcount--;
	if (ctx->refcount > 0)
		return NULL;
	info(ctx, "context %p released\n", ctx);
	free(ctx);
	return NULL;
}

/**
 * ndctl_set_log_fn:
 * @ctx: ndctl library context
 * @log_fn: function to be called for logging messages
 *
 * The built-in logging writes to stderr. It can be
 * overridden by a custom function, to plug log messages
 * into the user's logging functionality.
 *
 **/
NDCTL_EXPORT void ndctl_set_log_fn(struct ndctl_ctx *ctx,
		void (*log_fn)(struct ndctl_ctx *ctx,
			int priority, const char *file,
			int line, const char *fn,
			const char *format, va_list args))
{
	ctx->log_fn = log_fn;
	info(ctx, "custom logging function %p registered\n", log_fn);
}

/**
 * ndctl_get_log_priority:
 * @ctx: ndctl library context
 *
 * Returns: the current logging priority
 **/
NDCTL_EXPORT int ndctl_get_log_priority(struct ndctl_ctx *ctx)
{
	return ctx->log_priority;
}

/**
 * ndctl_set_log_priority:
 * @ctx: ndctl library context
 * @priority: the new logging priority
 *
 * Set the current logging priority. The value controls which messages
 * are logged.
 **/
NDCTL_EXPORT void ndctl_set_log_priority(struct ndctl_ctx *ctx, int priority)
{
	ctx->log_priority = priority;
}
