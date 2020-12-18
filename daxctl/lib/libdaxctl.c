// SPDX-License-Identifier: LGPL-2.1
// Copyright (C) 2016-2020, Intel Corporation. All rights reserved.
#include <stdio.h>
#include <errno.h>
#include <limits.h>
#include <libgen.h>
#include <stdlib.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <uuid/uuid.h>
#include <ccan/list/list.h>
#include <ccan/array_size/array_size.h>

#include <util/log.h>
#include <util/sysfs.h>
#include <util/iomem.h>
#include <daxctl/libdaxctl.h>
#include "libdaxctl-private.h"

static const char *attrs = "dax_region";

static void free_region(struct daxctl_region *region, struct list_head *head);

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
	int regions_init;
	struct list_head regions;
	struct kmod_ctx *kmod_ctx;
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
	struct kmod_ctx *kmod_ctx;
	struct daxctl_ctx *c;
	int rc = 0;

	c = calloc(1, sizeof(struct daxctl_ctx));
	if (!c)
		return -ENOMEM;

	kmod_ctx = kmod_new(NULL, NULL);
	if (check_kmod(kmod_ctx) != 0) {
		rc = -ENXIO;
		goto out;
	}

	c->refcount = 1;
	log_init(&c->ctx, "libdaxctl", "DAXCTL_LOG");
	info(c, "ctx %p created\n", c);
	dbg(c, "log_priority=%d\n", c->ctx.log_priority);
	*ctx = c;
	list_head_init(&c->regions);
	c->kmod_ctx = kmod_ctx;

	return 0;
out:
	free(c);
	return rc;
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
	struct daxctl_region *region, *_r;

	if (ctx == NULL)
		return;
	ctx->refcount--;
	if (ctx->refcount > 0)
		return;

	list_for_each_safe(&ctx->regions, region, _r, list)
		free_region(region, &ctx->regions);

	kmod_unref(ctx->kmod_ctx);
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

static void free_mem(struct daxctl_dev *dev)
{
	if (dev->mem) {
		free(dev->mem->node_path);
		free(dev->mem->mem_buf);
		free(dev->mem);
		dev->mem = NULL;
	}
}

static void free_dev(struct daxctl_dev *dev, struct list_head *head)
{
	if (head)
		list_del_from(head, &dev->list);
	kmod_module_unref(dev->module);
	free(dev->dev_buf);
	free(dev->dev_path);
	free_mem(dev);
	free(dev);
}

static void free_region(struct daxctl_region *region, struct list_head *head)
{
	struct daxctl_dev *dev, *_d;

	list_for_each_safe(&region->devices, dev, _d, list)
		free_dev(dev, &region->devices);
	if (head)
		list_del_from(head, &region->list);
	free(region->region_path);
	free(region->region_buf);
	free(region->devname);
	free(region);
}

DAXCTL_EXPORT void daxctl_region_unref(struct daxctl_region *region)
{
	struct daxctl_ctx *ctx;

	if (!region)
		return;
	region->refcount--;
	if (region->refcount)
		return;

	ctx = region->ctx;
	dbg(ctx, "%s: %s\n", __func__, daxctl_region_get_devname(region));
	free_region(region, &ctx->regions);
}

DAXCTL_EXPORT void daxctl_region_ref(struct daxctl_region *region)
{
	if (region)
		region->refcount++;
}

static struct daxctl_region *add_dax_region(void *parent, int id,
		const char *base)
{
	struct daxctl_region *region, *region_dup;
	struct daxctl_ctx *ctx = parent;
	char buf[SYSFS_ATTR_SIZE];
	char *path;

	dbg(ctx, "%s: \'%s\'\n", __func__, base);

	daxctl_region_foreach(ctx, region_dup)
		if (strcmp(region_dup->region_path, base) == 0)
			return region_dup;

	path = calloc(1, strlen(base) + 100);
	if (!path)
		return NULL;

	region = calloc(1, sizeof(*region));
	if (!region)
		goto err_region;

	region->id = id;
	region->align = -1;
	region->size = -1;
	region->ctx = ctx;
	region->refcount = 1;
	list_head_init(&region->devices);
	region->devname = strdup(devpath_to_devname(base));
	if (!region->devname)
		goto err_read;

	sprintf(path, "%s/%s/size", base, attrs);
	if (sysfs_read_attr(ctx, path, buf) == 0)
		region->size = strtoull(buf, NULL, 0);

	sprintf(path, "%s/%s/align", base, attrs);
	if (sysfs_read_attr(ctx, path, buf) == 0)
		region->align = strtoul(buf, NULL, 0);

	region->region_path = strdup(base);
	if (!region->region_path)
		goto err_read;

	region->region_buf = calloc(1, strlen(path) + strlen(attrs)
			+ REGION_BUF_SIZE);
	if (!region->region_buf)
		goto err_read;
	region->buf_len = strlen(path) + REGION_BUF_SIZE;

	list_add(&ctx->regions, &region->list);

	free(path);
	return region;

 err_read:
	free(region->region_buf);
	free(region->region_path);
	free(region->devname);
	free(region);
 err_region:
	free(path);
	return NULL;
}

DAXCTL_EXPORT struct daxctl_region *daxctl_new_region(struct daxctl_ctx *ctx,
		int id, uuid_t uuid, const char *path)
{
	struct daxctl_region *region;

	region = add_dax_region(ctx, id, path);
	if (!region)
		return NULL;
	uuid_copy(region->uuid, uuid);

	dbg(ctx, "%s: %s\n", __func__, daxctl_region_get_devname(region));

	return region;
}

