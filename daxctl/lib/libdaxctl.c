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
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <uuid/uuid.h>
#include <ccan/list/list.h>
#include <ccan/array_size/array_size.h>

#include <util/log.h>
#include <util/sysfs.h>
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

DAXCTL_EXPORT struct daxctl_ctx *daxctl_region_get_ctx(
		struct daxctl_region *region)
{
	return region->ctx;
}

DAXCTL_EXPORT void daxctl_region_get_uuid(struct daxctl_region *region, uuid_t uu)
{
	uuid_copy(uu, region->uuid);
}

static void free_dev(struct daxctl_dev *dev, struct list_head *head)
{
	if (head)
		list_del_from(head, &dev->list);
	free(dev->dev_buf);
	free(dev->dev_path);
	free(dev);
}

DAXCTL_EXPORT void daxctl_region_unref(struct daxctl_region *region)
{
	struct daxctl_ctx *ctx;
	struct daxctl_dev *dev, *_d;

	if (!region)
		return;
	region->refcount--;
	if (region->refcount)
		return;

	ctx = region->ctx;
	dbg(ctx, "%s: %s\n", __func__, daxctl_region_get_devname(region));
	list_for_each_safe(&region->devices, dev, _d, list)
		free_dev(dev, &region->devices);
	free(region->region_path);
	free(region->region_buf);
	free(region);
}

DAXCTL_EXPORT void daxctl_region_ref(struct daxctl_region *region)
{
	if (region)
		region->refcount++;
}

DAXCTL_EXPORT struct daxctl_region *daxctl_new_region(struct daxctl_ctx *ctx,
		int id, uuid_t uuid, const char *path)
{
	struct daxctl_region *region;

	region = calloc(1, sizeof(*region));
	if (!region)
		return NULL;
	region->ctx = ctx;
	region->id = id;
	region->refcount = 1;
	uuid_copy(region->uuid, uuid);

	region->region_path = strdup(path);
	if (!region->region_path) {
		free(region);
		return NULL;
	}

	list_head_init(&region->devices);
	region->buf_len = strlen(region->region_path) + REGION_BUF_SIZE;
	region->region_buf = calloc(1, region->buf_len);
	if (!region->region_buf) {
		daxctl_region_unref(region);
		return NULL;
	}

	dbg(ctx, "%s: %s\n", __func__, daxctl_region_get_devname(region));

	return region;
}

static int add_dax_dev(void *parent, int id, const char *daxdev_base)
{
	const char *devname = devpath_to_devname(daxdev_base);
	char *path = calloc(1, strlen(daxdev_base) + 100);
	struct daxctl_region *region = parent;
	struct daxctl_ctx *ctx = region->ctx;
	struct daxctl_dev *dev, *dev_dup;
	char buf[SYSFS_ATTR_SIZE];
	int rc = -ENOMEM;
	struct stat st;

	if (!path)
		return -ENOMEM;

	dev = calloc(1, sizeof(*dev));
	if (!dev)
		goto err_dev;
	dev->id = id;
	dev->region = region;

	sprintf(path, "/dev/%s", devname);
	if (stat(path, &st) < 0)
		goto err_read;
	dev->major = major(st.st_rdev);
	dev->minor = minor(st.st_rdev);

	sprintf(path, "%s/size", daxdev_base);
	if (sysfs_read_attr(ctx, path, buf) < 0)
		goto err_read;
	dev->size = strtoull(buf, NULL, 0);

	dev->dev_path = strdup(daxdev_base);
	if (!dev->dev_path)
		goto err_read;

	dev->dev_buf = calloc(1, strlen(daxdev_base) + 50);
	if (!dev->dev_buf)
		goto err_read;
	dev->buf_len = strlen(daxdev_base) + 50;

	daxctl_dev_foreach(region, dev_dup)
		if (dev_dup->id == dev->id) {
			free_dev(dev, NULL);
			free(path);
			return 1;
		}

	list_add(&region->devices, &dev->list);
	free(path);
	return 0;

 err_read:
	free(dev->dev_buf);
	free(dev->dev_path);
	free(dev);
 err_dev:
	free(path);
	return rc;
}

