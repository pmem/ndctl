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
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ccan/list/list.h>

#include <ndctl/libndctl.h>
#include "libndctl-private.h"

/**
 * DOC: General note, the structure layouts are privately defined.
 * Access struct member fields with ndctl_<object>_get_<property>.
 */

struct ndctl_ctx;
/**
 * struct ndctl_bus - a nfit table instance
 * @major: control character device major number
 * @minor: control character device minor number
 * @format: format-interface-code number (see NFIT spec)
 * @revision: NFIT table revision number
 * @provider: identifier for the source of the NFIT table
 *
 * The expectation is one NFIT/nd bus per system provided by platform
 * firmware (for example @provider == "ACPI.NFIT").  However, the
 * nfit_test module provides multiple test busses with provider names of
 * the format "nfit_test.N"
 */
struct ndctl_bus {
	struct ndctl_ctx *ctx;
	int id, major, minor, revision;
	short format;
	char *provider;
	struct list_head dimms;
	struct list_head regions;
	struct list_node list;
	int dimms_init;
	int regions_init;
	char *bus_path;
	char *wait_probe_path;
};

/**
 * struct ndctl_dimm - memory device as identified by NFIT
 * @handle: NFIT-handle value to be used for ioctl calls
 * @phys_id: SMBIOS physical id
 * @vendor_id: hardware component vendor
 * @device_id: hardware device id
 * @revision_id: hardware revision id
 * @format_id: format interface code number
 * @node: system node-id
 * @socket: socket-id in the node
 * @imc: memory-controller-id in the socket
 * @channel: channel-id in the memory-controller
 * @dimm: dimm-id in the channel
 */
struct ndctl_dimm {
	struct ndctl_bus *bus;
	unsigned int handle;
	unsigned short phys_id;
	unsigned short vendor_id;
	unsigned short device_id;
	unsigned short revision_id;
	unsigned short format_id;
	int id;
	struct list_node list;
};

/**
 * struct ndctl_mapping - dimm extent relative to a region
 * @dimm: backing dimm for the mapping
 * @offset: dimm relative offset
 * @length: span of the extent
 *
 * This data can be used to identify the dimm ranges contributing to a
 * region / interleave-set and identify how regions alias each other.
 */
struct ndctl_mapping {
	struct ndctl_region *region;
	struct ndctl_dimm *dimm;
	unsigned long long offset, length;
	struct list_node list;
};

/**
 * struct ndctl_region - container for 'pmem' or 'block' capacity
 * @interleave_ways: number of dimms in region
 * @mappings: number of extent ranges contributing to the region
 * @size: total capacity of the region before resolving aliasing
 * @type: integer nd-bus device-type
 * @type_name: 'pmem' or 'block'
 *
 * A region may alias between pmem and block-window access methods.  The
 * region driver is tasked with parsing the label (if their is one) and
 * coordinating configuration with peer regions.
 */
struct ndctl_region {
	struct ndctl_bus *bus;
	int id, interleave_ways, num_mappings, nstype, spa_index;
	int mappings_init;
	int namespaces_init;
	unsigned long long size;
	char *region_path;
	struct list_head mappings;
	struct list_head namespaces;
	struct list_node list;
};

/**
 * struct ndctl_namespace - device claimed by the nd_blk or nd_pmem driver
 * @module: kernel module
 * @type: integer nd-bus device-type
 * @type_name: 'namespace_io', 'namespace_pmem', or 'namespace_block'
 * @namespace_path: devpath for namespace device
 *
 * A 'namespace' is the resulting device after region-aliasing and
 * label-parsing is resolved.
 */
struct ndctl_namespace {
	struct kmod_module *module;
	struct ndctl_region *region;
	struct list_node list;
	char *ndns_path;
	char *ndns_buf;
	int type, id, buf_len;
};

/**
 * struct ndctl_ctx - library user context to find "nd_bus" instances
 *
 * Instantiate with ndctl_new(), which takes an initial reference.  Free
 * the context by dropping the reference count to zero with
 * ndctrl_unref(), or take additional references with ndctl_ref()
 */
struct ndctl_ctx {
	int refcount;
	void (*log_fn)(struct ndctl_ctx *ctx,
			int priority, const char *file, int line, const char *fn,
			const char *format, va_list args);
	void *userdata;
	int log_priority;
	struct list_head busses;
	int busses_init;
	struct udev *udev;
	struct udev_queue *udev_queue;
	struct kmod_ctx *kmod_ctx;
};

void ndctl_log(struct ndctl_ctx *ctx,
		int priority, const char *file, int line, const char *fn,
		const char *format, ...)
{
	va_list args;

	va_start(args, format);
	ctx->log_fn(ctx, priority, file, line, fn, format, args);
	va_end(args);
}

static void log_stderr(struct ndctl_ctx *ctx,
		int priority, const char *file, int line, const char *fn,
		const char *format, va_list args)
{
	fprintf(stderr, "libndctl: %s: ", fn);
	vfprintf(stderr, format, args);
}