static bool device_model_is_dax_bus(struct daxctl_dev *dev)
{
	const char *devname = daxctl_dev_get_devname(dev);
	struct daxctl_ctx *ctx = daxctl_dev_get_ctx(dev);
	char *path = dev->dev_buf, *resolved;
	size_t len = dev->buf_len;
	struct stat sb;

	if (snprintf(path, len, "/dev/%s", devname) < 0)
		return false;

	if (lstat(path, &sb) < 0) {
		err(ctx, "%s: stat for %s failed: %s\n",
			devname, path, strerror(errno));
		return false;
	}

	if (snprintf(path, len, "/sys/dev/char/%d:%d/subsystem",
			major(sb.st_rdev), minor(sb.st_rdev)) < 0)
		return false;

	resolved = realpath(path, NULL);
	if (!resolved) {
		err(ctx, "%s:  unable to determine subsys: %s\n",
			devname, strerror(errno));
		return false;
	}

	if (strcmp(resolved, "/sys/bus/dax") == 0) {
		free(resolved);
		return true;
	}

	free(resolved);
	return false;
}

static int dev_is_system_ram_capable(struct daxctl_dev *dev)
{
	const char *devname = daxctl_dev_get_devname(dev);
	struct daxctl_ctx *ctx = daxctl_dev_get_ctx(dev);
	char *mod_path, *mod_base;
	char path[200];
	const int len = sizeof(path);

	if (!device_model_is_dax_bus(dev))
		return false;

	if (!daxctl_dev_is_enabled(dev))
		return false;

	if (snprintf(path, len, "%s/driver", dev->dev_path) >= len) {
		err(ctx, "%s: buffer too small!\n", devname);
		return false;
	}

	mod_path = realpath(path, NULL);
	if (!mod_path)
		return false;

	mod_base = basename(mod_path);
	if (strcmp(mod_base, dax_modules[DAXCTL_DEV_MODE_RAM]) == 0) {
		free(mod_path);
		return true;
	}

	free(mod_path);
	return false;
}

/*
 * This checks for the device to be in system-ram mode, so calling
 * daxctl_dev_get_memory() on a devdax mode device will always return NULL.
 */
static struct daxctl_memory *daxctl_dev_alloc_mem(struct daxctl_dev *dev)
{
	const char *size_path = "/sys/devices/system/memory/block_size_bytes";
	const char *node_base = "/sys/devices/system/node/node";
	const char *devname = daxctl_dev_get_devname(dev);
	struct daxctl_ctx *ctx = daxctl_dev_get_ctx(dev);
	struct daxctl_memory *mem;
	char buf[SYSFS_ATTR_SIZE];
	int node_num;

	if (!dev_is_system_ram_capable(dev))
		return NULL;

	mem = calloc(1, sizeof(*mem));
	if (!mem)
		return NULL;

	mem->dev = dev;

	if (sysfs_read_attr(ctx, size_path, buf) == 0) {
		mem->block_size = strtoul(buf, NULL, 16);
		if (mem->block_size == 0 || mem->block_size == ULONG_MAX) {
			err(ctx, "%s: Unable to determine memblock size: %s\n",
				devname, strerror(errno));
			mem->block_size = 0;
		}
	}

	node_num = daxctl_dev_get_target_node(dev);
	if (node_num >= 0) {
		if (asprintf(&mem->node_path, "%s%d", node_base,
				node_num) < 0) {
			err(ctx, "%s: Unable to set node_path\n", devname);
			goto err_mem;
		}
	}

	mem->mem_buf = calloc(1, strlen(node_base) + 256);
	if (!mem->mem_buf)
		goto err_node;
	mem->buf_len = strlen(node_base) + 256;

	dev->mem = mem;
	return mem;

err_node:
	free(mem->node_path);
err_mem:
	free(mem);
	return NULL;
}

static void *add_dax_dev(void *parent, int id, const char *daxdev_base)
{
	const char *devname = devpath_to_devname(daxdev_base);
	char *path = calloc(1, strlen(daxdev_base) + 100);
	struct daxctl_region *region = parent;
	struct daxctl_ctx *ctx = region->ctx;
	struct daxctl_dev *dev, *dev_dup;
	char buf[SYSFS_ATTR_SIZE];
	struct stat st;

	if (!path)
		return NULL;
	dbg(ctx, "%s: base: \'%s\'\n", __func__, daxdev_base);

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

	sprintf(path, "%s/resource", daxdev_base);
	if (sysfs_read_attr(ctx, path, buf) == 0)
		dev->resource = strtoull(buf, NULL, 0);
	else
		dev->resource = iomem_get_dev_resource(ctx, daxdev_base);

	sprintf(path, "%s/size", daxdev_base);
	if (sysfs_read_attr(ctx, path, buf) < 0)
		goto err_read;
	dev->size = strtoull(buf, NULL, 0);

	/* Device align attribute is only available in v5.10 or up */
	sprintf(path, "%s/align", daxdev_base);
	if (!sysfs_read_attr(ctx, path, buf))
		dev->align = strtoull(buf, NULL, 0);
	else
		dev->align = 0;

	dev->dev_path = strdup(daxdev_base);
	if (!dev->dev_path)
		goto err_read;

	dev->dev_buf = calloc(1, strlen(daxdev_base) + 50);
	if (!dev->dev_buf)
		goto err_read;
	dev->buf_len = strlen(daxdev_base) + 50;

	sprintf(path, "%s/target_node", daxdev_base);
	if (sysfs_read_attr(ctx, path, buf) == 0)
		dev->target_node = strtol(buf, NULL, 0);
	else
		dev->target_node = -1;

	daxctl_dev_foreach(region, dev_dup)
		if (dev_dup->id == dev->id) {
			free_dev(dev, NULL);
			free(path);
			return dev_dup;
		}
	dev->num_mappings = -1;
	list_head_init(&dev->mappings);
	list_add(&region->devices, &dev->list);
	free(path);
	return dev;

 err_read:
	free(dev->dev_buf);
	free(dev->dev_path);
	free(dev);
 err_dev:
	free(path);
	return NULL;
}