DAXCTL_EXPORT int daxctl_region_get_id(struct daxctl_region *region)
{
	return region->id;
}

DAXCTL_EXPORT const char *daxctl_region_get_devname(struct daxctl_region *region)
{
        return devpath_to_devname(region->region_path);
}

DAXCTL_EXPORT unsigned long long daxctl_region_get_available_size(
		struct daxctl_region *region)
{
	struct daxctl_ctx *ctx = daxctl_region_get_ctx(region);
	char *path = region->region_buf;
	char buf[SYSFS_ATTR_SIZE], *end;
	int len = region->buf_len;
	unsigned long long avail;

	if (snprintf(path, len, "%s/dax_region/available_size",
				region->region_path) >= len) {
		err(ctx, "%s: buffer too small!\n",
				daxctl_region_get_devname(region));
		return 0;
	}

	if (sysfs_read_attr(ctx, path, buf) < 0)
		return 0;

	avail = strtoull(buf, &end, 0);
	if (buf[0] && *end == '\0')
		return avail;
	return 0;
}

DAXCTL_EXPORT struct daxctl_dev *daxctl_region_get_dev_seed(
		struct daxctl_region *region)
{
	struct daxctl_ctx *ctx = daxctl_region_get_ctx(region);
	char *path = region->region_buf;
	int len = region->buf_len;
	char buf[SYSFS_ATTR_SIZE];
	struct daxctl_dev *dev;

	if (snprintf(path, len, "%s/dax_region/seed", region->region_path) >= len) {
		err(ctx, "%s: buffer too small!\n",
				daxctl_region_get_devname(region));
		return NULL;
	}

	if (sysfs_read_attr(ctx, path, buf) < 0)
		return NULL;

	daxctl_dev_foreach(region, dev)
		if (strcmp(buf, daxctl_dev_get_devname(dev)) == 0)
			return dev;
	return NULL;
}

static void dax_devices_init(struct daxctl_region *region)
{
	struct daxctl_ctx *ctx = daxctl_region_get_ctx(region);
	char daxdev_fmt[50];
	char *region_path;

	if (region->devices_init)
		return;

	region->devices_init = 1;
	sprintf(daxdev_fmt, "dax%d.", region->id);
	if (asprintf(&region_path, "%s/dax", region->region_path) < 0)
		region_path = NULL;
	if (region_path)
		sysfs_device_parse(ctx, region_path, daxdev_fmt, region,
				add_dax_dev);
	free(region_path);
}

DAXCTL_EXPORT struct daxctl_dev *daxctl_dev_get_first(struct daxctl_region *region)
{
	dax_devices_init(region);

	return list_top(&region->devices, struct daxctl_dev, list);
}

DAXCTL_EXPORT struct daxctl_dev *daxctl_dev_get_next(struct daxctl_dev *dev)
{
	struct daxctl_region *region = dev->region;

	return list_next(&region->devices, dev, list);
}

DAXCTL_EXPORT struct daxctl_region *daxctl_dev_get_region(struct daxctl_dev *dev)
{
	return dev->region;
}

DAXCTL_EXPORT int daxctl_dev_get_id(struct daxctl_dev *dev)
{
	return dev->id;
}

DAXCTL_EXPORT const char *daxctl_dev_get_devname(struct daxctl_dev *dev)
{
	return devpath_to_devname(dev->dev_path);
}

DAXCTL_EXPORT int daxctl_dev_get_major(struct daxctl_dev *dev)
{
	return dev->major;
}

DAXCTL_EXPORT int daxctl_dev_get_minor(struct daxctl_dev *dev)
{
	return dev->minor;
}

DAXCTL_EXPORT unsigned long long daxctl_dev_get_size(struct daxctl_dev *dev)
{
	return dev->size;
}
