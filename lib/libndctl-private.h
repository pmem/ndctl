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
#ifndef _LIBNDCTL_PRIVATE_H_
#define _LIBNDCTL_PRIVATE_H_

#include <stdbool.h>
#include <syslog.h>

#include <ndctl/libndctl.h>

static inline void __attribute__((always_inline, format(printf, 2, 3)))
ndctl_log_null(struct ndctl_ctx *ctx, const char *format, ...) {}

#define ndctl_log_cond(ctx, prio, arg...) \
do { \
	if (ndctl_get_log_priority(ctx) >= prio) \
		ndctl_log(ctx, prio, __FILE__, __LINE__, __FUNCTION__, ## arg); \
} while (0)

#ifdef ENABLE_LOGGING
#  ifdef ENABLE_DEBUG
#    define dbg(ctx, arg...) ndctl_log_cond(ctx, LOG_DEBUG, ## arg)
#  else
#    define dbg(ctx, arg...) ndctl_log_null(ctx, ## arg)
#  endif
#  define info(ctx, arg...) ndctl_log_cond(ctx, LOG_INFO, ## arg)
#  define err(ctx, arg...) ndctl_log_cond(ctx, LOG_ERR, ## arg)
#else
#  define dbg(ctx, arg...) ndctl_log_null(ctx, ## arg)
#  define info(ctx, arg...) ndctl_log_null(ctx, ## arg)
#  define err(ctx, arg...) ndctl_log_null(ctx, ## arg)
#endif

#ifndef HAVE_SECURE_GETENV
#  ifdef HAVE___SECURE_GETENV
#    define secure_getenv __secure_getenv
#  else
#    error neither secure_getenv nor __secure_getenv is available
#  endif
#endif

#define NDCTL_EXPORT __attribute__ ((visibility("default")))

void ndctl_log(struct ndctl_ctx *ctx,
		int priority, const char *file, int line, const char *fn,
		const char *format, ...)
	__attribute__((format(printf, 6, 7)));

#endif
