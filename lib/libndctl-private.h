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
#include <linux/ndctl.h>
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

static inline const char *devpath_to_devname(const char *devpath)
{
	return strrchr(devpath, '/') + 1;
}

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

#ifdef HAVE_LIBKMOD
#include <libkmod.h>
static inline int check_kmod(struct kmod_ctx *kmod_ctx)
{
	return kmod_ctx ? 0 : -ENXIO;
}

#else
struct kmod_ctx;
struct kmod_list;
struct kmod_module;

enum {
	KMOD_PROBE_APPLY_BLACKLIST,
};

static inline int check_kmod(struct kmod_ctx *kmod_cts)
{
	return 0;
}

static inline struct kmod_ctx *kmod_new(const char *dirname,
		const char * const *config_paths)
{
	return NULL;
}

static inline struct kmod_ctx *kmod_unref(struct kmod_ctx *ctx)
{
	return NULL;
}

static inline struct kmod_module *kmod_module_unref(struct kmod_module *mod)
{
	return NULL;
}

static inline int kmod_module_new_from_lookup(struct kmod_ctx *ctx, const char *alias,
						struct kmod_list **list)
{
	return -ENOTTY;
}

static inline struct kmod_module *kmod_module_get_module(const struct kmod_list *entry)
{
	return NULL;
}

static inline const char *kmod_module_get_name(const struct kmod_module *mod)
{
	return "unknown";
}

static inline int kmod_module_unref_list(struct kmod_list *list)
{
	return -ENOTTY;
}

static inline int kmod_module_probe_insert_module(struct kmod_module *mod,
			unsigned int flags, const char *extra_options,
			int (*run_install)(struct kmod_module *m,
						const char *cmdline, void *data),
			const void *data,
			void (*print_action)(struct kmod_module *m, bool install,
						const char *options))
{
	return -ENOTTY;
}

#endif

#ifdef HAVE_LIBUUID
#include <uuid/uuid.h>
#else
static inline int uuid_parse(const char *in, uuid_t uu)
{
	return -1;
}
#endif

static int ndctl_bind(struct ndctl_ctx *ctx, struct kmod_module *module,
		const char *devname);
static int ndctl_unbind(struct ndctl_ctx *ctx, const char *devpath);
static struct kmod_module *to_module(struct ndctl_ctx *ctx, const char *alias);

#endif /* _LIBNDCTL_PRIVATE_H_ */
