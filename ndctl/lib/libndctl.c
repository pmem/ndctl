// SPDX-License-Identifier: LGPL-2.1
// Copyright (C) 2014-2020, Intel Corporation. All rights reserved.
#include <poll.h>
#include <stdio.h>
#include <signal.h>
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
#include <ccan/build_assert/build_assert.h>

#include <ndctl.h>
#include <util/util.h>
#include <util/size.h>
#include <util/sysfs.h>
#include <ndctl/libndctl.h>
#include <ndctl/namespace.h>
#include <daxctl/libdaxctl.h>
#include <ndctl/libndctl-nfit.h>
#include "private.h"

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
	return ALIGN(sizeof(struct namespace_index), NSINDEX_ALIGN);
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
	/* TODO: v1.2 label support */
	return offsetof(struct namespace_label, type_guid);
}

NDCTL_EXPORT double ndctl_decode_smart_temperature(unsigned int temp)
{
	bool negative = !!(temp & (1 << 15));
	double t;

	temp &= ~(1 << 15);
	t = temp;
	t /= 16;
	if (negative)
		t *= -1;
	return t;
}

NDCTL_EXPORT unsigned int ndctl_encode_smart_temperature(double temp)
{
	bool negative = false;
	unsigned int t;

	if  (temp < 0) {
		negative = true;
		temp *= -1;
	}
	t = temp;
	t *= 16;
	if (negative)
		t |= (1 << 15);
	return t;
}

struct ndctl_ctx;

/**
 * struct ndctl_mapping - dimm extent relative to a region
 * @dimm: backing dimm for the mapping
 * @offset: dimm relative offset
 * @length: span of the extent
 * @position: interleave-order of the extent
 *
 * This data can be used to identify the dimm ranges contributing to a
 * region / interleave-set and identify how regions alias each other.
 */
struct ndctl_mapping {
	struct ndctl_region *region;
	struct ndctl_dimm *dimm;
	unsigned long long offset, length;
	int position;
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
 * @numa_node: numa node attribute
 * @target_node: target node were this region to be onlined
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
	unsigned long align;
	int mappings_init;
	int namespaces_init;
	int btts_init;
	int pfns_init;
	int daxs_init;
	int refresh_type;
	unsigned long long size;
	char *region_path;
	char *region_buf;
	int buf_len;
	int generation;
	int numa_node, target_node;
	struct list_head btts;
	struct list_head pfns;
	struct list_head daxs;
	struct list_head mappings;
	struct list_head namespaces;
	struct list_head stale_namespaces;
	struct list_head stale_btts;
	struct list_head stale_pfns;
	struct list_head stale_daxs;
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
	struct badblocks_iter bb_iter;
	enum ndctl_persistence_domain persistence_domain;
	/* file descriptor for deep flush sysfs entry */
	int flush_fd;
};

/**
 * struct ndctl_btt - stacked block device provided sector atomicity
 * @module: kernel module (nd_btt)
 * @lbasize: sector size info
 * @size: usable size of the btt after removing metadata etc
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
	unsigned long long size;
	char *btt_path;
	char *btt_buf;
	char *bdev;
	int buf_len;
	uuid_t uuid;
	int id, generation;
};

/**
 * struct ndctl_pfn - reservation for per-page-frame metadata
 * @module: kernel module (nd_pfn)
 * @ndns: host namespace for the pfn instance
 * @loc: host metadata location (ram or pmem (default))
 * @align: data offset alignment
 * @region: parent region
 * @pfn_path: pfn devpath
 * @uuid: unique identifier for a pfn instance
 * @pfn_buf: space to print paths for bind/unbind operations
 * @bdev: block device associated with a pfn
 */
struct ndctl_pfn {
	struct kmod_module *module;
	struct ndctl_region *region;
	struct ndctl_namespace *ndns;
	struct list_node list;
	enum ndctl_pfn_loc loc;
	unsigned long align;
	unsigned long long resource, size;
	char *pfn_path;
	char *pfn_buf;
	char *bdev;
	int buf_len;
	uuid_t uuid;
	int id, generation;
	struct ndctl_lbasize alignments;
};

struct ndctl_dax {
	struct ndctl_pfn pfn;
	struct daxctl_region *region;
};

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
	struct daxctl_ctx *daxctl_ctx;
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

	rc = daxctl_new(&daxctl_ctx);
	if (rc)
		goto err_daxctl;

	c = calloc(1, sizeof(struct ndctl_ctx));
	if (!c) {
		rc = -ENOMEM;
		goto err_ctx;
	}

	c->refcount = 1;
	log_init(&c->ctx, "libndctl", "NDCTL_LOG");
	c->udev = udev;
	c->timeout = 5000;
	list_head_init(&c->busses);

	info(c, "ctx %p created\n", c);
	dbg(c, "log_priority=%d\n", c->ctx.log_priority);
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
	c->daxctl_ctx = daxctl_ctx;

	return 0;
 err_ctx:
	daxctl_unref(daxctl_ctx);
 err_daxctl:
	kmod_unref(kmod_ctx);
 err_kmod:
	udev_unref(udev);
	return rc;
}

NDCTL_EXPORT void ndctl_set_private_data(struct ndctl_ctx *ctx, void *data)
{
	ctx->private_data = data;
}

NDCTL_EXPORT void *ndctl_get_private_data(struct ndctl_ctx *ctx)
{
	return ctx->private_data;
}