DAXCTL_EXPORT int daxctl_region_get_id(struct daxctl_region *region)
{
	return region->id;
}

DAXCTL_EXPORT unsigned long daxctl_region_get_align(struct daxctl_region *region)
{
	return region->align;
}

DAXCTL_EXPORT unsigned long long daxctl_region_get_size(struct daxctl_region *region)
{
	return region->size;
}

DAXCTL_EXPORT const char *daxctl_region_get_devname(struct daxctl_region *region)
{
	return region->devname;
}

DAXCTL_EXPORT const char *daxctl_region_get_path(struct daxctl_region *region)
{
	return region->region_path;
}

DAXCTL_EXPORT unsigned long long daxctl_region_get_available_size(
		struct daxctl_region *region)
{
	struct daxctl_ctx *ctx = daxctl_region_get_ctx(region);
	char *path = region->region_buf;
	char buf[SYSFS_ATTR_SIZE], *end;
	int len = region->buf_len;
	unsigned long long avail;

	if (snprintf(path, len, "%s/%s/available_size",
				region->region_path, attrs) >= len) {
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

DAXCTL_EXPORT int daxctl_region_create_dev(struct daxctl_region *region)
{
	struct daxctl_ctx *ctx = daxctl_region_get_ctx(region);
	char *path = region->region_buf;
	int rc, len = region->buf_len;
	char *num_devices;

	if (snprintf(path, len, "%s/%s/create", region->region_path, attrs) >= len) {
		err(ctx, "%s: buffer too small!\n",
				daxctl_region_get_devname(region));
		return -EFAULT;
	}

	if (asprintf(&num_devices, "%d", 1) < 0) {
		err(ctx, "%s: buffer too small!\n",
				daxctl_region_get_devname(region));
		return -EFAULT;
	}

	rc = sysfs_write_attr(ctx, path, num_devices);
	if (rc)
		return rc;

	return 0;
}

DAXCTL_EXPORT int daxctl_region_destroy_dev(struct daxctl_region *region,
					    struct daxctl_dev *dev)
{
	struct daxctl_ctx *ctx = daxctl_region_get_ctx(region);
	char *path = region->region_buf;
	int rc, len = region->buf_len;

	if (snprintf(path, len, "%s/%s/delete", region->region_path, attrs) >= len) {
		err(ctx, "%s: buffer too small!\n",
				daxctl_region_get_devname(region));
		return -EFAULT;
	}

	rc = sysfs_write_attr(ctx, path, daxctl_dev_get_devname(dev));
	return rc;
}

DAXCTL_EXPORT struct daxctl_dev *daxctl_region_get_dev_seed(
		struct daxctl_region *region)
{
	struct daxctl_ctx *ctx = daxctl_region_get_ctx(region);
	char *path = region->region_buf;
	int len = region->buf_len;
	char buf[SYSFS_ATTR_SIZE];
	struct daxctl_dev *dev;

	if (snprintf(path, len, "%s/%s/seed", region->region_path, attrs) >= len) {
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
	size_t i;

	if (region->devices_init)
		return;

	region->devices_init = 1;
	sprintf(daxdev_fmt, "dax%d.", region->id);
	for (i = 0; i < ARRAY_SIZE(dax_subsystems); i++) {
		char *region_path;

		if (i == DAX_BUS)
			region_path = region->region_path;
		else if (i == DAX_CLASS) {
			if (asprintf(&region_path, "%s/dax",
						region->region_path) < 0) {
				dbg(ctx, "region path alloc fail\n");
				continue;
			}
		} else
			continue;
		sysfs_device_parse(ctx, region_path, daxdev_fmt, region,
				add_dax_dev);
		if (i == DAX_CLASS)
			free(region_path);
	}
}

static char *dax_region_path(const char *device, enum dax_subsystem subsys)
{
	char *path, *region_path, *c;

	if (asprintf(&path, "%s/%s", dax_subsystems[subsys], device) < 0)
		return NULL;

	/* dax_region must be the instance's direct parent */
	region_path = realpath(path, NULL);
	free(path);
	if (!region_path)
		return NULL;

	/*
	 * 'region_path' is now regionX/dax/daxX.Y' (DAX_CLASS), or
	 * regionX/daxX.Y (DAX_BUS), trim it back to the regionX
	 * component
	 */
	c = strrchr(region_path, '/');
	if (!c) {
		free(region_path);
		return NULL;
	}
	*c = '\0';

	if (subsys == DAX_BUS)
		return region_path;

	c = strrchr(region_path, '/');
	if (!c) {
		free(region_path);
		return NULL;
	}
	*c = '\0';

	return region_path;
}

static void __dax_regions_init(struct daxctl_ctx *ctx, enum dax_subsystem subsys)
{
	struct dirent *de;
	DIR *dir = NULL;

	dir = opendir(dax_subsystems[subsys]);
	if (!dir) {
		dbg(ctx, "no dax regions found via: %s\n",
				dax_subsystems[subsys]);
		return;
	}

	while ((de = readdir(dir)) != NULL) {
		struct daxctl_region *region;
		int id, region_id;
		char *dev_path;

		if (de->d_ino == 0)
			continue;
		if (sscanf(de->d_name, "dax%d.%d", &region_id, &id) != 2)
			continue;
		dev_path = dax_region_path(de->d_name, subsys);
		if (!dev_path) {
			err(ctx, "dax region path allocation failure\n");
			continue;
		}
		region = add_dax_region(ctx, region_id, dev_path);
		free(dev_path);
		if (!region)
			err(ctx, "add_dax_region() for %s failed\n", de->d_name);
	}
	closedir(dir);
}

static void dax_regions_init(struct daxctl_ctx *ctx)
{
	size_t i;

	if (ctx->regions_init)
		return;

	ctx->regions_init = 1;

	for (i = 0; i < ARRAY_SIZE(dax_subsystems); i++) {
		if (i == DAX_UNKNOWN)
			continue;
		__dax_regions_init(ctx, i);
	}
}

static int is_enabled(const char *drvpath)
{
	struct stat st;

	if (lstat(drvpath, &st) < 0 || !S_ISLNK(st.st_mode))
		return 0;
	else
		return 1;
}

static int daxctl_bind(struct daxctl_ctx *ctx, const char *devname,
		const char *mod_name)
{
	DIR *dir;
	int rc = 0;
	char path[200];
	struct dirent *de;
	const int len = sizeof(path);

	if (!devname) {
		err(ctx, "missing devname\n");
		return -EINVAL;
	}

	if (snprintf(path, len, "/sys/bus/dax/drivers") >= len) {
		err(ctx, "%s: buffer too small!\n", devname);
		return -ENXIO;
	}

	dir = opendir(path);
	if (!dir) {
		err(ctx, "%s: opendir(\"%s\") failed\n", devname, path);
		return -ENXIO;
	}

	while ((de = readdir(dir)) != NULL) {
		char *drv_path;

		if (de->d_ino == 0)
			continue;
		if (de->d_name[0] == '.')
			continue;
		if (strcmp(de->d_name, mod_name) != 0)
			continue;

		if (asprintf(&drv_path, "%s/%s/new_id", path, de->d_name) < 0) {
			err(ctx, "%s: path allocation failure\n", devname);
			rc = -ENOMEM;
			break;
		}
		rc = sysfs_write_attr_quiet(ctx, drv_path, devname);
		free(drv_path);

		if (asprintf(&drv_path, "%s/%s/bind", path, de->d_name) < 0) {
			err(ctx, "%s: path allocation failure\n", devname);
			rc = -ENOMEM;
			break;
		}
		rc = sysfs_write_attr_quiet(ctx, drv_path, devname);
		free(drv_path);
		break;
	}
	closedir(dir);

	if (rc) {
		dbg(ctx, "%s: bind failed\n", devname);
		return rc;
	}
	return 0;
}

static int daxctl_unbind(struct daxctl_ctx *ctx, const char *devpath)
{
	const char *devname = devpath_to_devname(devpath);
	char path[200];
	const int len = sizeof(path);
	int rc;

	if (snprintf(path, len, "%s/driver/remove_id", devpath) >= len) {
		err(ctx, "%s: buffer too small!\n", devname);
		return -ENXIO;
	}

	rc = sysfs_write_attr(ctx, path, devname);
	if (rc)
		return rc;

	if (snprintf(path, len, "%s/driver/unbind", devpath) >= len) {
		err(ctx, "%s: buffer too small!\n", devname);
		return -ENXIO;
	}

	return sysfs_write_attr(ctx, path, devname);

}

DAXCTL_EXPORT int daxctl_dev_is_enabled(struct daxctl_dev *dev)
{
	struct daxctl_ctx *ctx = daxctl_dev_get_ctx(dev);
	char *path = dev->dev_buf;
	int len = dev->buf_len;

	if (!device_model_is_dax_bus(dev))
		return 1;

	if (snprintf(path, len, "%s/driver", dev->dev_path) >= len) {
		err(ctx, "%s: buffer too small!\n",
				daxctl_dev_get_devname(dev));
		return 0;
	}

	return is_enabled(path);
}

static int daxctl_insert_kmod_for_mode(struct daxctl_dev *dev,
		const char *mod_name)
{
	const char *devname = daxctl_dev_get_devname(dev);
	struct daxctl_ctx *ctx = daxctl_dev_get_ctx(dev);
	struct kmod_module *kmod;
	int rc;

	rc = kmod_module_new_from_name(ctx->kmod_ctx, mod_name, &kmod);
	if (rc < 0) {
		err(ctx, "%s: failed getting module for: %s: %s\n",
			devname, mod_name, strerror(-rc));
		return rc;
	}

	/* if the driver is builtin, this Just Works */
	dbg(ctx, "%s inserting module: %s\n", devname,
		kmod_module_get_name(kmod));
	rc = kmod_module_probe_insert_module(kmod,
			KMOD_PROBE_APPLY_BLACKLIST,
			NULL, NULL, NULL, NULL);
	if (rc < 0) {
		err(ctx, "%s: insert failure: %d\n", devname, rc);
		return rc;
	}
	dev->module = kmod;

	return 0;
}

static int daxctl_dev_enable(struct daxctl_dev *dev, enum daxctl_dev_mode mode)
{
	struct daxctl_region *region = daxctl_dev_get_region(dev);
	const char *devname = daxctl_dev_get_devname(dev);
	struct daxctl_ctx *ctx = daxctl_dev_get_ctx(dev);
	const char *mod_name = dax_modules[mode];
	int rc;

	if (!device_model_is_dax_bus(dev)) {
		err(ctx, "%s: error: device model is dax-class\n", devname);
		err(ctx, "%s: see man daxctl-migrate-device-model\n", devname);
		return -EOPNOTSUPP;
	}

	if (daxctl_dev_is_enabled(dev))
		return 0;

	if (mode >= DAXCTL_DEV_MODE_END || mod_name == NULL) {
		err(ctx, "%s: Invalid mode: %d\n", devname, mode);
		return -EINVAL;
	}

	rc = daxctl_insert_kmod_for_mode(dev, mod_name);
	if (rc)
		return rc;

	rc = daxctl_bind(ctx, devname, mod_name);
	if (!daxctl_dev_is_enabled(dev)) {
		err(ctx, "%s: failed to enable\n", devname);
		return rc ? rc : -ENXIO;
	}

	region->devices_init = 0;
	dax_devices_init(region);
	rc = 0;
	dbg(ctx, "%s: enabled\n", devname);
	return rc;
}

DAXCTL_EXPORT int daxctl_dev_enable_devdax(struct daxctl_dev *dev)
{
	return daxctl_dev_enable(dev, DAXCTL_DEV_MODE_DEVDAX);
}

DAXCTL_EXPORT int daxctl_dev_enable_ram(struct daxctl_dev *dev)
{
	return daxctl_dev_enable(dev, DAXCTL_DEV_MODE_RAM);
}

DAXCTL_EXPORT int daxctl_dev_disable(struct daxctl_dev *dev)
{
	const char *devname = daxctl_dev_get_devname(dev);
	struct daxctl_ctx *ctx = daxctl_dev_get_ctx(dev);

	if (!device_model_is_dax_bus(dev)) {
		err(ctx, "%s: error: device model is dax-class\n", devname);
		err(ctx, "%s: see man daxctl-migrate-device-model\n", devname);
		return -EOPNOTSUPP;
	}

	if (!daxctl_dev_is_enabled(dev))
		return 0;

	/* If there is a memory object, first free that */
	free_mem(dev);

	daxctl_unbind(ctx, dev->dev_path);

	if (daxctl_dev_is_enabled(dev)) {
		err(ctx, "%s: failed to disable\n", devname);
		return -EBUSY;
	}

	kmod_module_unref(dev->module);
	dbg(ctx, "%s: disabled\n", devname);

	return 0;
}

DAXCTL_EXPORT struct daxctl_ctx *daxctl_dev_get_ctx(struct daxctl_dev *dev)
{
	return dev->region->ctx;
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

DAXCTL_EXPORT struct daxctl_region *daxctl_region_get_first(
		struct daxctl_ctx *ctx)
{
	dax_regions_init(ctx);

	return list_top(&ctx->regions, struct daxctl_region, list);
}

DAXCTL_EXPORT struct daxctl_region *daxctl_region_get_next(
		struct daxctl_region *region)
{
	struct daxctl_ctx *ctx = region->ctx;

	return list_next(&ctx->regions, region, list);
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

DAXCTL_EXPORT unsigned long long daxctl_dev_get_resource(struct daxctl_dev *dev)
{
	return dev->resource;
}

DAXCTL_EXPORT unsigned long long daxctl_dev_get_size(struct daxctl_dev *dev)
{
	return dev->size;
}

DAXCTL_EXPORT int daxctl_dev_set_size(struct daxctl_dev *dev, unsigned long long size)
{
	struct daxctl_ctx *ctx = daxctl_dev_get_ctx(dev);
	char buf[SYSFS_ATTR_SIZE];
	char *path = dev->dev_buf;
	int len = dev->buf_len;

	if (snprintf(path, len, "%s/size", dev->dev_path) >= len) {
		err(ctx, "%s: buffer too small!\n",
				daxctl_dev_get_devname(dev));
		return -ENXIO;
	}

	sprintf(buf, "%#llx\n", size);
	if (sysfs_write_attr(ctx, path, buf) < 0) {
		err(ctx, "%s: failed to set size\n",
				daxctl_dev_get_devname(dev));
		return -ENXIO;
	}

	dev->size = size;
	return 0;
}

DAXCTL_EXPORT unsigned long daxctl_dev_get_align(struct daxctl_dev *dev)
{
	return dev->align;
}

DAXCTL_EXPORT int daxctl_dev_set_align(struct daxctl_dev *dev, unsigned long align)
{
	struct daxctl_ctx *ctx = daxctl_dev_get_ctx(dev);
	char buf[SYSFS_ATTR_SIZE];
	char *path = dev->dev_buf;
	int len = dev->buf_len;

	if (snprintf(path, len, "%s/align", dev->dev_path) >= len) {
		err(ctx, "%s: buffer too small!\n",
				daxctl_dev_get_devname(dev));
		return -ENXIO;
	}

	sprintf(buf, "%#lx\n", align);
	if (sysfs_write_attr(ctx, path, buf) < 0) {
		err(ctx, "%s: failed to set align\n",
				daxctl_dev_get_devname(dev));
		return -ENXIO;
	}

	dev->align = align;
	return 0;
}

DAXCTL_EXPORT int daxctl_dev_set_mapping(struct daxctl_dev *dev,
					unsigned long long start,
					unsigned long long end)
{
	struct daxctl_ctx *ctx = daxctl_dev_get_ctx(dev);
	unsigned long long size = end - start + 1;
	char buf[SYSFS_ATTR_SIZE];
	char *path = dev->dev_buf;
	int len = dev->buf_len;

	if (snprintf(path, len, "%s/mapping", dev->dev_path) >= len) {
		err(ctx, "%s: buffer too small!\n",
				daxctl_dev_get_devname(dev));
		return -ENXIO;
	}

	sprintf(buf, "%#llx-%#llx\n", start, end);
	if (sysfs_write_attr(ctx, path, buf) < 0) {
		err(ctx, "%s: failed to set mapping\n",
				daxctl_dev_get_devname(dev));
		return -ENXIO;
	}
	dev->size += size;

	return 0;
}

DAXCTL_EXPORT int daxctl_dev_get_target_node(struct daxctl_dev *dev)
{
	return dev->target_node;
}

DAXCTL_EXPORT struct daxctl_memory *daxctl_dev_get_memory(struct daxctl_dev *dev)
{
	if (dev->mem)
		return dev->mem;
	else
		return daxctl_dev_alloc_mem(dev);
}

DAXCTL_EXPORT struct daxctl_dev *daxctl_memory_get_dev(struct daxctl_memory *mem)
{
	return mem->dev;
}

DAXCTL_EXPORT const char *daxctl_memory_get_node_path(struct daxctl_memory *mem)
{
	return mem->node_path;
}

DAXCTL_EXPORT unsigned long daxctl_memory_get_block_size(struct daxctl_memory *mem)
{
	return mem->block_size;
}

static void mappings_init(struct daxctl_dev *dev)
{
	struct daxctl_ctx *ctx = daxctl_dev_get_ctx(dev);
	char buf[SYSFS_ATTR_SIZE];
	char *path = dev->dev_buf;
	int i;

	if (dev->num_mappings != -1)
		return;

	dev->num_mappings = 0;
	for (;;) {
		struct daxctl_mapping *mapping;
		unsigned long long pgoff, start, end;

		i = dev->num_mappings;
		mapping = calloc(1, sizeof(*mapping));
		if (!mapping) {
			err(ctx, "%s: mapping%u allocation failure\n",
				daxctl_dev_get_devname(dev), i);
			continue;
		}

		sprintf(path, "%s/mapping%d/start", dev->dev_path, i);
		if (sysfs_read_attr(ctx, path, buf) < 0) {
			free(mapping);
			break;
		}
		start = strtoull(buf, NULL, 0);

		sprintf(path, "%s/mapping%d/end", dev->dev_path, i);
		if (sysfs_read_attr(ctx, path, buf) < 0) {
			free(mapping);
			break;
		}
		end = strtoull(buf, NULL, 0);

		sprintf(path, "%s/mapping%d/page_offset", dev->dev_path, i);
		if (sysfs_read_attr(ctx, path, buf) < 0) {
			free(mapping);
			break;
		}
		pgoff = strtoull(buf, NULL, 0);

		mapping->dev = dev;
		mapping->start = start;
		mapping->end = end;
		mapping->pgoff = pgoff;

		dev->num_mappings++;
		list_add(&dev->mappings, &mapping->list);
	}
}

DAXCTL_EXPORT struct daxctl_mapping *daxctl_mapping_get_first(struct daxctl_dev *dev)
{
	mappings_init(dev);

	return list_top(&dev->mappings, struct daxctl_mapping, list);
}

DAXCTL_EXPORT struct daxctl_mapping *daxctl_mapping_get_next(struct daxctl_mapping *mapping)
{
	struct daxctl_dev *dev = mapping->dev;

	return list_next(&dev->mappings, mapping, list);
}

DAXCTL_EXPORT unsigned long long daxctl_mapping_get_start(struct daxctl_mapping *mapping)
{
	return mapping->start;
}

DAXCTL_EXPORT unsigned long long daxctl_mapping_get_end(struct daxctl_mapping *mapping)
{
	return mapping->end;
}

DAXCTL_EXPORT unsigned long long  daxctl_mapping_get_offset(struct daxctl_mapping *mapping)
{
	return mapping->pgoff;
}

DAXCTL_EXPORT unsigned long long daxctl_mapping_get_size(struct daxctl_mapping *mapping)
{
	return mapping->end - mapping->start + 1;
}

static int memblock_is_online(struct daxctl_memory *mem, char *memblock)
{
	struct daxctl_dev *dev = daxctl_memory_get_dev(mem);
	const char *devname = daxctl_dev_get_devname(dev);
	struct daxctl_ctx *ctx = daxctl_dev_get_ctx(dev);
	int len = mem->buf_len, rc;
	char buf[SYSFS_ATTR_SIZE];
	char *path = mem->mem_buf;
	const char *node_path;

	node_path = daxctl_memory_get_node_path(mem);
	if (!node_path)
		return -ENXIO;

	rc = snprintf(path, len, "%s/%s/state", node_path, memblock);
	if (rc < 0)
		return -ENOMEM;

	rc = sysfs_read_attr(ctx, path, buf);
	if (rc) {
		err(ctx, "%s: Failed to read %s: %s\n",
			devname, path, strerror(-rc));
		return rc;
	}

	if (strncmp(buf, "online", 6) == 0)
		return 1;

	/* offline */
	return 0;
}

static int online_one_memblock(struct daxctl_memory *mem, char *memblock,
		enum memory_zones zone, int *status)
{
	struct daxctl_dev *dev = daxctl_memory_get_dev(mem);
	struct daxctl_ctx *ctx = daxctl_dev_get_ctx(dev);
	int len = mem->buf_len, rc;
	char *path = mem->mem_buf;
	const char *node_path;

	node_path = daxctl_memory_get_node_path(mem);
	if (!node_path)
		return -ENXIO;

	rc = snprintf(path, len, "%s/%s/state", node_path, memblock);
	if (rc < 0)
		return -ENOMEM;

	rc = memblock_is_online(mem, memblock);
	if (rc)
		return rc;

	switch (zone) {
	case MEM_ZONE_MOVABLE:
	case MEM_ZONE_NORMAL:
		rc = sysfs_write_attr_quiet(ctx, path, state_strings[zone]);
		break;
	default:
		rc = -EINVAL;
	}
	if (rc) {
		/*
		 * If the block got onlined, potentially by some other agent,
		 * do nothing for now. There will be a full scan for zone
		 * correctness later.
		 */
		if (memblock_is_online(mem, memblock) == 1)
			return 0;
	}

	return rc;
}

static int offline_one_memblock(struct daxctl_memory *mem, char *memblock)
{
	struct daxctl_dev *dev = daxctl_memory_get_dev(mem);
	const char *devname = daxctl_dev_get_devname(dev);
	struct daxctl_ctx *ctx = daxctl_dev_get_ctx(dev);
	const char *mode = "offline";
	int len = mem->buf_len, rc;
	char *path = mem->mem_buf;
	const char *node_path;

	node_path = daxctl_memory_get_node_path(mem);
	if (!node_path)
		return -ENXIO;

	rc = snprintf(path, len, "%s/%s/state", node_path, memblock);
	if (rc < 0)
		return -ENOMEM;

	/* if already offline, there is nothing to do */
	rc = memblock_is_online(mem, memblock);
	if (rc < 0)
		return rc;
	if (!rc)
		return 1;

	rc = sysfs_write_attr_quiet(ctx, path, mode);
	if (rc) {
		/* check if something raced us to offline (unlikely) */
		if (!memblock_is_online(mem, memblock))
			return 1;
		err(ctx, "%s: Failed to offline %s: %s\n",
			devname, path, strerror(-rc));
	}
	return rc;
}

static int memblock_find_zone(struct daxctl_memory *mem, char *memblock,
		int *status)
{
	struct daxctl_dev *dev = daxctl_memory_get_dev(mem);
	const char *devname = daxctl_dev_get_devname(dev);
	struct daxctl_ctx *ctx = daxctl_dev_get_ctx(dev);
	enum memory_zones cur_zone;
	int len = mem->buf_len, rc;
	char buf[SYSFS_ATTR_SIZE];
	char *path = mem->mem_buf;
	const char *node_path;

	rc = memblock_is_online(mem, memblock);
	if (rc < 0)
		return rc;
	if (rc == 0)
		return -ENXIO;

	node_path = daxctl_memory_get_node_path(mem);
	if (!node_path)
		return -ENXIO;

	rc = snprintf(path, len, "%s/%s/valid_zones", node_path, memblock);
	if (rc < 0)
		return -ENOMEM;

	rc = sysfs_read_attr(ctx, path, buf);
	if (rc) {
		err(ctx, "%s: Failed to read %s: %s\n",
			devname, path, strerror(-rc));
		return rc;
	}

	if (strcmp(buf, zone_strings[MEM_ZONE_MOVABLE]) == 0)
		cur_zone = MEM_ZONE_MOVABLE;
	else if (strcmp(buf, zone_strings[MEM_ZONE_NORMAL]) == 0)
		cur_zone = MEM_ZONE_NORMAL;
	else
		cur_zone = MEM_ZONE_UNKNOWN;

	if (mem->zone) {
		if (mem->zone == cur_zone)
			return 0;
		else
			*status |= MEM_ST_ZONE_INCONSISTENT;
	} else {
		mem->zone = cur_zone;
	}

	return 0;
}

static int memblock_in_dev(struct daxctl_memory *mem, const char *memblock)
{
	const char *mem_base = "/sys/devices/system/memory/";
	struct daxctl_dev *dev = daxctl_memory_get_dev(mem);
	unsigned long long memblock_res, dev_start, dev_end;
	const char *devname = daxctl_dev_get_devname(dev);
	struct daxctl_ctx *ctx = daxctl_dev_get_ctx(dev);
	int rc, path_len = mem->buf_len;
	unsigned long memblock_size;
	char buf[SYSFS_ATTR_SIZE];
	unsigned long phys_index;
	char *path = mem->mem_buf;

	if (snprintf(path, path_len, "%s/%s/phys_index",
			mem_base, memblock) < 0)
		return -ENXIO;

	rc = sysfs_read_attr(ctx, path, buf);
	if (rc == 0) {
		phys_index = strtoul(buf, NULL, 16);
		if (phys_index == ULONG_MAX) {
			rc = -errno;
			err(ctx, "%s: %s: Unable to determine phys_index: %s\n",
				devname, memblock, strerror(-rc));
			return rc;
		}
	} else {
		err(ctx, "%s: %s: Unable to determine phys_index: %s\n",
			devname, memblock, strerror(-rc));
		return rc;
	}

	dev_start = daxctl_dev_get_resource(dev);
	if (!dev_start) {
		err(ctx, "%s: Unable to determine resource\n", devname);
		return -EACCES;
	}
	dev_end = dev_start + daxctl_dev_get_size(dev);

	memblock_size = daxctl_memory_get_block_size(mem);
	if (!memblock_size) {
		err(ctx, "%s: Unable to determine memory block size\n",
			devname);
		return -ENXIO;
	}
	memblock_res = phys_index * memblock_size;

	if (memblock_res >= dev_start && memblock_res <= dev_end)
		return 1;

	return 0;
}

static int op_for_one_memblock(struct daxctl_memory *mem, char *memblock,
		enum memory_op op, int *status)
{
	struct daxctl_dev *dev = daxctl_memory_get_dev(mem);
	const char *devname = daxctl_dev_get_devname(dev);
	struct daxctl_ctx *ctx = daxctl_dev_get_ctx(dev);
	int rc;

	switch (op) {
	case MEM_SET_ONLINE:
		return online_one_memblock(mem, memblock, MEM_ZONE_MOVABLE,
				status);
	case MEM_SET_ONLINE_NO_MOVABLE:
		return online_one_memblock(mem, memblock, MEM_ZONE_NORMAL,
				status);
	case MEM_SET_OFFLINE:
		return offline_one_memblock(mem, memblock);
	case MEM_IS_ONLINE:
		rc = memblock_is_online(mem, memblock);
		if (rc < 0)
			return rc;
		/*
		 * Retain the 'normal' semantics for if (memblock_is_online()),
		 * but since count needs rc == 0, we'll just flip rc for this op
		 */
		return !rc;
	case MEM_COUNT:
		return 0;
	case MEM_GET_ZONE:
		return memblock_find_zone(mem, memblock, status);
	}

	err(ctx, "%s: BUG: unknown op: %d\n", devname, op);
	return -EINVAL;
}

static int daxctl_memory_op(struct daxctl_memory *mem, enum memory_op op)
{
	struct daxctl_dev *dev = daxctl_memory_get_dev(mem);
	const char *devname = daxctl_dev_get_devname(dev);
	struct daxctl_ctx *ctx = daxctl_dev_get_ctx(dev);
	int rc, count = 0, status_flags = 0;
	const char *node_path;
	struct dirent *de;
	DIR *node_dir;

	node_path = daxctl_memory_get_node_path(mem);
	if (!node_path) {
		err(ctx, "%s: Failed to get node_path\n", devname);
		return -ENXIO;
	}

	node_dir = opendir(node_path);
	if (!node_dir)
		return -errno;

	errno = 0;
	while ((de = readdir(node_dir)) != NULL) {
		if (strncmp(de->d_name, "memory", 6) == 0) {
			rc = memblock_in_dev(mem, de->d_name);
			if (rc < 0)
				goto out_dir;
			if (rc == 0) /* memblock not in dev */
				continue;
			/* memblock is in dev, perform op */
			rc = op_for_one_memblock(mem, de->d_name, op,
					&status_flags);
			if (rc < 0)
				goto out_dir;
			if (rc == 0)
				count++;
		}
		errno = 0;
	}

	if (status_flags & MEM_ST_ZONE_INCONSISTENT)
		mem->zone = MEM_ZONE_UNKNOWN;

	if (errno) {
		rc = -errno;
		goto out_dir;
	}
	rc = count;

out_dir:
	closedir(node_dir);
	return rc;
}

/*
 * daxctl_memory_online() will online to ZONE_MOVABLE by default
 */
static int daxctl_memory_online_with_zone(struct daxctl_memory *mem,
		enum memory_zones zone)
{
	struct daxctl_dev *dev = daxctl_memory_get_dev(mem);
	const char *devname = daxctl_dev_get_devname(dev);
	struct daxctl_ctx *ctx = daxctl_dev_get_ctx(dev);
	int rc;

	switch (zone) {
	case MEM_ZONE_MOVABLE:
		rc = daxctl_memory_op(mem, MEM_SET_ONLINE);
		break;
	case MEM_ZONE_NORMAL:
		rc = daxctl_memory_op(mem, MEM_SET_ONLINE_NO_MOVABLE);
		break;
	default:
		err(ctx, "%s: BUG: invalid zone for onlining\n", devname);
		rc = -EINVAL;
	}
	if (rc)
		return rc;

	/*
	 * Detect any potential races when blocks were being brought online by
	 * checking the zone in which the memory blocks are at this point. If
	 * any of the blocks are not in ZONE_MOVABLE, emit a warning.
	 */
	mem->zone = 0;
	rc = daxctl_memory_op(mem, MEM_GET_ZONE);
	if (rc)
		return rc;
	if (mem->zone != zone) {
		err(ctx,
		    "%s:\n  WARNING: detected a race while onlining memory\n"
		    "  See 'man daxctl-reconfigure-device' for more details\n",
		    devname);
		return -EBUSY;
	}

	return rc;
}

DAXCTL_EXPORT int daxctl_memory_online(struct daxctl_memory *mem)
{
	return daxctl_memory_online_with_zone(mem, MEM_ZONE_MOVABLE);
}

DAXCTL_EXPORT int daxctl_memory_online_no_movable(struct daxctl_memory *mem)
{
	return daxctl_memory_online_with_zone(mem, MEM_ZONE_NORMAL);
}

DAXCTL_EXPORT int daxctl_memory_offline(struct daxctl_memory *mem)
{
	return daxctl_memory_op(mem, MEM_SET_OFFLINE);
}

DAXCTL_EXPORT int daxctl_memory_is_online(struct daxctl_memory *mem)
{
	return daxctl_memory_op(mem, MEM_IS_ONLINE);
}

DAXCTL_EXPORT int daxctl_memory_num_sections(struct daxctl_memory *mem)
{
	return daxctl_memory_op(mem, MEM_COUNT);
}

DAXCTL_EXPORT int daxctl_memory_is_movable(struct daxctl_memory *mem)
{
	int rc;

	/* Start a fresh zone scan, clear any previous info */
	mem->zone = 0;
	rc = daxctl_memory_op(mem, MEM_GET_ZONE);
	if (rc < 0)
		return rc;
	return (mem->zone == MEM_ZONE_MOVABLE) ? 1 : 0;
}
