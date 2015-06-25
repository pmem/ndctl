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
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <ccan/list/list.h>
#include <ccan/minmax/minmax.h>
#include <ccan/array_size/array_size.h>

#ifdef HAVE_NDCTL_H
#include <linux/ndctl.h>
#else
#include <ndctl.h>
#endif

#include <ndctl/libndctl.h>
#include "libndctl-private.h"

static uuid_t null_uuid;

/**
 * DOC: General note, the structure layouts are privately defined.
 * Access struct member fields with ndctl_<object>_get_<property>.  This
 * library is multithread-aware in that it supports multiple
 * simultaneous reference-counted contexts, but it is not multithread
 * safe.  Also note that there is no coordination between contexts,
 * changes made in one context instance may not be reflected in another.
 */

/**
 * ndctl_sizeof_namespace_index - min size of a namespace index block plus padding
 */
NDCTL_EXPORT size_t ndctl_sizeof_namespace_index(void)
{
	return sizeof_namespace_index();
}

/**
 * ndctl_min_namespace_size - minimum namespace size that btt suports
 */
NDCTL_EXPORT size_t ndctl_min_namespace_size(void)
{
	return NSLABEL_NAMESPACE_MIN_SIZE;
}

/**
 * ndctl_sizeof_namespace_label - single entry size in a dimm label set
 */
NDCTL_EXPORT size_t ndctl_sizeof_namespace_label(void)
{
	return sizeof(struct namespace_label);
}

struct ndctl_ctx;
/**
 * struct ndctl_bus - a nfit table instance
 * @major: control character device major number
 * @minor: control character device minor number
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
	unsigned int id, major, minor, revision;
	char *provider;
	struct list_head dimms;
	struct list_head regions;
	struct list_node list;
	int dimms_init;
	int regions_init;
	int has_nfit;
	char *bus_path;
	char *bus_buf;
	size_t buf_len;
	char *wait_probe_path;
	unsigned long dsm_mask;
};

/**
 * struct ndctl_dimm - memory device as identified by NFIT
 * @module: kernel module (libnvdimm)
 * @handle: NFIT-handle value
 * @major: /dev/nmemX major character device number
 * @minor: /dev/nmemX minor character device number
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
	struct kmod_module *module;
	struct ndctl_bus *bus;
	unsigned int handle, major, minor, serial;
	unsigned short phys_id;
	unsigned short vendor_id;
	unsigned short device_id;
	unsigned short revision_id;
	unsigned short format_id;
	unsigned long dsm_mask;
	char *dimm_path;
	char *dimm_buf;
	int buf_len;
	int id;
	union {
		unsigned long flags;
		struct {
			unsigned int f_arm:1;
			unsigned int f_save:1;
			unsigned int f_flush:1;
			unsigned int f_smart:1;
			unsigned int f_restore:1;
		};
	};
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
 * @module: kernel module
 * @mappings: number of extent ranges contributing to the region
 * @size: total capacity of the region before resolving aliasing
 * @type: integer nd-bus device-type
 * @type_name: 'pmem' or 'block'
 * @generation: incremented everytime the region is disabled
 * @nstype: the resulting type of namespace this region produces
 *
 * A region may alias between pmem and block-window access methods.  The
 * region driver is tasked with parsing the label (if their is one) and
 * coordinating configuration with peer regions.
 *
 * When a region is disabled a client may have pending references to
 * namespaces and btts.  After a disable event the client can
 * ndctl_region_cleanup() to clean up invalid objects, or it can
 * specify the cleanup flag to ndctl_region_disable().
 */
struct ndctl_region {
	struct kmod_module *module;
	struct ndctl_bus *bus;
	int id, num_mappings, nstype, range_index, ro;
	int mappings_init;
	int namespaces_init;
	int btts_init;
	unsigned long long size;
	char *region_path;
	char *region_buf;
	int buf_len;
	int generation;
	struct list_head btts;
	struct list_head mappings;
	struct list_head namespaces;
	struct list_head stale_namespaces;
	struct list_head stale_btts;
	struct list_node list;
	/**
	 * struct ndctl_interleave_set - extra info for interleave sets
	 * @state: are any interleave set members active or all idle
	 * @cookie: summary cookie identifying the NFIT config for the set
	 */
	struct ndctl_interleave_set {
		int state;
		unsigned long long cookie;
	} iset;
};

/**
 * struct ndctl_lbasize - lbasize info for btt and blk-namespace devices
 * @select: currently selected sector_size
 * @supported: possible sector_size options
 * @num: number of entries in @supported
 */
struct ndctl_lbasize {
	int select;
	unsigned int *supported;
	int num;
};

/**
 * struct ndctl_namespace - device claimed by the nd_blk or nd_pmem driver
 * @module: kernel module
 * @type: integer nd-bus device-type
 * @type_name: 'namespace_io', 'namespace_pmem', or 'namespace_block'
 * @namespace_path: devpath for namespace device
 * @bdev: associated block_device of a namespace
 * @size: unsigned
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
	char *bdev;
	int type, id, buf_len, raw_mode;
	int generation;
	unsigned long long size;
	char *alt_name;
	uuid_t uuid;
	struct ndctl_lbasize lbasize;
};

/**
 * struct ndctl_btt - stacked block device provided sector atomicity
 * @module: kernel module (nd_btt)
 * @lbasize: sector size info
 * @ndns: host namespace for the btt instance
 * @region: parent region
 * @btt_path: btt devpath
 * @uuid: unique identifier for a btt instance
 * @btt_buf: space to print paths for bind/unbind operations
 * @bdev: block device associated with a btt
 */
struct ndctl_btt {
	struct kmod_module *module;
	struct ndctl_region *region;
	struct ndctl_namespace *ndns;
	struct list_node list;
	struct ndctl_lbasize lbasize;
	char *btt_path;
	char *btt_buf;
	char *bdev;
	int buf_len;
	uuid_t uuid;
	int id, generation;
};

/**
 * struct ndctl_ctx - library user context to find "nd" instances
 *
 * Instantiate with ndctl_new(), which takes an initial reference.  Free
 * the context by dropping the reference count to zero with
 * ndctrl_unref(), or take additional references with ndctl_ref()
 * @timeout: default library timeout in milliseconds
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
	unsigned long timeout;
};

/**
 * struct ndctl_cmd - device-specific-method (_DSM ioctl) container
 * @dimm: set if the command is relative to a dimm, NULL otherwise
 * @bus: set if the command is relative to a bus (like ARS), NULL otherwise
 * @refcount: reference for passing command buffer around
 * @type: cmd number
 * @size: total size of the ndctl_cmd allocation
 * @status: negative if failed, 0 if success, > 0 if never submitted
 * @firmware_status: NFIT command output status code
 * @iter: iterator for multi-xfer commands
 * @source: source cmd of an inherited iter.total_buf
 *
 * For dynamically sized commands like 'get_config', 'set_config', or
 * 'vendor', @size encompasses the entire buffer for the command input
 * and response output data.
 *
 * A command may only specify one of @source, or @iter.total_buf, not both.
 */
