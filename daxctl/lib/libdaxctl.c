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
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <util/log.h>
#include <uuid/uuid.h>
#include <ccan/list/list.h>
#include <ccan/array_size/array_size.h>

#include <daxctl/libdaxctl.h>
#include "libdaxctl-private.h"

/**
 * struct daxctl_ctx - library user context to find "nd" instances
 *
 * Instantiate with daxctl_new(), which takes an initial reference.  Free
 * the context by dropping the reference count to zero with
 * daxctl_unref(), or take additional references with daxctl_ref()
 * @timeout: default library timeout in milliseconds
 */
struct daxctl_ctx {
	/* log_ctx must be first member for daxctl_set_log_fn compat */
	struct log_ctx ctx;
	int refcount;
	void *userdata;
};

/**
 * daxctl_get_userdata - retrieve stored data pointer from library context
 * @ctx: daxctl library context
 *
 * This might be useful to access from callbacks like a custom logging
 * function.
 */
DAXCTL_EXPORT void *daxctl_get_userdata(struct daxctl_ctx *ctx)
{
	if (ctx == NULL)
		return NULL;
	return ctx->userdata;
}

/**
 * daxctl_set_userdata - store custom @userdata in the library context
 * @ctx: daxctl library context
 * @userdata: data pointer
 */
DAXCTL_EXPORT void daxctl_set_userdata(struct daxctl_ctx *ctx, void *userdata)
{
	if (ctx == NULL)
		return;
	ctx->userdata = userdata;
}

/**
 * daxctl_new - instantiate a new library context
 * @ctx: context to establish
 *
 * Returns zero on success and stores an opaque pointer in ctx.  The
 * context is freed by daxctl_unref(), i.e. daxctl_new() implies an
 * internal daxctl_ref().
 */
DAXCTL_EXPORT int daxctl_new(struct daxctl_ctx **ctx)
{
	struct daxctl_ctx *c;

	c = calloc(1, sizeof(struct daxctl_ctx));
	if (!c)
		return -ENOMEM;

	c->refcount = 1;
	log_init(&c->ctx, "libdaxctl", "DAXCTL_LOG");
	info(c, "ctx %p created\n", c);
	dbg(c, "log_priority=%d\n", c->ctx.log_priority);
	*ctx = c;

	return 0;
}

/**
 * daxctl_ref - take an additional reference on the context
 * @ctx: context established by daxctl_new()
 */
DAXCTL_EXPORT struct daxctl_ctx *daxctl_ref(struct daxctl_ctx *ctx)
{
	if (ctx == NULL)
		return NULL;
	ctx->refcount++;
	return ctx;
}

/**
 * daxctl_unref - drop a context reference count
 * @ctx: context established by daxctl_new()
 *
 * Drop a reference and if the resulting reference count is 0 destroy
 * the context.
 */
DAXCTL_EXPORT void daxctl_unref(struct daxctl_ctx *ctx)
{
	if (ctx == NULL)
		return;
	ctx->refcount--;
	if (ctx->refcount > 0)
		return;
	info(ctx, "context %p released\n", ctx);
	free(ctx);
}

/**
 * daxctl_set_log_fn - override default log routine
 * @ctx: daxctl library context
 * @log_fn: function to be called for logging messages
 *
 * The built-in logging writes to stderr. It can be overridden by a
 * custom function, to plug log messages into the user's logging
 * functionality.
 */
DAXCTL_EXPORT void daxctl_set_log_fn(struct daxctl_ctx *ctx,
		void (*daxctl_log_fn)(struct daxctl_ctx *ctx, int priority,
			const char *file, int line, const char *fn,
			const char *format, va_list args))
{
	ctx->ctx.log_fn = (log_fn) daxctl_log_fn;
	info(ctx, "custom logging function %p registered\n", daxctl_log_fn);
}

/**
 * daxctl_get_log_priority - retrieve current library loglevel (syslog)
 * @ctx: daxctl library context
 */
DAXCTL_EXPORT int daxctl_get_log_priority(struct daxctl_ctx *ctx)
{
	return ctx->ctx.log_priority;
}

/**
 * daxctl_set_log_priority - set log verbosity
 * @priority: from syslog.h, LOG_ERR, LOG_INFO, LOG_DEBUG
 *
 * Note: LOG_DEBUG requires library be built with "configure --enable-debug"
 */
DAXCTL_EXPORT void daxctl_set_log_priority(struct daxctl_ctx *ctx, int priority)
{
	ctx->ctx.log_priority = priority;
}
