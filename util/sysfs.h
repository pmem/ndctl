/* SPDX-License-Identifier: LGPL-2.1 */
/* Copyright (C) 2014-2020, Intel Corporation. All rights reserved. */
#ifndef __UTIL_SYSFS_H__
#define __UTIL_SYSFS_H__

#include <string.h>

typedef void *(*add_dev_fn)(void *parent, int id, const char *dev_path);

#define SYSFS_ATTR_SIZE 1024

struct log_ctx;
int __sysfs_read_attr(struct log_ctx *ctx, const char *path, char *buf);
int __sysfs_write_attr(struct log_ctx *ctx, const char *path, const char *buf);
int __sysfs_write_attr_quiet(struct log_ctx *ctx, const char *path,
		const char *buf);
int __sysfs_device_parse(struct log_ctx *ctx, const char *base_path,
		const char *dev_name, void *parent, add_dev_fn add_dev);

#define sysfs_read_attr(c, p, b) __sysfs_read_attr(&(c)->ctx, (p), (b))
#define sysfs_write_attr(c, p, b) __sysfs_write_attr(&(c)->ctx, (p), (b))
#define sysfs_write_attr_quiet(c, p, b) __sysfs_write_attr_quiet(&(c)->ctx, (p), (b))
#define sysfs_device_parse(c, b, d, p, fn) __sysfs_device_parse(&(c)->ctx, \
		(b), (d), (p), (fn))

static inline const char *devpath_to_devname(const char *devpath)
{
	return strrchr(devpath, '/') + 1;
}

struct kmod_ctx;
struct kmod_module;
struct kmod_module *__util_modalias_to_module(struct kmod_ctx *kmod_ctx,
					      const char *alias,
					      struct log_ctx *log);
#define util_modalias_to_module(ctx, buf)                                      \
	__util_modalias_to_module((ctx)->kmod_ctx, buf, &(ctx)->ctx)

int __util_bind(const char *devname, struct kmod_module *module, const char *bus,
	      struct log_ctx *ctx);
#define util_bind(n, m, b, c) __util_bind(n, m, b, &(c)->ctx)

int __util_unbind(const char *devpath, struct log_ctx *ctx);
#define util_unbind(p, c) __util_unbind(p, &(c)->ctx)

#endif /* __UTIL_SYSFS_H__ */