NDCTL_EXPORT struct daxctl_ctx *ndctl_get_daxctl_ctx(struct ndctl_ctx *ctx)
{
	return ctx->daxctl_ctx;
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

static void badblocks_iter_free(struct badblocks_iter *bb_iter)
{
	if (bb_iter->file)
		fclose(bb_iter->file);
}

static int badblocks_iter_init(struct badblocks_iter *bb_iter, const char *path)
{
	char *bb_path;
	int rc = 0;

	/* if the file is already open */
	if (bb_iter->file) {
		fclose(bb_iter->file);
		bb_iter->file = NULL;
	}

	if (asprintf(&bb_path, "%s/badblocks", path) < 0)
		return -errno;

	bb_iter->file = fopen(bb_path, "re");
	if (!bb_iter->file) {
		rc = -errno;
		free(bb_path);
		return rc;
	}

	free(bb_path);
	return rc;
}

static struct badblock *badblocks_iter_next(struct badblocks_iter *bb_iter)
{
	int rc;
	char *buf = NULL;
	size_t rlen = 0;

	if (!bb_iter->file)
		return NULL;

	rc = getline(&buf, &rlen, bb_iter->file);
	if (rc == -1) {
		free(buf);
		return NULL;
	}

	rc = sscanf(buf, "%llu %u", &bb_iter->bb.offset, &bb_iter->bb.len);
	free(buf);
	if (rc != 2) {
		fclose(bb_iter->file);
		bb_iter->file = NULL;
		bb_iter->bb.offset = 0;
		bb_iter->bb.len = 0;
		return NULL;
	}

	return &bb_iter->bb;
}

static struct badblock *badblocks_iter_first(struct badblocks_iter *bb_iter,
		struct ndctl_ctx *ctx, const char *path)
{
	int rc;

	rc = badblocks_iter_init(bb_iter, path);
	if (rc < 0)
		return NULL;

	return badblocks_iter_next(bb_iter);
}

static void free_namespace(struct ndctl_namespace *ndns, struct list_head *head)
{
	struct ndctl_bb *bb, *next;

	if (head)
		list_del_from(head, &ndns->list);
	list_for_each_safe(&ndns->injected_bb, bb, next, list)
		free(bb);
	free(ndns->lbasize.supported);
	free(ndns->ndns_path);
	free(ndns->ndns_buf);
	free(ndns->bdev);
	free(ndns->alt_name);
	badblocks_iter_free(&ndns->bb_iter);
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
	free(btt->bdev);
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

static void __free_pfn(struct ndctl_pfn *pfn, struct list_head *head, void *to_free)
{
	if (head)
		list_del_from(head, &pfn->list);
	kmod_module_unref(pfn->module);
	free(pfn->pfn_path);
	free(pfn->pfn_buf);
	free(pfn->bdev);
	free(pfn->alignments.supported);
	free(to_free);
}

static void free_pfn(struct ndctl_pfn *pfn, struct list_head *head)
{
	__free_pfn(pfn, head, pfn);
}

static void free_dax(struct ndctl_dax *dax, struct list_head *head)
{
	__free_pfn(&dax->pfn, head, dax);
}

static void free_pfns(struct ndctl_region *region)
{
	struct ndctl_pfn *pfn, *_b;

	list_for_each_safe(&region->pfns, pfn, _b, list)
		free_pfn(pfn, &region->pfns);
}

static void free_daxs(struct ndctl_region *region)
{
	struct ndctl_dax *dax, *_b;

	list_for_each_safe(&region->daxs, dax, _b, pfn.list)
		free_dax(dax, &region->daxs);
}

static void free_stale_pfns(struct ndctl_region *region)
{
	struct ndctl_pfn *pfn, *_b;

	list_for_each_safe(&region->stale_pfns, pfn, _b, list)
		free_pfn(pfn, &region->stale_pfns);
}

static void free_stale_daxs(struct ndctl_region *region)
{
	struct ndctl_dax *dax, *_b;

	list_for_each_safe(&region->stale_daxs, dax, _b, pfn.list)
		free_dax(dax, &region->stale_daxs);
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
	free_pfns(region);
	free_stale_pfns(region);
	free_daxs(region);
	free_stale_daxs(region);
	free_namespaces(region);
	free_stale_namespaces(region);
	list_del_from(&bus->regions, &region->list);
	kmod_module_unref(region->module);
	free(region->region_buf);
	free(region->region_path);
	badblocks_iter_free(&region->bb_iter);
	if (region->flush_fd > 0)
		close(region->flush_fd);
	free(region);
}

static void free_dimm(struct ndctl_dimm *dimm)
{
	if (!dimm)
		return;
	free(dimm->unique_id);
	free(dimm->dimm_buf);
	free(dimm->dimm_path);
	if (dimm->module)
		kmod_module_unref(dimm->module);
	if (dimm->health_eventfd > -1)
		close(dimm->health_eventfd);
	ndctl_cmd_unref(dimm->ndd.cmd_read);
	free(dimm);
}

static void free_bus(struct ndctl_bus *bus, struct list_head *head)
{
	struct ndctl_dimm *dimm, *_d;
	struct ndctl_region *region, *_r;

	list_for_each_safe(&bus->dimms, dimm, _d, list) {
		list_del_from(&bus->dimms, &dimm->list);
		free_dimm(dimm);
	}
	list_for_each_safe(&bus->regions, region, _r, list)
		free_region(region);
	if (head)
		list_del_from(head, &bus->list);
	free(bus->provider);
	free(bus->bus_path);
	free(bus->bus_buf);
	free(bus->wait_probe_path);
	free(bus->scrub_path);
	free(bus);
}

static void free_context(struct ndctl_ctx *ctx)
{
	struct ndctl_bus *bus, *_b;

	list_for_each_safe(&ctx->busses, bus, _b, list)
		free_bus(bus, &ctx->busses);
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
	kmod_unref(ctx->kmod_ctx);
	daxctl_unref(ctx->daxctl_ctx);
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
                 void (*ndctl_log_fn)(struct ndctl_ctx *ctx,
                                 int priority, const char *file, int line, const char *fn,
                                 const char *format, va_list args))
{
	ctx->ctx.log_fn = (log_fn) ndctl_log_fn;
	info(ctx, "custom logging function %p registered\n", ndctl_log_fn);
}

/**
 * ndctl_get_log_priority - retrieve current library loglevel (syslog)
 * @ctx: ndctl library context
 */
NDCTL_EXPORT int ndctl_get_log_priority(struct ndctl_ctx *ctx)
{
	return ctx->ctx.log_priority;
}

/**
 * ndctl_set_log_priority - set log verbosity
 * @priority: from syslog.h, LOG_ERR, LOG_INFO, LOG_DEBUG
 *
 * Note: LOG_DEBUG requires library be built with "configure --enable-debug"
 */
NDCTL_EXPORT void ndctl_set_log_priority(struct ndctl_ctx *ctx, int priority)
{
	ctx->ctx.log_priority = priority;
	/* forward the debug level to our internal libdaxctl instance */
	daxctl_set_log_priority(ctx->daxctl_ctx, priority);
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

static int device_parse(struct ndctl_ctx *ctx, struct ndctl_bus *bus,
		const char *base_path, const char *dev_name, void *parent,
		add_dev_fn add_dev)
{
	if (bus)
		ndctl_bus_wait_probe(bus);
	return sysfs_device_parse(ctx, base_path, dev_name, parent, add_dev);
}

static int to_cmd_index(const char *name, int dimm)
{
	const char *(*cmd_name_fn)(unsigned cmd);
	int i, end_cmd;

	if (dimm) {
		end_cmd = ND_CMD_CALL;
		cmd_name_fn = nvdimm_cmd_name;
	} else {
		end_cmd = ND_CMD_CLEAR_ERROR;
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
	unsigned long cmd_mask = 0;
	char *start, *end;

	start = commands;
	while ((end = strchr(start, ' '))) {
		int cmd;

		*end = '\0';
		cmd = to_cmd_index(start, dimm);
		if (cmd)
			cmd_mask |= 1 << cmd;
		start = end + 1;
	}
	return cmd_mask;
}

static void parse_nfit_mem_flags(struct ndctl_dimm *dimm, char *flags)
{
	char *start, *end;

	start = flags;
	while ((end = strchr(start, ' '))) {
		*end = '\0';
		if (strcmp(start, "not_armed") == 0)
			dimm->flags.f_arm = 1;
		else if (strcmp(start, "save_fail") == 0)
			dimm->flags.f_save = 1;
		else if (strcmp(start, "flush_fail") == 0)
			dimm->flags.f_flush = 1;
		else if (strcmp(start, "smart_event") == 0)
			dimm->flags.f_smart = 1;
		else if (strcmp(start, "restore_fail") == 0)
			dimm->flags.f_restore = 1;
		else if (strcmp(start, "map_fail") == 0)
			dimm->flags.f_map = 1;
		else if (strcmp(start, "smart_notify") == 0)
			dimm->flags.f_notify = 1;
		start = end + 1;
	}
	if (end != start)
		dbg(ndctl_dimm_get_ctx(dimm), "%s: %s\n",
				ndctl_dimm_get_devname(dimm), flags);
}

static void parse_papr_flags(struct ndctl_dimm *dimm, char *flags)
{
	struct ndctl_ctx *ctx = ndctl_dimm_get_ctx(dimm);
	char *start, *end;

	start = flags;
	while ((end = strchr(start, ' '))) {
		*end = '\0';
		if (strcmp(start, "not_armed") == 0)
			dimm->flags.f_arm = 1;
		else if (strcmp(start, "flush_fail") == 0)
			dimm->flags.f_flush = 1;
		else if (strcmp(start, "restore_fail") == 0)
			dimm->flags.f_restore = 1;
		else if (strcmp(start, "smart_notify") == 0)
			dimm->flags.f_smart = 1;
		start = end + 1;
	}
	if (end != start)
		dbg(ctx, "%s: Flags:%s\n", ndctl_dimm_get_devname(dimm), flags);
}

static void parse_dimm_flags(struct ndctl_dimm *dimm, char *flags)
{
	char *start, *end;

	dimm->locked = 0;
	dimm->aliased = 0;
	start = flags;
	while ((end = strchr(start, ' '))) {
		*end = '\0';
		if (strcmp(start, "lock") == 0)
			dimm->locked = 1;
		else if (strcmp(start, "alias") == 0)
			dimm->aliased = 1;
		start = end + 1;
	}
	if (end != start)
		dbg(ndctl_dimm_get_ctx(dimm), "%s: %s\n",
				ndctl_dimm_get_devname(dimm), flags);
}

static enum ndctl_fwa_state fwa_to_state(const char *fwa)
{
	if (strcmp(fwa, "idle") == 0)
		return NDCTL_FWA_IDLE;
	if (strcmp(fwa, "busy") == 0)
		return NDCTL_FWA_BUSY;
	if (strcmp(fwa, "armed") == 0)
		return NDCTL_FWA_ARMED;
	if (strcmp(fwa, "overflow") == 0)
		return NDCTL_FWA_ARM_OVERFLOW;
	return NDCTL_FWA_INVALID;
}

static enum ndctl_fwa_method fwa_method_to_method(const char *fwa_method)
{
	if (!fwa_method)
		return NDCTL_FWA_METHOD_RESET;

	if (strcmp(fwa_method, "quiesce") == 0)
		return NDCTL_FWA_METHOD_SUSPEND;
	if (strcmp(fwa_method, "live") == 0)
		return NDCTL_FWA_METHOD_LIVE;
	return NDCTL_FWA_METHOD_RESET;
}

static void *add_bus(void *parent, int id, const char *ctl_base)
{
	char buf[SYSFS_ATTR_SIZE];
	struct ndctl_ctx *ctx = parent;
	struct ndctl_bus *bus, *bus_dup;
	char *path = calloc(1, strlen(ctl_base) + 100);

	if (!path)
		return NULL;

	bus = calloc(1, sizeof(*bus));
	if (!bus)
		goto err_bus;
	list_head_init(&bus->dimms);
	list_head_init(&bus->regions);
	bus->ctx = ctx;
	bus->id = id;

	sprintf(path, "%s/dev", ctl_base);
	if (sysfs_read_attr(ctx, path, buf) < 0
			|| sscanf(buf, "%d:%d", &bus->major, &bus->minor) != 2)
		goto err_read;

	sprintf(path, "%s/device/commands", ctl_base);
	if (sysfs_read_attr(ctx, path, buf) < 0)
		goto err_read;
	bus->cmd_mask = parse_commands(buf, 0);

	sprintf(path, "%s/device/nfit/revision", ctl_base);
	if (sysfs_read_attr(ctx, path, buf) < 0) {
		bus->has_nfit = 0;
		bus->revision = -1;
	} else {
		bus->has_nfit = 1;
		bus->revision = strtoul(buf, NULL, 0);
	}

	sprintf(path, "%s/device/of_node/compatible", ctl_base);
	if (sysfs_read_attr(ctx, path, buf) < 0)
		bus->has_of_node = 0;
	else
		bus->has_of_node = 1;

	sprintf(path, "%s/device/nfit/dsm_mask", ctl_base);
	if (sysfs_read_attr(ctx, path, buf) < 0)
		bus->nfit_dsm_mask = 0;
	else
		bus->nfit_dsm_mask = strtoul(buf, NULL, 0);

	sprintf(path, "%s/device/provider", ctl_base);
	if (sysfs_read_attr(ctx, path, buf) < 0)
		goto err_read;

	bus->provider = strdup(buf);
	if (!bus->provider)
		goto err_read;

	sprintf(path, "%s/device/wait_probe", ctl_base);
	bus->wait_probe_path = strdup(path);
	if (!bus->wait_probe_path)
		goto err_read;

	sprintf(path, "%s/device/nfit/scrub", ctl_base);
	bus->scrub_path = strdup(path);
	if (!bus->scrub_path)
		goto err_read;

	sprintf(path, "%s/device/firmware/activate", ctl_base);
	if (sysfs_read_attr(ctx, path, buf) < 0)
		bus->fwa_state = NDCTL_FWA_INVALID;
	else
		bus->fwa_state = fwa_to_state(buf);

	sprintf(path, "%s/device/firmware/capability", ctl_base);
	if (sysfs_read_attr(ctx, path, buf) < 0)
		bus->fwa_method = fwa_method_to_method(NULL);
	else
		bus->fwa_method = fwa_method_to_method(buf);


	bus->bus_path = parent_dev_path("char", bus->major, bus->minor);
	if (!bus->bus_path)
		goto err_dev_path;

	bus->bus_buf = calloc(1, strlen(bus->bus_path) + 50);
	if (!bus->bus_buf)
		goto err_read;
	bus->buf_len = strlen(bus->bus_path) + 50;

	ndctl_bus_foreach(ctx, bus_dup)
		if (strcmp(ndctl_bus_get_provider(bus_dup),
					ndctl_bus_get_provider(bus)) == 0
				&& strcmp(ndctl_bus_get_devname(bus_dup),
					ndctl_bus_get_devname(bus)) == 0) {
			free_bus(bus, NULL);
			free(path);
			return bus_dup;
		}

	list_add(&ctx->busses, &bus->list);
	free(path);

	return bus;

 err_dev_path:
 err_read:
	free(bus->wait_probe_path);
	free(bus->scrub_path);
	free(bus->provider);
	free(bus->bus_path);
	free(bus->bus_buf);
	free(bus);
 err_bus:
	free(path);

	return NULL;
}

static void busses_init(struct ndctl_ctx *ctx)
{
	if (ctx->busses_init)
		return;
	ctx->busses_init = 1;

	device_parse(ctx, NULL, "/sys/class/nd", "ndctl", ctx, add_bus);
}

NDCTL_EXPORT void ndctl_invalidate(struct ndctl_ctx *ctx)
{
	ctx->busses_init = 0;
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

NDCTL_EXPORT int ndctl_bus_has_of_node(struct ndctl_bus *bus)
{
	return bus->has_of_node;
}

NDCTL_EXPORT int ndctl_bus_is_papr_scm(struct ndctl_bus *bus)
{
	char buf[SYSFS_ATTR_SIZE];

	snprintf(bus->bus_buf, bus->buf_len,
		 "%s/of_node/compatible", bus->bus_path);
	if (sysfs_read_attr(bus->ctx, bus->bus_buf, buf) < 0)
		return 0;

	return (strcmp(buf, "ibm,pmemory") == 0);
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

NDCTL_EXPORT enum ndctl_persistence_domain
ndctl_bus_get_persistence_domain(struct ndctl_bus *bus)
{
	struct ndctl_region *region;
	enum ndctl_persistence_domain pd = -1;

	/* iterate through region to get the region persistence domain */
	ndctl_region_foreach(bus, region) {
		/* we are looking for the least persistence domain */
		if (pd < region->persistence_domain)
			pd = region->persistence_domain;
	}

	return pd < 0 ? PERSISTENCE_UNKNOWN : pd;
}

NDCTL_EXPORT struct ndctl_btt *ndctl_region_get_btt_seed(struct ndctl_region *region)
{
	struct ndctl_ctx *ctx = ndctl_region_get_ctx(region);
	char *path = region->region_buf;
	int len = region->buf_len;
	struct ndctl_btt *btt;
	char buf[SYSFS_ATTR_SIZE];

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

NDCTL_EXPORT struct ndctl_pfn *ndctl_region_get_pfn_seed(struct ndctl_region *region)
{
	struct ndctl_ctx *ctx = ndctl_region_get_ctx(region);
	char *path = region->region_buf;
	int len = region->buf_len;
	struct ndctl_pfn *pfn;
	char buf[SYSFS_ATTR_SIZE];

	if (snprintf(path, len, "%s/pfn_seed", region->region_path) >= len) {
		err(ctx, "%s: buffer too small!\n",
				ndctl_region_get_devname(region));
		return NULL;
	}

	if (sysfs_read_attr(ctx, path, buf) < 0)
		return NULL;

	ndctl_pfn_foreach(region, pfn)
		if (strcmp(buf, ndctl_pfn_get_devname(pfn)) == 0)
			return pfn;
	return NULL;
}

NDCTL_EXPORT struct ndctl_dax *ndctl_region_get_dax_seed(struct ndctl_region *region)
{
	struct ndctl_ctx *ctx = ndctl_region_get_ctx(region);
	char *path = region->region_buf;
	int len = region->buf_len;
	struct ndctl_dax *dax;
	char buf[SYSFS_ATTR_SIZE];

	if (snprintf(path, len, "%s/dax_seed", region->region_path) >= len) {
		err(ctx, "%s: buffer too small!\n",
				ndctl_region_get_devname(region));
		return NULL;
	}

	if (sysfs_read_attr(ctx, path, buf) < 0)
		return NULL;

	ndctl_dax_foreach(region, dax)
		if (strcmp(buf, ndctl_dax_get_devname(dax)) == 0)
			return dax;
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
	int len = region->buf_len, rc;

	if (snprintf(path, len, "%s/read_only", region->region_path) >= len) {
		err(ctx, "%s: buffer too small!\n",
				ndctl_region_get_devname(region));
		return -ENXIO;
	}

	ro = !!ro;
	rc = sysfs_write_attr(ctx, path, ro ? "1\n" : "0\n");
	if (rc < 0)
		return rc;

	region->ro = ro;
	return ro;
}

NDCTL_EXPORT unsigned long ndctl_region_get_align(struct ndctl_region *region)
{
	return region->align;
}

/**
 * ndctl_region_set_align() - Align namespace dpa allocations to @align
 * @region: region to modify
 * @align: alignment that must be a power-of-2 and >= the platform minimum
 *
 * WARNING: setting the region align value to anything less than the
 * kernel default (16M) may result in namespaces that are not cross-arch
 * (PowerPC) compatible. The minimum alignment for raw mode namespaces
 * is PAGE_SIZE.
 */
NDCTL_EXPORT int ndctl_region_set_align(struct ndctl_region *region,
		unsigned long align)
{
	struct ndctl_ctx *ctx = ndctl_region_get_ctx(region);
	char *path = region->region_buf;
	int len = region->buf_len, rc;
	char buf[SYSFS_ATTR_SIZE];

	if (snprintf(path, len, "%s/align", region->region_path) >= len) {
		err(ctx, "%s: buffer too small!\n",
				ndctl_region_get_devname(region));
		return -ENXIO;
	}

	sprintf(buf, "%#lx\n", align);
	rc = sysfs_write_attr(ctx, path, buf);
	if (rc < 0)
		return rc;

	region->align = align;
	return 0;
}

NDCTL_EXPORT unsigned long long ndctl_region_get_resource(struct ndctl_region *region)
{
	struct ndctl_ctx *ctx = ndctl_region_get_ctx(region);
	char *path = region->region_buf;
	int len = region->buf_len;
	char buf[SYSFS_ATTR_SIZE];
	int rc;

	if (snprintf(path, len, "%s/resource", region->region_path) >= len) {
		err(ctx, "%s: buffer too small!\n",
				ndctl_region_get_devname(region));
		errno = ENOMEM;
		return ULLONG_MAX;
	}

	rc = sysfs_read_attr(ctx, path, buf);
	if (rc < 0) {
		errno = -rc;
		return ULLONG_MAX;
	}

	return strtoull(buf, NULL, 0);
}

NDCTL_EXPORT int ndctl_region_deep_flush(struct ndctl_region *region)
{
	int rc = pwrite(region->flush_fd, "1\n", 1, 0);

	return (rc == -1) ? -errno : 0;
}


NDCTL_EXPORT const char *ndctl_bus_get_cmd_name(struct ndctl_bus *bus, int cmd)
{
	return nvdimm_bus_cmd_name(cmd);
}

NDCTL_EXPORT int ndctl_bus_is_cmd_supported(struct ndctl_bus *bus,
		int cmd)
{
	return !!(bus->cmd_mask & (1ULL << cmd));
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

NDCTL_EXPORT struct ndctl_ctx *ndctl_bus_get_ctx(struct ndctl_bus *bus)
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

static int __ndctl_bus_get_scrub_state(struct ndctl_bus *bus,
		unsigned int *scrub_count, bool *active)
{
	struct ndctl_ctx *ctx = ndctl_bus_get_ctx(bus);
	char buf[SYSFS_ATTR_SIZE];
	char in_progress = '\0';
	int rc;

	rc = sysfs_read_attr(ctx, bus->scrub_path, buf);
	if (rc < 0)
		return -EOPNOTSUPP;

	rc = sscanf(buf, "%u%c", scrub_count, &in_progress);
	if (rc < 0)
		return -ENXIO;

	switch (rc) {
	case 1:
		*active = false;
		return 0;
	case 2:
		if (in_progress == '+') {
			*active = true;
			return 0;
		}
		/* fall through */
	default:
		/* unable to read scrub count */
		return -ENXIO;
	}
}

NDCTL_EXPORT int ndctl_bus_start_scrub(struct ndctl_bus *bus)
{
	struct ndctl_ctx *ctx = ndctl_bus_get_ctx(bus);
	int rc;

	rc = sysfs_write_attr(ctx, bus->scrub_path, "1\n");
	if (rc == -EBUSY)
		return rc;
	else if (rc < 0)
		return -EOPNOTSUPP;
	return 0;
}

NDCTL_EXPORT int ndctl_bus_get_scrub_state(struct ndctl_bus *bus)
{
	unsigned int scrub_count = 0;
	bool active = false;
	int rc;

	rc = __ndctl_bus_get_scrub_state(bus, &scrub_count, &active);
	if (rc < 0)
		return rc;
	return active;
}

NDCTL_EXPORT unsigned int ndctl_bus_get_scrub_count(struct ndctl_bus *bus)
{
	unsigned int scrub_count = 0;
	bool active = false;
	int rc;

	rc = __ndctl_bus_get_scrub_state(bus, &scrub_count, &active);
	if (rc) {
		errno = -rc;
		return UINT_MAX;
	}
	return scrub_count;
}

/**
 * ndctl_bus_poll_scrub_completion - wait for a scrub to complete
 * @bus: bus for which to check whether a scrub is in progress
 * @poll_interval: nr seconds between wake up and re-read the status
 * @timeout: total number of seconds to wait
 *
 * Upon return this bus has completed any in-progress scrubs if @timeout
 * is 0 otherwise -ETIMEDOUT when @timeout seconds have expired. This
 * is different from ndctl_cmd_ars_in_progress in that the latter checks
 * the output of an ars_status command to see if the in-progress flag is
 * set, i.e. provides the firmware's view of whether a scrub is in
 * progress. ndctl_bus_wait_for_scrub_completion() instead checks the
 * kernel's view of whether a scrub is in progress by looking at the
 * 'scrub' file in sysfs.
 *
 * The @poll_interval option changes the frequency at which the kernel
 * status is polled, but it requires a supporting kernel for that poll
 * interval to be reflected to the kernel's polling of the ARS
 * interface. Kernel's with poll interval support limit that polling to
 * root (CAP_SYS_RAWIO) processes.
 */
NDCTL_EXPORT int ndctl_bus_poll_scrub_completion(struct ndctl_bus *bus,
		unsigned int poll_interval, unsigned int timeout)
{
	struct ndctl_ctx *ctx = ndctl_bus_get_ctx(bus);
	const char *provider = ndctl_bus_get_provider(bus);
	char buf[SYSFS_ATTR_SIZE] = { 0 };
	unsigned int scrub_count;
	struct pollfd fds;
	char in_progress;
	int fd = 0, rc;

	fd = open(bus->scrub_path, O_RDONLY|O_CLOEXEC);
	if (fd < 0)
		return -errno;
	memset(&fds, 0, sizeof(fds));
	fds.fd = fd;
	for (;;) {
		rc = sysfs_read_attr(ctx, bus->scrub_path, buf);
		if (rc < 0) {
			rc = -EOPNOTSUPP;
			break;
		}

		rc = sscanf(buf, "%u%c", &scrub_count, &in_progress);
		if (rc < 0) {
			rc = -EOPNOTSUPP;
			break;
		}

		if (rc == 1) {
			/* scrub complete, break successfully */
			rc = 0;
			break;
		} else if (rc == 2 && in_progress == '+') {
			long tmo;

			if (!timeout)
				tmo = poll_interval;
			else if (!poll_interval)
				tmo = timeout;
			else
				tmo = min(poll_interval, timeout);

			tmo *= 1000;
			if (tmo == 0)
				tmo = -1;

			/* scrub in progress, wait */
			rc = poll(&fds, 1, tmo);
			dbg(ctx, "%s: poll wake: rc: %d status: \'%s\'\n",
					provider, rc, buf);
			if (rc > 0)
				fds.revents = 0;
			if (pread(fd, buf, 1, 0) == -1) {
				rc = -errno;
				break;
			}

			if (rc < 0) {
				rc = -errno;
				dbg(ctx, "%s: poll error: %s\n", provider,
						strerror(errno));
				break;
			} else if (rc == 0) {
				dbg(ctx, "%s: poll timeout: interval: %d timeout: %d\n",
						provider, poll_interval, timeout);
				if (!timeout)
					continue;

				if (!poll_interval || poll_interval > timeout) {
					rc = -ETIMEDOUT;
					break;
				}

				if (timeout > poll_interval)
					timeout -= poll_interval;
				else if (timeout == poll_interval) {
					timeout = 1;
					poll_interval = 0;
				}
			}
		}
	}

	if (rc == 0)
		dbg(ctx, "%s: scrub complete, status: \'%s\'\n", provider, buf);
	else
		dbg(ctx, "%s: error waiting for scrub completion: %s\n",
			provider, strerror(-rc));
	if (fd)
		close (fd);
	return rc;
}

NDCTL_EXPORT int ndctl_bus_wait_for_scrub_completion(struct ndctl_bus *bus)
{
	return ndctl_bus_poll_scrub_completion(bus, 0, 0);
}

NDCTL_EXPORT enum ndctl_fwa_state ndctl_bus_get_fw_activate_state(
		struct ndctl_bus *bus)
{
	struct ndctl_ctx *ctx = ndctl_bus_get_ctx(bus);
	char *path = bus->bus_buf;
	char buf[SYSFS_ATTR_SIZE];
	int len = bus->buf_len;

	if (bus->fwa_state == NDCTL_FWA_INVALID)
		return NDCTL_FWA_INVALID;

	if (snprintf(path, len, "%s/firmware/activate", bus->bus_path) >= len) {
		err(ctx, "%s: buffer too small!\n",
				ndctl_bus_get_devname(bus));
		return NDCTL_FWA_INVALID;
	}

	if (sysfs_read_attr(ctx, path, buf) < 0)
		return NDCTL_FWA_INVALID;

	bus->fwa_state = fwa_to_state(buf);

	return bus->fwa_state;
}

NDCTL_EXPORT enum ndctl_fwa_method ndctl_bus_get_fw_activate_method(struct ndctl_bus *bus)
{
	return bus->fwa_method;
}

NDCTL_EXPORT int ndctl_bus_activate_firmware(struct ndctl_bus *bus, enum ndctl_fwa_method method)
{
	struct ndctl_ctx *ctx = ndctl_bus_get_ctx(bus);
	char *path = bus->bus_buf;
	char buf[SYSFS_ATTR_SIZE];
	int len = bus->buf_len;

	if (snprintf(path, len, "%s/firmware/activate", bus->bus_path) >= len) {
		err(ctx, "%s: buffer too small!\n", ndctl_bus_get_devname(bus));
		return -ENOMEM;
	}

	switch (method) {
	case NDCTL_FWA_METHOD_LIVE:
	case NDCTL_FWA_METHOD_SUSPEND:
		break;
	default:
		err(ctx, "%s: method: %d invalid\n", ndctl_bus_get_devname(bus), method);
		return -EINVAL;
	}

	sprintf(buf, "%s\n", method == NDCTL_FWA_METHOD_LIVE ? "live" : "quiesce");

	return sysfs_write_attr(ctx, path, buf);
}

static int write_fw_activate_noidle(struct ndctl_bus *bus, int arg)
{
	struct ndctl_ctx *ctx = ndctl_bus_get_ctx(bus);
	char *path = bus->bus_buf;
	char buf[SYSFS_ATTR_SIZE];
	int len = bus->buf_len;

	if (!ndctl_bus_has_nfit(bus))
		return -EOPNOTSUPP;

	if (snprintf(path, len, "%s/nfit/firmware_activate_noidle", bus->bus_path) >= len) {
		err(ctx, "%s: buffer too small!\n", ndctl_bus_get_devname(bus));
		return -ENOMEM;
	}

	sprintf(buf, "%d\n", arg);

	return sysfs_write_attr(ctx, path, buf);
}

NDCTL_EXPORT int ndctl_bus_set_fw_activate_noidle(struct ndctl_bus *bus)
{
	return write_fw_activate_noidle(bus, 1);
}

NDCTL_EXPORT int ndctl_bus_clear_fw_activate_noidle(struct ndctl_bus *bus)
{
	return write_fw_activate_noidle(bus, 0);
}

static int write_fw_activate_nosuspend(struct ndctl_bus *bus, int arg)
{
	struct ndctl_ctx *ctx = ndctl_bus_get_ctx(bus);
	char *path = bus->bus_buf;
	char buf[SYSFS_ATTR_SIZE];
	int len = bus->buf_len;

	if (!ndctl_bus_has_nfit(bus))
		return -EOPNOTSUPP;

	if (snprintf(path, len, "%s/nfit/firmware_activate_nosuspend", bus->bus_path) >= len) {
		err(ctx, "%s: buffer too small!\n", ndctl_bus_get_devname(bus));
		return -ENOMEM;
	}

	sprintf(buf, "%d\n", arg);

	return sysfs_write_attr(ctx, path, buf);
}

NDCTL_EXPORT int ndctl_bus_set_fw_activate_nosuspend(struct ndctl_bus *bus)
{
	return write_fw_activate_nosuspend(bus, 1);
}

NDCTL_EXPORT int ndctl_bus_clear_fw_activate_nosuspend(struct ndctl_bus *bus)
{
	return write_fw_activate_nosuspend(bus, 0);
}

static enum ndctl_fwa_result fwa_result_to_result(const char *result)
{
	if (strcmp(result, "none") == 0)
		return NDCTL_FWA_RESULT_NONE;
	if (strcmp(result, "success") == 0)
		return NDCTL_FWA_RESULT_SUCCESS;
	if (strcmp(result, "fail") == 0)
		return NDCTL_FWA_RESULT_FAIL;
	if (strcmp(result, "not_staged") == 0)
		return NDCTL_FWA_RESULT_NOTSTAGED;
	if (strcmp(result, "need_reset") == 0)
		return NDCTL_FWA_RESULT_NEEDRESET;
	return NDCTL_FWA_RESULT_INVALID;
}

static int ndctl_bind(struct ndctl_ctx *ctx, struct kmod_module *module,
		const char *devname);
static int ndctl_unbind(struct ndctl_ctx *ctx, const char *devpath);
static struct kmod_module *to_module(struct ndctl_ctx *ctx, const char *alias);

static int add_papr_dimm(struct ndctl_dimm *dimm, const char *dimm_base)
{
	int rc = -ENODEV;
	char buf[SYSFS_ATTR_SIZE];
	struct ndctl_ctx *ctx = dimm->bus->ctx;
	char *path = calloc(1, strlen(dimm_base) + 100);
	const char * const devname = ndctl_dimm_get_devname(dimm);

	dbg(ctx, "%s: Probing of_pmem dimm at %s\n", devname, dimm_base);

	if (!path)
		return -ENOMEM;

	/* construct path to the papr compatible dimm flags file */
	sprintf(path, "%s/papr/flags", dimm_base);

	if (ndctl_bus_is_papr_scm(dimm->bus) &&
	    sysfs_read_attr(ctx, path, buf) == 0) {

		dbg(ctx, "%s: Adding papr-scm dimm flags:\"%s\"\n", devname, buf);
		dimm->cmd_family = NVDIMM_FAMILY_PAPR;

		/* Parse dimm flags */
		parse_papr_flags(dimm, buf);

		/* Allocate monitor mode fd */
		dimm->health_eventfd = open(path, O_RDONLY|O_CLOEXEC);
		rc = 0;
	}

	free(path);
	return rc;
}

static int add_nfit_dimm(struct ndctl_dimm *dimm, const char *dimm_base)
{
	int i, rc = -1;
	char buf[SYSFS_ATTR_SIZE];
	struct ndctl_ctx *ctx = dimm->bus->ctx;
	char *path = calloc(1, strlen(dimm_base) + 100);

	if (!path)
		return -ENOMEM;

	/*
	 * 'unique_id' may not be available on older kernels, so don't
	 * fail if the read fails.
	 */
	sprintf(path, "%s/nfit/id", dimm_base);
	if (sysfs_read_attr(ctx, path, buf) == 0) {
		unsigned int b[9];

		dimm->unique_id = strdup(buf);
		if (!dimm->unique_id)
			goto err_read;
		if (sscanf(dimm->unique_id, "%02x%02x-%02x-%02x%02x-%02x%02x%02x%02x",
					&b[0], &b[1], &b[2], &b[3], &b[4],
					&b[5], &b[6], &b[7], &b[8]) == 9) {
			dimm->manufacturing_date = b[3] << 8 | b[4];
			dimm->manufacturing_location = b[2];
		}
	}

	sprintf(path, "%s/nfit/handle", dimm_base);
	if (sysfs_read_attr(ctx, path, buf) < 0)
		goto err_read;
	dimm->handle = strtoul(buf, NULL, 0);

	sprintf(path, "%s/nfit/phys_id", dimm_base);
	if (sysfs_read_attr(ctx, path, buf) < 0)
		goto err_read;
	dimm->phys_id = strtoul(buf, NULL, 0);

	sprintf(path, "%s/nfit/serial", dimm_base);
	if (sysfs_read_attr(ctx, path, buf) == 0)
		dimm->serial = strtoul(buf, NULL, 0);

	sprintf(path, "%s/nfit/vendor", dimm_base);
	if (sysfs_read_attr(ctx, path, buf) == 0)
		dimm->vendor_id = strtoul(buf, NULL, 0);

	sprintf(path, "%s/nfit/device", dimm_base);
	if (sysfs_read_attr(ctx, path, buf) == 0)
		dimm->device_id = strtoul(buf, NULL, 0);

	sprintf(path, "%s/nfit/rev_id", dimm_base);
	if (sysfs_read_attr(ctx, path, buf) == 0)
		dimm->revision_id = strtoul(buf, NULL, 0);

	sprintf(path, "%s/nfit/dirty_shutdown", dimm_base);
	if (sysfs_read_attr(ctx, path, buf) == 0)
		dimm->dirty_shutdown = strtoll(buf, NULL, 0);

	sprintf(path, "%s/nfit/subsystem_vendor", dimm_base);
	if (sysfs_read_attr(ctx, path, buf) == 0)
		dimm->subsystem_vendor_id = strtoul(buf, NULL, 0);

	sprintf(path, "%s/nfit/subsystem_device", dimm_base);
	if (sysfs_read_attr(ctx, path, buf) == 0)
		dimm->subsystem_device_id = strtoul(buf, NULL, 0);

	sprintf(path, "%s/nfit/subsystem_rev_id", dimm_base);
	if (sysfs_read_attr(ctx, path, buf) == 0)
		dimm->subsystem_revision_id = strtoul(buf, NULL, 0);

	sprintf(path, "%s/nfit/family", dimm_base);
	if (sysfs_read_attr(ctx, path, buf) == 0)
		dimm->cmd_family = strtoul(buf, NULL, 0);

	sprintf(path, "%s/nfit/dsm_mask", dimm_base);
	if (sysfs_read_attr(ctx, path, buf) == 0)
		dimm->nfit_dsm_mask = strtoul(buf, NULL, 0);

	sprintf(path, "%s/nfit/format", dimm_base);
	if (sysfs_read_attr(ctx, path, buf) == 0)
		dimm->format[0] = strtoul(buf, NULL, 0);
	for (i = 1; i < dimm->formats; i++) {
		sprintf(path, "%s/nfit/format%d", dimm_base, i);
		if (sysfs_read_attr(ctx, path, buf) == 0)
			dimm->format[i] = strtoul(buf, NULL, 0);
	}

	sprintf(path, "%s/nfit/flags", dimm_base);
	if (sysfs_read_attr(ctx, path, buf) == 0)
		parse_nfit_mem_flags(dimm, buf);

	dimm->health_eventfd = open(path, O_RDONLY|O_CLOEXEC);
	rc = 0;
 err_read:

	free(path);
	return rc;
}

static void *add_dimm(void *parent, int id, const char *dimm_base)
{
	int formats, i, rc = -ENODEV;
	struct ndctl_dimm *dimm = NULL;
	char buf[SYSFS_ATTR_SIZE];
	struct ndctl_bus *bus = parent;
	struct ndctl_ctx *ctx = bus->ctx;
	char *path = calloc(1, strlen(dimm_base) + 100);

	if (!path)
		return NULL;

	sprintf(path, "%s/nfit/formats", dimm_base);
	if (sysfs_read_attr(ctx, path, buf) < 0)
		formats = 1;
	else
		formats = clamp(strtoul(buf, NULL, 0), 1UL, 2UL);

	dimm = calloc(1, sizeof(*dimm) + sizeof(int) * formats);
	if (!dimm)
		goto err_dimm;
	dimm->bus = bus;
	dimm->id = id;

	sprintf(path, "%s/dev", dimm_base);
	if (sysfs_read_attr(ctx, path, buf) < 0)
		goto err_read;
	if (sscanf(buf, "%d:%d", &dimm->major, &dimm->minor) != 2)
		goto err_read;

	sprintf(path, "%s/commands", dimm_base);
	if (sysfs_read_attr(ctx, path, buf) < 0)
		goto err_read;
	dimm->cmd_mask = parse_commands(buf, 1);

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
	dimm->serial = -1;
	dimm->vendor_id = -1;
	dimm->device_id = -1;
	dimm->revision_id = -1;
	dimm->health_eventfd = -1;
	dimm->dirty_shutdown = -ENOENT;
	dimm->subsystem_vendor_id = -1;
	dimm->subsystem_device_id = -1;
	dimm->subsystem_revision_id = -1;
	dimm->manufacturing_date = -1;
	dimm->manufacturing_location = -1;
	dimm->cmd_family = -1;
	dimm->nfit_dsm_mask = ULONG_MAX;
	for (i = 0; i < formats; i++)
		dimm->format[i] = -1;

	sprintf(path, "%s/flags", dimm_base);
	if (sysfs_read_attr(ctx, path, buf) < 0) {
		dimm->locked = -1;
		dimm->aliased = -1;
	} else
		parse_dimm_flags(dimm, buf);

	sprintf(path, "%s/firmware/activate", dimm_base);
	if (sysfs_read_attr(ctx, path, buf) < 0)
		dimm->fwa_state = NDCTL_FWA_INVALID;
	else
		dimm->fwa_state = fwa_to_state(buf);

	sprintf(path, "%s/firmware/result", dimm_base);
	if (sysfs_read_attr(ctx, path, buf) < 0)
		dimm->fwa_result = NDCTL_FWA_RESULT_INVALID;
	else
		dimm->fwa_result = fwa_result_to_result(buf);

	/* Check if the given dimm supports nfit */
	if (ndctl_bus_has_nfit(bus)) {
		dimm->formats = formats;
		rc = add_nfit_dimm(dimm, dimm_base);
	} else if (ndctl_bus_has_of_node(bus)) {
		rc = add_papr_dimm(dimm, dimm_base);
	}

	if (rc == -ENODEV) {
		/* Unprobed dimm with no family */
		rc = 0;
		goto out;
	}

	/* Assign dimm-ops based on command family */
	if (dimm->cmd_family == NVDIMM_FAMILY_INTEL)
		dimm->ops = intel_dimm_ops;
	if (dimm->cmd_family == NVDIMM_FAMILY_HPE1)
		dimm->ops = hpe1_dimm_ops;
	if (dimm->cmd_family == NVDIMM_FAMILY_MSFT)
		dimm->ops = msft_dimm_ops;
	if (dimm->cmd_family == NVDIMM_FAMILY_HYPERV)
		dimm->ops = hyperv_dimm_ops;
	if (dimm->cmd_family == NVDIMM_FAMILY_PAPR)
		dimm->ops = papr_dimm_ops;

 out:
	if (rc) {
		err(ctx, "%s: probe failed: %s\n", ndctl_dimm_get_devname(dimm),
		    strerror(-rc));
		goto err_read;
	}

	list_add(&bus->dimms, &dimm->list);
	free(path);

	return dimm;

 err_read:
	free_dimm(dimm);
 err_dimm:
	free(path);
	return NULL;
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

NDCTL_EXPORT long long ndctl_dimm_get_dirty_shutdown(struct ndctl_dimm *dimm)
{
	return dimm->dirty_shutdown;
}

NDCTL_EXPORT unsigned short ndctl_dimm_get_subsystem_vendor(
		struct ndctl_dimm *dimm)
{
	return dimm->subsystem_vendor_id;
}

NDCTL_EXPORT unsigned short ndctl_dimm_get_subsystem_device(
		struct ndctl_dimm *dimm)
{
	return dimm->subsystem_device_id;
}

NDCTL_EXPORT unsigned short ndctl_dimm_get_subsystem_revision(
		struct ndctl_dimm *dimm)
{
	return dimm->subsystem_revision_id;
}

NDCTL_EXPORT unsigned short ndctl_dimm_get_manufacturing_date(
		struct ndctl_dimm *dimm)
{
	return dimm->manufacturing_date;
}

NDCTL_EXPORT unsigned char ndctl_dimm_get_manufacturing_location(
		struct ndctl_dimm *dimm)
{
	return dimm->manufacturing_location;
}

NDCTL_EXPORT unsigned short ndctl_dimm_get_format(struct ndctl_dimm *dimm)
{
	return dimm->format[0];
}

NDCTL_EXPORT int ndctl_dimm_get_formats(struct ndctl_dimm *dimm)
{
	return dimm->formats;
}

NDCTL_EXPORT int ndctl_dimm_get_formatN(struct ndctl_dimm *dimm, int i)
{
	if (i < dimm->formats && i >= 0)
		return dimm->format[i];
	return -EINVAL;
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

NDCTL_EXPORT const char *ndctl_dimm_get_unique_id(struct ndctl_dimm *dimm)
{
	return dimm->unique_id;
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
	union dimm_flags flags = dimm->flags;

	flags.f_notify = 0;
	return flags.flags != 0;
}

NDCTL_EXPORT int ndctl_dimm_locked(struct ndctl_dimm *dimm)
{
	return dimm->locked;
}

NDCTL_EXPORT int ndctl_dimm_aliased(struct ndctl_dimm *dimm)
{
	return dimm->aliased;
}

NDCTL_EXPORT int ndctl_dimm_has_notifications(struct ndctl_dimm *dimm)
{
	return dimm->flags.f_notify;
}

NDCTL_EXPORT int ndctl_dimm_failed_save(struct ndctl_dimm *dimm)
{
	return dimm->flags.f_save;
}

NDCTL_EXPORT int ndctl_dimm_failed_arm(struct ndctl_dimm *dimm)
{
	return dimm->flags.f_arm;
}

NDCTL_EXPORT int ndctl_dimm_failed_restore(struct ndctl_dimm *dimm)
{
	return dimm->flags.f_restore;
}

NDCTL_EXPORT int ndctl_dimm_smart_pending(struct ndctl_dimm *dimm)
{
	return dimm->flags.f_smart;
}

NDCTL_EXPORT int ndctl_dimm_failed_flush(struct ndctl_dimm *dimm)
{
	return dimm->flags.f_flush;
}

NDCTL_EXPORT int ndctl_dimm_failed_map(struct ndctl_dimm *dimm)
{
	return dimm->flags.f_map;
}

NDCTL_EXPORT int ndctl_dimm_is_cmd_supported(struct ndctl_dimm *dimm,
		int cmd)
{
	struct ndctl_dimm_ops *ops = dimm->ops;

	if (ops && ops->cmd_is_supported)
		return ops->cmd_is_supported(dimm, cmd);

	return !!(dimm->cmd_mask & (1ULL << cmd));
}

NDCTL_EXPORT int ndctl_dimm_get_health_eventfd(struct ndctl_dimm *dimm)
{
	return dimm->health_eventfd;
}

NDCTL_EXPORT unsigned int ndctl_dimm_get_health(struct ndctl_dimm *dimm)
{
	struct ndctl_cmd *cmd = NULL;
	unsigned int health;
	struct ndctl_ctx *ctx = ndctl_dimm_get_ctx(dimm);
	const char *devname = ndctl_dimm_get_devname(dimm);
	int rc;

	cmd = ndctl_dimm_cmd_new_smart(dimm);
	if (!cmd) {
		err(ctx, "%s: no smart command support\n", devname);
		return UINT_MAX;
	}
	rc = ndctl_cmd_submit(cmd);
	if (rc) {
		err(ctx, "%s: smart command failed\n", devname);
		ndctl_cmd_unref(cmd);
		if (rc < 0)
			errno = -rc;
		return UINT_MAX;
	}

	health = ndctl_cmd_smart_get_health(cmd);
	ndctl_cmd_unref(cmd);
	return health;
}

NDCTL_EXPORT unsigned int ndctl_dimm_get_flags(struct ndctl_dimm *dimm)
{
	struct ndctl_cmd *cmd = NULL;
	unsigned int flags;
	struct ndctl_ctx *ctx = ndctl_dimm_get_ctx(dimm);
	const char *devname = ndctl_dimm_get_devname(dimm);
	int rc;

	cmd = ndctl_dimm_cmd_new_smart(dimm);
	if (!cmd) {
		dbg(ctx, "%s: no smart command support\n", devname);
		return UINT_MAX;
	}
	rc = ndctl_cmd_submit(cmd);
	if (rc) {
		dbg(ctx, "%s: smart command failed\n", devname);
		ndctl_cmd_unref(cmd);
		if (rc < 0)
			errno = -rc;
		return UINT_MAX;
	}

	flags = ndctl_cmd_smart_get_flags(cmd);
	ndctl_cmd_unref(cmd);
	return flags;
}

NDCTL_EXPORT int ndctl_dimm_is_flag_supported(struct ndctl_dimm *dimm,
		unsigned int flag)
{
	unsigned int flags = ndctl_dimm_get_flags(dimm);
	return (flags ==  UINT_MAX) ? 0 : !!(flags & flag);
}

NDCTL_EXPORT unsigned int ndctl_dimm_get_event_flags(struct ndctl_dimm *dimm)
{
	int rc;
	struct ndctl_cmd *cmd = NULL;
	unsigned int alarm_flags, event_flags = 0;
	struct ndctl_ctx *ctx = ndctl_dimm_get_ctx(dimm);
	const char *devname = ndctl_dimm_get_devname(dimm);

	cmd = ndctl_dimm_cmd_new_smart(dimm);
	if (!cmd) {
		err(ctx, "%s: no smart command support\n", devname);
		return UINT_MAX;
	}
	rc = ndctl_cmd_submit(cmd);
	if (rc) {
		err(ctx, "%s: smart command failed\n", devname);
		ndctl_cmd_unref(cmd);
		if (rc < 0)
			errno = -rc;
		return UINT_MAX;
	}

	alarm_flags = ndctl_cmd_smart_get_alarm_flags(cmd);
	if (alarm_flags & ND_SMART_SPARE_TRIP)
		event_flags |= ND_EVENT_SPARES_REMAINING;
	if (alarm_flags & ND_SMART_MTEMP_TRIP)
		event_flags |= ND_EVENT_MEDIA_TEMPERATURE;
	if (alarm_flags & ND_SMART_CTEMP_TRIP)
		event_flags |= ND_EVENT_CTRL_TEMPERATURE;
	if (ndctl_cmd_smart_get_shutdown_state(cmd))
		event_flags |= ND_EVENT_UNCLEAN_SHUTDOWN;

	ndctl_cmd_unref(cmd);
	return event_flags;
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

static int dimm_set_arm(struct ndctl_dimm *dimm, bool arm)
{
	struct ndctl_ctx *ctx = ndctl_dimm_get_ctx(dimm);
	char *path = dimm->dimm_buf;
	int len = dimm->buf_len;

	if (dimm->fwa_state == NDCTL_FWA_INVALID)
		return NDCTL_FWA_INVALID;

	if (snprintf(path, len, "%s/firmware/activate", dimm->dimm_path) >= len) {
		err(ctx, "%s: buffer too small!\n", ndctl_dimm_get_devname(dimm));
		return NDCTL_FWA_INVALID;
	}

	if (sysfs_write_attr(ctx, path, arm ? "arm" : "disarm") < 0)
		return NDCTL_FWA_INVALID;
	return NDCTL_FWA_ARMED;
}

NDCTL_EXPORT enum ndctl_fwa_state ndctl_dimm_fw_activate_disarm(
		struct ndctl_dimm *dimm)
{
	return dimm_set_arm(dimm, false);
}

NDCTL_EXPORT enum ndctl_fwa_state ndctl_dimm_fw_activate_arm(
		struct ndctl_dimm *dimm)
{
	return dimm_set_arm(dimm, true);
}

NDCTL_EXPORT enum ndctl_fwa_state ndctl_dimm_get_fw_activate_state(
		struct ndctl_dimm *dimm)
{
	struct ndctl_ctx *ctx = ndctl_dimm_get_ctx(dimm);
	char *path = dimm->dimm_buf;
	char buf[SYSFS_ATTR_SIZE];
	int len = dimm->buf_len;

	if (dimm->fwa_state == NDCTL_FWA_INVALID)
		return NDCTL_FWA_INVALID;

	if (snprintf(path, len, "%s/firmware/activate", dimm->dimm_path) >= len) {
		err(ctx, "%s: buffer too small!\n", ndctl_dimm_get_devname(dimm));
		return NDCTL_FWA_INVALID;
	}

	if (sysfs_read_attr(ctx, path, buf) < 0)
		return NDCTL_FWA_INVALID;

	dimm->fwa_state = fwa_to_state(buf);
	return dimm->fwa_state;
}

NDCTL_EXPORT enum ndctl_fwa_result ndctl_dimm_get_fw_activate_result(
		struct ndctl_dimm *dimm)
{
	struct ndctl_ctx *ctx = ndctl_dimm_get_ctx(dimm);
	char *path = dimm->dimm_buf;
	char buf[SYSFS_ATTR_SIZE];
	int len = dimm->buf_len;

	if (dimm->fwa_result == NDCTL_FWA_RESULT_INVALID)
		return NDCTL_FWA_RESULT_INVALID;

	if (snprintf(path, len, "%s/firmware/result", dimm->dimm_path) >= len) {
		err(ctx, "%s: buffer too small!\n", ndctl_dimm_get_devname(dimm));
		return NDCTL_FWA_RESULT_INVALID;
	}

	if (sysfs_read_attr(ctx, path, buf) < 0)
		return NDCTL_FWA_RESULT_INVALID;

	return fwa_result_to_result(buf);
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

/**
 * ndctl_bus_get_region_by_physical_address - get region by physical address
 * @bus: ndctl_bus instance
 * @address: (System) Physical Address
 *
 * If @bus and @address is valid, returns a region address, which
 * physical address belongs to.
 */
NDCTL_EXPORT struct ndctl_region *ndctl_bus_get_region_by_physical_address(
		struct ndctl_bus *bus, unsigned long long address)
{
	unsigned long long region_start, region_end;
	struct ndctl_region *region;

	ndctl_region_foreach(bus, region) {
		region_start = ndctl_region_get_resource(region);
		region_end = region_start + ndctl_region_get_size(region);
		if (region_start <= address && address < region_end)
			return region;
	}

	return NULL;
}

/**
 * ndctl_bus_get_dimm_by_physical_address - get ndctl_dimm pointer by physical address
 * @bus: ndctl_bus instance
 * @address: (System) Physical Address
 *
 * Returns address of ndctl_dimm on success.
 */
NDCTL_EXPORT struct ndctl_dimm *ndctl_bus_get_dimm_by_physical_address(
		struct ndctl_bus *bus, unsigned long long address)
{
	unsigned int handle;
	unsigned long long dpa;
	struct ndctl_region *region;

	if (!bus)
		return NULL;

	region = ndctl_bus_get_region_by_physical_address(bus, address);
	if (!region)
		return NULL;

	if (ndctl_region_get_interleave_ways(region) == 1) {
		struct ndctl_mapping *mapping = ndctl_mapping_get_first(region);

		/* No need to ask firmware, there's only one dimm */
		if (!mapping)
			return NULL;
		return ndctl_mapping_get_dimm(mapping);
	}

	/*
	 * Since the region is interleaved, we need to ask firmware about it.
	 * If it supports Translate SPA, the dimm is returned.
	 */
	if (ndctl_bus_has_nfit(bus)) {
		int rc;

		rc = ndctl_bus_nfit_translate_spa(bus, address, &handle, &dpa);
		if (rc)
			return NULL;

		return ndctl_dimm_get_by_handle(bus, handle);
	}
	/* No way to get dimm info */
	return NULL;
}

static int region_set_type(struct ndctl_region *region, char *path)
{
	struct ndctl_ctx *ctx = ndctl_region_get_ctx(region);
	char buf[SYSFS_ATTR_SIZE];
	int rc;

	sprintf(path, "%s/nstype", region->region_path);
	rc = sysfs_read_attr(ctx, path, buf);
	if (rc < 0)
		return rc;
	region->nstype = strtoul(buf, NULL, 0);

	sprintf(path, "%s/set_cookie", region->region_path);
	if (region->nstype == ND_DEVICE_NAMESPACE_PMEM) {
		rc = sysfs_read_attr(ctx, path, buf);
		if (rc < 0)
			return rc;
		region->iset.cookie = strtoull(buf, NULL, 0);
		dbg(ctx, "%s: iset-%#.16llx added\n",
				ndctl_region_get_devname(region),
				region->iset.cookie);
	}

	return 0;
}

static enum ndctl_persistence_domain region_get_pd_type(char *name)
{
	if (strncmp("cpu_cache", name, 9) == 0)
		return PERSISTENCE_CPU_CACHE;
	else if (strncmp("memory_controller", name, 17) == 0)
		return PERSISTENCE_MEM_CTRL;
	else if (strncmp("none", name, 4) == 0)
		return PERSISTENCE_NONE;
	else
		return PERSISTENCE_UNKNOWN;
}

static void *add_region(void *parent, int id, const char *region_base)
{
	char buf[SYSFS_ATTR_SIZE];
	struct ndctl_region *region;
	struct ndctl_bus *bus = parent;
	struct ndctl_ctx *ctx = bus->ctx;
	char *path = calloc(1, strlen(region_base) + 100);
	int perm, rc;

	if (!path)
		return NULL;

	region = calloc(1, sizeof(*region));
	if (!region)
		goto err_region;
	list_head_init(&region->btts);
	list_head_init(&region->pfns);
	list_head_init(&region->daxs);
	list_head_init(&region->stale_btts);
	list_head_init(&region->stale_pfns);
	list_head_init(&region->stale_daxs);
	list_head_init(&region->mappings);
	list_head_init(&region->namespaces);
	list_head_init(&region->stale_namespaces);
	region->region_path = (char *) region_base;
	region->bus = bus;
	region->id = id;

	sprintf(path, "%s/size", region_base);
	if (sysfs_read_attr(ctx, path, buf) < 0)
		goto err_read;
	region->size = strtoull(buf, NULL, 0);

	sprintf(path, "%s/mappings", region_base);
	if (sysfs_read_attr(ctx, path, buf) < 0)
		goto err_read;
	region->num_mappings = strtoul(buf, NULL, 0);

	sprintf(path, "%s/nfit/range_index", region_base);
	if (ndctl_bus_has_nfit(bus)) {
		if (sysfs_read_attr(ctx, path, buf) < 0)
			goto err_read;
		region->range_index = strtoul(buf, NULL, 0);
	} else
		region->range_index = -1;

	sprintf(path, "%s/read_only", region_base);
	if (sysfs_read_attr(ctx, path, buf) < 0)
		goto err_read;
	region->ro = strtoul(buf, NULL, 0);

	sprintf(path, "%s/modalias", region_base);
	if (sysfs_read_attr(ctx, path, buf) < 0)
		goto err_read;
	region->module = to_module(ctx, buf);

	sprintf(path, "%s/numa_node", region_base);
	if ((rc = sysfs_read_attr(ctx, path, buf)) == 0)
		region->numa_node = strtol(buf, NULL, 0);
	else if (rc == -ENOENT)
		region->numa_node = NUMA_NO_ATTR;
	else
		region->numa_node = NUMA_NO_NODE;

	sprintf(path, "%s/target_node", region_base);
	if (sysfs_read_attr(ctx, path, buf) == 0)
		region->target_node = strtol(buf, NULL, 0);
	else
		region->target_node = -1;

	sprintf(path, "%s/align", region_base);
	if (sysfs_read_attr(ctx, path, buf) == 0)
		region->align = strtoul(buf, NULL, 0);
	else
		region->align = ULONG_MAX;

	if (region_set_type(region, path) < 0)
		goto err_read;

        region->region_buf = calloc(1, strlen(region_base) + 50);
        if (!region->region_buf)
                goto err_read;
        region->buf_len = strlen(region_base) + 50;

	region->region_path = strdup(region_base);
	if (!region->region_path)
		goto err_read;

	list_add(&bus->regions, &region->list);

	/* get the persistence domain attrib */
	sprintf(path, "%s/persistence_domain", region_base);
	if (sysfs_read_attr(ctx, path, buf) < 0)
		region->persistence_domain = PERSISTENCE_UNKNOWN;
	else
		region->persistence_domain = region_get_pd_type(buf);

	sprintf(path, "%s/deep_flush", region_base);
	region->flush_fd = open(path, O_RDWR | O_CLOEXEC);
	if (region->flush_fd == -1)
		goto out;

	if (pread(region->flush_fd, buf, 1, 0) == -1) {
		close(region->flush_fd);
		region->flush_fd = -1;
		goto out;
	}

	perm = strtol(buf, NULL, 0);
	if (perm == 0) {
		close(region->flush_fd);
		region->flush_fd = -1;
	}

 out:
	free(path);
	return region;

 err_read:
	free(region->region_buf);
	free(region);
 err_region:
	free(path);

	return NULL;
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
	return max(1U, ndctl_region_get_mappings(region));
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
	int rc, len = region->buf_len;
	char buf[SYSFS_ATTR_SIZE];

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
		errno = ENOMEM;
		return ULLONG_MAX;
	}

	rc = sysfs_read_attr(ctx, path, buf);
	if (rc < 0) {
		errno = -rc;
		return ULLONG_MAX;
	}

	return strtoull(buf, NULL, 0);
}

NDCTL_EXPORT unsigned long long ndctl_region_get_max_available_extent(
		struct ndctl_region *region)
{
	unsigned int nstype = ndctl_region_get_nstype(region);
	struct ndctl_ctx *ctx = ndctl_region_get_ctx(region);
	char *path = region->region_buf;
	int rc, len = region->buf_len;
	char buf[SYSFS_ATTR_SIZE];

	switch (nstype) {
	case ND_DEVICE_NAMESPACE_PMEM:
	case ND_DEVICE_NAMESPACE_BLK:
		break;
	default:
		return 0;
	}

	if (snprintf(path, len,
		     "%s/max_available_extent", region->region_path) >= len) {
		err(ctx, "%s: buffer too small!\n",
				ndctl_region_get_devname(region));
		errno = ENOMEM;
		return ULLONG_MAX;
	}

	/* fall back to legacy behavior if max extents is not exported */
	rc = sysfs_read_attr(ctx, path, buf);
	if (rc < 0) {
		dbg(ctx, "max extents attribute not exported on older kernels\n");
		errno = -rc;
		return ULLONG_MAX;
	}

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
	char buf[SYSFS_ATTR_SIZE];

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
	case ND_DEVICE_DAX_PMEM:       return "dax_pmem";
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

NDCTL_EXPORT int ndctl_region_has_numa(struct ndctl_region *region)
{
	return (region->numa_node != NUMA_NO_ATTR);
}

NDCTL_EXPORT int ndctl_region_get_numa_node(struct ndctl_region *region)
{
	return region->numa_node;
}

NDCTL_EXPORT int ndctl_region_get_target_node(struct ndctl_region *region)
{
	return region->target_node;
}

NDCTL_EXPORT struct badblock *ndctl_region_get_next_badblock(struct ndctl_region *region)
{
	return badblocks_iter_next(&region->bb_iter);
}

NDCTL_EXPORT struct badblock *ndctl_region_get_first_badblock(struct ndctl_region *region)
{
	return badblocks_iter_first(&region->bb_iter,
			ndctl_region_get_ctx(region), region->region_path);
}

NDCTL_EXPORT enum ndctl_persistence_domain
ndctl_region_get_persistence_domain(struct ndctl_region *region)
{
	return region->persistence_domain;
}

static struct nd_cmd_vendor_tail *to_vendor_tail(struct ndctl_cmd *cmd)
{
	struct nd_cmd_vendor_tail *tail = (struct nd_cmd_vendor_tail *)
		(cmd->cmd_buf + sizeof(struct nd_cmd_vendor_hdr)
		 + cmd->vendor->in_length);
	return tail;
}

static u32 cmd_get_firmware_status(struct ndctl_cmd *cmd)
{
	switch (cmd->type) {
	case ND_CMD_VENDOR:
		return to_vendor_tail(cmd)->status;
	case ND_CMD_GET_CONFIG_SIZE:
		return cmd->get_size->status;
	case ND_CMD_GET_CONFIG_DATA:
		return cmd->get_data->status;
	case ND_CMD_SET_CONFIG_DATA:
		return *(u32 *) (cmd->cmd_buf
				+ sizeof(struct nd_cmd_set_config_hdr)
				+ cmd->iter.max_xfer);
	}
	return -1U;
}

static void cmd_set_xfer(struct ndctl_cmd *cmd, u32 xfer)
{
	if (cmd->type == ND_CMD_GET_CONFIG_DATA)
		cmd->get_data->in_length = xfer;
	else
		cmd->set_data->in_length = xfer;
}

static u32 cmd_get_xfer(struct ndctl_cmd *cmd)
{
	if (cmd->type == ND_CMD_GET_CONFIG_DATA)
		return cmd->get_data->in_length;
	return cmd->set_data->in_length;
}

static void cmd_set_offset(struct ndctl_cmd *cmd, u32 offset)
{
	if (cmd->type == ND_CMD_GET_CONFIG_DATA)
		cmd->get_data->in_offset = offset;
	else
		cmd->set_data->in_offset = offset;
}

static u32 cmd_get_offset(struct ndctl_cmd *cmd)
{
	if (cmd->type == ND_CMD_GET_CONFIG_DATA)
		return cmd->get_data->in_offset;
	return cmd->set_data->in_offset;
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
	cmd->get_firmware_status = cmd_get_firmware_status;
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
	if (cmd->type != ND_CMD_VENDOR)
		return -EINVAL;
	/*
	 * When cmd->status is non-zero it contains either a negative
	 * error code, or the number of bytes that are available in the
	 * output buffer.
	 */
	if (cmd->status)
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
	cmd->get_firmware_status = cmd_get_firmware_status;

	return cmd;
}

NDCTL_EXPORT struct ndctl_cmd *ndctl_dimm_cmd_new_cfg_read(struct ndctl_cmd *cfg_size)
{
	struct ndctl_ctx *ctx = ndctl_bus_get_ctx(cmd_to_bus(cfg_size));
	struct ndctl_dimm *dimm = cfg_size->dimm;
	struct ndctl_cmd *cmd;
	size_t size;

	if (cfg_size->type != ND_CMD_GET_CONFIG_SIZE
			|| cfg_size->status != 0) {
		dbg(ctx, "expected sucessfully completed cfg_size command\n");
		return NULL;
	}

	if (!dimm || cfg_size->get_size->config_size == 0) {
		dbg(ctx, "invalid cfg_size\n");
		return NULL;
	}

	if (!ndctl_dimm_is_cmd_supported(dimm, ND_CMD_GET_CONFIG_DATA)) {
		dbg(ctx, "unsupported cmd\n");
		return NULL;
	}

	size = sizeof(*cmd) + sizeof(struct nd_cmd_get_config_data_hdr)
		+ cfg_size->get_size->max_xfer;
	cmd = calloc(1, size);
	if (!cmd)
		return NULL;

	cmd->dimm = dimm;
	cmd->refcount = 1;
	cmd->type = ND_CMD_GET_CONFIG_DATA;
	cmd->size = size;
	cmd->status = 1;
	cmd->get_data->in_offset = 0;
	cmd->get_data->in_length = cfg_size->get_size->max_xfer;
	cmd->get_firmware_status = cmd_get_firmware_status;
	cmd->get_xfer = cmd_get_xfer;
	cmd->set_xfer = cmd_set_xfer;
	cmd->get_offset = cmd_get_offset;
	cmd->set_offset = cmd_set_offset;
	cmd->iter.init_offset = 0;
	cmd->iter.max_xfer = cfg_size->get_size->max_xfer;
	cmd->iter.data = cmd->get_data->out_buf;
	cmd->iter.total_xfer = cfg_size->get_size->config_size;
	cmd->iter.total_buf = calloc(1, cmd->iter.total_xfer);
	cmd->iter.dir = READ;
	if (!cmd->iter.total_buf) {
		free(cmd);
		return NULL;
	}
	cmd->source = cfg_size;
	ndctl_cmd_ref(cfg_size);

	return cmd;
}

static void iter_set_extent(struct ndctl_cmd_iter *iter, unsigned int len,
		unsigned int offset)
{
	struct ndctl_cmd *cmd = container_of(iter, typeof(*cmd), iter);

	iter->init_offset = offset;
	cmd->set_offset(cmd, offset);
	cmd->set_xfer(cmd, min(cmd->get_xfer(cmd), len));
	iter->total_xfer = len;
}

NDCTL_EXPORT int ndctl_cmd_cfg_read_set_extent(struct ndctl_cmd *cfg_read,
		unsigned int len, unsigned int offset)
{
	struct ndctl_ctx *ctx = ndctl_bus_get_ctx(cmd_to_bus(cfg_read));
	struct ndctl_cmd *cfg_size = cfg_read->source;

	if (cfg_read->type != ND_CMD_GET_CONFIG_DATA
			|| cfg_read->status <= 0) {
		dbg(ctx, "expected unsubmitted cfg_read command\n");
		return -EINVAL;
	}

	if (offset + len > cfg_size->get_size->config_size) {
		dbg(ctx, "read %d from %d exceeds %d\n", len, offset,
				cfg_size->get_size->config_size);
		return -EINVAL;
	}

	iter_set_extent(&cfg_read->iter, len, offset);
	return 0;
}

NDCTL_EXPORT struct ndctl_cmd *ndctl_dimm_cmd_new_cfg_write(struct ndctl_cmd *cfg_read)
{
	struct ndctl_ctx *ctx = ndctl_bus_get_ctx(cmd_to_bus(cfg_read));
	struct ndctl_dimm *dimm = cfg_read->dimm;
	struct ndctl_cmd *cmd;
	size_t size;

	/* enforce rmw */
	if (cfg_read->type != ND_CMD_GET_CONFIG_DATA
		       || cfg_read->status != 0) {
		dbg(ctx, "expected sucessfully completed cfg_read command\n");
		return NULL;
	}

	if (!dimm || cfg_read->get_data->in_length == 0) {
		dbg(ctx, "invalid cfg_read\n");
		return NULL;
	}

	if (!ndctl_dimm_is_cmd_supported(dimm, ND_CMD_SET_CONFIG_DATA)) {
		dbg(ctx, "unsupported cmd\n");
		return NULL;
	}

	size = sizeof(*cmd) + sizeof(struct nd_cmd_set_config_hdr)
		+ cfg_read->iter.max_xfer + 4;
	cmd = calloc(1, size);
	if (!cmd)
		return NULL;

	cmd->dimm = dimm;
	ndctl_cmd_ref(cmd);
	cmd->type = ND_CMD_SET_CONFIG_DATA;
	cmd->size = size;
	cmd->status = 1;
	cmd->set_data->in_offset = cfg_read->iter.init_offset;
	cmd->set_data->in_length = cfg_read->iter.max_xfer;
	cmd->get_firmware_status = cmd_get_firmware_status;
	cmd->get_xfer = cmd_get_xfer;
	cmd->set_xfer = cmd_set_xfer;
	cmd->get_offset = cmd_get_offset;
	cmd->set_offset = cmd_set_offset;
	cmd->iter.init_offset = cfg_read->iter.init_offset;
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

static ssize_t iter_access(struct ndctl_cmd_iter *iter, unsigned int len,
		unsigned int offset)
{
	if (offset < iter->init_offset
			|| offset > iter->init_offset + iter->total_xfer
			|| len + offset < len)
		return -EINVAL;
	if (len + offset > iter->init_offset + iter->total_xfer)
		len = iter->total_xfer - offset;
	return len;
}

NDCTL_EXPORT ssize_t ndctl_cmd_cfg_read_get_data(struct ndctl_cmd *cfg_read,
		void *buf, unsigned int _len, unsigned int offset)
{
	struct ndctl_cmd_iter *iter;
	ssize_t len;

	if (cfg_read->type != ND_CMD_GET_CONFIG_DATA || cfg_read->status > 0)
		return -EINVAL;
	if (cfg_read->status < 0)
		return cfg_read->status;

	iter = &cfg_read->iter;
	len = iter_access(&cfg_read->iter, _len, offset);
	if (len >= 0)
		memcpy(buf, iter->total_buf + offset, len);
	return len;
}

NDCTL_EXPORT ssize_t ndctl_cmd_cfg_read_get_size(struct ndctl_cmd *cfg_read)
{
	if (cfg_read->type != ND_CMD_GET_CONFIG_DATA || cfg_read->status > 0)
		return -EINVAL;
	if (cfg_read->status < 0)
		return cfg_read->status;
	return cfg_read->iter.total_xfer;
}

NDCTL_EXPORT int ndctl_cmd_cfg_write_set_extent(struct ndctl_cmd *cfg_write,
		unsigned int len, unsigned int offset)
{
	struct ndctl_ctx *ctx = ndctl_bus_get_ctx(cmd_to_bus(cfg_write));
	struct ndctl_cmd *cfg_size, *cfg_read;

	if (cfg_write->type != ND_CMD_SET_CONFIG_DATA
			|| cfg_write->status <= 0) {
		dbg(ctx, "expected unsubmitted cfg_write command\n");
		return -EINVAL;
	}

	cfg_read = cfg_write->source;
	cfg_size = cfg_read->source;

	if (offset + len > cfg_size->get_size->config_size) {
		dbg(ctx, "write %d from %d exceeds %d\n", len, offset,
				cfg_size->get_size->config_size);
		return -EINVAL;
	}

	iter_set_extent(&cfg_write->iter, len, offset);
	return 0;
}

NDCTL_EXPORT ssize_t ndctl_cmd_cfg_write_set_data(struct ndctl_cmd *cfg_write,
		void *buf, unsigned int _len, unsigned int offset)
{
	ssize_t len;

	if (cfg_write->type != ND_CMD_SET_CONFIG_DATA || cfg_write->status < 1)
		return -EINVAL;
	if (cfg_write->status < 0)
		return cfg_write->status;
	len = iter_access(&cfg_write->iter, _len, offset);
	if (len >= 0)
		memcpy(cfg_write->iter.total_buf + offset, buf, len);
	return len;
}

NDCTL_EXPORT ssize_t ndctl_cmd_cfg_write_zero_data(struct ndctl_cmd *cfg_write)
{
	struct ndctl_cmd_iter *iter = &cfg_write->iter;

	if (cfg_write->type != ND_CMD_SET_CONFIG_DATA || cfg_write->status < 1)
		return -EINVAL;
	if (cfg_write->status < 0)
		return cfg_write->status;
	memset(iter->total_buf + iter->init_offset, 0, iter->total_xfer);
	return iter->total_xfer;
}

NDCTL_EXPORT void ndctl_cmd_unref(struct ndctl_cmd *cmd)
{
	if (!cmd)
		return;
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
		case ND_CMD_CLEAR_ERROR:     return ND_IOCTL_CLEAR_ERROR;
		case ND_CMD_CALL:            return ND_IOCTL_CALL;
		default:
					     return 0;
		};
	}

	switch (cmd) {
	case ND_CMD_DIMM_FLAGS:             return ND_IOCTL_DIMM_FLAGS;
	case ND_CMD_GET_CONFIG_SIZE:        return ND_IOCTL_GET_CONFIG_SIZE;
	case ND_CMD_GET_CONFIG_DATA:        return ND_IOCTL_GET_CONFIG_DATA;
	case ND_CMD_SET_CONFIG_DATA:        return ND_IOCTL_SET_CONFIG_DATA;
	case ND_CMD_VENDOR:                 return ND_IOCTL_VENDOR;
	case ND_CMD_CALL:                   return ND_IOCTL_CALL;
	case ND_CMD_VENDOR_EFFECT_LOG_SIZE:
	case ND_CMD_VENDOR_EFFECT_LOG:
	default:
					    return 0;
	}
}

static const char *ndctl_dimm_get_cmd_subname(struct ndctl_cmd *cmd)
{
	struct ndctl_dimm *dimm = cmd->dimm;
	struct ndctl_dimm_ops *ops = dimm ? dimm->ops : NULL;

	if (!dimm || cmd->type != ND_CMD_CALL || !ops || !ops->cmd_desc)
		return NULL;
	return ops->cmd_desc(cmd->pkg->nd_command);
}

NDCTL_EXPORT int ndctl_cmd_xlat_firmware_status(struct ndctl_cmd *cmd)
{
	struct ndctl_dimm *dimm = cmd->dimm;
	struct ndctl_dimm_ops *ops = dimm ? dimm->ops : NULL;

	if (!dimm || !ops || !ops->xlat_firmware_status)
		return 0;
	return ops->xlat_firmware_status(cmd);
}

static int do_cmd(int fd, int ioctl_cmd, struct ndctl_cmd *cmd)
{
	int rc;
	u32 offset;
	const char *name, *sub_name = NULL;
	struct ndctl_dimm *dimm = cmd->dimm;
	struct ndctl_bus *bus = cmd_to_bus(cmd);
	struct ndctl_cmd_iter *iter = &cmd->iter;
	struct ndctl_ctx *ctx = ndctl_bus_get_ctx(bus);

	if (dimm) {
		name = ndctl_dimm_get_cmd_name(dimm, cmd->type);
		sub_name = ndctl_dimm_get_cmd_subname(cmd);
	} else
		name = ndctl_bus_get_cmd_name(cmd->bus, cmd->type);


	if (iter->total_xfer == 0) {
		rc = ioctl(fd, ioctl_cmd, cmd->cmd_buf);
		dbg(ctx, "bus: %d dimm: %#x cmd: %s%s%s status: %d fw: %d (%s)\n",
				bus->id, dimm ? ndctl_dimm_get_handle(dimm) : 0,
				name, sub_name ? ":" : "", sub_name ? sub_name : "",
				rc, cmd->get_firmware_status(cmd), rc < 0 ?
				strerror(errno) : "success");
		if (rc < 0)
			return -errno;
		else
			return rc;
	}

	for (offset = 0; offset < iter->total_xfer; offset += iter->max_xfer) {
		cmd->set_xfer(cmd, min(iter->total_xfer - offset,
				iter->max_xfer));
		cmd->set_offset(cmd, offset);
		if (iter->dir == WRITE)
			memcpy(iter->data, iter->total_buf + offset,
					cmd->get_xfer(cmd));
		rc = ioctl(fd, ioctl_cmd, cmd->cmd_buf);
		if (rc < 0) {
			rc = -errno;
			break;
		}

		if (iter->dir == READ)
			memcpy(iter->total_buf + offset, iter->data,
					cmd->get_xfer(cmd) - rc);
		if (cmd->get_firmware_status(cmd) || rc) {
			rc = offset + cmd->get_xfer(cmd) - rc;
			break;
		}
	}

	dbg(ctx, "bus: %d dimm: %#x cmd: %s%s%s total: %d max_xfer: %d status: %d fw: %d (%s)\n",
			bus->id, dimm ? ndctl_dimm_get_handle(dimm) : 0,
			name, sub_name ? ":" : "", sub_name ? sub_name : "",
			iter->total_xfer, iter->max_xfer, rc,
			cmd->get_firmware_status(cmd),
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

	if (!cmd->get_firmware_status) {
		err(ctx, "missing status retrieval\n");
		return -EINVAL;
	}

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

NDCTL_EXPORT int ndctl_cmd_submit_xlat(struct ndctl_cmd *cmd)
{
	int rc, xlat_rc;

	rc = ndctl_cmd_submit(cmd);
	if (rc < 0)
		return rc;

	/*
	 * NOTE: This can lose a positive rc when xlat_rc is non-zero. The
	 * positive rc indicates a buffer underrun from the original command
	 * submission. If the caller cares about that (generally not very
	 * useful), then the xlat function is available separately as well.
	 */
	xlat_rc = ndctl_cmd_xlat_firmware_status(cmd);
	return (xlat_rc == 0) ? rc : xlat_rc;
}

NDCTL_EXPORT int ndctl_cmd_get_status(struct ndctl_cmd *cmd)
{
	return cmd->status;
}

NDCTL_EXPORT unsigned int ndctl_cmd_get_firmware_status(struct ndctl_cmd *cmd)
{
	return cmd->get_firmware_status(cmd);
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

	if (region->refresh_type) {
		region->refresh_type = 0;
		region_set_type(region, region->region_buf);
	}

	dbg(ctx, "%s: enabled\n", devname);
	return 0;
}

void region_flag_refresh(struct ndctl_region *region)
{
	region->refresh_type = 1;
}

NDCTL_EXPORT void ndctl_region_cleanup(struct ndctl_region *region)
{
	free_stale_namespaces(region);
	free_stale_btts(region);
	free_stale_pfns(region);
	free_stale_daxs(region);
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
	region->pfns_init = 0;
	region->daxs_init = 0;
	list_append_list(&region->stale_namespaces, &region->namespaces);
	list_append_list(&region->stale_btts, &region->btts);
	list_append_list(&region->stale_pfns, &region->pfns);
	list_append_list(&region->stale_daxs, &region->daxs);
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
	char buf[SYSFS_ATTR_SIZE];
	int len = dimm->buf_len;

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
		int position, match;

		sprintf(mapping_path, "%s/mapping%d", region->region_path, i);
		if (sysfs_read_attr(ctx, mapping_path, buf) < 0) {
			err(ctx, "bus%d region%d: failed to read mapping%d\n",
					bus->id, region->id, i);
			continue;
		}

		match = sscanf(buf, "nmem%u,%llu,%llu,%d", &dimm_id, &offset,
				&length, &position);
		if (match < 4)
			position = -1;
		if (match < 3) {
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
		mapping->position = position;
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

NDCTL_EXPORT int ndctl_mapping_get_position(struct ndctl_mapping *mapping)
{
	return mapping->position;
}

NDCTL_EXPORT struct ndctl_region *ndctl_mapping_get_region(
		struct ndctl_mapping *mapping)
{
	return mapping->region;
}

NDCTL_EXPORT struct ndctl_bus *ndctl_mapping_get_bus(
		struct ndctl_mapping *mapping)
{
	return ndctl_mapping_get_region(mapping)->bus;
}

NDCTL_EXPORT struct ndctl_ctx *ndctl_mapping_get_ctx(
		struct ndctl_mapping *mapping)
{
	return ndctl_mapping_get_bus(mapping)->ctx;
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

static const char *enforce_id_to_name(enum ndctl_namespace_mode mode)
{
	static const char *id_to_name[] = {
		[NDCTL_NS_MODE_MEMORY] = "pfn",
		[NDCTL_NS_MODE_SECTOR] = "btt", /* TODO: convert to btt2 */
		[NDCTL_NS_MODE_RAW] = "",
		[NDCTL_NS_MODE_DAX] = "dax",
		[NDCTL_NS_MODE_UNKNOWN] = "<unknown>",
	};

	if (mode < NDCTL_NS_MODE_UNKNOWN && mode >= 0)
		return id_to_name[mode];
	return id_to_name[NDCTL_NS_MODE_UNKNOWN];
}

static enum ndctl_namespace_mode enforce_name_to_id(const char *name)
{
	int i;

	for (i = 0; i < NDCTL_NS_MODE_UNKNOWN; i++)
		if (strcmp(enforce_id_to_name(i), name) == 0)
			return i;
	return NDCTL_NS_MODE_UNKNOWN;
}

static void *add_namespace(void *parent, int id, const char *ndns_base)
{
	const char *devname = devpath_to_devname(ndns_base);
	char *path = calloc(1, strlen(ndns_base) + 100);
	struct ndctl_namespace *ndns, *ndns_dup;
	struct ndctl_region *region = parent;
	struct ndctl_bus *bus = region->bus;
	struct ndctl_ctx *ctx = bus->ctx;
	char buf[SYSFS_ATTR_SIZE];

	if (!path)
		return NULL;

	ndns = calloc(1, sizeof(*ndns));
	if (!ndns)
		goto err_namespace;
	ndns->id = id;
	ndns->region = region;
	ndns->generation = region->generation;
	list_head_init(&ndns->injected_bb);

	sprintf(path, "%s/nstype", ndns_base);
	if (sysfs_read_attr(ctx, path, buf) < 0)
		goto err_read;
	ndns->type = strtoul(buf, NULL, 0);

	sprintf(path, "%s/size", ndns_base);
	if (sysfs_read_attr(ctx, path, buf) < 0)
		goto err_read;
	ndns->size = strtoull(buf, NULL, 0);

	sprintf(path, "%s/resource", ndns_base);
	if (sysfs_read_attr(ctx, path, buf) < 0)
		ndns->resource = ULLONG_MAX;
	else
		ndns->resource = strtoull(buf, NULL, 0);

	sprintf(path, "%s/force_raw", ndns_base);
	if (sysfs_read_attr(ctx, path, buf) < 0)
		goto err_read;
	ndns->raw_mode = strtoul(buf, NULL, 0);

	sprintf(path, "%s/numa_node", ndns_base);
	if (sysfs_read_attr(ctx, path, buf) == 0)
		ndns->numa_node = strtol(buf, NULL, 0);
	else
		ndns->numa_node = -1;

	sprintf(path, "%s/target_node", ndns_base);
	if (sysfs_read_attr(ctx, path, buf) == 0)
		ndns->target_node = strtol(buf, NULL, 0);
	else
		ndns->target_node = -1;

	sprintf(path, "%s/holder_class", ndns_base);
	if (sysfs_read_attr(ctx, path, buf) == 0)
		ndns->enforce_mode = enforce_name_to_id(buf);

	switch (ndns->type) {
	case ND_DEVICE_NAMESPACE_BLK:
	case ND_DEVICE_NAMESPACE_PMEM:
		sprintf(path, "%s/sector_size", ndns_base);
		if (sysfs_read_attr(ctx, path, buf) == 0)
			parse_lbasize_supported(ctx, devname, buf,
					&ndns->lbasize);
		else if (ndns->type == ND_DEVICE_NAMESPACE_BLK) {
			/*
			 * sector_size support is mandatory for blk,
			 * optional for pmem.
			 */
			goto err_read;
		} else
			parse_lbasize_supported(ctx, devname, "",
					&ndns->lbasize);
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
			free(path);
			return ndns_dup;
		}

	list_add(&region->namespaces, &ndns->list);
	free(path);
	return ndns;

 err_read:
	free(ndns->ndns_buf);
	free(ndns->ndns_path);
	free(ndns->alt_name);
	free(ndns);
 err_namespace:
	free(path);
	return NULL;
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
	char buf[SYSFS_ATTR_SIZE];

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

NDCTL_EXPORT struct ndctl_pfn *ndctl_namespace_get_pfn(struct ndctl_namespace *ndns)
{
	struct ndctl_region *region = ndctl_namespace_get_region(ndns);
	struct ndctl_ctx *ctx = ndctl_namespace_get_ctx(ndns);
	char *path = ndns->ndns_buf;
	int len = ndns->buf_len;
	struct ndctl_pfn *pfn;
	char buf[SYSFS_ATTR_SIZE];

	if (snprintf(path, len, "%s/holder", ndns->ndns_path) >= len) {
		err(ctx, "%s: buffer too small!\n",
				ndctl_namespace_get_devname(ndns));
		return NULL;
	}

	if (sysfs_read_attr(ctx, path, buf) < 0)
		return NULL;

	ndctl_pfn_foreach(region, pfn)
		if (strcmp(buf, ndctl_pfn_get_devname(pfn)) == 0)
			return pfn;
	return NULL;
}

NDCTL_EXPORT struct ndctl_dax *ndctl_namespace_get_dax(struct ndctl_namespace *ndns)
{
	struct ndctl_region *region = ndctl_namespace_get_region(ndns);
	struct ndctl_ctx *ctx = ndctl_namespace_get_ctx(ndns);
	char *path = ndns->ndns_buf;
	int len = ndns->buf_len;
	struct ndctl_dax *dax;
	char buf[SYSFS_ATTR_SIZE];

	if (snprintf(path, len, "%s/holder", ndns->ndns_path) >= len) {
		err(ctx, "%s: buffer too small!\n",
				ndctl_namespace_get_devname(ndns));
		return NULL;
	}

	if (sysfs_read_attr(ctx, path, buf) < 0)
		return NULL;

	ndctl_dax_foreach(region, dax)
		if (strcmp(buf, ndctl_dax_get_devname(dax)) == 0)
			return dax;
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

NDCTL_EXPORT enum ndctl_namespace_mode ndctl_namespace_get_mode(
		struct ndctl_namespace *ndns)
{
	struct ndctl_ctx *ctx = ndctl_namespace_get_ctx(ndns);
	char *path = ndns->ndns_buf;
	int len = ndns->buf_len;
	char buf[SYSFS_ATTR_SIZE];

	if (snprintf(path, len, "%s/mode", ndns->ndns_path) >= len) {
		err(ctx, "%s: buffer too small!\n",
				ndctl_namespace_get_devname(ndns));
		return -ENOMEM;
	}

	if (sysfs_read_attr(ctx, path, buf) < 0)
		return -ENXIO;

	if (strcmp("memory", buf) == 0)
		return NDCTL_NS_MODE_MEMORY;
	if (strcmp("dax", buf) == 0)
		return NDCTL_NS_MODE_DAX;
	if (strcmp("raw", buf) == 0)
		return NDCTL_NS_MODE_RAW;
	if (strcmp("safe", buf) == 0)
		return NDCTL_NS_MODE_SECTOR;
	return -ENXIO;
}

NDCTL_EXPORT enum ndctl_namespace_mode ndctl_namespace_get_enforce_mode(
		struct ndctl_namespace *ndns)
{
	return ndns->enforce_mode;
}

NDCTL_EXPORT int ndctl_namespace_set_enforce_mode(struct ndctl_namespace *ndns,
		enum ndctl_namespace_mode mode)
{
	struct ndctl_ctx *ctx = ndctl_namespace_get_ctx(ndns);
	char *path = ndns->ndns_buf;
	int len = ndns->buf_len;
	int rc;

	if (mode < 0 || mode >= NDCTL_NS_MODE_UNKNOWN)
		return -EINVAL;

	if (snprintf(path, len, "%s/holder_class", ndns->ndns_path) >= len) {
		err(ctx, "%s: buffer too small!\n",
				ndctl_namespace_get_devname(ndns));
		return -ENOMEM;
	}

	rc = sysfs_write_attr(ctx, path, enforce_id_to_name(mode));
	if (rc >= 0)
		ndns->enforce_mode = mode;
	return rc;
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
	int len = ndns->buf_len, rc;

	if (snprintf(path, len, "%s/force_raw", ndns->ndns_path) >= len) {
		err(ctx, "%s: buffer too small!\n",
				ndctl_namespace_get_devname(ndns));
		return -ENXIO;
	}

	raw_mode = !!raw_mode;
	rc = sysfs_write_attr(ctx, path, raw_mode ? "1\n" : "0\n");
	if (rc < 0)
		return rc;

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

NDCTL_EXPORT struct badblock *ndctl_namespace_get_next_badblock(
		struct ndctl_namespace *ndns)
{
	return badblocks_iter_next(&ndns->bb_iter);
}

NDCTL_EXPORT struct badblock *ndctl_namespace_get_first_badblock(
		struct ndctl_namespace *ndns)
{
	struct ndctl_btt *btt = ndctl_namespace_get_btt(ndns);
	struct ndctl_pfn *pfn = ndctl_namespace_get_pfn(ndns);
	struct ndctl_dax *dax = ndctl_namespace_get_dax(ndns);
	struct ndctl_ctx *ctx = ndctl_namespace_get_ctx(ndns);
	const char *dev = ndctl_namespace_get_devname(ndns);
	char path[SYSFS_ATTR_SIZE];
	ssize_t len = sizeof(path);
	const char *bdev;

	if (btt || dax) {
		dbg(ctx, "%s: badblocks not supported for %s\n", dev,
				btt ? "btt" : "device-dax");
		return NULL;
	}

	if (pfn)
		bdev = ndctl_pfn_get_block_device(pfn);
	else
		bdev = ndctl_namespace_get_block_device(ndns);

	if (!bdev) {
		dbg(ctx, "%s: failed to determine block device\n", dev);
		return NULL;
	}

	if (snprintf(path, len, "/sys/block/%s", bdev) >= len) {
		err(ctx, "%s: buffer too small!\n", dev);
		return NULL;
	}

	return badblocks_iter_first(&ndns->bb_iter, ctx, path);
}

static int ndctl_bind(struct ndctl_ctx *ctx, struct kmod_module *module,
		const char *devname)
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
		dbg(ctx, "%s: bind failed\n", devname);
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

static void *add_btt(void *parent, int id, const char *btt_base);
static void *add_pfn(void *parent, int id, const char *pfn_base);
static void *add_dax(void *parent, int id, const char *dax_base);

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

static void pfns_init(struct ndctl_region *region)
{
	struct ndctl_bus *bus = ndctl_region_get_bus(region);
	char pfn_fmt[20];

	if (region->pfns_init)
		return;
	region->pfns_init = 1;

	sprintf(pfn_fmt, "pfn%d.", region->id);
	device_parse(bus->ctx, bus, region->region_path, pfn_fmt, region, add_pfn);
}

static void daxs_init(struct ndctl_region *region)
{
	struct ndctl_bus *bus = ndctl_region_get_bus(region);
	char dax_fmt[20];

	if (region->daxs_init)
		return;
	region->daxs_init = 1;

	sprintf(dax_fmt, "dax%d.", region->id);
	device_parse(bus->ctx, bus, region->region_path, dax_fmt, region, add_dax);
}

static void region_refresh_children(struct ndctl_region *region)
{
	region->namespaces_init = 0;
	region->btts_init = 0;
	region->pfns_init = 0;
	region->daxs_init = 0;
	namespaces_init(region);
	btts_init(region);
	pfns_init(region);
	daxs_init(region);
}

NDCTL_EXPORT bool ndctl_namespace_is_active(struct ndctl_namespace *ndns)
{
	struct ndctl_btt *btt = ndctl_namespace_get_btt(ndns);
	struct ndctl_pfn *pfn = ndctl_namespace_get_pfn(ndns);
	struct ndctl_dax *dax = ndctl_namespace_get_dax(ndns);

	if ((btt && ndctl_btt_is_enabled(btt))
			|| (pfn && ndctl_pfn_is_enabled(pfn))
			|| (dax && ndctl_dax_is_enabled(dax))
			|| (!btt && !pfn && !dax
				&& ndctl_namespace_is_enabled(ndns)))
		return true;
	return false;
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
	unsigned long long size = ndctl_namespace_get_size(ndns);
	int rc;

	if (ndctl_namespace_is_enabled(ndns))
		return 0;

	/* Don't try to enable idle namespace (no capacity allocated) */
	if (size == 0)
		return -ENXIO;

	rc = ndctl_bind(ctx, ndns->module, devname);

	/*
	 * Rescan now as successfully enabling a namespace device leads
	 * to a new one being created, and potentially btts, pfns, or
	 * daxs being attached
	 */
	region_refresh_children(region);

	if (!ndctl_namespace_is_enabled(ndns)) {
		struct ndctl_btt *btt = ndctl_namespace_get_btt(ndns);
		struct ndctl_pfn *pfn = ndctl_namespace_get_pfn(ndns);
		struct ndctl_dax *dax = ndctl_namespace_get_dax(ndns);

		if (btt && ndctl_btt_is_enabled(btt)) {
			dbg(ctx, "%s: enabled via %s\n", devname,
					ndctl_btt_get_devname(btt));
			return 1;
		}
		if (pfn && ndctl_pfn_is_enabled(pfn)) {
			dbg(ctx, "%s: enabled via %s\n", devname,
					ndctl_pfn_get_devname(pfn));
			return 1;
		}
		if (dax && ndctl_dax_is_enabled(dax)) {
			dbg(ctx, "%s: enabled via %s\n", devname,
					ndctl_dax_get_devname(dax));
			return 1;
		}

		err(ctx, "%s: failed to enable\n", devname);
		return rc ? rc : -ENXIO;
	}
	rc = 0;
	dbg(ctx, "%s: enabled\n", devname);


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

NDCTL_EXPORT int ndctl_namespace_disable_invalidate(struct ndctl_namespace *ndns)
{
	struct ndctl_btt *btt = ndctl_namespace_get_btt(ndns);
	struct ndctl_pfn *pfn = ndctl_namespace_get_pfn(ndns);
	struct ndctl_dax *dax = ndctl_namespace_get_dax(ndns);
	int rc = 0;

	if (btt)
		rc = ndctl_btt_delete(btt);
	if (pfn)
		rc = ndctl_pfn_delete(pfn);
	if (dax)
		rc = ndctl_dax_delete(dax);

	if (rc)
		return rc;

	return ndctl_namespace_disable(ndns);
}

NDCTL_EXPORT int ndctl_namespace_disable_safe(struct ndctl_namespace *ndns)
{
	const char *devname = ndctl_namespace_get_devname(ndns);
	struct ndctl_ctx *ctx = ndctl_namespace_get_ctx(ndns);
	struct ndctl_pfn *pfn = ndctl_namespace_get_pfn(ndns);
	struct ndctl_btt *btt = ndctl_namespace_get_btt(ndns);
	const char *bdev = NULL;
	char path[50];
	int fd;

	if (pfn && ndctl_pfn_is_enabled(pfn))
		bdev = ndctl_pfn_get_block_device(pfn);
	else if (btt && ndctl_btt_is_enabled(btt))
		bdev = ndctl_btt_get_block_device(btt);
	else if (ndctl_namespace_is_enabled(ndns))
		bdev = ndctl_namespace_get_block_device(ndns);

	if (bdev) {
		sprintf(path, "/dev/%s", bdev);
		fd = open(path, O_RDWR|O_EXCL);
		if (fd >= 0) {
			/*
			 * Got it, now block new mounts while we have it
			 * pinned.
			 */
			ndctl_namespace_disable_invalidate(ndns);
			close(fd);
		} else {
			/*
			 * Yes, TOCTOU hole, but if you're racing namespace
			 * creation you have other problems, and there's nothing
			 * stopping the !bdev case from racing to mount an fs or
			 * re-enabling the namepace.
			 */
			dbg(ctx, "%s: %s failed exclusive open: %s\n",
					devname, bdev, strerror(errno));
			return -errno;
		}
	} else
		ndctl_namespace_disable_invalidate(ndns);

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

/*
 * Check if a given 'seed' namespace is ok to configure.
 * If a size or uuid is present, it is considered not configuration-idle,
 * except in the case of legacy (ND_DEVICE_NAMESPACE_IO) namespaces. In
 * that case, the size is never zero, but the namespace can still be
 * reconfigured.
 */
NDCTL_EXPORT int ndctl_namespace_is_configuration_idle(
		struct ndctl_namespace *ndns)
{
	if (ndctl_namespace_is_active(ndns))
		return 0;
	if (ndctl_namespace_is_configured(ndns)) {
		if (ndctl_namespace_get_type(ndns) == ND_DEVICE_NAMESPACE_IO)
			return 1;
		return 0;
	}
	/* !active and !configured is configuration-idle */
	return 1;
}

NDCTL_EXPORT void ndctl_namespace_get_uuid(struct ndctl_namespace *ndns, uuid_t uu)
{
	memcpy(uu, ndns->uuid, sizeof(uuid_t));
}

NDCTL_EXPORT int ndctl_namespace_set_uuid(struct ndctl_namespace *ndns, uuid_t uu)
{
	struct ndctl_ctx *ctx = ndctl_namespace_get_ctx(ndns);
	char *path = ndns->ndns_buf;
	int len = ndns->buf_len, rc;
	char uuid[40];

	if (snprintf(path, len, "%s/uuid", ndns->ndns_path) >= len) {
		err(ctx, "%s: buffer too small!\n",
				ndctl_namespace_get_devname(ndns));
		return -ENXIO;
	}

	uuid_unparse(uu, uuid);
	rc = sysfs_write_attr(ctx, path, uuid);
	if (rc != 0)
		return rc;
	memcpy(ndns->uuid, uu, sizeof(uuid_t));
	return 0;
}

NDCTL_EXPORT unsigned int ndctl_namespace_get_supported_sector_size(
		struct ndctl_namespace *ndns, int i)
{
	if (ndns->lbasize.num == 0)
		return 0;

	if (i < 0 || i > ndns->lbasize.num) {
		errno = EINVAL;
		return UINT_MAX;
	} else
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
	int len = ndns->buf_len, rc;
	char sector_str[40];
	int i;

	for (i = 0; i < ndns->lbasize.num; i++)
		if (ndns->lbasize.supported[i] == sector_size)
			break;

	if (i > ndns->lbasize.num) {
		err(ctx, "%s: unsupported sector size %d\n",
				ndctl_namespace_get_devname(ndns), sector_size);
		return -EOPNOTSUPP;
	}

	if (snprintf(path, len, "%s/sector_size", ndns->ndns_path) >= len) {
		err(ctx, "%s: buffer too small!\n",
				ndctl_namespace_get_devname(ndns));
		return -ENXIO;
	}

	sprintf(sector_str, "%d\n", sector_size);
	rc = sysfs_write_attr(ctx, path, sector_str);
	if (rc != 0)
		return rc;

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
	int len = ndns->buf_len, rc;
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

	rc = sysfs_write_attr(ctx, path, buf);
	if (rc < 0) {
		free(buf);
		return rc;
	}

	free(ndns->alt_name);
	ndns->alt_name = buf;
	return 0;
}

NDCTL_EXPORT unsigned long long ndctl_namespace_get_size(struct ndctl_namespace *ndns)
{
	return ndns->size;
}

NDCTL_EXPORT unsigned long long ndctl_namespace_get_resource(struct ndctl_namespace *ndns)
{
	return ndns->resource;
}

static int namespace_set_size(struct ndctl_namespace *ndns,
		unsigned long long size)
{
	struct ndctl_ctx *ctx = ndctl_namespace_get_ctx(ndns);
	char *path = ndns->ndns_buf;
	int len = ndns->buf_len, rc;
	char buf[SYSFS_ATTR_SIZE];

	if (snprintf(path, len, "%s/size", ndns->ndns_path) >= len) {
		err(ctx, "%s: buffer too small!\n",
				ndctl_namespace_get_devname(ndns));
		return -ENXIO;
	}

	sprintf(buf, "%#llx\n", size);
	rc = sysfs_write_attr(ctx, path, buf);
	if (rc < 0)
		return rc;

	ndns->size = size;

	/*
	 * A size change event invalidates / establishes 'resource', try
	 * to refresh it.
	 */
	if (snprintf(path, len, "%s/resource", ndns->ndns_path) >= len) {
		err(ctx, "%s: buffer too small!\n",
				ndctl_namespace_get_devname(ndns));
		ndns->resource = ULLONG_MAX;
		return 0;
	}

	if (sysfs_read_attr(ctx, path, buf) < 0) {
		ndns->resource = ULLONG_MAX;
		return 0;
	}

	ndns->resource = strtoull(buf, NULL, 0);
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

NDCTL_EXPORT int ndctl_namespace_get_numa_node(struct ndctl_namespace *ndns)
{
    return ndns->numa_node;
}

NDCTL_EXPORT int ndctl_namespace_get_target_node(struct ndctl_namespace *ndns)
{
	return ndns->target_node;
}

static int __ndctl_namespace_set_write_cache(struct ndctl_namespace *ndns,
		int state)
{
	struct ndctl_ctx *ctx = ndctl_namespace_get_ctx(ndns);
	struct ndctl_pfn *pfn = ndctl_namespace_get_pfn(ndns);
	char *path = ndns->ndns_buf;
	char buf[SYSFS_ATTR_SIZE];
	int len = ndns->buf_len;
	const char *bdev;

	if (state != 1 && state != 0)
		return -ENXIO;
	if (pfn)
		bdev = ndctl_pfn_get_block_device(pfn);
	else
		bdev = ndctl_namespace_get_block_device(ndns);

	if (!bdev)
		return -ENXIO;

	if (snprintf(path, len, "/sys/block/%s/dax/write_cache", bdev) >= len) {
		err(ctx, "%s: buffer too small!\n",
				ndctl_namespace_get_devname(ndns));
		return -ENXIO;
	}

	sprintf(buf, "%d\n", state);
	return sysfs_write_attr(ctx, path, buf);
}

NDCTL_EXPORT int ndctl_namespace_enable_write_cache(
		struct ndctl_namespace *ndns)
{
	return __ndctl_namespace_set_write_cache(ndns, 1);
}

NDCTL_EXPORT int ndctl_namespace_disable_write_cache(
		struct ndctl_namespace *ndns)
{
	return __ndctl_namespace_set_write_cache(ndns, 0);
}

NDCTL_EXPORT int ndctl_namespace_write_cache_is_enabled(
		struct ndctl_namespace *ndns)
{
	struct ndctl_ctx *ctx = ndctl_namespace_get_ctx(ndns);
	struct ndctl_pfn *pfn = ndctl_namespace_get_pfn(ndns);
	int len = ndns->buf_len, wc;
	char *path = ndns->ndns_buf;
	char buf[SYSFS_ATTR_SIZE];
	const char *bdev;

	if (pfn)
		bdev = ndctl_pfn_get_block_device(pfn);
	else
		bdev = ndctl_namespace_get_block_device(ndns);

	if (!bdev)
		return -ENXIO;

	if (snprintf(path, len, "/sys/block/%s/dax/write_cache", bdev) >= len) {
		err(ctx, "%s: buffer too small!\n",
				ndctl_namespace_get_devname(ndns));
		return -ENXIO;
	}

	if (sysfs_read_attr(ctx, path, buf) < 0)
		return -ENXIO;

	if (sscanf(buf, "%d", &wc) == 1)
		if (wc)
			return 1;

	return 0;
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
		dbg(ctx, "%s: nstype: %d not deletable\n",
				ndctl_namespace_get_devname(ndns),
				ndctl_namespace_get_type(ndns));
		return 0;
	}

	rc = namespace_set_size(ndns, 0);
	/*
	 * if the namespace has already been deleted, this will return
	 * -ENXIO due to the uuid check in __size_store. We can safely
	 *  ignore it in the case of writing a zero.
	 */
	if (rc && (rc != -ENXIO))
		return rc;

	region->namespaces_init = 0;
	free_namespace(ndns, &region->namespaces);
	return 0;
}

static int parse_lbasize_supported(struct ndctl_ctx *ctx, const char *devname,
		const char *buf, struct ndctl_lbasize *lba)
{
	char *s = strdup(buf), *end, *field;
	void *temp;

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

		temp = realloc(lba->supported,
				sizeof(unsigned int) * ++lba->num);
		if (temp != NULL)
			lba->supported = temp;
		else
			goto err;
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

static void *add_btt(void *parent, int id, const char *btt_base)
{
	struct ndctl_ctx *ctx = ndctl_region_get_ctx(parent);
	const char *devname = devpath_to_devname(btt_base);
	char *path = calloc(1, strlen(btt_base) + 100);
	struct ndctl_region *region = parent;
	struct ndctl_btt *btt, *btt_dup;
	char buf[SYSFS_ATTR_SIZE];

	if (!path)
		return NULL;

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
	if (strlen(buf) && uuid_parse(buf, btt->uuid) < 0)
		goto err_read;

	sprintf(path, "%s/sector_size", btt_base);
	if (sysfs_read_attr(ctx, path, buf) < 0)
		goto err_read;
	if (parse_lbasize_supported(ctx, devname, buf, &btt->lbasize) < 0)
		goto err_read;

	sprintf(path, "%s/size", btt_base);
	if (sysfs_read_attr(ctx, path, buf) < 0)
		btt->size = ULLONG_MAX;
	else
		btt->size = strtoull(buf, NULL, 0);

	free(path);
	ndctl_btt_foreach(region, btt_dup)
		if (btt->id == btt_dup->id) {
			btt_dup->size = btt->size;
			free_btt(btt, NULL);
			return btt_dup;
		}

	list_add(&region->btts, &btt->list);
	return btt;

 err_read:
	free(btt->lbasize.supported);
	free(btt->btt_buf);
	free(btt->btt_path);
	free(btt);
 err_btt:
	free(path);
	return NULL;
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
	if (i < 0 || i > btt->lbasize.num) {
		errno = EINVAL;
		return UINT_MAX;
	} else
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
	char buf[SYSFS_ATTR_SIZE];

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

NDCTL_EXPORT unsigned long long ndctl_btt_get_size(struct ndctl_btt *btt)
{
	return btt->size;
}

NDCTL_EXPORT int ndctl_btt_set_uuid(struct ndctl_btt *btt, uuid_t uu)
{
	struct ndctl_ctx *ctx = ndctl_btt_get_ctx(btt);
	char *path = btt->btt_buf;
	int len = btt->buf_len, rc;
	char uuid[40];

	if (snprintf(path, len, "%s/uuid", btt->btt_path) >= len) {
		err(ctx, "%s: buffer too small!\n",
				ndctl_btt_get_devname(btt));
		return -ENXIO;
	}

	uuid_unparse(uu, uuid);
	rc = sysfs_write_attr(ctx, path, uuid);
	if (rc != 0)
		return rc;
	memcpy(btt->uuid, uu, sizeof(uuid_t));
	return 0;
}

NDCTL_EXPORT int ndctl_btt_set_sector_size(struct ndctl_btt *btt,
		unsigned int sector_size)
{
	struct ndctl_ctx *ctx = ndctl_btt_get_ctx(btt);
	char *path = btt->btt_buf;
	int len = btt->buf_len, rc;
	char sector_str[40];
	int i;

	if (snprintf(path, len, "%s/sector_size", btt->btt_path) >= len) {
		err(ctx, "%s: buffer too small!\n",
				ndctl_btt_get_devname(btt));
		return -ENXIO;
	}

	sprintf(sector_str, "%d\n", sector_size);
	rc = sysfs_write_attr(ctx, path, sector_str);
	if (rc != 0)
		return rc;

	for (i = 0; i < btt->lbasize.num; i++)
		if (btt->lbasize.supported[i] == sector_size)
			btt->lbasize.select = i;
	return 0;
}

NDCTL_EXPORT int ndctl_btt_set_namespace(struct ndctl_btt *btt,
		struct ndctl_namespace *ndns)
{
	struct ndctl_ctx *ctx = ndctl_btt_get_ctx(btt);
	int len = btt->buf_len, rc;
	char *path = btt->btt_buf;

	if (snprintf(path, len, "%s/namespace", btt->btt_path) >= len) {
		err(ctx, "%s: buffer too small!\n",
				ndctl_btt_get_devname(btt));
		return -ENXIO;
	}

	rc = sysfs_write_attr(ctx, path, ndns
				? ndctl_namespace_get_devname(ndns) : "\n");
	if (rc != 0)
		return rc;

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
	 * new one being created, and potentially the backing namespace
	 * as well.
	 */
	region_refresh_children(region);

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

static void *__add_pfn(struct ndctl_pfn *pfn, const char *pfn_base)
{
	struct ndctl_ctx *ctx = ndctl_region_get_ctx(pfn->region);
	char *path = calloc(1, strlen(pfn_base) + 100);
	struct ndctl_region *region = pfn->region;
	char buf[SYSFS_ATTR_SIZE];

	if (!path)
		return NULL;

	pfn->generation = region->generation;

	pfn->pfn_path = strdup(pfn_base);
	if (!pfn->pfn_path)
		goto err_read;

	pfn->pfn_buf = calloc(1, strlen(pfn_base) + 50);
	if (!pfn->pfn_buf)
		goto err_read;
	pfn->buf_len = strlen(pfn_base) + 50;

	sprintf(path, "%s/modalias", pfn_base);
	if (sysfs_read_attr(ctx, path, buf) < 0)
		goto err_read;
	pfn->module = to_module(ctx, buf);

	sprintf(path, "%s/uuid", pfn_base);
	if (sysfs_read_attr(ctx, path, buf) < 0)
		goto err_read;
	if (strlen(buf) && uuid_parse(buf, pfn->uuid) < 0)
		goto err_read;

	sprintf(path, "%s/mode", pfn_base);
	if (sysfs_read_attr(ctx, path, buf) < 0)
		goto err_read;
	if (strcmp(buf, "none") == 0)
		pfn->loc = NDCTL_PFN_LOC_NONE;
	else if (strcmp(buf, "ram") == 0)
		pfn->loc = NDCTL_PFN_LOC_RAM;
	else if (strcmp(buf, "pmem") == 0)
		pfn->loc = NDCTL_PFN_LOC_PMEM;
	else
		goto err_read;

	sprintf(path, "%s/align", pfn_base);
	if (sysfs_read_attr(ctx, path, buf) < 0)
		pfn->align = 0;
	else
		pfn->align = strtoul(buf, NULL, 0);

	sprintf(path, "%s/resource", pfn_base);
	if (sysfs_read_attr(ctx, path, buf) < 0)
		pfn->resource = ULLONG_MAX;
	else
		pfn->resource = strtoull(buf, NULL, 0);

	sprintf(path, "%s/size", pfn_base);
	if (sysfs_read_attr(ctx, path, buf) < 0)
		pfn->size = ULLONG_MAX;
	else
		pfn->size = strtoull(buf, NULL, 0);

	/*
	 * The supported_alignments attribute was added before arches other
	 * than x86 had pmem support. If the kernel doesn't provide the
	 * attribute then it's safe to assume that we running on x86 where
	 * 4KiB and 2MiB have always been supported.
	 */
	sprintf(path, "%s/supported_alignments", pfn_base);
	if (sysfs_read_attr(ctx, path, buf) < 0)
		sprintf(buf, "%d %d", SZ_4K, SZ_2M);

	if (parse_lbasize_supported(ctx, pfn_base, buf, &pfn->alignments) < 0)
		goto err_read;

	free(path);
	return pfn;

 err_read:
	free(pfn->pfn_buf);
	free(pfn->pfn_path);
	free(path);
	return NULL;
}

static void *add_pfn(void *parent, int id, const char *pfn_base)
{
	struct ndctl_pfn *pfn = calloc(1, sizeof(*pfn)), *pfn_dup;
	struct ndctl_region *region = parent;

	if (!pfn)
		return NULL;

	pfn->id = id;
	pfn->region = region;
	if (!__add_pfn(pfn, pfn_base)) {
		free(pfn);
		return NULL;
	}

	ndctl_pfn_foreach(region, pfn_dup)
		if (pfn->id == pfn_dup->id) {
			pfn_dup->resource = pfn->resource;
			pfn_dup->size = pfn->size;
			free_pfn(pfn, NULL);
			return pfn_dup;
		}

	list_add(&region->pfns, &pfn->list);

	return pfn;
}

static void *add_dax(void *parent, int id, const char *dax_base)
{
	struct ndctl_dax *dax = calloc(1, sizeof(*dax)), *dax_dup;
	struct ndctl_region *region = parent;
	struct ndctl_pfn *pfn = &dax->pfn;

	if (!dax)
		return NULL;

	pfn->id = id;
	pfn->region = region;
	if (!__add_pfn(pfn, dax_base)) {
		free(dax);
		return NULL;
	}

	ndctl_dax_foreach(region, dax_dup) {
		struct ndctl_pfn *pfn_dup = &dax_dup->pfn;

		if (pfn->id == pfn_dup->id) {
			pfn_dup->resource = pfn->resource;
			pfn_dup->size = pfn->size;
			free_dax(dax, NULL);
			return dax_dup;
		}
	}

	list_add(&region->daxs, &dax->pfn.list);

	return dax;
}

NDCTL_EXPORT struct ndctl_pfn *ndctl_pfn_get_first(struct ndctl_region *region)
{
	pfns_init(region);

	return list_top(&region->pfns, struct ndctl_pfn, list);
}

NDCTL_EXPORT struct ndctl_pfn *ndctl_pfn_get_next(struct ndctl_pfn *pfn)
{
	struct ndctl_region *region = pfn->region;

	return list_next(&region->pfns, pfn, list);
}

NDCTL_EXPORT unsigned int ndctl_pfn_get_id(struct ndctl_pfn *pfn)
{
	return pfn->id;
}

NDCTL_EXPORT struct ndctl_namespace *ndctl_pfn_get_namespace(struct ndctl_pfn *pfn)
{
	struct ndctl_ctx *ctx = ndctl_pfn_get_ctx(pfn);
	struct ndctl_namespace *ndns, *found = NULL;
	struct ndctl_region *region = pfn->region;
	char *path = region->region_buf;
	int len = region->buf_len;
	char buf[SYSFS_ATTR_SIZE];

	if (pfn->ndns)
		return pfn->ndns;

	if (snprintf(path, len, "%s/namespace", pfn->pfn_path) >= len) {
		err(ctx, "%s: buffer too small!\n",
				ndctl_pfn_get_devname(pfn));
		return NULL;
	}

	if (sysfs_read_attr(ctx, path, buf) < 0)
		return NULL;

	ndctl_namespace_foreach(region, ndns)
		if (strcmp(buf, ndctl_namespace_get_devname(ndns)) == 0)
			found = ndns;
	pfn->ndns = found;
	return found;
}

NDCTL_EXPORT void ndctl_pfn_get_uuid(struct ndctl_pfn *pfn, uuid_t uu)
{
	memcpy(uu, pfn->uuid, sizeof(uuid_t));
}

NDCTL_EXPORT unsigned long long ndctl_pfn_get_size(struct ndctl_pfn *pfn)
{
	return pfn->size;
}

NDCTL_EXPORT unsigned long long ndctl_pfn_get_resource(struct ndctl_pfn *pfn)
{
	return pfn->resource;
}

NDCTL_EXPORT int ndctl_pfn_set_uuid(struct ndctl_pfn *pfn, uuid_t uu)
{
	struct ndctl_ctx *ctx = ndctl_pfn_get_ctx(pfn);
	int len = pfn->buf_len, rc;
	char *path = pfn->pfn_buf;
	char uuid[40];

	if (snprintf(path, len, "%s/uuid", pfn->pfn_path) >= len) {
		err(ctx, "%s: buffer too small!\n",
				ndctl_pfn_get_devname(pfn));
		return -ENXIO;
	}

	uuid_unparse(uu, uuid);
	rc = sysfs_write_attr(ctx, path, uuid);
	if (rc != 0)
		return rc;
	memcpy(pfn->uuid, uu, sizeof(uuid_t));
	return 0;
}

NDCTL_EXPORT enum ndctl_pfn_loc ndctl_pfn_get_location(struct ndctl_pfn *pfn)
{
	return pfn->loc;
}

NDCTL_EXPORT int ndctl_pfn_set_location(struct ndctl_pfn *pfn,
		enum ndctl_pfn_loc loc)
{
	struct ndctl_ctx *ctx = ndctl_pfn_get_ctx(pfn);
	int len = pfn->buf_len, rc;
	char *path = pfn->pfn_buf;
	const char *locations[] = {
		[NDCTL_PFN_LOC_NONE] = "none",
		[NDCTL_PFN_LOC_RAM] = "ram",
		[NDCTL_PFN_LOC_PMEM] = "pmem",
	};

	switch (loc) {
	case NDCTL_PFN_LOC_NONE:
	case NDCTL_PFN_LOC_RAM:
	case NDCTL_PFN_LOC_PMEM:
		break;
	default:
		return -EINVAL;
	}

	if (snprintf(path, len, "%s/mode", pfn->pfn_path) >= len) {
		err(ctx, "%s: buffer too small!\n",
				ndctl_pfn_get_devname(pfn));
		return -ENXIO;
	}

	rc = sysfs_write_attr(ctx, path, locations[loc]);
	if (rc != 0)
		return rc;
	pfn->loc = loc;
	return 0;
}

NDCTL_EXPORT unsigned long ndctl_pfn_get_align(struct ndctl_pfn *pfn)
{
	return pfn->align;
}

NDCTL_EXPORT int ndctl_pfn_has_align(struct ndctl_pfn *pfn)
{
	struct ndctl_ctx *ctx = ndctl_pfn_get_ctx(pfn);
	char *path = pfn->pfn_buf;
	int len = pfn->buf_len;
	struct stat st;

	if (snprintf(path, len, "%s/align", pfn->pfn_path) >= len) {
		err(ctx, "%s: buffer too small!\n",
				ndctl_pfn_get_devname(pfn));
		return 0;
	}

	return stat(path, &st) == 0;
}

NDCTL_EXPORT int ndctl_pfn_set_align(struct ndctl_pfn *pfn, unsigned long align)
{
	struct ndctl_ctx *ctx = ndctl_pfn_get_ctx(pfn);
	int len = pfn->buf_len, rc;
	char *path = pfn->pfn_buf;
	char align_str[40];

	if (snprintf(path, len, "%s/align", pfn->pfn_path) >= len) {
		err(ctx, "%s: buffer too small!\n",
				ndctl_pfn_get_devname(pfn));
		return -ENXIO;
	}

	sprintf(align_str, "%lu\n", align);
	rc = sysfs_write_attr(ctx, path, align_str);
	if (rc != 0)
		return rc;
	pfn->align = align;
	return 0;
}

NDCTL_EXPORT int ndctl_pfn_get_num_alignments(struct ndctl_pfn *pfn)
{
	return pfn->alignments.num;
}

NDCTL_EXPORT unsigned long ndctl_pfn_get_supported_alignment(
		struct ndctl_pfn *pfn, int i)
{
	if (pfn->alignments.num == 0)
		return 0;

	if (i < 0 || i > pfn->alignments.num)
		return -EINVAL;
	else
		return pfn->alignments.supported[i];
}

NDCTL_EXPORT int ndctl_pfn_set_namespace(struct ndctl_pfn *pfn,
		struct ndctl_namespace *ndns)
{
	struct ndctl_ctx *ctx = ndctl_pfn_get_ctx(pfn);
	int len = pfn->buf_len, rc;
	char *path = pfn->pfn_buf;

	if (snprintf(path, len, "%s/namespace", pfn->pfn_path) >= len) {
		err(ctx, "%s: buffer too small!\n",
				ndctl_pfn_get_devname(pfn));
		return -ENXIO;
	}

	rc = sysfs_write_attr(ctx, path, ndns
				? ndctl_namespace_get_devname(ndns) : "\n");
	if (rc != 0)
		return rc;

	pfn->ndns = ndns;
	return 0;
}

NDCTL_EXPORT struct ndctl_bus *ndctl_pfn_get_bus(struct ndctl_pfn *pfn)
{
	return pfn->region->bus;
}

NDCTL_EXPORT struct ndctl_ctx *ndctl_pfn_get_ctx(struct ndctl_pfn *pfn)
{
	return ndctl_bus_get_ctx(ndctl_pfn_get_bus(pfn));
}

NDCTL_EXPORT const char *ndctl_pfn_get_devname(struct ndctl_pfn *pfn)
{
	return devpath_to_devname(pfn->pfn_path);
}

NDCTL_EXPORT const char *ndctl_pfn_get_block_device(struct ndctl_pfn *pfn)
{
	struct ndctl_ctx *ctx = ndctl_pfn_get_ctx(pfn);
	struct ndctl_bus *bus = ndctl_pfn_get_bus(pfn);
	char *path = pfn->pfn_buf;
	int len = pfn->buf_len;

	if (pfn->bdev)
		return pfn->bdev;

	if (snprintf(path, len, "%s/block", pfn->pfn_path) >= len) {
		err(ctx, "%s: buffer too small!\n",
				ndctl_pfn_get_devname(pfn));
		return "";
	}

	ndctl_bus_wait_probe(bus);
	pfn->bdev = get_block_device(ctx, path);
	return pfn->bdev ? pfn->bdev : "";
}

NDCTL_EXPORT int ndctl_pfn_is_valid(struct ndctl_pfn *pfn)
{
	struct ndctl_region *region = ndctl_pfn_get_region(pfn);

	return pfn->generation == region->generation;
}

NDCTL_EXPORT int ndctl_pfn_is_enabled(struct ndctl_pfn *pfn)
{
	struct ndctl_ctx *ctx = ndctl_pfn_get_ctx(pfn);
	char *path = pfn->pfn_buf;
	int len = pfn->buf_len;

	if (snprintf(path, len, "%s/driver", pfn->pfn_path) >= len) {
		err(ctx, "%s: buffer too small!\n",
				ndctl_pfn_get_devname(pfn));
		return 0;
	}

	return is_enabled(ndctl_pfn_get_bus(pfn), path);
}

NDCTL_EXPORT struct ndctl_region *ndctl_pfn_get_region(struct ndctl_pfn *pfn)
{
	return pfn->region;
}

NDCTL_EXPORT int ndctl_pfn_enable(struct ndctl_pfn *pfn)
{
	struct ndctl_region *region = ndctl_pfn_get_region(pfn);
	const char *devname = ndctl_pfn_get_devname(pfn);
	struct ndctl_ctx *ctx = ndctl_pfn_get_ctx(pfn);
	char *path = pfn->pfn_buf;
	int len = pfn->buf_len;

	if (ndctl_pfn_is_enabled(pfn))
		return 0;

	ndctl_bind(ctx, pfn->module, devname);

	if (!ndctl_pfn_is_enabled(pfn)) {
		err(ctx, "%s: failed to enable\n", devname);
		return -ENXIO;
	}

	dbg(ctx, "%s: enabled\n", devname);

	if (snprintf(path, len, "%s/block", pfn->pfn_path) >= len) {
		err(ctx, "%s: buffer too small!\n",
				devname);
	} else {
		pfn->bdev = get_block_device(ctx, path);
	}

	/*
	 * Rescan now as successfully enabling a pfn device leads to a
	 * new one being created, and potentially the backing namespace
	 * as well.
	 */
	region_refresh_children(region);

	return 0;
}

NDCTL_EXPORT int ndctl_pfn_delete(struct ndctl_pfn *pfn)
{
	struct ndctl_region *region = ndctl_pfn_get_region(pfn);
	struct ndctl_ctx *ctx = ndctl_pfn_get_ctx(pfn);
	int rc;

	if (!ndctl_pfn_is_valid(pfn)) {
		free_pfn(pfn, &region->stale_pfns);
		return 0;
	}

	ndctl_unbind(ctx, pfn->pfn_path);

	rc = ndctl_pfn_set_namespace(pfn, NULL);
	if (rc) {
		dbg(ctx, "%s: failed to clear namespace: %d\n",
			ndctl_pfn_get_devname(pfn), rc);
		return rc;
	}

	free_pfn(pfn, &region->pfns);
	region->pfns_init = 0;

	return 0;
}

NDCTL_EXPORT int ndctl_pfn_is_configured(struct ndctl_pfn *pfn)
{
	if (ndctl_pfn_get_namespace(pfn))
		return 1;

	if (ndctl_pfn_get_location(pfn) != NDCTL_PFN_LOC_NONE)
		return 1;

	if (memcmp(&pfn->uuid, null_uuid, sizeof(null_uuid)) != 0)
		return 1;

	return 0;
}

NDCTL_EXPORT struct ndctl_dax *ndctl_dax_get_first(struct ndctl_region *region)
{
	daxs_init(region);

	return list_top(&region->daxs, struct ndctl_dax, pfn.list);
}

NDCTL_EXPORT struct ndctl_dax *ndctl_dax_get_next(struct ndctl_dax *dax)
{
	struct ndctl_region *region = dax->pfn.region;

	return list_next(&region->daxs, dax, pfn.list);
}

NDCTL_EXPORT unsigned int ndctl_dax_get_id(struct ndctl_dax *dax)
{
	return ndctl_pfn_get_id(&dax->pfn);
}

NDCTL_EXPORT struct ndctl_namespace *ndctl_dax_get_namespace(struct ndctl_dax *dax)
{
	return ndctl_pfn_get_namespace(&dax->pfn);
}

NDCTL_EXPORT void ndctl_dax_get_uuid(struct ndctl_dax *dax, uuid_t uu)
{
	ndctl_pfn_get_uuid(&dax->pfn, uu);
}

NDCTL_EXPORT unsigned long long ndctl_dax_get_size(struct ndctl_dax *dax)
{
	return ndctl_pfn_get_size(&dax->pfn);
}

NDCTL_EXPORT unsigned long long ndctl_dax_get_resource(struct ndctl_dax *dax)
{
	return ndctl_pfn_get_resource(&dax->pfn);
}

NDCTL_EXPORT int ndctl_dax_set_uuid(struct ndctl_dax *dax, uuid_t uu)
{
	return ndctl_pfn_set_uuid(&dax->pfn, uu);
}

NDCTL_EXPORT enum ndctl_pfn_loc ndctl_dax_get_location(struct ndctl_dax *dax)
{
	return ndctl_pfn_get_location(&dax->pfn);
}

NDCTL_EXPORT int ndctl_dax_set_location(struct ndctl_dax *dax,
		enum ndctl_pfn_loc loc)
{
	return ndctl_pfn_set_location(&dax->pfn, loc);
}

NDCTL_EXPORT unsigned long ndctl_dax_get_align(struct ndctl_dax *dax)
{
	return ndctl_pfn_get_align(&dax->pfn);
}

NDCTL_EXPORT int ndctl_dax_get_num_alignments(struct ndctl_dax *dax)
{
	return ndctl_pfn_get_num_alignments(&dax->pfn);
}

NDCTL_EXPORT unsigned long ndctl_dax_get_supported_alignment(
		struct ndctl_dax *dax, int i)
{
	return ndctl_pfn_get_supported_alignment(&dax->pfn, i);
}

NDCTL_EXPORT int ndctl_dax_has_align(struct ndctl_dax *dax)
{
	return ndctl_pfn_has_align(&dax->pfn);
}

NDCTL_EXPORT int ndctl_dax_set_align(struct ndctl_dax *dax, unsigned long align)
{
	return ndctl_pfn_set_align(&dax->pfn, align);
}

NDCTL_EXPORT int ndctl_dax_set_namespace(struct ndctl_dax *dax,
		struct ndctl_namespace *ndns)
{
	return ndctl_pfn_set_namespace(&dax->pfn, ndns);
}

NDCTL_EXPORT struct ndctl_bus *ndctl_dax_get_bus(struct ndctl_dax *dax)
{
	return ndctl_pfn_get_bus(&dax->pfn);
}

NDCTL_EXPORT struct ndctl_ctx *ndctl_dax_get_ctx(struct ndctl_dax *dax)
{
	return ndctl_pfn_get_ctx(&dax->pfn);
}

NDCTL_EXPORT const char *ndctl_dax_get_devname(struct ndctl_dax *dax)
{
	return ndctl_pfn_get_devname(&dax->pfn);
}

NDCTL_EXPORT int ndctl_dax_is_valid(struct ndctl_dax *dax)
{
	return ndctl_pfn_is_valid(&dax->pfn);
}

NDCTL_EXPORT int ndctl_dax_is_enabled(struct ndctl_dax *dax)
{
	return ndctl_pfn_is_enabled(&dax->pfn);
}

NDCTL_EXPORT struct ndctl_region *ndctl_dax_get_region(struct ndctl_dax *dax)
{
	return ndctl_pfn_get_region(&dax->pfn);
}

NDCTL_EXPORT int ndctl_dax_enable(struct ndctl_dax *dax)
{
	struct ndctl_region *region = ndctl_dax_get_region(dax);
	const char *devname = ndctl_dax_get_devname(dax);
	struct ndctl_ctx *ctx = ndctl_dax_get_ctx(dax);
	struct ndctl_pfn *pfn = &dax->pfn;

	if (ndctl_dax_is_enabled(dax))
		return 0;

	ndctl_bind(ctx, pfn->module, devname);

	if (!ndctl_dax_is_enabled(dax)) {
		err(ctx, "%s: failed to enable\n", devname);
		return -ENXIO;
	}

	dbg(ctx, "%s: enabled\n", devname);

	/*
	 * Rescan now as successfully enabling a dax device leads to a
	 * new one being created, and potentially the backing namespace
	 * as well.
	 */
	region_refresh_children(region);

	return 0;
}

NDCTL_EXPORT int ndctl_dax_delete(struct ndctl_dax *dax)
{
	struct ndctl_region *region = ndctl_dax_get_region(dax);
	struct ndctl_ctx *ctx = ndctl_dax_get_ctx(dax);
	struct ndctl_pfn *pfn = &dax->pfn;
	int rc;

	if (!ndctl_dax_is_valid(dax)) {
		free_dax(dax, &region->stale_daxs);
		return 0;
	}

	ndctl_unbind(ctx, pfn->pfn_path);

	rc = ndctl_dax_set_namespace(dax, NULL);
	if (rc) {
		dbg(ctx, "%s: failed to clear namespace: %d\n",
			ndctl_dax_get_devname(dax), rc);
		return rc;
	}

	free_dax(dax, &region->daxs);
	region->daxs_init = 0;

	return 0;
}

NDCTL_EXPORT int ndctl_dax_is_configured(struct ndctl_dax *dax)
{
	return ndctl_pfn_is_configured(&dax->pfn);
}

NDCTL_EXPORT struct daxctl_region *ndctl_dax_get_daxctl_region(
		struct ndctl_dax *dax)
{
	struct ndctl_ctx *ctx = ndctl_dax_get_ctx(dax);
	struct ndctl_region *region;
	uuid_t uuid;
	int id;

	if (dax->region)
		return dax->region;
	region = ndctl_dax_get_region(dax);

	id = ndctl_region_get_id(region);
	ndctl_dax_get_uuid(dax, uuid);
	dax->region = daxctl_new_region(ctx->daxctl_ctx, id, uuid,
			dax->pfn.pfn_path);

	return dax->region;
}