/**
 * ndctl_get_userdata - retrieve stored data pointer from library context
 * @ctx: ndctl library context
 *
 * This might be useful to access from callbacks like a custom logging
 * function.
 */
NDCTL_EXPORT void *ndctl_get_userdata(struct ndctl_ctx *ctx)
{
	if (ctx == NULL)
		return NULL;
	return ctx->userdata;
}

/**
 * ndctl_set_userdata - store custom @userdata in the library context
 * @ctx: ndctl library context
 * @userdata: data pointer
 */
NDCTL_EXPORT void ndctl_set_userdata(struct ndctl_ctx *ctx, void *userdata)
{
	if (ctx == NULL)
		return;
	ctx->userdata = userdata;
}

static int log_priority(const char *priority)
{
	char *endptr;
	int prio;

	prio = strtol(priority, &endptr, 10);
	if (endptr[0] == '\0' || isspace(endptr[0]))
		return prio;
	if (strncmp(priority, "err", 3) == 0)
		return LOG_ERR;
	if (strncmp(priority, "info", 4) == 0)
		return LOG_INFO;
	if (strncmp(priority, "debug", 5) == 0)
		return LOG_DEBUG;
	return 0;
}

NDCTL_EXPORT int ndctl_new(struct ndctl_ctx **ctx)
{
	struct kmod_ctx *kmod_ctx;
	const char *cfg = NULL;
	struct ndctl_ctx *c;
	struct udev *udev;
	const char *env;
	int rc = 0;

	udev = udev_new();
	if (check_udev(udev) != 0)
		return -ENXIO;

	kmod_ctx = kmod_new(NULL, &cfg);
	if (check_kmod(kmod_ctx) != 0) {
		rc = -ENXIO;
		goto err_kmod;
	}

	c = calloc(1, sizeof(struct ndctl_ctx));
	if (!c) {
		rc = -ENOMEM;
		goto err_ctx;
	}

	c->refcount = 1;
	c->log_fn = log_stderr;
	c->log_priority = LOG_ERR;
	c->udev = udev;
	list_head_init(&c->busses);

	/* environment overwrites config */
	env = secure_getenv("NDCTL_LOG");
	if (env != NULL)
		ndctl_set_log_priority(c, log_priority(env));

	info(c, "ctx %p created\n", c);
	dbg(c, "log_priority=%d\n", c->log_priority);
	*ctx = c;

	if (udev) {
		c->udev = udev;
		c->udev_queue = udev_queue_new(udev);
		if (!c->udev_queue)
			err(c, "failed to retrieve udev queue\n");
	}

	c->kmod_ctx = kmod_ctx;

	return 0;
 err_ctx:
	kmod_unref(kmod_ctx);
 err_kmod:
	udev_unref(udev);
	return rc;
}

NDCTL_EXPORT struct ndctl_ctx *ndctl_ref(struct ndctl_ctx *ctx)
{
	if (ctx == NULL)
		return NULL;
	ctx->refcount++;
	return ctx;
}

static void free_namespace(struct ndctl_namespace *ndns)
{
	free(ndns->ndns_path);
	free(ndns->ndns_buf);
	kmod_module_unref(ndns->module);
	free(ndns);
}

static void free_region(struct ndctl_region *region)
{
	struct ndctl_bus *bus = region->bus;
	struct ndctl_mapping *mapping, *_m;
	struct ndctl_namespace *ndns, *_n;

	list_for_each_safe(&region->mappings, mapping, _m, list) {
		list_del_from(&region->mappings, &mapping->list);
		free(mapping);
	}
	list_for_each_safe(&region->namespaces, ndns, _n, list) {
		list_del_from(&region->namespaces, &ndns->list);
		free_namespace(ndns);
	}
	list_del_from(&bus->regions, &region->list);
	free(region->region_path);
	free(region);
}

static void free_bus(struct ndctl_bus *bus)
{
	struct ndctl_dimm *dimm, *_d;
	struct ndctl_region *region, *_r;

	list_for_each_safe(&bus->dimms, dimm, _d, list) {
		list_del_from(&bus->dimms, &dimm->list);
		free(dimm);
	}
	list_for_each_safe(&bus->regions, region, _r, list)
		free_region(region);
	list_del_from(&bus->ctx->busses, &bus->list);
	free(bus->provider);
	free(bus->bus_path);
	free(bus->wait_probe_path);
	free(bus);
}

static void free_context(struct ndctl_ctx *ctx)
{
	struct ndctl_bus *bus, *_b;

	list_for_each_safe(&ctx->busses, bus, _b, list)
		free_bus(bus);
	free(ctx);
}

NDCTL_EXPORT struct ndctl_ctx *ndctl_unref(struct ndctl_ctx *ctx)
{
	if (ctx == NULL)
		return NULL;
	ctx->refcount--;
	if (ctx->refcount > 0)
		return NULL;
	udev_queue_unref(ctx->udev_queue);
	udev_unref(ctx->udev);
	info(ctx, "context %p released\n", ctx);
	free_context(ctx);
	return NULL;
}

