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
#ifndef __UTIL_LOG_H__
#define __UTIL_LOG_H__
#include <stdio.h>
#include <stdarg.h>
#include <syslog.h>

struct log_ctx;
typedef void (*log_fn)(struct log_ctx *ctx, int priority, const char *file,
		int line, const char *fn, const char *format, va_list args);

struct log_ctx {
	log_fn log_fn;
	const char *owner;
	int log_priority;
};


void do_log(struct log_ctx *ctx, int priority, const char *file, int line,
		const char *fn, const char *format, ...)
	__attribute__((format(printf, 6, 7)));
void log_init(struct log_ctx *ctx, const char *owner, const char *log_env);
static inline void __attribute__((always_inline, format(printf, 2, 3)))
	log_null(struct log_ctx *ctx, const char *format, ...) {}

#define log_cond(ctx, prio, arg...) \
do { \
	if ((ctx)->log_priority >= prio) \
		do_log(ctx, prio, __FILE__, __LINE__, __FUNCTION__, ## arg); \
} while (0)

#ifdef ENABLE_LOGGING
#  ifdef ENABLE_DEBUG
#    define log_dbg(ctx, arg...) log_cond(ctx, LOG_DEBUG, ## arg)
#  else
#    define log_dbg(ctx, arg...) log_null(ctx, ## arg)
#  endif
#  define log_info(ctx, arg...) log_cond(ctx, LOG_INFO, ## arg)
#  define log_err(ctx, arg...) log_cond(ctx, LOG_ERR, ## arg)
#else
#  define log_dbg(ctx, arg...) log_null(ctx, ## arg)
#  define log_info(ctx, arg...) log_null(ctx, ## arg)
#  define log_err(ctx, arg...) log_null(ctx, ## arg)
#endif

#define dbg(x, arg...) log_dbg(&(x)->ctx, ## arg)
#define info(x, arg...) log_info(&(x)->ctx, ## arg)
#define err(x, arg...) log_err(&(x)->ctx, ## arg)

#ifndef HAVE_SECURE_GETENV
#  ifdef HAVE___SECURE_GETENV
#    define secure_getenv __secure_getenv
#  else
#    warning neither secure_getenv nor __secure_getenv is available.
#    define secure_getenv getenv
#  endif
#endif

#endif /* __UTIL_LOG_H__ */
