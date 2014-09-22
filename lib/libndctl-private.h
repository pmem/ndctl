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

#define ND_DEVICE_DIMM 1            /* dimm (no driver, informational) */
#define ND_DEVICE_REGION_PMEM 2     /* region (parent of pmem namespaces) */
#define ND_DEVICE_REGION_BLOCK 3    /* region (parent of block namespaces) */
#define ND_DEVICE_NAMESPACE_IO 4    /* legacy persistent memory */
#define ND_DEVICE_NAMESPACE_PMEM 5  /* persistent memory namespace (may alias) */
#define ND_DEVICE_NAMESPACE_BLOCK 6 /* block-data-window namespace (may alias) */

#ifdef HAVE_LIBUDEV
#include <libudev.h>

static inline int check_udev(struct udev *udev)
{
	return udev ? 0 : -ENXIO;
}
#else
struct udev;
struct udev_queue;

static inline struct udev *udev_new(void)
{
	return NULL;
}

static inline void udev_unref(struct udev *udev)
{
}

static inline int check_udev(struct udev *udev)
{
	return 0;
}

static inline struct udev_queue *udev_queue_new(struct udev *udev)
{
	return NULL;
}

static inline void udev_queue_unref(struct udev_queue *udev_queue)
{
}

static inline int udev_queue_get_queue_is_empty(struct udev_queue *udev_queue)
{
	return 0;
}
#endif

#endif /* _LIBNDCTL_PRIVATE_H_ */