/**
 * ndctl_set_log_fn - override default log routine
 * @ctx: ndctl library context
 * @log_fn: function to be called for logging messages
 *
 * The built-in logging writes to stderr. It can be overridden by a
 * custom function, to plug log messages into the user's logging
 * functionality.
 */
NDCTL_EXPORT void ndctl_set_log_fn(struct ndctl_ctx *ctx,
		void (*log_fn)(struct ndctl_ctx *ctx,
			int priority, const char *file,
			int line, const char *fn,
			const char *format, va_list args))
{
	ctx->log_fn = log_fn;
	info(ctx, "custom logging function %p registered\n", log_fn);
}

NDCTL_EXPORT int ndctl_get_log_priority(struct ndctl_ctx *ctx)
{
	return ctx->log_priority;
}

/**
 * ndctl_set_log_priority - set log verbosity
 * @priority: from syslog.h, LOG_ERR, LOG_INFO, LOG_DEBUG
 *
 * Note: LOG_DEBUG requires library be built with "configure --enable-debug"
 */
NDCTL_EXPORT void ndctl_set_log_priority(struct ndctl_ctx *ctx, int priority)
{
	ctx->log_priority = priority;
}

#define SYSFS_ATTR_SIZE 1024

static int sysfs_read_attr(struct ndctl_ctx *ctx, char *path, char *buf)
{
	int fd = open(path, O_RDONLY|O_CLOEXEC);
	int n;

	if (fd < 0) {
		dbg(ctx, "failed to open %s: %s\n", path, strerror(errno));
		return -1;
	}
	n = read(fd, buf, SYSFS_ATTR_SIZE);
	close(fd);
	if (n < 0 || n >= SYSFS_ATTR_SIZE) {
		dbg(ctx, "failed to read %s: %s\n", path, strerror(errno));
		return -1;
	}
	buf[n] = 0;
	if (n && buf[n-1] == '\n')
		buf[n-1] = 0;
	return 0;
}

static int sysfs_write_attr(struct ndctl_ctx *ctx, char *path, const char *buf)
{
	int fd = open(path, O_WRONLY|O_CLOEXEC);
	int n, len = strlen(buf) + 1;

	if (fd < 0) {
		dbg(ctx, "failed to open %s: %s\n", path, strerror(errno));
		return -1;
	}
	n = write(fd, buf, len);
	close(fd);
	if (n < len) {
		dbg(ctx, "failed to write %s: %s\n", path, strerror(errno));
		return -1;
	}
	return 0;
}

static char *__dev_path(char *type, int major, int minor, int parent)
{
	char *path, *dev_path;

	if (asprintf(&path, "/sys/dev/%s/%d:%d%s", type, major, minor,
				parent ? "/device" : "") < 0)
		return NULL;

	dev_path = realpath(path, NULL);
	free(path);
	return dev_path;
}

static char *parent_dev_path(char *type, int major, int minor)
{
        return __dev_path(type, major, minor, 1);
}

typedef int (*add_dev_fn)(void *parent, int id, const char *dev_path);

static int device_parse(struct ndctl_ctx *ctx, const char *base_path,
		const char *dev_name, void *parent, add_dev_fn add_dev)
{
	int add_errors = 0;
	struct dirent *de;
	DIR *dir;

	dir = opendir(base_path);
	if (!dir) {
		dbg(ctx, "no \"%s\" devices found\n", dev_name);
		return -ENODEV;
	}

	while ((de = readdir(dir)) != NULL) {
		char *dev_path;
		char fmt[20];
		int id, rc;

		sprintf(fmt, "%s%%d", dev_name);
		if (de->d_ino == 0)
			continue;
		if (sscanf(de->d_name, fmt, &id) != 1)
			continue;
		if (asprintf(&dev_path, "%s/%s", base_path, de->d_name) < 0) {
			err(ctx, "%s%d: path allocation failure\n",
					dev_name, id);
			continue;
		}

		rc = add_dev(parent, id, dev_path);
		free(dev_path);
		if (rc) {
			add_errors++;
			err(ctx, "%s%d: add_dev() failed: %d\n",
					dev_name, id, rc);
		} else {
			dbg(ctx, "%s%d: added\n", dev_name, id);
		}
	}
	closedir(dir);

	return add_errors;
}