enum {
	READ, WRITE,
};
struct ndctl_cmd {
	struct ndctl_dimm *dimm;
	struct ndctl_bus *bus;
	int refcount;
	int type;
	int size;
	int status;
	u32 *firmware_status;
	struct ndctl_cmd_iter {
		u32 *offset;
		u8 *data; /* pointer to the data buffer location in cmd */
		u32 max_xfer;
		char *total_buf;
		u32 total_xfer;
		int dir;
	} iter;
	struct ndctl_cmd *source;
	union {
		struct nd_cmd_get_config_size get_size[0];
		struct nd_cmd_get_config_data_hdr get_data[0];
		struct nd_cmd_set_config_hdr set_data[0];
		struct nd_cmd_vendor_hdr vendor[0];
		char cmd_buf[0];
	};
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

/**
 * ndctl_new - instantiate a new library context
 * @ctx: context to establish
 *
 * Returns zero on success and stores an opaque pointer in ctx.  The
 * context is freed by ndctl_unref(), i.e. ndctl_new() implies an
 * internal ndctl_ref().
 */
NDCTL_EXPORT int ndctl_new(struct ndctl_ctx **ctx)
{
	struct kmod_ctx *kmod_ctx;
	struct ndctl_ctx *c;
	struct udev *udev;
	const char *env;
	int rc = 0;

	udev = udev_new();
	if (check_udev(udev) != 0)
		return -ENXIO;

	kmod_ctx = kmod_new(NULL, NULL);
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
	c->timeout = 5000;
	list_head_init(&c->busses);

	/* environment overwrites config */
	env = secure_getenv("NDCTL_LOG");
	if (env != NULL)
		ndctl_set_log_priority(c, log_priority(env));
	info(c, "ctx %p created\n", c);
	dbg(c, "log_priority=%d\n", c->log_priority);
	*ctx = c;

	env = secure_getenv("NDCTL_TIMEOUT");
	if (env != NULL) {
		unsigned long tmo;
		char *end;

		tmo = strtoul(env, &end, 0);
		if (tmo < ULONG_MAX && !end)
			c->timeout = tmo;
		dbg(c, "timeout = %ld\n", tmo);
	}

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

/**
 * ndctl_ref - take an additional reference on the context
 * @ctx: context established by ndctl_new()
 */
NDCTL_EXPORT struct ndctl_ctx *ndctl_ref(struct ndctl_ctx *ctx)
{
	if (ctx == NULL)
		return NULL;
	ctx->refcount++;
	return ctx;
}

static void free_namespace(struct ndctl_namespace *ndns, struct list_head *head)
{
	if (head)
		list_del_from(head, &ndns->list);
	free(ndns->ndns_path);
	free(ndns->ndns_buf);
	free(ndns->bdev);
	kmod_module_unref(ndns->module);
	free(ndns);
}

static void free_namespaces(struct ndctl_region *region)
{
	struct ndctl_namespace *ndns, *_n;

	list_for_each_safe(&region->namespaces, ndns, _n, list)
		free_namespace(ndns, &region->namespaces);
}

static void free_stale_namespaces(struct ndctl_region *region)
{
	struct ndctl_namespace *ndns, *_n;

	list_for_each_safe(&region->stale_namespaces, ndns, _n, list)
		free_namespace(ndns, &region->stale_namespaces);
}

static void free_btt(struct ndctl_btt *btt, struct list_head *head)
{
	if (head)
		list_del_from(head, &btt->list);
	kmod_module_unref(btt->module);
	free(btt->lbasize.supported);
	free(btt->btt_path);
	free(btt->btt_buf);
	free(btt);
}

static void free_btts(struct ndctl_region *region)
{
	struct ndctl_btt *btt, *_b;

	list_for_each_safe(&region->btts, btt, _b, list)
		free_btt(btt, &region->btts);
}

static void free_stale_btts(struct ndctl_region *region)
{
	struct ndctl_btt *btt, *_b;

	list_for_each_safe(&region->stale_btts, btt, _b, list)
		free_btt(btt, &region->stale_btts);
}

static void free_region(struct ndctl_region *region)
{
	struct ndctl_bus *bus = region->bus;
	struct ndctl_mapping *mapping, *_m;

	list_for_each_safe(&region->mappings, mapping, _m, list) {
		list_del_from(&region->mappings, &mapping->list);
		free(mapping);
	}
	free_btts(region);
	free_stale_btts(region);
	free_namespaces(region);
	free_stale_namespaces(region);
	list_del_from(&bus->regions, &region->list);
	kmod_module_unref(region->module);
	free(region->region_buf);
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

/**
 * ndctl_unref - drop a context reference count
 * @ctx: context established by ndctl_new()
 *
 * Drop a reference and if the resulting reference count is 0 destroy
 * the context.
 */
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

/**
 * ndctl_get_log_priority - retrieve current library loglevel (syslog)
 * @ctx: ndctl library context
 */
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

static int sysfs_read_attr(struct ndctl_ctx *ctx, const char *path, char *buf)
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

static int __sysfs_write_attr(struct ndctl_ctx *ctx, const char *path,
		const char *buf, int quiet)
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
		if (!quiet)
			dbg(ctx, "failed to write %s to %s: %s\n", buf, path,
					strerror(errno));
		return -1;
	}
	return 0;
}

static int sysfs_write_attr(struct ndctl_ctx *ctx, const char *path,
		const char *buf)
{
	return __sysfs_write_attr(ctx, path, buf, 0);
}

static int sysfs_write_attr_quiet(struct ndctl_ctx *ctx, const char *path,
		const char *buf)
{
	return __sysfs_write_attr(ctx, path, buf, 1);
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

static int device_parse(struct ndctl_ctx *ctx, struct ndctl_bus *bus,
		const char *base_path, const char *dev_name, void *parent,
		add_dev_fn add_dev)
{
	int add_errors = 0;
	struct dirent *de;
	DIR *dir;

	if (bus)
		ndctl_bus_wait_probe(bus);
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
		if (rc < 0) {
			add_errors++;
			err(ctx, "%s%d: add_dev() failed: %d\n",
					dev_name, id, rc);
		} else if (rc == 0) {
			dbg(ctx, "%s%d: added\n", dev_name, id);
		} else
			dbg(ctx, "%s%d: duplicate\n", dev_name, id);
	}
	closedir(dir);

	return add_errors;
}

static int to_dsm_index(const char *name, int dimm)
{
	const char *(*cmd_name_fn)(unsigned cmd);
	int i, end_cmd;

	if (dimm) {
		end_cmd = ND_CMD_VENDOR;
		cmd_name_fn = nvdimm_cmd_name;
	} else {
		end_cmd = ND_CMD_ARS_STATUS;
		cmd_name_fn = nvdimm_bus_cmd_name;
	}

	for (i = 1; i <= end_cmd; i++) {
		const char *cmd_name = cmd_name_fn(i);

		if (!cmd_name)
			continue;
		if (strcmp(name, cmd_name) == 0)
			return i;
	}
	return 0;
}

static unsigned long parse_commands(char *commands, int dimm)
{
	unsigned long dsm_mask = 0;
	char *start, *end;

	start = commands;
	while ((end = strchr(start, ' '))) {
		int cmd;

		*end = '\0';
		cmd = to_dsm_index(start, dimm);
		if (cmd)
			dsm_mask |= 1 << cmd;
		start = end + 1;
	}
	return dsm_mask;
}

static void parse_nfit_mem_flags(struct ndctl_dimm *dimm, char *flags)
{
	char *start, *end;

	start = flags;
	while ((end = strchr(start, ' '))) {
		*end = '\0';
		if (strcmp(start, "arm") == 0)
			dimm->f_arm = 1;
		else if (strcmp(start, "save") == 0)
			dimm->f_save = 1;
		else if (strcmp(start, "flush") == 0)
			dimm->f_flush = 1;
		else if (strcmp(start, "smart") == 0)
			dimm->f_smart = 1;
		else if (strcmp(start, "restore") == 0)
			dimm->f_restore = 1;
		start = end + 1;
	}
	if (end != start)
		dbg(ndctl_dimm_get_ctx(dimm), "%s: %s\n",
				ndctl_dimm_get_devname(dimm), flags);
}

static int add_bus(void *parent, int id, const char *ctl_base)
{
	int rc = -ENOMEM;
	struct ndctl_bus *bus;
	char buf[SYSFS_ATTR_SIZE];
	struct ndctl_ctx *ctx = parent;
	char *path = calloc(1, strlen(ctl_base) + 100);

	if (!path)
		return -ENOMEM;

	bus = calloc(1, sizeof(*bus));
	if (!bus)
		goto err_bus;
	list_head_init(&bus->dimms);
	list_head_init(&bus->regions);
	bus->ctx = ctx;
	bus->id = id;

	rc = -EINVAL;
	sprintf(path, "%s/dev", ctl_base);
	if (sysfs_read_attr(ctx, path, buf) < 0
			|| sscanf(buf, "%d:%d", &bus->major, &bus->minor) != 2)
		goto err_read;

	sprintf(path, "%s/device/commands", ctl_base);
	if (sysfs_read_attr(ctx, path, buf) < 0)
		goto err_read;
	bus->dsm_mask = parse_commands(buf, 0);

	sprintf(path, "%s/device/nfit/revision", ctl_base);
	if (sysfs_read_attr(ctx, path, buf) < 0) {
		bus->has_nfit = 0;
		bus->revision = -1;
	} else {
		bus->has_nfit = 1;
		bus->revision = strtoul(buf, NULL, 0);
	}

	sprintf(path, "%s/device/provider", ctl_base);
	if (sysfs_read_attr(ctx, path, buf) < 0)
		goto err_read;

	rc = -ENOMEM;
	bus->provider = strdup(buf);
	if (!bus->provider)
		goto err_read;

	sprintf(path, "%s/device/wait_probe", ctl_base);
	bus->wait_probe_path = strdup(path);
	if (!bus->wait_probe_path)
		goto err_read;

	bus->bus_path = parent_dev_path("char", bus->major, bus->minor);
	if (!bus->bus_path)
		goto err_dev_path;

	bus->bus_buf = calloc(1, strlen(bus->bus_path) + 50);
	if (!bus->bus_buf)
		goto err_read;
	bus->buf_len = strlen(bus->bus_path) + 50;

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

	device_parse(ctx, NULL, "/sys/class/nd", "ndctl", ctx, add_bus);
}

/**
 * ndctl_bus_get_first - retrieve first "nd bus" in the system
 * @ctx: context established by ndctl_new
 *
 * Returns an ndctl_bus if an nd bus exists in the system.  This return
 * value can be used to iterate to the next available bus in the system
 * ia ndctl_bus_get_next()
 */
NDCTL_EXPORT struct ndctl_bus *ndctl_bus_get_first(struct ndctl_ctx *ctx)
{
	busses_init(ctx);

	return list_top(&ctx->busses, struct ndctl_bus, list);
}

/**
 * ndctl_bus_get_next - retrieve the "next" nd bus in the system
 * @bus: ndctl_bus instance returned from ndctl_bus_get_{first|next}
 *
 * Returns NULL if @bus was the "last" bus available in the system
 */
NDCTL_EXPORT struct ndctl_bus *ndctl_bus_get_next(struct ndctl_bus *bus)
{
	struct ndctl_ctx *ctx = bus->ctx;

	return list_next(&ctx->busses, bus, list);
}

NDCTL_EXPORT int ndctl_bus_has_nfit(struct ndctl_bus *bus)
{
	return bus->has_nfit;
}

/**
 * ndctl_bus_get_major - nd bus character device major number
 * @bus: ndctl_bus instance returned from ndctl_bus_get_{first|next}
 */
NDCTL_EXPORT unsigned int ndctl_bus_get_major(struct ndctl_bus *bus)
{
	return bus->major;
}

/**
 * ndctl_bus_get_minor - nd bus character device minor number
 * @bus: ndctl_bus instance returned from ndctl_bus_get_{first|next}
 */
NDCTL_EXPORT unsigned int ndctl_bus_get_minor(struct ndctl_bus *bus)
{
	return bus->minor;
}

NDCTL_EXPORT const char *ndctl_bus_get_devname(struct ndctl_bus *bus)
{
	return devpath_to_devname(bus->bus_path);
}

NDCTL_EXPORT struct ndctl_bus *ndctl_bus_get_by_provider(struct ndctl_ctx *ctx,
		const char *provider)
{
	struct ndctl_bus *bus;

        ndctl_bus_foreach(ctx, bus)
		if (strcmp(provider, ndctl_bus_get_provider(bus)) == 0)
			return bus;

	return NULL;
}

NDCTL_EXPORT struct ndctl_btt *ndctl_region_get_btt_seed(struct ndctl_region *region)
{
	struct ndctl_ctx *ctx = ndctl_region_get_ctx(region);
	char *path = region->region_buf;
	int len = region->buf_len;
	struct ndctl_btt *btt;
	char buf[50];

	if (snprintf(path, len, "%s/btt_seed", region->region_path) >= len) {
		err(ctx, "%s: buffer too small!\n",
				ndctl_region_get_devname(region));
		return NULL;
	}

	if (sysfs_read_attr(ctx, path, buf) < 0)
		return NULL;

	ndctl_btt_foreach(region, btt)
		if (strcmp(buf, ndctl_btt_get_devname(btt)) == 0)
			return btt;
	return NULL;
}

NDCTL_EXPORT int ndctl_region_get_ro(struct ndctl_region *region)
{
	return region->ro;
}

NDCTL_EXPORT int ndctl_region_set_ro(struct ndctl_region *region, int ro)
{
	struct ndctl_ctx *ctx = ndctl_region_get_ctx(region);
	char *path = region->region_buf;
	int len = region->buf_len;

	if (snprintf(path, len, "%s/read_only", region->region_path) >= len) {
		err(ctx, "%s: buffer too small!\n",
				ndctl_region_get_devname(region));
		return -ENXIO;
	}

	ro = !!ro;
	if (sysfs_write_attr(ctx, path, ro ? "1\n" : "0\n") < 0)
		return -ENXIO;

	region->ro = ro;
	return ro;
}

NDCTL_EXPORT const char *ndctl_bus_get_cmd_name(struct ndctl_bus *bus, int cmd)
{
	return nvdimm_bus_cmd_name(cmd);
}

NDCTL_EXPORT int ndctl_bus_is_cmd_supported(struct ndctl_bus *bus,
		int cmd)
{
	return !!(bus->dsm_mask & (1ULL << cmd));
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
 * Upon return this bus's dimm and region devices are probed, the region
 * child namespace devices are registered, and drivers for namespaces
 * and btts are loaded (if module policy allows)
 */
NDCTL_EXPORT int ndctl_bus_wait_probe(struct ndctl_bus *bus)
{
	struct ndctl_ctx *ctx = ndctl_bus_get_ctx(bus);
	unsigned long tmo = ctx->timeout;
	char buf[SYSFS_ATTR_SIZE];
	int rc, sleep = 0;

	do {
		rc = sysfs_read_attr(bus->ctx, bus->wait_probe_path, buf);
		if (rc < 0)
			break;
		if (!ctx->udev_queue)
			break;
		if (udev_queue_get_queue_is_empty(ctx->udev_queue))
			break;
		sleep++;
		usleep(1000);
	} while (ctx->timeout == 0 || tmo-- != 0);

	if (sleep)
		dbg(ctx, "waited %d millisecond%s for bus%d...\n", sleep,
				sleep == 1 ? "" : "s", ndctl_bus_get_id(bus));

	return rc < 0 ? -ENXIO : 0;
}

static int add_dimm(void *parent, int id, const char *dimm_base)
{
	int rc = -ENOMEM;
	struct ndctl_dimm *dimm;
	char buf[SYSFS_ATTR_SIZE];
	struct ndctl_bus *bus = parent;
	struct ndctl_ctx *ctx = bus->ctx;
	char *path = calloc(1, strlen(dimm_base) + 100);

	if (!path)
		return -ENOMEM;

	dimm = calloc(1, sizeof(*dimm));
	if (!dimm)
		goto err_dimm;
	dimm->bus = bus;
	dimm->id = id;

	rc = -ENXIO;

	sprintf(path, "%s/dev", dimm_base);
	if (sysfs_read_attr(ctx, path, buf) < 0)
		goto err_read;
	if (sscanf(buf, "%d:%d", &dimm->major, &dimm->minor) != 2)
		goto err_read;

	sprintf(path, "%s/commands", dimm_base);
	if (sysfs_read_attr(ctx, path, buf) < 0)
		goto err_read;
	dimm->dsm_mask = parse_commands(buf, 1);

        dimm->dimm_buf = calloc(1, strlen(dimm_base) + 50);
        if (!dimm->dimm_buf)
                goto err_read;
        dimm->buf_len = strlen(dimm_base) + 50;

	dimm->dimm_path = strdup(dimm_base);
	if (!dimm->dimm_path)
		goto err_read;

	sprintf(path, "%s/modalias", dimm_base);
	if (sysfs_read_attr(ctx, path, buf) < 0)
		goto err_read;
	dimm->module = to_module(ctx, buf);

	dimm->handle = -1;
	dimm->phys_id = -1;
	dimm->vendor_id = -1;
	dimm->serial = -1;
	dimm->device_id = -1;
	dimm->revision_id = -1;
	dimm->format_id = -1;
	if (!ndctl_bus_has_nfit(bus))
		goto out;

	sprintf(path, "%s/nfit/handle", dimm_base);
	if (sysfs_read_attr(ctx, path, buf) < 0)
		goto err_read;
	dimm->handle = strtoul(buf, NULL, 0);

	sprintf(path, "%s/nfit/phys_id", dimm_base);
	if (sysfs_read_attr(ctx, path, buf) < 0)
		goto err_read;
	dimm->phys_id = strtoul(buf, NULL, 0);

	sprintf(path, "%s/nfit/vendor", dimm_base);
	if (sysfs_read_attr(ctx, path, buf) < 0)
		dimm->vendor_id = -1;
	else
		dimm->vendor_id = strtoul(buf, NULL, 0);

	sprintf(path, "%s/nfit/serial", dimm_base);
	if (sysfs_read_attr(ctx, path, buf) == 0)
		dimm->serial = strtoul(buf, NULL, 0);

	sprintf(path, "%s/nfit/device", dimm_base);
	if (sysfs_read_attr(ctx, path, buf) == 0)
		dimm->device_id = strtoul(buf, NULL, 0);

	sprintf(path, "%s/nfit/rev_id", dimm_base);
	if (sysfs_read_attr(ctx, path, buf) == 0)
		dimm->revision_id = strtoul(buf, NULL, 0);

	sprintf(path, "%s/nfit/format", dimm_base);
	if (sysfs_read_attr(ctx, path, buf) == 0)
		dimm->format_id = strtoul(buf, NULL, 0);

	sprintf(path, "%s/nfit/flags", dimm_base);
	if (sysfs_read_attr(ctx, path, buf) == 0)
		parse_nfit_mem_flags(dimm, buf);
 out:
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
	device_parse(bus->ctx, bus, bus->bus_path, "nmem", bus, add_dimm);
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

NDCTL_EXPORT unsigned int ndctl_dimm_get_major(struct ndctl_dimm *dimm)
{
	return dimm->major;
}

NDCTL_EXPORT unsigned int ndctl_dimm_get_minor(struct ndctl_dimm *dimm)
{
	return dimm->minor;
}

NDCTL_EXPORT unsigned int ndctl_dimm_get_id(struct ndctl_dimm *dimm)
{
	return dimm->id;
}

NDCTL_EXPORT unsigned int ndctl_dimm_get_serial(struct ndctl_dimm *dimm)
{
	return dimm->serial;
}

NDCTL_EXPORT const char *ndctl_dimm_get_devname(struct ndctl_dimm *dimm)
{
	return devpath_to_devname(dimm->dimm_path);
}

NDCTL_EXPORT const char *ndctl_dimm_get_cmd_name(struct ndctl_dimm *dimm, int cmd)
{
	return nvdimm_cmd_name(cmd);
}

NDCTL_EXPORT int ndctl_dimm_has_errors(struct ndctl_dimm *dimm)
{
	return dimm->flags != 0;
}

NDCTL_EXPORT int ndctl_dimm_failed_save(struct ndctl_dimm *dimm)
{
	return dimm->f_save;
}

NDCTL_EXPORT int ndctl_dimm_failed_arm(struct ndctl_dimm *dimm)
{
	return dimm->f_arm;
}

NDCTL_EXPORT int ndctl_dimm_failed_restore(struct ndctl_dimm *dimm)
{
	return dimm->f_restore;
}

NDCTL_EXPORT int ndctl_dimm_smart_pending(struct ndctl_dimm *dimm)
{
	return dimm->f_smart;
}

NDCTL_EXPORT int ndctl_dimm_failed_flush(struct ndctl_dimm *dimm)
{
	return dimm->f_flush;
}

NDCTL_EXPORT int ndctl_dimm_is_cmd_supported(struct ndctl_dimm *dimm,
		int cmd)
{
	return !!(dimm->dsm_mask & (1ULL << cmd));
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

NDCTL_EXPORT int ndctl_dimm_disable(struct ndctl_dimm *dimm)
{
	struct ndctl_ctx *ctx = ndctl_dimm_get_ctx(dimm);
	const char *devname = ndctl_dimm_get_devname(dimm);

	if (!ndctl_dimm_is_enabled(dimm))
		return 0;

	ndctl_unbind(ctx, dimm->dimm_path);

	if (ndctl_dimm_is_enabled(dimm)) {
		err(ctx, "%s: failed to disable\n", devname);
		return -EBUSY;
	}

	dbg(ctx, "%s: disabled\n", devname);
	return 0;
}

NDCTL_EXPORT int ndctl_dimm_enable(struct ndctl_dimm *dimm)
{
	const char *devname = ndctl_dimm_get_devname(dimm);
	struct ndctl_ctx *ctx = ndctl_dimm_get_ctx(dimm);

	if (ndctl_dimm_is_enabled(dimm))
		return 0;

	ndctl_bind(ctx, dimm->module, devname);

	if (!ndctl_dimm_is_enabled(dimm)) {
		err(ctx, "%s: failed to enable\n", devname);
		return -ENXIO;
	}

	dbg(ctx, "%s: enabled\n", devname);

	return 0;
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

static struct ndctl_dimm *ndctl_dimm_get_by_id(struct ndctl_bus *bus, unsigned int id)
{
	struct ndctl_dimm *dimm;

	ndctl_dimm_foreach(bus, dimm)
		if (ndctl_dimm_get_id(dimm) == id)
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
	char *path = calloc(1, strlen(region_base) + 100);

	if (!path)
		return -ENOMEM;

	region = calloc(1, sizeof(*region));
	if (!region)
		goto err_region;
	list_head_init(&region->btts);
	list_head_init(&region->stale_btts);
	list_head_init(&region->mappings);
	list_head_init(&region->namespaces);
	list_head_init(&region->stale_namespaces);
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

	sprintf(path, "%s/nstype", region_base);
	if (sysfs_read_attr(ctx, path, buf) < 0)
		goto err_read;
	region->nstype = strtoul(buf, NULL, 0);

	sprintf(path, "%s/nfit/range_index", region_base);
	if (ndctl_bus_has_nfit(bus)) {
		if (sysfs_read_attr(ctx, path, buf) < 0)
			goto err_read;
		region->range_index = strtoul(buf, NULL, 0);
	} else
		region->range_index = -1;

	sprintf(path, "%s/set_cookie", region_base);
	if (region->nstype == ND_DEVICE_NAMESPACE_PMEM) {
		if (sysfs_read_attr(ctx, path, buf) < 0)
			goto err_read;
		region->iset.cookie = strtoull(buf, NULL, 0);
		dbg(ctx, "region%d: iset-%#.16llx added\n", id,
				region->iset.cookie);
	}

	sprintf(path, "%s/read_only", region_base);
	if (sysfs_read_attr(ctx, path, buf) < 0)
		goto err_read;
	region->ro = strtoul(buf, NULL, 0);

	sprintf(path, "%s/modalias", region_base);
	if (sysfs_read_attr(ctx, path, buf) < 0)
		goto err_read;
	region->module = to_module(ctx, buf);

        region->region_buf = calloc(1, strlen(region_base) + 50);
        if (!region->region_buf)
                goto err_read;
        region->buf_len = strlen(region_base) + 50;

	region->region_path = strdup(region_base);
	if (!region->region_path)
		goto err_read;

	list_add(&bus->regions, &region->list);

	free(path);
	return 0;

 err_read:
	free(region->region_buf);
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
	device_parse(bus->ctx, bus, bus->bus_path, "region", bus, add_region);
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
	return ndctl_region_get_mappings(region);
}

NDCTL_EXPORT unsigned int ndctl_region_get_mappings(struct ndctl_region *region)
{
	return region->num_mappings;
}

NDCTL_EXPORT unsigned long long ndctl_region_get_size(struct ndctl_region *region)
{
	return region->size;
}

NDCTL_EXPORT unsigned long long ndctl_region_get_available_size(
		struct ndctl_region *region)
{
	unsigned int nstype = ndctl_region_get_nstype(region);
	struct ndctl_ctx *ctx = ndctl_region_get_ctx(region);
	char *path = region->region_buf;
	int len = region->buf_len;
	char buf[50];

	switch (nstype) {
	case ND_DEVICE_NAMESPACE_PMEM:
	case ND_DEVICE_NAMESPACE_BLK:
		break;
	default:
		return 0;
	}

	if (snprintf(path, len, "%s/available_size", region->region_path) >= len) {
		err(ctx, "%s: buffer too small!\n",
				ndctl_region_get_devname(region));
		return -ENOMEM;
	}

	if (sysfs_read_attr(ctx, path, buf) < 0)
		return -ENXIO;

	return strtoull(buf, NULL, 0);
}

NDCTL_EXPORT unsigned int ndctl_region_get_range_index(struct ndctl_region *region)
{
	return region->range_index;
}

NDCTL_EXPORT unsigned int ndctl_region_get_nstype(struct ndctl_region *region)
{
	return region->nstype;
}

NDCTL_EXPORT unsigned int ndctl_region_get_type(struct ndctl_region *region)
{
	switch (region->nstype) {
	case ND_DEVICE_NAMESPACE_IO:
	case ND_DEVICE_NAMESPACE_PMEM:
		return ND_DEVICE_REGION_PMEM;
	default:
		return ND_DEVICE_REGION_BLK;
	}
}

NDCTL_EXPORT struct ndctl_namespace *ndctl_region_get_namespace_seed(
		struct ndctl_region *region)
{
	struct ndctl_bus *bus = ndctl_region_get_bus(region);
	struct ndctl_ctx *ctx = ndctl_bus_get_ctx(bus);
	char *path = region->region_buf;
	struct ndctl_namespace *ndns;
	int len = region->buf_len;
	char buf[50];

	if (snprintf(path, len, "%s/namespace_seed", region->region_path) >= len) {
		err(ctx, "%s: buffer too small!\n",
				ndctl_region_get_devname(region));
		return NULL;
	}

	if (sysfs_read_attr(ctx, path, buf) < 0)
		return NULL;

	ndctl_namespace_foreach(region, ndns)
		if (strcmp(buf, ndctl_namespace_get_devname(ndns)) == 0)
			return ndns;
	return NULL;
}

static const char *ndctl_device_type_name(int type)
{
	switch (type) {
	case ND_DEVICE_DIMM:           return "dimm";
	case ND_DEVICE_REGION_PMEM:    return "pmem";
	case ND_DEVICE_REGION_BLK:     return "blk";
	case ND_DEVICE_NAMESPACE_IO:   return "namespace_io";
	case ND_DEVICE_NAMESPACE_PMEM: return "namespace_pmem";
	case ND_DEVICE_NAMESPACE_BLK:  return "namespace_blk";
	default:                       return "unknown";
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

static struct nd_cmd_vendor_tail *to_vendor_tail(struct ndctl_cmd *cmd)
{
	struct nd_cmd_vendor_tail *tail = (struct nd_cmd_vendor_tail *)
		(cmd->cmd_buf + sizeof(struct nd_cmd_vendor_hdr)
		 + cmd->vendor->in_length);
	return tail;
}

NDCTL_EXPORT struct ndctl_cmd *ndctl_dimm_cmd_new_vendor_specific(
		struct ndctl_dimm *dimm, unsigned int opcode, size_t input_size,
		size_t output_size)
{
	struct ndctl_bus *bus = ndctl_dimm_get_bus(dimm);
	struct ndctl_ctx *ctx = ndctl_bus_get_ctx(bus);
	struct ndctl_cmd *cmd;
	size_t size;

	if (!ndctl_dimm_is_cmd_supported(dimm, ND_CMD_VENDOR)) {
		dbg(ctx, "unsupported cmd\n");
		return NULL;
	}

	size = sizeof(*cmd) + sizeof(struct nd_cmd_vendor_hdr)
		+ sizeof(struct nd_cmd_vendor_tail) + input_size
		+ output_size;

	cmd = calloc(1, size);
	if (!cmd)
		return NULL;

	cmd->dimm = dimm;
	ndctl_cmd_ref(cmd);
	cmd->type = ND_CMD_VENDOR;
	cmd->size = size;
	cmd->status = 1;
	cmd->vendor->opcode = opcode;
	cmd->vendor->in_length = input_size;
	cmd->firmware_status = &to_vendor_tail(cmd)->status;
	to_vendor_tail(cmd)->out_length = output_size;

	return cmd;
}

NDCTL_EXPORT ssize_t ndctl_cmd_vendor_set_input(struct ndctl_cmd *cmd,
		void *buf, unsigned int len)
{
	if (cmd->type != ND_CMD_VENDOR)
		return -EINVAL;
	len = min(len, cmd->vendor->in_length);
	memcpy(cmd->vendor->in_buf, buf, len);
	return len;
}

NDCTL_EXPORT ssize_t ndctl_cmd_vendor_get_output_size(struct ndctl_cmd *cmd)
{
	if (cmd->type != ND_CMD_VENDOR || cmd->status > 0)
		return -EINVAL;
	if (cmd->status < 0)
		return cmd->status;

	return to_vendor_tail(cmd)->out_length;
}

NDCTL_EXPORT ssize_t ndctl_cmd_vendor_get_output(struct ndctl_cmd *cmd,
		void *buf, unsigned int len)
{
	ssize_t out_length = ndctl_cmd_vendor_get_output_size(cmd);

	if (out_length < 0)
		return out_length;

	len = min(len, out_length);
	memcpy(buf, to_vendor_tail(cmd)->out_buf, len);
	return len;
}

NDCTL_EXPORT struct ndctl_cmd *ndctl_dimm_cmd_new_cfg_size(struct ndctl_dimm *dimm)
{
	struct ndctl_bus *bus = ndctl_dimm_get_bus(dimm);
	struct ndctl_ctx *ctx = ndctl_bus_get_ctx(bus);
	struct ndctl_cmd *cmd;
	size_t size;

	if (!ndctl_dimm_is_cmd_supported(dimm, ND_CMD_GET_CONFIG_SIZE)) {
		dbg(ctx, "unsupported cmd\n");
		return NULL;
	}

	size = sizeof(*cmd) + sizeof(struct nd_cmd_get_config_size);
	cmd = calloc(1, size);
	if (!cmd)
		return NULL;

	cmd->dimm = dimm;
	ndctl_cmd_ref(cmd);
	cmd->type = ND_CMD_GET_CONFIG_SIZE;
	cmd->size = size;
	cmd->status = 1;
	cmd->firmware_status = &cmd->get_size->status;

	return cmd;
}

static struct ndctl_bus *cmd_to_bus(struct ndctl_cmd *cmd)
{
	if (cmd->dimm)
		return ndctl_dimm_get_bus(cmd->dimm);
	return cmd->bus;
}

NDCTL_EXPORT struct ndctl_cmd *ndctl_dimm_cmd_new_cfg_read(struct ndctl_cmd *cfg_size)
{
	struct ndctl_ctx *ctx = ndctl_bus_get_ctx(cmd_to_bus(cfg_size));
	struct ndctl_dimm *dimm = cfg_size->dimm;
	struct ndctl_cmd *cmd;
	size_t size;

	if (!ndctl_dimm_is_cmd_supported(dimm, ND_CMD_GET_CONFIG_DATA)) {
		dbg(ctx, "unsupported cmd\n");
		return NULL;
	}

	if (cfg_size->type != ND_CMD_GET_CONFIG_SIZE
			|| cfg_size->status != 0) {
		dbg(ctx, "expected sucessfully completed cfg_size command\n");
		return NULL;
	}
	if (!cfg_size->dimm || cfg_size->get_size->config_size == 0) {
		dbg(ctx, "invalid cfg_size\n");
		return NULL;
	}

	size = sizeof(*cmd) + sizeof(struct nd_cmd_get_config_data_hdr)
		+ cfg_size->get_size->max_xfer;
	cmd = calloc(1, size);
	if (!cmd)
		return NULL;

	cmd->dimm = cfg_size->dimm;
	cmd->refcount = 1;
	cmd->type = ND_CMD_GET_CONFIG_DATA;
	cmd->size = size;
	cmd->status = 1;
	cmd->get_data->in_offset = 0;
	cmd->get_data->in_length = cfg_size->get_size->max_xfer;
	cmd->firmware_status = &cmd->get_data->status;
	cmd->iter.offset = &cmd->get_data->in_offset;
	cmd->iter.max_xfer = cfg_size->get_size->max_xfer;
	cmd->iter.data = cmd->get_data->out_buf;
	cmd->iter.total_xfer = cfg_size->get_size->config_size;
	cmd->iter.total_buf = calloc(1, cmd->iter.total_xfer);
	cmd->iter.dir = READ;
	if (!cmd->iter.total_buf) {
		free(cmd);
		return NULL;
	}

	return cmd;
}

NDCTL_EXPORT struct ndctl_cmd *ndctl_dimm_cmd_new_cfg_write(struct ndctl_cmd *cfg_read)
{
	struct ndctl_ctx *ctx = ndctl_bus_get_ctx(cmd_to_bus(cfg_read));
	struct ndctl_dimm *dimm = cfg_read->dimm;
	struct ndctl_cmd *cmd;
	size_t size;

	if (!ndctl_dimm_is_cmd_supported(dimm, ND_CMD_SET_CONFIG_DATA)) {
		dbg(ctx, "unsupported cmd\n");
		return NULL;
	}

	/* enforce rmw */
	if (cfg_read->type != ND_CMD_GET_CONFIG_DATA
		       || cfg_read->status != 0) {
		dbg(ctx, "expected sucessfully completed cfg_read command\n");
		return NULL;
	}

	if (!cfg_read->dimm || cfg_read->get_data->in_length == 0) {
		dbg(ctx, "invalid cfg_read\n");
		return NULL;
	}

	size = sizeof(*cmd) + sizeof(struct nd_cmd_set_config_hdr)
		+ cfg_read->iter.max_xfer + 4;
	cmd = calloc(1, size);
	if (!cmd)
		return NULL;

	cmd->dimm = cfg_read->dimm;
	ndctl_cmd_ref(cmd);
	cmd->type = ND_CMD_SET_CONFIG_DATA;
	cmd->size = size;
	cmd->status = 1;
	cmd->set_data->in_offset = 0;
	cmd->set_data->in_length = cfg_read->iter.max_xfer;
	cmd->firmware_status = (u32 *) (cmd->cmd_buf
		+ sizeof(struct nd_cmd_set_config_hdr) + cfg_read->iter.max_xfer);
	cmd->iter.offset = &cmd->set_data->in_offset;
	cmd->iter.max_xfer = cfg_read->iter.max_xfer;
	cmd->iter.data = cmd->set_data->in_buf;
	cmd->iter.total_xfer = cfg_read->iter.total_xfer;
	cmd->iter.total_buf = cfg_read->iter.total_buf;
	cmd->iter.dir = WRITE;
	cmd->source = cfg_read;
	ndctl_cmd_ref(cfg_read);

	return cmd;
}

NDCTL_EXPORT unsigned int ndctl_cmd_cfg_size_get_size(struct ndctl_cmd *cfg_size)
{
	if (cfg_size->type == ND_CMD_GET_CONFIG_SIZE
			&& cfg_size->status == 0)
		return cfg_size->get_size->config_size;
	return 0;
}

NDCTL_EXPORT ssize_t ndctl_cmd_cfg_read_get_data(struct ndctl_cmd *cfg_read,
		void *buf, unsigned int len, unsigned int offset)
{
	if (cfg_read->type != ND_CMD_GET_CONFIG_DATA || cfg_read->status > 0)
		return -EINVAL;
	if (cfg_read->status < 0)
		return cfg_read->status;
	if (offset > cfg_read->iter.total_xfer || len + offset < len)
		return -EINVAL;
	if (len + offset > cfg_read->iter.total_xfer)
		len = cfg_read->iter.total_xfer - offset;
	memcpy(buf, cfg_read->iter.total_buf + offset, len);
	return len;
}

NDCTL_EXPORT ssize_t ndctl_cmd_cfg_write_set_data(struct ndctl_cmd *cfg_write,
		void *buf, unsigned int len, unsigned int offset)
{
	if (cfg_write->type != ND_CMD_SET_CONFIG_DATA || cfg_write->status < 1)
		return -EINVAL;
	if (cfg_write->status < 0)
		return cfg_write->status;
	if (offset > cfg_write->iter.total_xfer || len + offset < len)
		return -EINVAL;
	if (len + offset > cfg_write->iter.total_xfer)
		len = cfg_write->iter.total_xfer - offset;
	memcpy(cfg_write->iter.total_buf + offset, buf, len);
	return len;
}

NDCTL_EXPORT ssize_t ndctl_cmd_cfg_write_zero_data(struct ndctl_cmd *cfg_write)
{
	if (cfg_write->type != ND_CMD_SET_CONFIG_DATA || cfg_write->status < 1)
		return -EINVAL;
	if (cfg_write->status < 0)
		return cfg_write->status;
	memset(cfg_write->iter.total_buf, 0, cfg_write->iter.total_xfer);
	return cfg_write->iter.total_xfer;
}

NDCTL_EXPORT int ndctl_dimm_zero_labels(struct ndctl_dimm *dimm)
{
	struct ndctl_cmd *cmd_size, *cmd_read, *cmd_write;
	struct ndctl_ctx *ctx = ndctl_dimm_get_ctx(dimm);
	struct ndctl_bus *bus = ndctl_dimm_get_bus(dimm);
	int rc;

	rc = ndctl_bus_wait_probe(bus);
	if (rc < 0)
		return rc;

	if (ndctl_dimm_is_active(dimm)) {
		dbg(ctx, "%s: regions active, abort label write\n",
			ndctl_dimm_get_devname(dimm));
		return -EBUSY;
	}

	cmd_size = ndctl_dimm_cmd_new_cfg_size(dimm);
	if (!cmd_size)
		return -ENOTTY;
	rc = ndctl_cmd_submit(cmd_size);
	if (rc || ndctl_cmd_get_firmware_status(cmd_size))
		goto out_size;

	cmd_read = ndctl_dimm_cmd_new_cfg_read(cmd_size);
	if (!cmd_read) {
		rc = -ENOTTY;
		goto out_size;
	}
	rc = ndctl_cmd_submit(cmd_read);
	if (rc || ndctl_cmd_get_firmware_status(cmd_read))
		goto out_read;

	cmd_write = ndctl_dimm_cmd_new_cfg_write(cmd_read);
	if (!cmd_write) {
		rc = -ENOTTY;
		goto out_read;
	}
	if (ndctl_cmd_cfg_write_zero_data(cmd_write) < 0) {
		rc = -ENXIO;
		goto out_write;
	}
	rc = ndctl_cmd_submit(cmd_write);
	if (rc || ndctl_cmd_get_firmware_status(cmd_write))
		goto out_write;

	/*
	 * If the dimm is already disabled the kernel is not holding a cached
	 * copy of the label space.
	 */
	if (!ndctl_dimm_is_enabled(dimm))
		goto out_write;

	rc = ndctl_dimm_disable(dimm);
	if (rc)
		goto out_write;
	rc = ndctl_dimm_enable(dimm);

 out_write:
	ndctl_cmd_unref(cmd_write);
 out_read:
	ndctl_cmd_unref(cmd_read);
 out_size:
	ndctl_cmd_unref(cmd_size);

	return rc;
}

NDCTL_EXPORT void ndctl_cmd_unref(struct ndctl_cmd *cmd)
{
	if (--cmd->refcount == 0) {
		if (cmd->source)
			ndctl_cmd_unref(cmd->source);
		else
			free(cmd->iter.total_buf);
		free(cmd);
	}
}

NDCTL_EXPORT void ndctl_cmd_ref(struct ndctl_cmd *cmd)
{
	cmd->refcount++;
}

NDCTL_EXPORT int ndctl_cmd_get_type(struct ndctl_cmd *cmd)
{
	return cmd->type;
}

static int to_ioctl_cmd(int cmd, int dimm)
{
	if (!dimm) {
		switch (cmd) {
		case ND_CMD_ARS_CAP:         return ND_IOCTL_ARS_CAP;
		case ND_CMD_ARS_START:       return ND_IOCTL_ARS_START;
		case ND_CMD_ARS_STATUS:      return ND_IOCTL_ARS_STATUS;
		default:
						       return 0;
		};
	}

	switch (cmd) {
	case ND_CMD_SMART:                  return ND_IOCTL_SMART;
	case ND_CMD_SMART_THRESHOLD:        return ND_IOCTL_SMART_THRESHOLD;
	case ND_CMD_DIMM_FLAGS:             return ND_IOCTL_DIMM_FLAGS;
	case ND_CMD_GET_CONFIG_SIZE:        return ND_IOCTL_GET_CONFIG_SIZE;
	case ND_CMD_GET_CONFIG_DATA:        return ND_IOCTL_GET_CONFIG_DATA;
	case ND_CMD_SET_CONFIG_DATA:        return ND_IOCTL_SET_CONFIG_DATA;
	case ND_CMD_VENDOR:                 return ND_IOCTL_VENDOR;
	case ND_CMD_VENDOR_EFFECT_LOG_SIZE:
	case ND_CMD_VENDOR_EFFECT_LOG:
	default:
					      return 0;
	}
}

static int do_cmd(int fd, int ioctl_cmd, struct ndctl_cmd *cmd)
{
	int rc;
	u32 offset;
	const char *name;
	struct ndctl_bus *bus = cmd_to_bus(cmd);
	struct ndctl_cmd_iter *iter = &cmd->iter;
	struct ndctl_ctx *ctx = ndctl_bus_get_ctx(bus);

	if (cmd->dimm)
		name = ndctl_dimm_get_cmd_name(cmd->dimm, cmd->type);
	else
		name = ndctl_bus_get_cmd_name(cmd->bus, cmd->type);

	if (iter->total_xfer == 0) {
		rc = ioctl(fd, ioctl_cmd, cmd->cmd_buf);
		dbg(ctx, "bus: %d dimm: %#x cmd: %s status: %d fw: %d (%s)\n",
				bus->id, cmd->dimm
				? ndctl_dimm_get_handle(cmd->dimm) : 0,
				name, rc, *(cmd->firmware_status), rc < 0 ?
				strerror(errno) : "success");
		return rc;
	}

	for (offset = 0; offset < iter->total_xfer; offset += iter->max_xfer) {
		if (iter->dir == WRITE)
			memcpy(iter->data, iter->total_buf + offset,
					iter->max_xfer);
		*(cmd->iter.offset) = offset;
		rc = ioctl(fd, ioctl_cmd, cmd->cmd_buf);
		if (rc)
			break;

		if (iter->dir == READ)
			memcpy(iter->total_buf + offset, iter->data,
					iter->max_xfer);
		if (*(cmd->firmware_status)) {
			rc = iter->total_xfer - offset;
			break;
		}
	}

	dbg(ctx, "bus: %d dimm: %#x cmd: %s total: %d max_xfer: %d status: %d fw: %d (%s)\n",
			bus->id,
			cmd->dimm ? ndctl_dimm_get_handle(cmd->dimm) : 0,
			name, iter->total_xfer, iter->max_xfer, rc,
			*(cmd->firmware_status),
			rc < 0 ? strerror(errno) : "success");

	return rc;
}

NDCTL_EXPORT int ndctl_cmd_submit(struct ndctl_cmd *cmd)
{
	struct stat st;
	char path[20], *prefix;
	unsigned int major, minor, id;
	int rc = 0, fd, len = sizeof(path);
	int ioctl_cmd = to_ioctl_cmd(cmd->type, !!cmd->dimm);
	struct ndctl_bus *bus = cmd_to_bus(cmd);
	struct ndctl_ctx *ctx = ndctl_bus_get_ctx(bus);

	if (ioctl_cmd == 0) {
		rc = -EINVAL;
		goto out;
	}

	if (cmd->dimm) {
		prefix = "nmem";
		id = ndctl_dimm_get_id(cmd->dimm);
		major = ndctl_dimm_get_major(cmd->dimm);
		minor = ndctl_dimm_get_minor(cmd->dimm);
	} else {
		prefix = "ndctl";
		id = ndctl_bus_get_id(cmd->bus);
		major = ndctl_bus_get_major(cmd->bus);
		minor = ndctl_bus_get_minor(cmd->bus);
	}

	if (snprintf(path, len, "/dev/%s%u", prefix, id) >= len) {
		rc = -EINVAL;
		goto out;
	}

	fd = open(path, O_RDWR);
	if (fd < 0) {
		err(ctx, "failed to open %s: %s\n", path, strerror(errno));
		rc = -errno;
		goto out;
	}

	if (fstat(fd, &st) >= 0 && S_ISCHR(st.st_mode)
			&& major(st.st_rdev) == major
			&& minor(st.st_rdev) == minor) {
		rc = do_cmd(fd, ioctl_cmd, cmd);
	} else {
		err(ctx, "failed to validate %s as a control node\n", path);
		rc = -ENXIO;
	}
	close(fd);
 out:
	cmd->status = rc;
	return rc;
}

NDCTL_EXPORT int ndctl_cmd_get_status(struct ndctl_cmd *cmd)
{
	return cmd->status;
}

NDCTL_EXPORT unsigned int ndctl_cmd_get_firmware_status(struct ndctl_cmd *cmd)
{
	return *(cmd->firmware_status);
}

NDCTL_EXPORT const char *ndctl_region_get_devname(struct ndctl_region *region)
{
	return devpath_to_devname(region->region_path);
}

static int is_enabled(struct ndctl_bus *bus, const char *drvpath)
{
	struct stat st;

	ndctl_bus_wait_probe(bus);
	if (lstat(drvpath, &st) < 0 || !S_ISLNK(st.st_mode))
		return 0;
	else
		return 1;
}

NDCTL_EXPORT int ndctl_region_is_enabled(struct ndctl_region *region)
{
	struct ndctl_ctx *ctx = ndctl_region_get_ctx(region);
	char *path = region->region_buf;
	int len = region->buf_len;

	if (snprintf(path, len, "%s/driver", region->region_path) >= len) {
		err(ctx, "%s: buffer too small!\n",
				ndctl_region_get_devname(region));
		return 0;
	}

	return is_enabled(ndctl_region_get_bus(region), path);
}

NDCTL_EXPORT int ndctl_region_enable(struct ndctl_region *region)
{
	struct ndctl_ctx *ctx = ndctl_region_get_ctx(region);
	const char *devname = ndctl_region_get_devname(region);

	if (ndctl_region_is_enabled(region))
		return 0;

	ndctl_bind(ctx, region->module, devname);

	if (!ndctl_region_is_enabled(region)) {
		err(ctx, "%s: failed to enable\n", devname);
		return -ENXIO;
	}

	dbg(ctx, "%s: enabled\n", devname);
	return 0;
}

NDCTL_EXPORT void ndctl_region_cleanup(struct ndctl_region *region)
{
	free_stale_namespaces(region);
	free_stale_btts(region);
}

static int ndctl_region_disable(struct ndctl_region *region, int cleanup)
{
	struct ndctl_ctx *ctx = ndctl_region_get_ctx(region);
	const char *devname = ndctl_region_get_devname(region);

	if (!ndctl_region_is_enabled(region))
		return 0;

	ndctl_unbind(ctx, region->region_path);

	if (ndctl_region_is_enabled(region)) {
		err(ctx, "%s: failed to disable\n", devname);
		return -EBUSY;
	}
	region->namespaces_init = 0;
	region->btts_init = 0;
	list_append_list(&region->stale_namespaces, &region->namespaces);
	list_append_list(&region->stale_btts, &region->btts);
	region->generation++;
	if (cleanup)
		ndctl_region_cleanup(region);

	dbg(ctx, "%s: disabled\n", devname);
	return 0;
}

NDCTL_EXPORT int ndctl_region_disable_invalidate(struct ndctl_region *region)
{
	return ndctl_region_disable(region, 1);
}

NDCTL_EXPORT int ndctl_region_disable_preserve(struct ndctl_region *region)
{
	return ndctl_region_disable(region, 0);
}

NDCTL_EXPORT struct ndctl_interleave_set *ndctl_region_get_interleave_set(
	struct ndctl_region *region)
{
	unsigned int nstype = ndctl_region_get_nstype(region);

	if (nstype == ND_DEVICE_NAMESPACE_PMEM)
		return &region->iset;

	return NULL;
}

NDCTL_EXPORT struct ndctl_region *ndctl_interleave_set_get_region(
		struct ndctl_interleave_set *iset)
{
	return container_of(iset, struct ndctl_region, iset);
}

NDCTL_EXPORT struct ndctl_interleave_set *ndctl_interleave_set_get_first(
		struct ndctl_bus *bus)
{
	struct ndctl_region *region;

	ndctl_region_foreach(bus, region) {
		struct ndctl_interleave_set *iset;

		iset = ndctl_region_get_interleave_set(region);
		if (iset)
			return iset;
	}

	return NULL;
}

NDCTL_EXPORT struct ndctl_interleave_set *ndctl_interleave_set_get_next(
		struct ndctl_interleave_set *iset)
{
	struct ndctl_region *region = ndctl_interleave_set_get_region(iset);

	iset = NULL;
	do {
		region = ndctl_region_get_next(region);
		if (!region)
			break;
		iset = ndctl_region_get_interleave_set(region);
		if (iset)
			break;
	} while (1);

	return iset;
}

NDCTL_EXPORT int ndctl_dimm_is_enabled(struct ndctl_dimm *dimm)
{
	struct ndctl_ctx *ctx = ndctl_dimm_get_ctx(dimm);
	char *path = dimm->dimm_buf;
	int len = dimm->buf_len;

	if (snprintf(path, len, "%s/driver", dimm->dimm_path) >= len) {
		err(ctx, "%s: buffer too small!\n",
				ndctl_dimm_get_devname(dimm));
		return 0;
	}

	return is_enabled(ndctl_dimm_get_bus(dimm), path);
}

NDCTL_EXPORT int ndctl_dimm_is_active(struct ndctl_dimm *dimm)
{
	struct ndctl_ctx *ctx = ndctl_dimm_get_ctx(dimm);
	char *path = dimm->dimm_buf;
	int len = dimm->buf_len;
	char buf[20];

	if (snprintf(path, len, "%s/state", dimm->dimm_path) >= len) {
		err(ctx, "%s: buffer too small!\n",
				ndctl_dimm_get_devname(dimm));
		return -ENOMEM;
	}

	if (sysfs_read_attr(ctx, path, buf) < 0)
		return -ENXIO;

	if (strcmp(buf, "active") == 0)
		return 1;
	return 0;
}

NDCTL_EXPORT unsigned long ndctl_dimm_get_available_labels(
		struct ndctl_dimm *dimm)
{
	struct ndctl_ctx *ctx = ndctl_dimm_get_ctx(dimm);
	char *path = dimm->dimm_buf;
	int len = dimm->buf_len;
	char buf[20];

	if (snprintf(path, len, "%s/available_slots", dimm->dimm_path) >= len) {
		err(ctx, "%s: buffer too small!\n",
				ndctl_dimm_get_devname(dimm));
		return -ENOMEM;
	}

	if (sysfs_read_attr(ctx, path, buf) < 0)
		return 0;

	return strtoul(buf, NULL, 0);
}

NDCTL_EXPORT int ndctl_interleave_set_is_active(
		struct ndctl_interleave_set *iset)
{
	struct ndctl_dimm *dimm;

	ndctl_dimm_foreach_in_interleave_set(iset, dimm) {
		int active = ndctl_dimm_is_active(dimm);

		if (active)
			return active;
	}

	return 0;
}

NDCTL_EXPORT unsigned long long ndctl_interleave_set_get_cookie(
		struct ndctl_interleave_set *iset)
{
	return iset->cookie;
}

NDCTL_EXPORT struct ndctl_dimm *ndctl_interleave_set_get_first_dimm(
	struct ndctl_interleave_set *iset)
{
	struct ndctl_region *region = ndctl_interleave_set_get_region(iset);

	return ndctl_region_get_first_dimm(region);
}

NDCTL_EXPORT struct ndctl_dimm *ndctl_interleave_set_get_next_dimm(
	struct ndctl_interleave_set *iset, struct ndctl_dimm *dimm)
{
	struct ndctl_region *region = ndctl_interleave_set_get_region(iset);

	return ndctl_region_get_next_dimm(region, dimm);
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

	mapping_path = calloc(1, strlen(region->region_path) + 100);
	if (!mapping_path) {
		err(ctx, "bus%d region%d: allocation failure\n",
				bus->id, region->id);
		return;
	}

	for (i = 0; i < region->num_mappings; i++) {
		struct ndctl_mapping *mapping;
		unsigned long long offset, length;
		struct ndctl_dimm *dimm;
		unsigned int dimm_id;

		sprintf(mapping_path, "%s/mapping%d", region->region_path, i);
		if (sysfs_read_attr(ctx, mapping_path, buf) < 0) {
			err(ctx, "bus%d region%d: failed to read mapping%d\n",
					bus->id, region->id, i);
			continue;
		}

		if (sscanf(buf, "nmem%u,%llu,%llu", &dimm_id, &offset,
					&length) != 3) {
			err(ctx, "bus%d mapping parse failure\n",
					ndctl_bus_get_id(bus));
			continue;
		}

		dimm = ndctl_dimm_get_by_id(bus, dimm_id);
		if (!dimm) {
			err(ctx, "bus%d region%d mapping%d: nmem%d lookup failure\n",
					bus->id, region->id, i, dimm_id);
			continue;
		}

		mapping = calloc(1, sizeof(*mapping));
		if (!mapping) {
			err(ctx, "bus%d region%d mapping%d: allocation failure\n",
					bus->id, region->id, i);
			continue;
		}

		mapping->region = region;
		mapping->offset = offset;
		mapping->length = length;
		mapping->dimm = dimm;
		list_add(&region->mappings, &mapping->list);
	}
	free(mapping_path);
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
	struct kmod_list *list = NULL;
	struct kmod_module *mod;
	int rc;

	if (!ctx->kmod_ctx)
		return NULL;

	rc = kmod_module_new_from_lookup(ctx->kmod_ctx, alias, &list);
	if (rc < 0 || !list) {
		dbg(ctx, "failed to find module for alias: %s %d list: %s\n",
				alias, rc, list ? "populated" : "empty");
		return NULL;
	}
	mod = kmod_module_get_module(list);
	dbg(ctx, "alias: %s module: %s\n", alias, kmod_module_get_name(mod));
	kmod_module_unref_list(list);

	return mod;
}

static char *get_block_device(struct ndctl_ctx *ctx, const char *block_path)
{
	char *bdev_name = NULL;
	struct dirent *de;
	DIR *dir;

	dir = opendir(block_path);
	if (!dir) {
		dbg(ctx, "no block device found: %s\n", block_path);
		return NULL;
	}

	while ((de = readdir(dir)) != NULL) {
		if (de->d_ino == 0 || de->d_name[0] == '.')
			continue;
		if (bdev_name) {
			dbg(ctx, "invalid block_path format: %s\n",
					block_path);
			free(bdev_name);
			bdev_name = NULL;
			break;
		}
		bdev_name = strdup(de->d_name);
	}
	closedir(dir);

	return bdev_name;
}

static int parse_lbasize_supported(struct ndctl_ctx *ctx, const char *devname,
		const char *buf, struct ndctl_lbasize *lba);

static int add_namespace(void *parent, int id, const char *ndns_base)
{
	const char *devname = devpath_to_devname(ndns_base);
	char *path = calloc(1, strlen(ndns_base) + 100);
	struct ndctl_namespace *ndns, *ndns_dup;
	struct ndctl_region *region = parent;
	struct ndctl_bus *bus = region->bus;
	struct ndctl_ctx *ctx = bus->ctx;
	char buf[SYSFS_ATTR_SIZE];
	int rc = -ENOMEM;

	if (!path)
		return -ENOMEM;

	ndns = calloc(1, sizeof(*ndns));
	if (!ndns)
		goto err_namespace;
	ndns->id = id;
	ndns->region = region;
	ndns->generation = region->generation;

	sprintf(path, "%s/nstype", ndns_base);
	if (sysfs_read_attr(ctx, path, buf) < 0)
		goto err_read;
	ndns->type = strtoul(buf, NULL, 0);

	sprintf(path, "%s/size", ndns_base);
	if (sysfs_read_attr(ctx, path, buf) < 0)
		goto err_read;
	ndns->size = strtoull(buf, NULL, 0);

	sprintf(path, "%s/force_raw", ndns_base);
	if (sysfs_read_attr(ctx, path, buf) < 0)
		goto err_read;
	ndns->raw_mode = strtoul(buf, NULL, 0);

	switch (ndns->type) {
	case ND_DEVICE_NAMESPACE_BLK:
		sprintf(path, "%s/sector_size", ndns_base);
		if (sysfs_read_attr(ctx, path, buf) < 0)
			goto err_read;
		if (parse_lbasize_supported(ctx, devname, buf, &ndns->lbasize) < 0)
			goto err_read;
		/* fall through */
	case ND_DEVICE_NAMESPACE_PMEM:
		sprintf(path, "%s/alt_name", ndns_base);
		if (sysfs_read_attr(ctx, path, buf) < 0)
			goto err_read;
		ndns->alt_name = strdup(buf);
		if (!ndns->alt_name)
			goto err_read;

		sprintf(path, "%s/uuid", ndns_base);
		if (sysfs_read_attr(ctx, path, buf) < 0)
			goto err_read;
		if (strlen(buf) && uuid_parse(buf, ndns->uuid) < 0) {
			dbg(ctx, "%s:%s\n", path, buf);
			rc = -EINVAL;
			goto err_read;
		}
		break;
	default:
		break;
	}

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

	ndctl_namespace_foreach(region, ndns_dup)
		if (ndns_dup->id == ndns->id) {
			free_namespace(ndns, NULL);
			return 1;
		}

	list_add(&region->namespaces, &ndns->list);
	free(path);
	return 0;

 err_read:
	free(ndns->ndns_buf);
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

	sprintf(ndns_fmt, "namespace%d.", region->id);
	device_parse(ctx, bus, region->region_path, ndns_fmt, region, add_namespace);
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

NDCTL_EXPORT const char *ndctl_namespace_get_devname(struct ndctl_namespace *ndns)
{
	return devpath_to_devname(ndns->ndns_path);
}

NDCTL_EXPORT struct ndctl_btt *ndctl_namespace_get_btt(struct ndctl_namespace *ndns)
{
	struct ndctl_region *region = ndctl_namespace_get_region(ndns);
	struct ndctl_ctx *ctx = ndctl_namespace_get_ctx(ndns);
	char *path = ndns->ndns_buf;
	int len = ndns->buf_len;
	struct ndctl_btt *btt;
	char buf[50];

	if (snprintf(path, len, "%s/holder", ndns->ndns_path) >= len) {
		err(ctx, "%s: buffer too small!\n",
				ndctl_namespace_get_devname(ndns));
		return NULL;
	}

	if (sysfs_read_attr(ctx, path, buf) < 0)
		return NULL;

	ndctl_btt_foreach(region, btt)
		if (strcmp(buf, ndctl_btt_get_devname(btt)) == 0)
			return btt;
	return NULL;
}

NDCTL_EXPORT const char *ndctl_namespace_get_block_device(struct ndctl_namespace *ndns)
{
	struct ndctl_ctx *ctx = ndctl_namespace_get_ctx(ndns);
	struct ndctl_bus *bus = ndctl_namespace_get_bus(ndns);
	char *path = ndns->ndns_buf;
	int len = ndns->buf_len;

	if (ndns->bdev)
		return ndns->bdev;

	if (snprintf(path, len, "%s/block", ndns->ndns_path) >= len) {
		err(ctx, "%s: buffer too small!\n",
				ndctl_namespace_get_devname(ndns));
		return "";
	}

	ndctl_bus_wait_probe(bus);
	ndns->bdev = get_block_device(ctx, path);
	return ndns->bdev ? ndns->bdev : "";
}

NDCTL_EXPORT int ndctl_namespace_is_valid(struct ndctl_namespace *ndns)
{
	struct ndctl_region *region = ndctl_namespace_get_region(ndns);

	return ndns->generation == region->generation;
}

NDCTL_EXPORT int ndctl_namespace_get_raw_mode(struct ndctl_namespace *ndns)
{
	return ndns->raw_mode;
}

NDCTL_EXPORT int ndctl_namespace_set_raw_mode(struct ndctl_namespace *ndns,
		int raw_mode)
{
	struct ndctl_ctx *ctx = ndctl_namespace_get_ctx(ndns);
	char *path = ndns->ndns_buf;
	int len = ndns->buf_len;

	if (snprintf(path, len, "%s/force_raw", ndns->ndns_path) >= len) {
		err(ctx, "%s: buffer too small!\n",
				ndctl_namespace_get_devname(ndns));
		return -ENXIO;
	}

	raw_mode = !!raw_mode;
	if (sysfs_write_attr(ctx, path, raw_mode ? "1\n" : "0\n") < 0)
		return -ENXIO;

	ndns->raw_mode = raw_mode;
	return raw_mode;
}

NDCTL_EXPORT int ndctl_namespace_is_enabled(struct ndctl_namespace *ndns)
{
	struct ndctl_ctx *ctx = ndctl_namespace_get_ctx(ndns);
	char *path = ndns->ndns_buf;
	int len = ndns->buf_len;

	if (snprintf(path, len, "%s/driver", ndns->ndns_path) >= len) {
		err(ctx, "%s: buffer too small!\n",
				ndctl_namespace_get_devname(ndns));
		return 0;
	}

	return is_enabled(ndctl_namespace_get_bus(ndns), path);
}

static int ndctl_bind(struct ndctl_ctx *ctx, struct kmod_module *module,
		const char *devname)
{
	int rc;
	DIR *dir;
	char path[200];
	struct dirent *de;
	const int len = sizeof(path);

	if (!devname) {
		err(ctx, "missing devname\n");
		return -EINVAL;
	}

	if (module) {
		rc = kmod_module_probe_insert_module(module,
				KMOD_PROBE_APPLY_BLACKLIST, NULL, NULL, NULL,
				NULL);
		if (rc < 0) {
			err(ctx, "%s: insert failure: %d\n", __func__, rc);
			return rc;
		}
	}

	if (snprintf(path, len, "/sys/bus/nd/drivers") >= len) {
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

		rc = sysfs_write_attr_quiet(ctx, drv_path, devname);
		free(drv_path);
		if (rc == 0)
			break;
	}
	closedir(dir);

	if (rc) {
		err(ctx, "%s: bind failed\n", devname);
		return -ENXIO;
	}
	return 0;
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

static int add_btt(void *parent, int id, const char *btt_base);

static void btts_init(struct ndctl_region *region)
{
	struct ndctl_bus *bus = ndctl_region_get_bus(region);
	char btt_fmt[20];

	if (region->btts_init)
		return;
	region->btts_init = 1;

	sprintf(btt_fmt, "btt%d.", region->id);
	device_parse(bus->ctx, bus, region->region_path, btt_fmt, region, add_btt);
}

/*
 * Return 0 if enabled, < 0 if failed to enable, and > 0 if claimed by
 * another device and that device is enabled.  In the > 0 case a
 * subsequent call to ndctl_namespace_is_enabled() will return 'false'.
 */
NDCTL_EXPORT int ndctl_namespace_enable(struct ndctl_namespace *ndns)
{
	const char *devname = ndctl_namespace_get_devname(ndns);
	struct ndctl_ctx *ctx = ndctl_namespace_get_ctx(ndns);
	struct ndctl_region *region = ndns->region;
	int rc;

	if (ndctl_namespace_is_enabled(ndns))
		return 0;

	rc = ndctl_bind(ctx, ndns->module, devname);

	if (!ndctl_namespace_is_enabled(ndns)) {
		struct ndctl_btt *btt = ndctl_namespace_get_btt(ndns);

		if (btt && ndctl_btt_is_enabled(btt)) {
			dbg(ctx, "%s: enabled via %s\n", devname,
					ndctl_btt_get_devname(btt));
			rc = 1;
			goto out;
		}
		err(ctx, "%s: failed to enable\n", devname);
		return rc ? rc : -ENXIO;
	}
	rc = 0;
	dbg(ctx, "%s: enabled\n", devname);

	/*
	 * Rescan now as successfully enabling a namespace device leads
	 * to a new one being created, and potentially btts being attached
	 */
 out:
	region->namespaces_init = 0;
	region->btts_init = 0;
	namespaces_init(region);
	btts_init(region);

	return rc;
}

NDCTL_EXPORT int ndctl_namespace_disable(struct ndctl_namespace *ndns)
{
	struct ndctl_ctx *ctx = ndctl_namespace_get_ctx(ndns);
	const char *devname = ndctl_namespace_get_devname(ndns);

	if (!ndctl_namespace_is_enabled(ndns))
		return 0;

	ndctl_unbind(ctx, ndns->ndns_path);

	if (ndctl_namespace_is_enabled(ndns)) {
		err(ctx, "%s: failed to disable\n", devname);
		return -EBUSY;
	}

	free(ndns->bdev);
	ndns->bdev = NULL;

	dbg(ctx, "%s: disabled\n", devname);
	return 0;
}

static int pmem_namespace_is_configured(struct ndctl_namespace *ndns)
{
	if (ndctl_namespace_get_size(ndns) < ND_MIN_NAMESPACE_SIZE)
		return 0;

	if (memcmp(&ndns->uuid, null_uuid, sizeof(null_uuid)) == 0)
		return 0;

	return 1;
}

static int blk_namespace_is_configured(struct ndctl_namespace *ndns)
{
	if (pmem_namespace_is_configured(ndns) == 0)
		return 0;

	if (ndctl_namespace_get_sector_size(ndns) == 0)
		return 0;

	return 1;
}

NDCTL_EXPORT int ndctl_namespace_is_configured(struct ndctl_namespace *ndns)
{
	struct ndctl_ctx *ctx = ndctl_namespace_get_ctx(ndns);

	switch (ndctl_namespace_get_type(ndns)) {
	case ND_DEVICE_NAMESPACE_PMEM:
		return pmem_namespace_is_configured(ndns);
	case ND_DEVICE_NAMESPACE_IO:
		return 1;
	case ND_DEVICE_NAMESPACE_BLK:
		return blk_namespace_is_configured(ndns);
	default:
		dbg(ctx, "%s: nstype: %d is_configured() not implemented\n",
				ndctl_namespace_get_devname(ndns),
				ndctl_namespace_get_type(ndns));
		return -ENXIO;
	}
}

NDCTL_EXPORT void ndctl_namespace_get_uuid(struct ndctl_namespace *ndns, uuid_t uu)
{
	memcpy(uu, ndns->uuid, sizeof(uuid_t));
}

NDCTL_EXPORT int ndctl_namespace_set_uuid(struct ndctl_namespace *ndns, uuid_t uu)
{
	struct ndctl_ctx *ctx = ndctl_namespace_get_ctx(ndns);
	char *path = ndns->ndns_buf;
	int len = ndns->buf_len;
	char uuid[40];

	if (snprintf(path, len, "%s/uuid", ndns->ndns_path) >= len) {
		err(ctx, "%s: buffer too small!\n",
				ndctl_namespace_get_devname(ndns));
		return -ENXIO;
	}

	uuid_unparse(uu, uuid);
	if (sysfs_write_attr(ctx, path, uuid) != 0)
		return -ENXIO;
	memcpy(ndns->uuid, uu, sizeof(uuid_t));
	return 0;
}

NDCTL_EXPORT unsigned int ndctl_namespace_get_supported_sector_size(
		struct ndctl_namespace *ndns, int i)
{
	if (ndns->lbasize.num == 0)
		return 0;

	if (i < 0 || i > ndns->lbasize.num)
		return UINT_MAX;
	else
		return ndns->lbasize.supported[i];
}

NDCTL_EXPORT unsigned int ndctl_namespace_get_sector_size(struct ndctl_namespace *ndns)
{
	return ndctl_namespace_get_supported_sector_size(ndns, ndns->lbasize.select);
}

NDCTL_EXPORT int ndctl_namespace_get_num_sector_sizes(struct ndctl_namespace *ndns)
{
	return ndns->lbasize.num;
}

NDCTL_EXPORT int ndctl_namespace_set_sector_size(struct ndctl_namespace *ndns,
		unsigned int sector_size)
{
	struct ndctl_ctx *ctx = ndctl_namespace_get_ctx(ndns);
	char *path = ndns->ndns_buf;
	int len = ndns->buf_len;
	char sector_str[40];
	int i;

	if (ndns->lbasize.num == 0)
		return -ENXIO;

	if (snprintf(path, len, "%s/sector_size", ndns->ndns_path) >= len) {
		err(ctx, "%s: buffer too small!\n",
				ndctl_namespace_get_devname(ndns));
		return -ENXIO;
	}

	sprintf(sector_str, "%d\n", sector_size);
	if (sysfs_write_attr(ctx, path, sector_str) != 0)
		return -ENXIO;

	for (i = 0; i < ndns->lbasize.num; i++)
		if (ndns->lbasize.supported[i] == sector_size)
			ndns->lbasize.select = i;
	return 0;
}

NDCTL_EXPORT const char *ndctl_namespace_get_alt_name(struct ndctl_namespace *ndns)
{
	if (ndns->alt_name)
		return ndns->alt_name;
	return "";
}

NDCTL_EXPORT int ndctl_namespace_set_alt_name(struct ndctl_namespace *ndns,
		const char *alt_name)
{
	struct ndctl_ctx *ctx = ndctl_namespace_get_ctx(ndns);
	char *path = ndns->ndns_buf;
	int len = ndns->buf_len;
	char *buf;

	if (!ndns->alt_name)
		return 0;

	if (strlen(alt_name) >= (size_t) NSLABEL_NAME_LEN)
		return -EINVAL;

	if (snprintf(path, len, "%s/alt_name", ndns->ndns_path) >= len) {
		err(ctx, "%s: buffer too small!\n",
				ndctl_namespace_get_devname(ndns));
		return -ENXIO;
	}

	buf = strdup(alt_name);
	if (!buf)
		return -ENOMEM;

	if (sysfs_write_attr(ctx, path, buf) < 0)
		return -ENXIO;

	free(ndns->alt_name);
	ndns->alt_name = buf;
	return 0;
}

NDCTL_EXPORT unsigned long long ndctl_namespace_get_size(struct ndctl_namespace *ndns)
{
	return ndns->size;
}

static int namespace_set_size(struct ndctl_namespace *ndns,
		unsigned long long size)
{
	struct ndctl_ctx *ctx = ndctl_namespace_get_ctx(ndns);
	char *path = ndns->ndns_buf;
	int len = ndns->buf_len;
	char buf[50];

	if (snprintf(path, len, "%s/size", ndns->ndns_path) >= len) {
		err(ctx, "%s: buffer too small!\n",
				ndctl_namespace_get_devname(ndns));
		return -ENXIO;
	}

	sprintf(buf, "%#llx\n", size);
	if (sysfs_write_attr(ctx, path, buf) < 0)
		return -ENXIO;

	ndns->size = size;
	return 0;
}

NDCTL_EXPORT int ndctl_namespace_set_size(struct ndctl_namespace *ndns,
		unsigned long long size)
{
	struct ndctl_ctx *ctx = ndctl_namespace_get_ctx(ndns);

	if (size == 0) {
		dbg(ctx, "%s: use ndctl_namespace_delete() instead\n",
				ndctl_namespace_get_devname(ndns));
		return -EINVAL;
	}

	if (ndctl_namespace_is_enabled(ndns))
		return -EBUSY;

	switch (ndctl_namespace_get_type(ndns)) {
	case ND_DEVICE_NAMESPACE_PMEM:
	case ND_DEVICE_NAMESPACE_BLK:
		return namespace_set_size(ndns, size);
	default:
		dbg(ctx, "%s: nstype: %d set size failed\n",
				ndctl_namespace_get_devname(ndns),
				ndctl_namespace_get_type(ndns));
		return -ENXIO;
	}
}

NDCTL_EXPORT int ndctl_namespace_delete(struct ndctl_namespace *ndns)
{
	struct ndctl_region *region = ndctl_namespace_get_region(ndns);
	struct ndctl_ctx *ctx = ndctl_namespace_get_ctx(ndns);
	int rc;

	if (!ndctl_namespace_is_valid(ndns)) {
		free_namespace(ndns, &region->stale_namespaces);
		return 0;
	}

	if (ndctl_namespace_is_enabled(ndns))
		return -EBUSY;

        switch (ndctl_namespace_get_type(ndns)) {
        case ND_DEVICE_NAMESPACE_PMEM:
        case ND_DEVICE_NAMESPACE_BLK:
		break;
	default:
		dbg(ctx, "%s: nstype: %d delete failed\n",
				ndctl_namespace_get_devname(ndns),
				ndctl_namespace_get_type(ndns));
		return -ENXIO;
	}

	rc = namespace_set_size(ndns, 0);
	if (rc)
		return rc;

	region->namespaces_init = 0;
	free_namespace(ndns, &region->namespaces);
	return 0;
}

static int parse_lbasize_supported(struct ndctl_ctx *ctx, const char *devname,
		const char *buf, struct ndctl_lbasize *lba)
{
	char *s = strdup(buf), *end, *field;

	if (!s)
		return -ENOMEM;

	field = s;
	lba->num = 0;
	end = strchr(s, ' ');
	lba->select = -1;
	lba->supported = NULL;
	while (end) {
		unsigned int val;

		*end = '\0';
		if (sscanf(field, "[%d]", &val) == 1) {
			if (lba->select >= 0)
				goto err;
			lba->select = lba->num;
		} else if (sscanf(field, "%d", &val) == 1) {
			/* pass */;
		} else {
			break;
		}

		lba->supported = realloc(lba->supported,
				sizeof(unsigned int) * ++lba->num);
		lba->supported[lba->num - 1] = val;
		field = end + 1;
		end = strchr(field, ' ');
	}

	free(s);
	dbg(ctx, "%s: %s\n", devname, buf);
	return 0;
 err:
	free(s);
	free(lba->supported);
	lba->supported = NULL;
	lba->select = -1;
	return -ENXIO;
}

static int add_btt(void *parent, int id, const char *btt_base)
{
	struct ndctl_ctx *ctx = ndctl_region_get_ctx(parent);
	const char *devname = devpath_to_devname(btt_base);
	char *path = calloc(1, strlen(btt_base) + 100);
	struct ndctl_region *region = parent;
	struct ndctl_btt *btt, *btt_dup;
	char buf[SYSFS_ATTR_SIZE];
	int rc = -ENOMEM;

	if (!path)
		return -ENOMEM;

	btt = calloc(1, sizeof(*btt));
	if (!btt)
		goto err_btt;
	btt->id = id;
	btt->region = region;
	btt->generation = region->generation;

	btt->btt_path = strdup(btt_base);
	if (!btt->btt_path)
		goto err_read;

	btt->btt_buf = calloc(1, strlen(btt_base) + 50);
	if (!btt->btt_buf)
		goto err_read;
	btt->buf_len = strlen(btt_base) + 50;

	sprintf(path, "%s/modalias", btt_base);
	if (sysfs_read_attr(ctx, path, buf) < 0)
		goto err_read;
	btt->module = to_module(ctx, buf);

	sprintf(path, "%s/uuid", btt_base);
	if (sysfs_read_attr(ctx, path, buf) < 0)
		goto err_read;
	if (strlen(buf) && uuid_parse(buf, btt->uuid) < 0) {
		rc = -EINVAL;
		goto err_read;
	}

	sprintf(path, "%s/sector_size", btt_base);
	if (sysfs_read_attr(ctx, path, buf) < 0)
		goto err_read;
	if (parse_lbasize_supported(ctx, devname, buf, &btt->lbasize) < 0)
		goto err_read;

	free(path);
	ndctl_btt_foreach(region, btt_dup)
		if (btt->id == btt_dup->id) {
			free_btt(btt, NULL);
			return 1;
		}

	list_add(&region->btts, &btt->list);
	return 0;

 err_read:
	free(btt->lbasize.supported);
	free(btt->btt_buf);
	free(btt->btt_path);
	free(btt);
 err_btt:
	free(path);
	return rc;
}

NDCTL_EXPORT struct ndctl_btt *ndctl_btt_get_first(struct ndctl_region *region)
{
	btts_init(region);

	return list_top(&region->btts, struct ndctl_btt, list);
}

NDCTL_EXPORT struct ndctl_btt *ndctl_btt_get_next(struct ndctl_btt *btt)
{
	struct ndctl_region *region = btt->region;

	return list_next(&region->btts, btt, list);
}

NDCTL_EXPORT unsigned int ndctl_btt_get_id(struct ndctl_btt *btt)
{
	return btt->id;
}

NDCTL_EXPORT unsigned int ndctl_btt_get_supported_sector_size(
		struct ndctl_btt *btt, int i)
{
	if (i < 0 || i > btt->lbasize.num)
		return UINT_MAX;
	else
		return btt->lbasize.supported[i];
}

NDCTL_EXPORT unsigned int ndctl_btt_get_sector_size(struct ndctl_btt *btt)
{
	return ndctl_btt_get_supported_sector_size(btt, btt->lbasize.select);
}

NDCTL_EXPORT int ndctl_btt_get_num_sector_sizes(struct ndctl_btt *btt)
{
	return btt->lbasize.num;
}

NDCTL_EXPORT struct ndctl_namespace *ndctl_btt_get_namespace(struct ndctl_btt *btt)
{
	struct ndctl_ctx *ctx = ndctl_btt_get_ctx(btt);
	struct ndctl_namespace *ndns, *found = NULL;
	struct ndctl_region *region = btt->region;
	char *path = region->region_buf;
	int len = region->buf_len;
	char buf[50];

	if (btt->ndns)
		return btt->ndns;

	if (snprintf(path, len, "%s/namespace", btt->btt_path) >= len) {
		err(ctx, "%s: buffer too small!\n",
				ndctl_btt_get_devname(btt));
		return NULL;
	}

	if (sysfs_read_attr(ctx, path, buf) < 0)
		return NULL;

	ndctl_namespace_foreach(region, ndns)
		if (strcmp(buf, ndctl_namespace_get_devname(ndns)) == 0)
			found = ndns;
	btt->ndns = found;
	return found;
}

NDCTL_EXPORT void ndctl_btt_get_uuid(struct ndctl_btt *btt, uuid_t uu)
{
	memcpy(uu, btt->uuid, sizeof(uuid_t));
}

NDCTL_EXPORT int ndctl_btt_set_uuid(struct ndctl_btt *btt, uuid_t uu)
{
	struct ndctl_ctx *ctx = ndctl_btt_get_ctx(btt);
	char *path = btt->btt_buf;
	int len = btt->buf_len;
	char uuid[40];

	if (snprintf(path, len, "%s/uuid", btt->btt_path) >= len) {
		err(ctx, "%s: buffer too small!\n",
				ndctl_btt_get_devname(btt));
		return -ENXIO;
	}

	uuid_unparse(uu, uuid);
	if (sysfs_write_attr(ctx, path, uuid) != 0)
		return -ENXIO;
	memcpy(btt->uuid, uu, sizeof(uuid_t));
	return 0;
}

NDCTL_EXPORT int ndctl_btt_set_sector_size(struct ndctl_btt *btt,
		unsigned int sector_size)
{
	struct ndctl_ctx *ctx = ndctl_btt_get_ctx(btt);
	char *path = btt->btt_buf;
	int len = btt->buf_len;
	char sector_str[40];
	int i;

	if (snprintf(path, len, "%s/sector_size", btt->btt_path) >= len) {
		err(ctx, "%s: buffer too small!\n",
				ndctl_btt_get_devname(btt));
		return -ENXIO;
	}

	sprintf(sector_str, "%d\n", sector_size);
	if (sysfs_write_attr(ctx, path, sector_str) != 0)
		return -ENXIO;

	for (i = 0; i < btt->lbasize.num; i++)
		if (btt->lbasize.supported[i] == sector_size)
			btt->lbasize.select = i;
	return 0;
}

NDCTL_EXPORT int ndctl_btt_set_namespace(struct ndctl_btt *btt,
		struct ndctl_namespace *ndns)
{
	struct ndctl_ctx *ctx = ndctl_btt_get_ctx(btt);
	char *path = btt->btt_buf;
	int len = btt->buf_len;

	if (snprintf(path, len, "%s/namespace", btt->btt_path) >= len) {
		err(ctx, "%s: buffer too small!\n",
				ndctl_btt_get_devname(btt));
		return -ENXIO;
	}

	if (sysfs_write_attr(ctx, path, ndns
				? ndctl_namespace_get_devname(ndns) : "\n"))
		return -ENXIO;

	btt->ndns = ndns;
	return 0;
}

NDCTL_EXPORT struct ndctl_bus *ndctl_btt_get_bus(struct ndctl_btt *btt)
{
	return btt->region->bus;
}

NDCTL_EXPORT struct ndctl_ctx *ndctl_btt_get_ctx(struct ndctl_btt *btt)
{
	return ndctl_bus_get_ctx(ndctl_btt_get_bus(btt));
}

NDCTL_EXPORT const char *ndctl_btt_get_devname(struct ndctl_btt *btt)
{
	return devpath_to_devname(btt->btt_path);
}

NDCTL_EXPORT const char *ndctl_btt_get_block_device(struct ndctl_btt *btt)
{
	struct ndctl_ctx *ctx = ndctl_btt_get_ctx(btt);
	struct ndctl_bus *bus = ndctl_btt_get_bus(btt);
	char *path = btt->btt_buf;
	int len = btt->buf_len;

	if (btt->bdev)
		return btt->bdev;

	if (snprintf(path, len, "%s/block", btt->btt_path) >= len) {
		err(ctx, "%s: buffer too small!\n",
				ndctl_btt_get_devname(btt));
		return "";
	}

	ndctl_bus_wait_probe(bus);
	btt->bdev = get_block_device(ctx, path);
	return btt->bdev ? btt->bdev : "";
}

NDCTL_EXPORT int ndctl_btt_is_valid(struct ndctl_btt *btt)
{
	struct ndctl_region *region = ndctl_btt_get_region(btt);

	return btt->generation == region->generation;
}

NDCTL_EXPORT int ndctl_btt_is_enabled(struct ndctl_btt *btt)
{
	struct ndctl_ctx *ctx = ndctl_btt_get_ctx(btt);
	char *path = btt->btt_buf;
	int len = btt->buf_len;

	if (snprintf(path, len, "%s/driver", btt->btt_path) >= len) {
		err(ctx, "%s: buffer too small!\n",
				ndctl_btt_get_devname(btt));
		return 0;
	}

	return is_enabled(ndctl_btt_get_bus(btt), path);
}

NDCTL_EXPORT struct ndctl_region *ndctl_btt_get_region(struct ndctl_btt *btt)
{
	return btt->region;
}

NDCTL_EXPORT int ndctl_btt_enable(struct ndctl_btt *btt)
{
	struct ndctl_region *region = ndctl_btt_get_region(btt);
	const char *devname = ndctl_btt_get_devname(btt);
	struct ndctl_ctx *ctx = ndctl_btt_get_ctx(btt);
	char *path = btt->btt_buf;
	int len = btt->buf_len;

	if (ndctl_btt_is_enabled(btt))
		return 0;

	ndctl_bind(ctx, btt->module, devname);

	if (!ndctl_btt_is_enabled(btt)) {
		err(ctx, "%s: failed to enable\n", devname);
		return -ENXIO;
	}

	dbg(ctx, "%s: enabled\n", devname);

	if (snprintf(path, len, "%s/block", btt->btt_path) >= len) {
		err(ctx, "%s: buffer too small!\n",
				devname);
	} else {
		btt->bdev = get_block_device(ctx, path);
	}

	/*
	 * Rescan now as successfully enabling a btt device leads to a
	 * new one being created
	 */
	region->btts_init = 0;
	btts_init(region);

	return 0;
}

NDCTL_EXPORT int ndctl_btt_delete(struct ndctl_btt *btt)
{
	struct ndctl_region *region = ndctl_btt_get_region(btt);
	struct ndctl_ctx *ctx = ndctl_btt_get_ctx(btt);
	int rc;

	if (!ndctl_btt_is_valid(btt)) {
		free_btt(btt, &region->stale_btts);
		return 0;
	}

	ndctl_unbind(ctx, btt->btt_path);

	rc = ndctl_btt_set_namespace(btt, NULL);
	if (rc) {
		dbg(ctx, "%s: failed to clear namespace: %d\n",
			ndctl_btt_get_devname(btt), rc);
		return rc;
	}

	free_btt(btt, &region->btts);
	region->btts_init = 0;

	return 0;
}

NDCTL_EXPORT int ndctl_btt_is_configured(struct ndctl_btt *btt)
{
	if (ndctl_btt_get_namespace(btt))
		return 1;

	if (ndctl_btt_get_sector_size(btt) != UINT_MAX)
		return 1;

	if (memcmp(&btt->uuid, null_uuid, sizeof(null_uuid)) != 0)
		return 1;

	return 0;
}
