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

struct ndctl_ctx;
struct ndctl_bus {
	struct ndctl_ctx *ctx;
	int id, major, minor, format, revision;
	char *provider;
	struct list_head dimms;
	struct list_head regions;
	struct list_node list;
	int dimms_init;
	int regions_init;
	char *bus_path;
};

struct ndctl_dimm {
	struct ndctl_bus *bus;
	unsigned int nfit_handle;
	struct list_node list;
};

struct ndctl_mapping {
	struct ndctl_region *region;
	struct ndctl_dimm *dimm;
	unsigned long long offset, length;
	struct list_node list;
};

struct ndctl_region {
	struct ndctl_bus *bus;
	int id, interleave_ways, num_mappings;
	int mappings_init;
	unsigned long long size;
	char *type;
	char *region_path;
	struct list_head mappings;
	struct list_node list;
};

/**
 * SECTION:libndctl
 * @short_description: libndctl context
 *
 * The context contains the default values for the library user,
 * and is passed to all library operations.
 */

/**
 * ndctl_ctx:
 *
 * Opaque object representing the library context.
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
 * ndctl_get_userdata:
 * @ctx: ndctl library context
 *
 * Retrieve stored data pointer from library context. This might be useful
 * to access from callbacks like a custom logging function.
 *
 * Returns: stored userdata
 **/
NDCTL_EXPORT void *ndctl_get_userdata(struct ndctl_ctx *ctx)
{
	if (ctx == NULL)
		return NULL;
	return ctx->userdata;
}

/**
 * ndctl_set_userdata:
 * @ctx: ndctl library context
 * @userdata: data pointer
 *
 * Store custom @userdata in the library context.
 **/
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
 * ndctl_new: create a ndctl library context
 * @ctx: context to establish
 *
 * The initial refcount is 1, and needs to be decremented to release the
 * resources of the ndctl library context.
 */
NDCTL_EXPORT int ndctl_new(struct ndctl_ctx **ctx)
{
	struct ndctl_ctx *c;
	const char *env;

	c = calloc(1, sizeof(struct ndctl_ctx));
	if (!c)
		return -ENOMEM;

	c->refcount = 1;
	c->log_fn = log_stderr;
	c->log_priority = LOG_ERR;
	list_head_init(&c->busses);

	/* environment overwrites config */
	env = secure_getenv("NDCTL_LOG");
	if (env != NULL)
		ndctl_set_log_priority(c, log_priority(env));

	info(c, "ctx %p created\n", c);
	dbg(c, "log_priority=%d\n", c->log_priority);
	*ctx = c;
	return 0;
}

/**
 * ndctl_ref:
 * @ctx: ndctl library context
 *
 * Take a reference of the ndctl library context.
 *
 * Returns: the passed ndctl library context
 **/
NDCTL_EXPORT struct ndctl_ctx *ndctl_ref(struct ndctl_ctx *ctx)
{
	if (ctx == NULL)
		return NULL;
	ctx->refcount++;
	return ctx;
}