static int add_bus(void *parent, int id, const char *ctl_base)
{
	int rc = -ENOMEM;
	struct ndctl_bus *bus;
	char buf[SYSFS_ATTR_SIZE];
	struct ndctl_ctx *ctx = parent;
	char *path = calloc(1, strlen(ctl_base) + 20);

	if (!path)
		return -ENOMEM;

	bus = calloc(1, sizeof(*bus));
	if (!bus)
		goto err_bus;
	list_head_init(&bus->dimms);
	list_head_init(&bus->regions);
	bus->ctx = ctx;

	rc = -EINVAL;
	sprintf(path, "%s/dev", ctl_base);
	if (sysfs_read_attr(ctx, path, buf) < 0
			|| sscanf(buf, "%d:%d", &bus->major, &bus->minor) != 2)
		goto err_read;

	sprintf(path, "%s/format", ctl_base);
	if (sysfs_read_attr(ctx, path, buf) < 0)
		bus->format = -1;
	else
		bus->format = strtoul(buf, NULL, 0);

	sprintf(path, "%s/revision", ctl_base);
	if (sysfs_read_attr(ctx, path, buf) < 0)
		goto err_read;
	bus->revision = strtoul(buf, NULL, 0);

	sprintf(path, "%s/provider", ctl_base);
	if (sysfs_read_attr(ctx, path, buf) < 0)
		goto err_read;

	rc = -ENOMEM;
	bus->provider = strdup(buf);
	if (!bus->provider)
		goto err_read;

	sprintf(path, "%s/wait_probe", ctl_base);
	bus->wait_probe_path = strdup(path);
	if (!bus->wait_probe_path)
		goto err_read;

	bus->bus_path = parent_dev_path("char", bus->major, bus->minor);
	if (!bus->bus_path)
		goto err_dev_path;
	list_add(&ctx->busses, &bus->list);

	rc = 0;
	goto out;

 err_dev_path:
 err_read:
	free(bus->provider);
	free(bus);
 out:
 err_bus:
	free(path);

	return rc;
}

static void busses_init(struct ndctl_ctx *ctx)
{
	if (ctx->busses_init)
		return;
	ctx->busses_init = 1;

	device_parse(ctx, "/sys/class/nd_bus", "ndctl", ctx, add_bus);
}

NDCTL_EXPORT struct ndctl_bus *ndctl_bus_get_first(struct ndctl_ctx *ctx)
{
	busses_init(ctx);

	return list_top(&ctx->busses, struct ndctl_bus, list);
}

NDCTL_EXPORT struct ndctl_bus *ndctl_bus_get_next(struct ndctl_bus *bus)
{
	struct ndctl_ctx *ctx = bus->ctx;

	return list_next(&ctx->busses, bus, list);
}

NDCTL_EXPORT unsigned int ndctl_bus_get_major(struct ndctl_bus *bus)
{
	return bus->major;
}

NDCTL_EXPORT unsigned int ndctl_bus_get_minor(struct ndctl_bus *bus)
{
	return bus->minor;
}

NDCTL_EXPORT unsigned short ndctl_bus_get_format(struct ndctl_bus *bus)
{
	return bus->format;
}

NDCTL_EXPORT unsigned int ndctl_bus_get_revision(struct ndctl_bus *bus)
{
	return bus->revision;
}

NDCTL_EXPORT unsigned int ndctl_bus_get_id(struct ndctl_bus *bus)
{
	return bus->id;
}

NDCTL_EXPORT const char *ndctl_bus_get_provider(struct ndctl_bus *bus)
{
	return bus->provider;
}

struct ndctl_ctx *ndctl_bus_get_ctx(struct ndctl_bus *bus)
{
	return bus->ctx;
}

/**
 * ndctl_bus_wait_probe - flush bus async probing
 * @bus: bus to sync
 *
 * Upon return this bus's dimm and region devices are probed and the
 * region child namespace devices are registered
 */
NDCTL_EXPORT int ndctl_bus_wait_probe(struct ndctl_bus *bus)
{
	char buf[SYSFS_ATTR_SIZE];
	int rc = sysfs_read_attr(bus->ctx, bus->wait_probe_path, buf);

	return rc < 0 ? -ENXIO : 0;
}

static int add_dimm(void *parent, int id, const char *dimm_base)
{
	int rc = -ENOMEM;
	struct ndctl_dimm *dimm;
	char buf[SYSFS_ATTR_SIZE];
	struct ndctl_bus *bus = parent;
	struct ndctl_ctx *ctx = bus->ctx;
	char *path = calloc(1, strlen(dimm_base) + 20);

	if (!path)
		return -ENOMEM;

	dimm = calloc(1, sizeof(*dimm));
	if (!dimm)
		goto err_dimm;
	dimm->bus = bus;
	dimm->id = id;

	rc = -ENXIO;
	sprintf(path, "%s/handle", dimm_base);
	if (sysfs_read_attr(ctx, path, buf) < 0)
		goto err_read;
	dimm->handle = strtoul(buf, NULL, 0);

	sprintf(path, "%s/phys_id", dimm_base);
	if (sysfs_read_attr(ctx, path, buf) < 0)
		goto err_read;
	dimm->phys_id = strtoul(buf, NULL, 0);

	sprintf(path, "%s/vendor", dimm_base);
	if (sysfs_read_attr(ctx, path, buf) < 0)
		dimm->vendor_id = -1;
	else
		dimm->vendor_id = strtoul(buf, NULL, 0);

	sprintf(path, "%s/device", dimm_base);
	if (sysfs_read_attr(ctx, path, buf) < 0)
		dimm->device_id = -1;
	else
		dimm->device_id = strtoul(buf, NULL, 0);

	sprintf(path, "%s/revision", dimm_base);
	if (sysfs_read_attr(ctx, path, buf) < 0)
		dimm->revision_id = -1;
	else
		dimm->revision_id = strtoul(buf, NULL, 0);

	sprintf(path, "%s/format", dimm_base);
	if (sysfs_read_attr(ctx, path, buf) < 0)
		dimm->format_id = -1;
	else
		dimm->format_id = strtoul(buf, NULL, 0);

	list_add(&bus->dimms, &dimm->list);
	free(path);

	return 0;

 err_read:
	free(dimm);
 err_dimm:
	free(path);
	return rc;
}

