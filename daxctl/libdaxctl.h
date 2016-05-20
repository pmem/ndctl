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
#ifndef _LIBDAXCTL_H_
#define _LIBDAXCTL_H_

#include <stdarg.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

struct daxctl_ctx;
struct daxctl_ctx *daxctl_ref(struct daxctl_ctx *ctx);
void daxctl_unref(struct daxctl_ctx *ctx);
int daxctl_new(struct daxctl_ctx **ctx);
void daxctl_set_log_fn(struct daxctl_ctx *ctx,
		void (*log_fn)(struct daxctl_ctx *ctx, int priority,
			const char *file, int line, const char *fn,
			const char *format, va_list args));
int daxctl_get_log_priority(struct daxctl_ctx *ctx);
void daxctl_set_log_priority(struct daxctl_ctx *ctx, int priority);
void daxctl_set_userdata(struct daxctl_ctx *ctx, void *userdata);
void *daxctl_get_userdata(struct daxctl_ctx *ctx);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