static void free_region(struct ndctl_region *region)
{
	struct ndctl_bus *bus = region->bus;
	struct ndctl_mapping *mapping, *_m;

	list_for_each_safe(&region->mappings, mapping, _m, list) {
		list_del_from(&region->mappings, &mapping->list);
		free(mapping);
	}
	list_del_from(&bus->regions, &region->list);
	free(region->region_path);
	free(region->type);
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
 * ndctl_unref:
 * @ctx: ndctl library context
 *
 * Drop a reference of the ndctl library context.
 *
 **/
NDCTL_EXPORT struct ndctl_ctx *ndctl_unref(struct ndctl_ctx *ctx)
{
	if (ctx == NULL)
		return NULL;
	ctx->refcount--;
	if (ctx->refcount > 0)
		return NULL;
	info(ctx, "context %p released\n", ctx);
	free_context(ctx);
	return NULL;
}

/**
 * ndctl_set_log_fn:
 * @ctx: ndctl library context
 * @log_fn: function to be called for logging messages
 *
 * The built-in logging writes to stderr. It can be
 * overridden by a custom function, to plug log messages
 * into the user's logging functionality.
 *
 **/
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
 * ndctl_get_log_priority:
 * @ctx: ndctl library context
 *
 * Returns: the current logging priority
 **/
NDCTL_EXPORT int ndctl_get_log_priority(struct ndctl_ctx *ctx)
{
	return ctx->log_priority;
}

/**
 * ndctl_set_log_priority:
 * @ctx: ndctl library context
 * @priority: the new logging priority
 *
 * Set the current logging priority. The value controls which messages
 * are logged.
 **/
NDCTL_EXPORT void ndctl_set_log_priority(struct ndctl_ctx *ctx, int priority)
{
	ctx->log_priority = priority;
}

#define SYSFS_ATTR_SIZE 1024

static int sysfs_read_attr(struct ndctl_ctx *ctx, char *path, char *buf)
{
	int fd = open(path, O_RDONLY);
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

static int add_bus(struct ndctl_ctx *ctx, int id, const char *ctl_base)
{
	int rc = -ENOMEM;
	struct ndctl_bus *bus;
	char buf[SYSFS_ATTR_SIZE];
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
		goto err_read;
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

	bus->bus_path = parent_dev_path("char", bus->major, bus->minor);
	if (!bus->bus_path)
		goto err_dev_path;
	list_add(&ctx->busses, &bus->list);
	dbg(ctx, "bus%d provider: %s format: %d revision: %d\n",
		bus->id, bus->provider, bus->format, bus->revision);

	rc = 0;
	goto out;

 err_dev_path:
	free(bus->provider);
 err_read:
	free(bus);
 out:
 err_bus:
	free(path);

	return rc;
}

static void busses_init(struct ndctl_ctx *ctx)
{
	const char *class_path = "/sys/class/nd_bus";
	struct dirent *de;
	DIR *dir;

	if (ctx->busses_init)
		return;
	ctx->busses_init = 1;

	dir = opendir(class_path);
	if (!dir) {
		info(ctx, "no busses found\n");
		return;
	}

	while ((de = readdir(dir)) != NULL) {
		char *ctl_path;
		int id, rc;

		if (de->d_ino == 0)
			continue;
		if (sscanf(de->d_name, "ndctl%d", &id) != 1)
			continue;
		if (asprintf(&ctl_path, "%s/%s", class_path, de->d_name) < 0) {
			err(ctx, "allocation failure\n");
			continue;
		}

		rc = add_bus(ctx, id, ctl_path);
		free(ctl_path);
		if (rc) {
			err(ctx, "failed to add bus: %d\n", rc);
			continue;
		}
	}
	closedir(dir);
}

/**
 * ndctl_bus_get_first: [initialize bus list] retrieve first bus
 * @ctx: ndctl ctx context
 */
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

NDCTL_EXPORT unsigned int ndctl_bus_get_format(struct ndctl_bus *bus)
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

static int add_dimm(struct ndctl_bus *bus, unsigned int node, unsigned int sicd)
{
	struct ndctl_dimm *dimm = calloc(1, sizeof(*dimm));
	struct ndctl_ctx *ctx = bus->ctx;
	
	if (!dimm)
		return -ENOMEM;

	dimm->bus = bus;
	dimm->nfit_handle = node << 16 | sicd;
	list_add(&bus->dimms, &dimm->list);
	dbg(ctx, "dimm-%.3x:%.4x\n", node, sicd);
	return 0;
}

static void dimms_init(struct ndctl_bus *bus)
{
	struct ndctl_ctx *ctx = bus->ctx;
	struct dirent *de;
	DIR *dir;

	if (bus->dimms_init)
		return;
	bus->dimms_init = 1;

	dir = opendir(bus->bus_path);
	if (!dir) {
		err(ctx, "failed to open bus%d\n", bus->id);
		return;
	}

	while ((de = readdir(dir)) != NULL) {
		unsigned int node, sicd;

		if (de->d_ino == 0)
			continue;
		if (sscanf(de->d_name, "dimm-%x:%x", &node, &sicd) != 2)
			continue;
		if (add_dimm(bus, node, sicd))
			err(ctx, "add_dimm: allocation failure\n");
	}
	closedir(dir);
}

/**
 * ndctl_dimm_get_first: [initialize dimm list] retrieve first dimm
 * @bus: ndctl bus context
 */
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
	return dimm->nfit_handle;
}

NDCTL_EXPORT unsigned int ndctl_dimm_get_node(struct ndctl_dimm *dimm)
{
	return dimm->nfit_handle >> 16 & 0xfff;
}

NDCTL_EXPORT unsigned int ndctl_dimm_get_socket(struct ndctl_dimm *dimm)
{
	return dimm->nfit_handle >> 12 & 0xf;
}

NDCTL_EXPORT unsigned int ndctl_dimm_get_imc(struct ndctl_dimm *dimm)
{
	return dimm->nfit_handle >> 8 & 0xf;
}

NDCTL_EXPORT unsigned int ndctl_dimm_get_channel(struct ndctl_dimm *dimm)
{
	return dimm->nfit_handle >> 4 & 0xf;
}

NDCTL_EXPORT unsigned int ndctl_dimm_get_dimm(struct ndctl_dimm *dimm)
{
	return dimm->nfit_handle & 0xf;
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
		if (dimm->nfit_handle == handle)
			return dimm;

	return NULL;
}

static int add_region(struct ndctl_bus *bus, int spa_id, const char *region_base)
{
	int rc = -ENOMEM;
	char buf[SYSFS_ATTR_SIZE];
	struct ndctl_region *region;
	struct ndctl_ctx *ctx = bus->ctx;
	char *path = calloc(1, strlen(region_base) + 20);

	if (!path)
		return -ENOMEM;

	region = calloc(1, sizeof(*region));
	if (!region)
		goto err_region;
	list_head_init(&region->mappings);
	region->bus = bus;
	region->id = spa_id;

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

	sprintf(path, "%s/type", region_base);
	if (sysfs_read_attr(ctx, path, buf) < 0)
		goto err_read;
	region->type = strdup(buf);
	if (!region->type)
		goto err_read;

	region->region_path = strdup(region_base);
	if (!region->region_path)
		goto err_region_path;
	list_add(&bus->regions, &region->list);
	dbg(ctx, "region%d type: %s\n", region->id, region->type);

	rc = 0;
	goto out;

 err_region_path:
	free(region->type);
 err_read:
	free(region);
 out:
 err_region:
	free(path);

	return rc;
}

static void regions_init(struct ndctl_bus *bus)
{
	struct ndctl_ctx *ctx = bus->ctx;
	struct dirent *de;
	DIR *dir;

	if (bus->regions_init)
		return;
	bus->regions_init = 1;

	dir = opendir(bus->bus_path);
	if (!dir) {
		err(ctx, "failed to open bus%d\n", bus->id);
		return;
	}
	while ((de = readdir(dir)) != NULL) {
		unsigned int bus_id, spa_id;
		char *region_path;
		int rc;

		if (de->d_ino == 0)
			continue;
		if (sscanf(de->d_name, "region%d-%d", &bus_id, &spa_id) != 2)
			continue;
		if (asprintf(&region_path, "%s/%s", bus->bus_path, de->d_name) < 0) {
			err(ctx, "allocation failure\n");
			continue;
		}

		rc = add_region(bus, spa_id, region_path);
		free(region_path);
		if (rc) {
			err(ctx, "failed to add region: %d\n", rc);
			continue;
		}
	}
	closedir(dir);
}

/**
 * ndctl_region_get_first: [initialize region list] retrieve first region
 * @bus: ndctl bus context
 */
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

NDCTL_EXPORT const char *ndctl_region_get_type(struct ndctl_region *region)
{
	return region->type;
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

/**
 * ndctl_mapping_get_first: [initialize mapping list] retrieve first mapping
 * @region: ndctl region context
 */
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