static void dimms_init(struct ndctl_bus *bus)
{
	if (bus->dimms_init)
		return;
	bus->dimms_init = 1;

	device_parse(bus->ctx, bus->bus_path, "dimm", bus, add_dimm);
}

NDCTL_EXPORT struct ndctl_dimm *ndctl_dimm_get_first(struct ndctl_bus *bus)
{
	dimms_init(bus);

	return list_top(&bus->dimms, struct ndctl_dimm, list);
}

NDCTL_EXPORT struct ndctl_dimm *ndctl_dimm_get_next(struct ndctl_dimm *dimm)
{
	struct ndctl_bus *bus = dimm->bus;

	return list_next(&bus->dimms, dimm, list);
}

NDCTL_EXPORT unsigned int ndctl_dimm_get_handle(struct ndctl_dimm *dimm)
{
	return dimm->handle;
}

NDCTL_EXPORT unsigned short ndctl_dimm_get_phys_id(struct ndctl_dimm *dimm)
{
	return dimm->phys_id;
}

NDCTL_EXPORT unsigned short ndctl_dimm_get_vendor(struct ndctl_dimm *dimm)
{
	return dimm->vendor_id;
}

NDCTL_EXPORT unsigned short ndctl_dimm_get_device(struct ndctl_dimm *dimm)
{
	return dimm->device_id;
}

NDCTL_EXPORT unsigned short ndctl_dimm_get_revision(struct ndctl_dimm *dimm)
{
	return dimm->revision_id;
}

NDCTL_EXPORT unsigned short ndctl_dimm_get_format(struct ndctl_dimm *dimm)
{
	return dimm->format_id;
}

NDCTL_EXPORT unsigned int ndctl_dimm_handle_get_node(struct ndctl_dimm *dimm)
{
	return dimm->handle >> 16 & 0xfff;
}

NDCTL_EXPORT unsigned int ndctl_dimm_handle_get_socket(struct ndctl_dimm *dimm)
{
	return dimm->handle >> 12 & 0xf;
}

NDCTL_EXPORT unsigned int ndctl_dimm_handle_get_imc(struct ndctl_dimm *dimm)
{
	return dimm->handle >> 8 & 0xf;
}

NDCTL_EXPORT unsigned int ndctl_dimm_handle_get_channel(struct ndctl_dimm *dimm)
{
	return dimm->handle >> 4 & 0xf;
}

NDCTL_EXPORT unsigned int ndctl_dimm_handle_get_dimm(struct ndctl_dimm *dimm)
{
	return dimm->handle & 0xf;
}

NDCTL_EXPORT struct ndctl_bus *ndctl_dimm_get_bus(struct ndctl_dimm *dimm)
{
	return dimm->bus;
}

NDCTL_EXPORT struct ndctl_ctx *ndctl_dimm_get_ctx(struct ndctl_dimm *dimm)
{
	return dimm->bus->ctx;
}

NDCTL_EXPORT struct ndctl_dimm *ndctl_dimm_get_by_handle(struct ndctl_bus *bus,
		unsigned int handle)
{
	struct ndctl_dimm *dimm;

	ndctl_dimm_foreach(bus, dimm)
		if (dimm->handle == handle)
			return dimm;

	return NULL;
}

static int add_region(void *parent, int id, const char *region_base)
{
	int rc = -ENOMEM;
	char buf[SYSFS_ATTR_SIZE];
	struct ndctl_region *region;
	struct ndctl_bus *bus = parent;
	struct ndctl_ctx *ctx = bus->ctx;
	char *path = calloc(1, strlen(region_base) + 20);

	if (!path)
		return -ENOMEM;

	region = calloc(1, sizeof(*region));
	if (!region)
		goto err_region;
	list_head_init(&region->mappings);
	list_head_init(&region->namespaces);
	region->bus = bus;
	region->id = id;

	rc = -EINVAL;
	sprintf(path, "%s/size", region_base);
	if (sysfs_read_attr(ctx, path, buf) < 0)
		goto err_read;
	region->size = strtoull(buf, NULL, 0);

	sprintf(path, "%s/mappings", region_base);
	if (sysfs_read_attr(ctx, path, buf) < 0)
		goto err_read;
	region->num_mappings = strtoul(buf, NULL, 0);

	sprintf(path, "%s/interleave_ways", region_base);
	if (sysfs_read_attr(ctx, path, buf) < 0)
		goto err_read;
	region->interleave_ways = strtoul(buf, NULL, 0);

	sprintf(path, "%s/nstype", region_base);
	if (sysfs_read_attr(ctx, path, buf) < 0)
		goto err_read;
	region->nstype = strtoul(buf, NULL, 0);

	sprintf(path, "%s/spa_index", region_base);
	if (region->nstype != ND_DEVICE_NAMESPACE_BLOCK) {
		if (sysfs_read_attr(ctx, path, buf) < 0)
			goto err_read;
		region->spa_index = strtoul(buf, NULL, 0);
	} else
		region->spa_index = -1;

	region->region_path = strdup(region_base);
	if (!region->region_path)
		goto err_read;
	list_add(&bus->regions, &region->list);

	free(path);
	return 0;

 err_read:
	free(region);
 err_region:
	free(path);

	return rc;
}

