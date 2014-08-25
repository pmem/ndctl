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
#ifndef _LIBNDCTL_H_
#define _LIBNDCTL_H_

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * ndctl_ctx
 *
 * library user context - reads the config and system
 * environment, user variables, allows custom logging
 */
struct ndctl_ctx;
struct ndctl_ctx *ndctl_ref(struct ndctl_ctx *ctx);
struct ndctl_ctx *ndctl_unref(struct ndctl_ctx *ctx);
int ndctl_new(struct ndctl_ctx **ctx);
void ndctl_set_log_fn(struct ndctl_ctx *ctx,
                  void (*log_fn)(struct ndctl_ctx *ctx,
                                 int priority, const char *file, int line, const char *fn,
                                 const char *format, va_list args));
int ndctl_get_log_priority(struct ndctl_ctx *ctx);
void ndctl_set_log_priority(struct ndctl_ctx *ctx, int priority);
void *ndctl_get_userdata(struct ndctl_ctx *ctx);
void ndctl_set_userdata(struct ndctl_ctx *ctx, void *userdata);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