static void regions_init(struct ndctl_bus *bus)
{
	if (bus->regions_init)
		return;
	bus->regions_init = 1;

	device_parse(bus->ctx, bus->bus_path, "region", bus, add_region);
}

NDCTL_EXPORT struct ndctl_region *ndctl_region_get_first(struct ndctl_bus *bus)
{
	regions_init(bus);

	return list_top(&bus->regions, struct ndctl_region, list);
}

NDCTL_EXPORT struct ndctl_region *ndctl_region_get_next(struct ndctl_region *region)
{
	struct ndctl_bus *bus = region->bus;

	return list_next(&bus->regions, region, list);
}

NDCTL_EXPORT unsigned int ndctl_region_get_id(struct ndctl_region *region)
{
	return region->id;
}

NDCTL_EXPORT unsigned int ndctl_region_get_interleave_ways(struct ndctl_region *region)
{
	return region->interleave_ways;
}

NDCTL_EXPORT unsigned int ndctl_region_get_mappings(struct ndctl_region *region)
{
	return region->num_mappings;
}

NDCTL_EXPORT unsigned long long ndctl_region_get_size(struct ndctl_region *region)
{
	return region->size;
}

NDCTL_EXPORT unsigned int ndctl_region_get_spa_index(struct ndctl_region *region)
{
	return region->spa_index;
}

NDCTL_EXPORT unsigned int ndctl_region_get_type(struct ndctl_region *region)
{
	switch (region->nstype) {
	case ND_DEVICE_NAMESPACE_IO:
	case ND_DEVICE_NAMESPACE_PMEM:
		return ND_DEVICE_REGION_PMEM;
	default:
		return ND_DEVICE_REGION_BLOCK;
	}
}

static const char *ndctl_device_type_name(int type)
{
	switch (type) {
	case ND_DEVICE_DIMM:            return "dimm";
	case ND_DEVICE_REGION_PMEM:     return "pmem";
	case ND_DEVICE_REGION_BLOCK:    return "block";
	case ND_DEVICE_NAMESPACE_IO:    return "namespace_io";
	case ND_DEVICE_NAMESPACE_PMEM:  return "namespace_pmem";
	case ND_DEVICE_NAMESPACE_BLOCK: return "namespace_block";
	default:                        return "unknown";
	}
}

NDCTL_EXPORT const char *ndctl_region_get_type_name(struct ndctl_region *region)
{
	return ndctl_device_type_name(ndctl_region_get_type(region));
}

NDCTL_EXPORT struct ndctl_bus *ndctl_region_get_bus(struct ndctl_region *region)
{
	return region->bus;
}

NDCTL_EXPORT struct ndctl_ctx *ndctl_region_get_ctx(struct ndctl_region *region)
{
	return region->bus->ctx;
}

NDCTL_EXPORT struct ndctl_dimm *ndctl_region_get_first_dimm(struct ndctl_region *region)
{
	struct ndctl_bus *bus = region->bus;
	struct ndctl_dimm *dimm;

	ndctl_dimm_foreach(bus, dimm) {
		struct ndctl_mapping *mapping;

		ndctl_mapping_foreach(region, mapping)
			if (mapping->dimm == dimm)
				return dimm;
	}

	return NULL;
}

NDCTL_EXPORT struct ndctl_dimm *ndctl_region_get_next_dimm(struct ndctl_region *region,
		struct ndctl_dimm *dimm)
{
	while ((dimm = ndctl_dimm_get_next(dimm))) {
		struct ndctl_mapping *mapping;

		ndctl_mapping_foreach(region, mapping)
			if (mapping->dimm == dimm)
				return dimm;
	}

	return NULL;
}

static void mappings_init(struct ndctl_region *region)
{
	char *mapping_path, buf[SYSFS_ATTR_SIZE];
	struct ndctl_bus *bus = region->bus;
	struct ndctl_ctx *ctx = bus->ctx;
	int i;

	if (region->mappings_init)
		return;
	region->mappings_init = 1;

	mapping_path = calloc(1, strlen(region->region_path) + 20);
	if (!mapping_path) {
		err(ctx, "bus%d region%d: allocation failure\n",
				bus->id, region->id);
		return;
	}

	for (i = 0; i < region->num_mappings; i++) {
		struct ndctl_mapping *mapping;
		unsigned int node, sicd;
		char *pos, *end;

		sprintf(mapping_path, "%s/mapping%d", region->region_path, i);
		if (sysfs_read_attr(ctx, mapping_path, buf) < 0) {
			err(ctx, "bus%d region%d: failed to read mapping%d\n",
					bus->id, region->id, i);
			continue;
		}

		mapping = calloc(1, sizeof(*mapping));
		if (!mapping) {
			err(ctx, "bus%d region%d mapping%d: allocation failure\n",
					bus->id, region->id, i);
			continue;
		}

		mapping->region = region;
		pos = buf;
		end = strchr(pos, ',');
		*end = '\0';
		if (sscanf(pos, "%x:%x", &node, &sicd) != 2
				|| !(mapping->dimm = ndctl_dimm_get_by_handle(bus,
						node << 16 | sicd))) {
			err(ctx, "bus%d region%d mapping%d: dimm lookup failure\n",
					bus->id, region->id, i);
			free(mapping);
			continue;
		}

		pos = end + 1;
		end = strchr(pos, ',');
		*end = '\0';
		mapping->offset = strtoull(pos, NULL, 0);

		pos = end + 1;
		mapping->length = strtoull(pos, NULL, 0);
		list_add(&region->mappings, &mapping->list);
	}
}

NDCTL_EXPORT struct ndctl_mapping *ndctl_mapping_get_first(struct ndctl_region *region)
{
	mappings_init(region);

	return list_top(&region->mappings, struct ndctl_mapping, list);
}

NDCTL_EXPORT struct ndctl_mapping *ndctl_mapping_get_next(struct ndctl_mapping *mapping)
{
	struct ndctl_region *region = mapping->region;

	return list_next(&region->mappings, mapping, list);
}

NDCTL_EXPORT struct ndctl_dimm *ndctl_mapping_get_dimm(struct ndctl_mapping *mapping)
{
	return mapping->dimm;
}

NDCTL_EXPORT unsigned long long ndctl_mapping_get_offset(struct ndctl_mapping *mapping)
{
	return mapping->offset;
}

NDCTL_EXPORT unsigned long long ndctl_mapping_get_length(struct ndctl_mapping *mapping)
{
	return mapping->length;
}

static struct kmod_module *to_module(struct ndctl_ctx *ctx, const char *alias)
{
	struct kmod_module *mod;
	struct kmod_list *list;
	int rc;

	if (!ctx->kmod_ctx)
		return NULL;

	rc = kmod_module_new_from_lookup(ctx->kmod_ctx, alias, &list);
	if (rc < 0 || !list)
		return NULL;
	mod = kmod_module_get_module(list);
	dbg(ctx, "alias: %s module: %s\n", alias, kmod_module_get_name(mod));
	kmod_module_unref_list(list);

	return mod;
}

static int add_namespace(void *parent, int id, const char *ndns_base)
{
	char *path = calloc(1, strlen(ndns_base) + 20);
	struct ndctl_region *region = parent;
	struct ndctl_bus *bus = region->bus;
	struct ndctl_ctx *ctx = bus->ctx;
	struct ndctl_namespace *ndns;
	char buf[SYSFS_ATTR_SIZE];
	int rc = -ENOMEM;

	if (!path)
		return -ENOMEM;

	ndns = calloc(1, sizeof(*ndns));
	if (!ndns)
		goto err_namespace;
	ndns->id = id;
	ndns->region = region;

	sprintf(path, "%s/type", ndns_base);
	if (sysfs_read_attr(ctx, path, buf) < 0)
		goto err_read;
	ndns->type = strtoul(buf, NULL, 0);

	ndns->ndns_path = strdup(ndns_base);
	if (!ndns->ndns_path)
		goto err_read;

	ndns->ndns_buf = calloc(1, strlen(ndns_base) + 50);
	if (!ndns->ndns_buf)
		goto err_read;
	ndns->buf_len = strlen(ndns_base) + 50;

	sprintf(path, "%s/modalias", ndns_base);
	if (sysfs_read_attr(ctx, path, buf) < 0)
		goto err_read;

	ndns->module = to_module(ctx, buf);

	list_add(&region->namespaces, &ndns->list);
	free(path);
	return 0;

 err_read:
	free(ndns->ndns_path);
	free(ndns);
 err_namespace:
	free(path);
	return rc;
}

static void namespaces_init(struct ndctl_region *region)
{
	struct ndctl_bus *bus = region->bus;
	struct ndctl_ctx *ctx = bus->ctx;
	char ndns_fmt[20];

	if (region->namespaces_init)
		return;
	region->namespaces_init = 1;

	ndctl_bus_wait_probe(bus);
	sprintf(ndns_fmt, "namespace%d.", region->id);
	device_parse(ctx, region->region_path, ndns_fmt, region, add_namespace);
}

NDCTL_EXPORT struct ndctl_namespace *ndctl_namespace_get_first(struct ndctl_region *region)
{
	namespaces_init(region);

	return list_top(&region->namespaces, struct ndctl_namespace, list);
}

NDCTL_EXPORT struct ndctl_namespace *ndctl_namespace_get_next(struct ndctl_namespace *ndns)
{
	struct ndctl_region *region = ndns->region;

	return list_next(&region->namespaces, ndns, list);
}

NDCTL_EXPORT unsigned int ndctl_namespace_get_id(struct ndctl_namespace *ndns)
{
	return ndns->id;
}

NDCTL_EXPORT unsigned int ndctl_namespace_get_type(struct ndctl_namespace *ndns)
{
	return ndns->type;
}

NDCTL_EXPORT const char *ndctl_namespace_get_type_name(struct ndctl_namespace *ndns)
{
	return ndctl_device_type_name(ndns->type);
}

NDCTL_EXPORT struct ndctl_region *ndctl_namespace_get_region(struct ndctl_namespace *ndns)
{
	return ndns->region;
}

NDCTL_EXPORT struct ndctl_bus *ndctl_namespace_get_bus(struct ndctl_namespace *ndns)
{
	return ndns->region->bus;
}

NDCTL_EXPORT struct ndctl_ctx *ndctl_namespace_get_ctx(struct ndctl_namespace *ndns)
{
	return ndns->region->bus->ctx;
}

static const char *devpath_to_devname(const char *devpath)
{
	return strrchr(devpath, '/') + 1;
}

static const char *ndctl_namespace_devname(struct ndctl_namespace *ndns)
{
	return devpath_to_devname(ndns->ndns_path);
}

NDCTL_EXPORT int ndctl_namespace_is_enabled(struct ndctl_namespace *ndns)
{
	struct ndctl_ctx *ctx = ndctl_namespace_get_ctx(ndns);
	char *path = ndns->ndns_buf;
	int len = ndns->buf_len;
	struct stat st;

	if (snprintf(path, len, "%s/driver", ndns->ndns_path) >= len) {
		err(ctx, "%s: buffer too small!\n",
				ndctl_namespace_devname(ndns));
		return 0;
	}

	if (lstat(path, &st) < 0 || !S_ISLNK(st.st_mode))
		return 0;
	else
		return 1;
}

static int ndctl_bind(struct ndctl_ctx *ctx, struct kmod_module *module,
		const char *devname)
{
	struct dirent *de;
	int rc = -ENXIO;
	char path[200];
	DIR *dir;
	const int len = sizeof(path);

	if (!module || !devname || kmod_module_probe_insert_module(module,
				KMOD_PROBE_APPLY_BLACKLIST,
				NULL, NULL, NULL, NULL) < 0)
		return -EINVAL;

	if (snprintf(path, len, "/sys/module/%s/drivers",
				kmod_module_get_name(module)) >= len) {
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
		if (asprintf(&drv_path, "%s/%s/bind", path, de->d_name) < 0) {
			err(ctx, "%s: path allocation failure\n", devname);
			continue;
		}

		if (sysfs_write_attr(ctx, drv_path, devname) == 0)
			rc = 0;
		free(drv_path);
		if (rc == 0)
			break;
	}
	closedir(dir);

	if (rc)
		err(ctx, "%s: bind failed\n", devname);
	return rc;
}

static int ndctl_unbind(struct ndctl_ctx *ctx, const char *devpath)
{
	const char *devname = devpath_to_devname(devpath);
	char path[200];
	const int len = sizeof(path);

	if (snprintf(path, len, "%s/driver/unbind", devpath) >= len) {
		err(ctx, "%s: buffer too small!\n", devname);
		return -ENXIO;
	}

	return sysfs_write_attr(ctx, path, devname);
}

NDCTL_EXPORT int ndctl_namespace_enable(struct ndctl_namespace *ndns)
{
	struct ndctl_ctx *ctx = ndctl_namespace_get_ctx(ndns);
	const char *devname = ndctl_namespace_devname(ndns);

	if (ndctl_namespace_is_enabled(ndns))
		return 0;

	ndctl_bind(ctx, ndns->module, devname);

	if (!ndctl_namespace_is_enabled(ndns)) {
		err(ctx, "%s: failed to enable\n", devname);
		return -ENXIO;
	}

	dbg(ctx, "%s: enabled\n", devname);
	return 0;
}

NDCTL_EXPORT int ndctl_namespace_disable(struct ndctl_namespace *ndns)
{
	struct ndctl_ctx *ctx = ndctl_namespace_get_ctx(ndns);
	const char *devname = ndctl_namespace_devname(ndns);

	if (!ndctl_namespace_is_enabled(ndns))
		return 0;

	ndctl_unbind(ctx, ndns->ndns_path);

	if (ndctl_namespace_is_enabled(ndns)) {
		err(ctx, "%s: failed to disable\n", devname);
		return -EBUSY;
	}

	dbg(ctx, "%s: disabled\n", devname);
	return 0;
}
