// SPDX-License-Identifier: LGPL-2.1
// Copyright (C) 2020-2021, Intel Corporation. All rights reserved.
#include <stdio.h>
#include <errno.h>
#include <limits.h>
#include <libgen.h>
#include <stdlib.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/sysmacros.h>
#include <uuid/uuid.h>
#include <ccan/list/list.h>
#include <ccan/endian/endian.h>
#include <ccan/minmax/minmax.h>
#include <ccan/array_size/array_size.h>
#include <ccan/short_types/short_types.h>

#include <util/log.h>
#include <util/size.h>
#include <util/sysfs.h>
#include <util/bitmap.h>
#include <cxl/cxl_mem.h>
#include <cxl/libcxl.h>
#include "private.h"

/**
 * struct cxl_ctx - library user context to find "nd" instances
 *
 * Instantiate with cxl_new(), which takes an initial reference.  Free
 * the context by dropping the reference count to zero with
 * cxl_unref(), or take additional references with cxl_ref()
 * @timeout: default library timeout in milliseconds
 */
struct cxl_ctx {
	/* log_ctx must be first member for cxl_set_log_fn compat */
	struct log_ctx ctx;
	int refcount;
	void *userdata;
	int memdevs_init;
	struct list_head memdevs;
	struct kmod_ctx *kmod_ctx;
	void *private_data;
};

static void free_pmem(struct cxl_pmem *pmem)
{
	free(pmem->dev_buf);
	free(pmem->dev_path);
	free(pmem);
}

static void free_memdev(struct cxl_memdev *memdev, struct list_head *head)
{
	if (head)
		list_del_from(head, &memdev->list);
	kmod_module_unref(memdev->module);
	free_pmem(memdev->pmem);
	free(memdev->firmware_version);
	free(memdev->dev_buf);
	free(memdev->dev_path);
	free(memdev);
}

/**
 * cxl_get_userdata - retrieve stored data pointer from library context
 * @ctx: cxl library context
 *
 * This might be useful to access from callbacks like a custom logging
 * function.
 */
CXL_EXPORT void *cxl_get_userdata(struct cxl_ctx *ctx)
{
	if (ctx == NULL)
		return NULL;
	return ctx->userdata;
}

/**
 * cxl_set_userdata - store custom @userdata in the library context
 * @ctx: cxl library context
 * @userdata: data pointer
 */
CXL_EXPORT void cxl_set_userdata(struct cxl_ctx *ctx, void *userdata)
{
	if (ctx == NULL)
		return;
	ctx->userdata = userdata;
}

CXL_EXPORT void cxl_set_private_data(struct cxl_ctx *ctx, void *data)
{
	ctx->private_data = data;
}

CXL_EXPORT void *cxl_get_private_data(struct cxl_ctx *ctx)
{
	return ctx->private_data;
}

/**
 * cxl_new - instantiate a new library context
 * @ctx: context to establish
 *
 * Returns zero on success and stores an opaque pointer in ctx.  The
 * context is freed by cxl_unref(), i.e. cxl_new() implies an
 * internal cxl_ref().
 */
CXL_EXPORT int cxl_new(struct cxl_ctx **ctx)
{
	struct kmod_ctx *kmod_ctx;
	struct cxl_ctx *c;
	int rc = 0;

	c = calloc(1, sizeof(struct cxl_ctx));
	if (!c)
		return -ENOMEM;

	kmod_ctx = kmod_new(NULL, NULL);
	if (check_kmod(kmod_ctx) != 0) {
		rc = -ENXIO;
		goto out;
	}

	c->refcount = 1;
	log_init(&c->ctx, "libcxl", "CXL_LOG");
	info(c, "ctx %p created\n", c);
	dbg(c, "log_priority=%d\n", c->ctx.log_priority);
	*ctx = c;
	list_head_init(&c->memdevs);
	c->kmod_ctx = kmod_ctx;

	return 0;
out:
	free(c);
	return rc;
}

/**
 * cxl_ref - take an additional reference on the context
 * @ctx: context established by cxl_new()
 */
CXL_EXPORT struct cxl_ctx *cxl_ref(struct cxl_ctx *ctx)
{
	if (ctx == NULL)
		return NULL;
	ctx->refcount++;
	return ctx;
}

/**
 * cxl_unref - drop a context reference count
 * @ctx: context established by cxl_new()
 *
 * Drop a reference and if the resulting reference count is 0 destroy
 * the context.
 */
CXL_EXPORT void cxl_unref(struct cxl_ctx *ctx)
{
	struct cxl_memdev *memdev, *_d;

	if (ctx == NULL)
		return;
	ctx->refcount--;
	if (ctx->refcount > 0)
		return;

	list_for_each_safe(&ctx->memdevs, memdev, _d, list)
		free_memdev(memdev, &ctx->memdevs);

	kmod_unref(ctx->kmod_ctx);
	info(ctx, "context %p released\n", ctx);
	free(ctx);
}

/**
 * cxl_set_log_fn - override default log routine
 * @ctx: cxl library context
 * @log_fn: function to be called for logging messages
 *
 * The built-in logging writes to stderr. It can be overridden by a
 * custom function, to plug log messages into the user's logging
 * functionality.
 */
CXL_EXPORT void cxl_set_log_fn(struct cxl_ctx *ctx,
		void (*cxl_log_fn)(struct cxl_ctx *ctx, int priority,
			const char *file, int line, const char *fn,
			const char *format, va_list args))
{
	ctx->ctx.log_fn = (log_fn) cxl_log_fn;
	info(ctx, "custom logging function %p registered\n", cxl_log_fn);
}

/**
 * cxl_get_log_priority - retrieve current library loglevel (syslog)
 * @ctx: cxl library context
 */
CXL_EXPORT int cxl_get_log_priority(struct cxl_ctx *ctx)
{
	return ctx->ctx.log_priority;
}

/**
 * cxl_set_log_priority - set log verbosity
 * @priority: from syslog.h, LOG_ERR, LOG_INFO, LOG_DEBUG
 *
 * Note: LOG_DEBUG requires library be built with "configure --enable-debug"
 */
CXL_EXPORT void cxl_set_log_priority(struct cxl_ctx *ctx, int priority)
{
	ctx->ctx.log_priority = priority;
}

static void *add_cxl_pmem(void *parent, int id, const char *br_base)
{
	const char *devname = devpath_to_devname(br_base);
	struct cxl_memdev *memdev = parent;
	struct cxl_ctx *ctx = memdev->ctx;
	struct cxl_pmem *pmem;

	dbg(ctx, "%s: pmem_base: \'%s\'\n", devname, br_base);

	pmem = calloc(1, sizeof(*pmem));
	if (!pmem)
		goto err_dev;
	pmem->id = id;

	pmem->dev_path = strdup(br_base);
	if (!pmem->dev_path)
		goto err_read;

	pmem->dev_buf = calloc(1, strlen(br_base) + 50);
	if (!pmem->dev_buf)
		goto err_read;
	pmem->buf_len = strlen(br_base) + 50;

	memdev->pmem = pmem;
	return pmem;

 err_read:
	free(pmem->dev_buf);
	free(pmem->dev_path);
	free(pmem);
 err_dev:
	return NULL;
}

static void *add_cxl_memdev(void *parent, int id, const char *cxlmem_base)
{
	const char *devname = devpath_to_devname(cxlmem_base);
	char *path = calloc(1, strlen(cxlmem_base) + 100);
	struct cxl_ctx *ctx = parent;
	struct cxl_memdev *memdev, *memdev_dup;
	char buf[SYSFS_ATTR_SIZE];
	struct stat st;

	if (!path)
		return NULL;
	dbg(ctx, "%s: base: \'%s\'\n", devname, cxlmem_base);

	memdev = calloc(1, sizeof(*memdev));
	if (!memdev)
		goto err_dev;
	memdev->id = id;
	memdev->ctx = ctx;

	sprintf(path, "/dev/cxl/%s", devname);
	if (stat(path, &st) < 0)
		goto err_read;
	memdev->major = major(st.st_rdev);
	memdev->minor = minor(st.st_rdev);

	sprintf(path, "%s/pmem/size", cxlmem_base);
	if (sysfs_read_attr(ctx, path, buf) < 0)
		goto err_read;
	memdev->pmem_size = strtoull(buf, NULL, 0);

	sprintf(path, "%s/ram/size", cxlmem_base);
	if (sysfs_read_attr(ctx, path, buf) < 0)
		goto err_read;
	memdev->ram_size = strtoull(buf, NULL, 0);

	sprintf(path, "%s/payload_max", cxlmem_base);
	if (sysfs_read_attr(ctx, path, buf) < 0)
		goto err_read;
	memdev->payload_max = strtoull(buf, NULL, 0);
	if (memdev->payload_max < 0)
		goto err_read;

	sprintf(path, "%s/label_storage_size", cxlmem_base);
	if (sysfs_read_attr(ctx, path, buf) < 0)
		goto err_read;
	memdev->lsa_size = strtoull(buf, NULL, 0);
	if (memdev->lsa_size == ULLONG_MAX)
		goto err_read;

	sprintf(path, "%s/serial", cxlmem_base);
	if (sysfs_read_attr(ctx, path, buf) < 0)
		memdev->serial = ULLONG_MAX;
	else
		memdev->serial = strtoull(buf, NULL, 0);

	memdev->dev_path = strdup(cxlmem_base);
	if (!memdev->dev_path)
		goto err_read;

	sprintf(path, "%s/firmware_version", cxlmem_base);
	if (sysfs_read_attr(ctx, path, buf) < 0)
		goto err_read;

	memdev->firmware_version = strdup(buf);
	if (!memdev->firmware_version)
		goto err_read;

	memdev->dev_buf = calloc(1, strlen(cxlmem_base) + 50);
	if (!memdev->dev_buf)
		goto err_read;
	memdev->buf_len = strlen(cxlmem_base) + 50;

	sysfs_device_parse(ctx, cxlmem_base, "pmem", memdev, add_cxl_pmem);

	cxl_memdev_foreach(ctx, memdev_dup)
		if (memdev_dup->id == memdev->id) {
			free_memdev(memdev, NULL);
			free(path);
			return memdev_dup;
		}

	list_add(&ctx->memdevs, &memdev->list);
	free(path);
	return memdev;

 err_read:
	free(memdev->firmware_version);
	free(memdev->dev_buf);
	free(memdev->dev_path);
	free(memdev);
 err_dev:
	free(path);
	return NULL;
}

static void cxl_memdevs_init(struct cxl_ctx *ctx)
{
	if (ctx->memdevs_init)
		return;

	ctx->memdevs_init = 1;

	sysfs_device_parse(ctx, "/sys/bus/cxl/devices", "mem", ctx,
			   add_cxl_memdev);
}

CXL_EXPORT struct cxl_ctx *cxl_memdev_get_ctx(struct cxl_memdev *memdev)
{
	return memdev->ctx;
}

CXL_EXPORT struct cxl_memdev *cxl_memdev_get_first(struct cxl_ctx *ctx)
{
	cxl_memdevs_init(ctx);

	return list_top(&ctx->memdevs, struct cxl_memdev, list);
}

CXL_EXPORT struct cxl_memdev *cxl_memdev_get_next(struct cxl_memdev *memdev)
{
	struct cxl_ctx *ctx = memdev->ctx;

	return list_next(&ctx->memdevs, memdev, list);
}

CXL_EXPORT int cxl_memdev_get_id(struct cxl_memdev *memdev)
{
	return memdev->id;
}

CXL_EXPORT unsigned long long cxl_memdev_get_serial(struct cxl_memdev *memdev)
{
	return memdev->serial;
}

CXL_EXPORT const char *cxl_memdev_get_devname(struct cxl_memdev *memdev)
{
	return devpath_to_devname(memdev->dev_path);
}

CXL_EXPORT int cxl_memdev_get_major(struct cxl_memdev *memdev)
{
	return memdev->major;
}

CXL_EXPORT int cxl_memdev_get_minor(struct cxl_memdev *memdev)
{
	return memdev->minor;
}

CXL_EXPORT unsigned long long cxl_memdev_get_pmem_size(struct cxl_memdev *memdev)
{
	return memdev->pmem_size;
}

CXL_EXPORT unsigned long long cxl_memdev_get_ram_size(struct cxl_memdev *memdev)
{
	return memdev->ram_size;
}

CXL_EXPORT const char *cxl_memdev_get_firmware_verison(struct cxl_memdev *memdev)
{
	return memdev->firmware_version;
}

CXL_EXPORT size_t cxl_memdev_get_label_size(struct cxl_memdev *memdev)
{
	return memdev->lsa_size;
}

static int is_enabled(const char *drvpath)
{
	struct stat st;

	if (lstat(drvpath, &st) < 0 || !S_ISLNK(st.st_mode))
		return 0;
	else
		return 1;
}

CXL_EXPORT int cxl_memdev_nvdimm_bridge_active(struct cxl_memdev *memdev)
{
	struct cxl_ctx *ctx = cxl_memdev_get_ctx(memdev);
	struct cxl_pmem *pmem = memdev->pmem;
	char *path;
	int len;

	if (!pmem)
		return 0;

	path = pmem->dev_buf;
	len = pmem->buf_len;

	if (snprintf(path, len, "%s/driver", pmem->dev_path) >= len) {
		err(ctx, "%s: nvdimm pmem buffer too small!\n",
				cxl_memdev_get_devname(memdev));
		return 0;
	}

	return is_enabled(path);
}

CXL_EXPORT void cxl_cmd_unref(struct cxl_cmd *cmd)
{
	if (!cmd)
		return;
	if (--cmd->refcount == 0) {
		free(cmd->query_cmd);
		free(cmd->send_cmd);
		free(cmd->input_payload);
		free(cmd->output_payload);
		free(cmd);
	}
}

CXL_EXPORT void cxl_cmd_ref(struct cxl_cmd *cmd)
{
	cmd->refcount++;
}

static int cxl_cmd_alloc_query(struct cxl_cmd *cmd, int num_cmds)
{
	size_t size;

	if (!cmd)
		return -EINVAL;

	if (cmd->query_cmd != NULL)
		free(cmd->query_cmd);

	size = struct_size(cmd->query_cmd, commands, num_cmds);
	if (size == SIZE_MAX)
		return -EOVERFLOW;

	cmd->query_cmd = calloc(1, size);
	if (!cmd->query_cmd)
		return -ENOMEM;

	cmd->query_cmd->n_commands = num_cmds;

	return 0;
}

static struct cxl_cmd *cxl_cmd_new(struct cxl_memdev *memdev)
{
	struct cxl_cmd *cmd;
	size_t size;

	size = sizeof(*cmd);
	cmd = calloc(1, size);
	if (!cmd)
		return NULL;

	cxl_cmd_ref(cmd);
	cmd->memdev = memdev;

	return cmd;
}

static int __do_cmd(struct cxl_cmd *cmd, int ioctl_cmd, int fd)
{
	void *cmd_buf;
	int rc;

	switch (ioctl_cmd) {
	case CXL_MEM_QUERY_COMMANDS:
		cmd_buf = cmd->query_cmd;
		break;
	case CXL_MEM_SEND_COMMAND:
		cmd_buf = cmd->send_cmd;
		break;
	default:
		return -EINVAL;
	}

	rc = ioctl(fd, ioctl_cmd, cmd_buf);
	if (rc < 0)
		rc = -errno;

	return rc;
}

static int do_cmd(struct cxl_cmd *cmd, int ioctl_cmd)
{
	char *path;
	struct stat st;
	unsigned int major, minor;
	int rc = 0, fd;
	struct cxl_memdev *memdev = cmd->memdev;
	struct cxl_ctx *ctx = cxl_memdev_get_ctx(memdev);
	const char *devname = cxl_memdev_get_devname(memdev);

	major = cxl_memdev_get_major(memdev);
	minor = cxl_memdev_get_minor(memdev);

	if (asprintf(&path, "/dev/cxl/%s", devname) < 0)
		return -ENOMEM;

	fd = open(path, O_RDWR);
	if (fd < 0) {
		err(ctx, "failed to open %s: %s\n", path, strerror(errno));
		rc = -errno;
		goto out;
	}

	if (fstat(fd, &st) >= 0 && S_ISCHR(st.st_mode)
			&& major(st.st_rdev) == major
			&& minor(st.st_rdev) == minor) {
		rc = __do_cmd(cmd, ioctl_cmd, fd);
	} else {
		err(ctx, "failed to validate %s as a CXL memdev node\n", path);
		rc = -ENXIO;
	}
	close(fd);
out:
	free(path);
	return rc;
}

static int alloc_do_query(struct cxl_cmd *cmd, int num_cmds)
{
	struct cxl_ctx *ctx = cxl_memdev_get_ctx(cmd->memdev);
	int rc;

	rc = cxl_cmd_alloc_query(cmd, num_cmds);
	if (rc)
		return rc;

	rc = do_cmd(cmd, CXL_MEM_QUERY_COMMANDS);
	if (rc < 0)
		err(ctx, "%s: query commands failed: %s\n",
			cxl_memdev_get_devname(cmd->memdev),
			strerror(-rc));
	return rc;
}

static int cxl_cmd_do_query(struct cxl_cmd *cmd)
{
	struct cxl_memdev *memdev = cmd->memdev;
	struct cxl_ctx *ctx = cxl_memdev_get_ctx(memdev);
	const char *devname = cxl_memdev_get_devname(memdev);
	int rc, n_commands;

	switch (cmd->query_status) {
	case CXL_CMD_QUERY_OK:
		return 0;
	case CXL_CMD_QUERY_UNSUPPORTED:
		return -EOPNOTSUPP;
	case CXL_CMD_QUERY_NOT_RUN:
		break;
	default:
		err(ctx, "%s: Unknown query_status %d\n",
			devname, cmd->query_status);
		return -EINVAL;
	}

	rc = alloc_do_query(cmd, 0);
	if (rc)
		return rc;

	n_commands = cmd->query_cmd->n_commands;
	dbg(ctx, "%s: supports %d commands\n", devname, n_commands);

	return alloc_do_query(cmd, n_commands);
}

static int cxl_cmd_validate(struct cxl_cmd *cmd, u32 cmd_id)
{
	struct cxl_memdev *memdev = cmd->memdev;
	struct cxl_mem_query_commands *query = cmd->query_cmd;
	const char *devname = cxl_memdev_get_devname(memdev);
	struct cxl_ctx *ctx = cxl_memdev_get_ctx(memdev);
	u32 i;

	for (i = 0; i < query->n_commands; i++) {
		struct cxl_command_info *cinfo = &query->commands[i];
		const char *cmd_name = cxl_command_names[cinfo->id].name;

		if (cinfo->id != cmd_id)
			continue;

		dbg(ctx, "%s: %s: in: %d, out %d, flags: %#08x\n",
			devname, cmd_name, cinfo->size_in,
			cinfo->size_out, cinfo->flags);

		cmd->query_idx = i;
		cmd->query_status = CXL_CMD_QUERY_OK;
		return 0;
	}
	cmd->query_status = CXL_CMD_QUERY_UNSUPPORTED;
	return -EOPNOTSUPP;
}

CXL_EXPORT int cxl_cmd_set_input_payload(struct cxl_cmd *cmd, void *buf,
		int size)
{
	struct cxl_memdev *memdev = cmd->memdev;

	if (size > memdev->payload_max || size < 0)
		return -EINVAL;

	if (!buf) {

		/* If the user didn't supply a buffer, allocate it */
		cmd->input_payload = calloc(1, size);
		if (!cmd->input_payload)
			return -ENOMEM;
		cmd->send_cmd->in.payload = (u64)cmd->input_payload;
	} else {
		/*
		 * Use user-buffer as is. If an automatic allocation was
		 * previously made (based on a fixed size from query),
		 * it will get freed during unref.
		 */
		cmd->send_cmd->in.payload = (u64)buf;
	}
	cmd->send_cmd->in.size = size;

	return 0;
}

CXL_EXPORT int cxl_cmd_set_output_payload(struct cxl_cmd *cmd, void *buf,
		int size)
{
	struct cxl_memdev *memdev = cmd->memdev;

	if (size > memdev->payload_max || size < 0)
		return -EINVAL;

	if (!buf) {

		/* If the user didn't supply a buffer, allocate it */
		cmd->output_payload = calloc(1, size);
		if (!cmd->output_payload)
			return -ENOMEM;
		cmd->send_cmd->out.payload = (u64)cmd->output_payload;
	} else {
		/*
		 * Use user-buffer as is. If an automatic allocation was
		 * previously made (based on a fixed size from query),
		 * it will get freed during unref.
		 */
		cmd->send_cmd->out.payload = (u64)buf;
	}
	cmd->send_cmd->out.size = size;

	return 0;
}

static int cxl_cmd_alloc_send(struct cxl_cmd *cmd, u32 cmd_id)
{
	struct cxl_mem_query_commands *query = cmd->query_cmd;
	struct cxl_command_info *cinfo = &query->commands[cmd->query_idx];
	size_t size;

	if (!query)
		return -EINVAL;

	size = sizeof(struct cxl_send_command);
	cmd->send_cmd = calloc(1, size);
	if (!cmd->send_cmd)
		return -ENOMEM;

	if (cinfo->id != cmd_id)
		return -EINVAL;

	cmd->send_cmd->id = cmd_id;

	if (cinfo->size_in > 0) {
		cmd->input_payload = calloc(1, cinfo->size_in);
		if (!cmd->input_payload)
			return -ENOMEM;
		cmd->send_cmd->in.payload = (u64)cmd->input_payload;
		cmd->send_cmd->in.size = cinfo->size_in;
	}
	if (cinfo->size_out > 0) {
		cmd->output_payload = calloc(1, cinfo->size_out);
		if (!cmd->output_payload)
			return -ENOMEM;
		cmd->send_cmd->out.payload = (u64)cmd->output_payload;
		cmd->send_cmd->out.size = cinfo->size_out;
	}

	return 0;
}

static struct cxl_cmd *cxl_cmd_new_generic(struct cxl_memdev *memdev,
		u32 cmd_id)
{
	const char *devname = cxl_memdev_get_devname(memdev);
	struct cxl_ctx *ctx = cxl_memdev_get_ctx(memdev);
	struct cxl_cmd *cmd;
	int rc;

	cmd = cxl_cmd_new(memdev);
	if (!cmd)
		return NULL;

	rc = cxl_cmd_do_query(cmd);
	if (rc) {
		err(ctx, "%s: query returned: %s\n", devname, strerror(-rc));
		goto fail;
	}

	rc = cxl_cmd_validate(cmd, cmd_id);
	if (rc) {
		errno = -rc;
		goto fail;
	}

	rc = cxl_cmd_alloc_send(cmd, cmd_id);
	if (rc) {
		errno = -rc;
		goto fail;
	}

	cmd->status = 1;
	return cmd;

fail:
	cxl_cmd_unref(cmd);
	return NULL;
}

CXL_EXPORT const char *cxl_cmd_get_devname(struct cxl_cmd *cmd)
{
	return cxl_memdev_get_devname(cmd->memdev);
}

static int cxl_cmd_validate_status(struct cxl_cmd *cmd, u32 id)
{
	if (cmd->send_cmd->id != id)
		return -EINVAL;
	if (cmd->status < 0)
		return cmd->status;
	return 0;
}

/* Helpers for health_info fields (no endian conversion) */
#define cmd_get_field_u8(cmd, n, N, field)				\
do {									\
	struct cxl_cmd_##n *c =						\
		(struct cxl_cmd_##n *)cmd->send_cmd->out.payload;	\
	int rc = cxl_cmd_validate_status(cmd, CXL_MEM_COMMAND_ID_##N);	\
	if (rc)								\
		return rc;						\
	return c->field;						\
} while(0)

#define cmd_get_field_u16(cmd, n, N, field)				\
do {									\
	struct cxl_cmd_##n *c =						\
		(struct cxl_cmd_##n *)cmd->send_cmd->out.payload;	\
	int rc = cxl_cmd_validate_status(cmd, CXL_MEM_COMMAND_ID_##N);	\
	if (rc)								\
		return rc;						\
	return le16_to_cpu(c->field);					\
} while(0)


#define cmd_get_field_u32(cmd, n, N, field)				\
do {									\
	struct cxl_cmd_##n *c =						\
		(struct cxl_cmd_##n *)cmd->send_cmd->out.payload;	\
	int rc = cxl_cmd_validate_status(cmd, CXL_MEM_COMMAND_ID_##N);	\
	if (rc)								\
		return rc;						\
	return le32_to_cpu(c->field);					\
} while(0)


#define cmd_get_field_u8_mask(cmd, n, N, field, mask)			\
do {									\
	struct cxl_cmd_##n *c =						\
		(struct cxl_cmd_##n *)cmd->send_cmd->out.payload;	\
	int rc = cxl_cmd_validate_status(cmd, CXL_MEM_COMMAND_ID_##N);	\
	if (rc)								\
		return rc;						\
	return !!(c->field & mask);					\
} while(0)

CXL_EXPORT struct cxl_cmd *cxl_cmd_new_get_health_info(
		struct cxl_memdev *memdev)
{
	return cxl_cmd_new_generic(memdev, CXL_MEM_COMMAND_ID_GET_HEALTH_INFO);
}

#define cmd_health_get_status_field(c, m)					\
	cmd_get_field_u8_mask(c, get_health_info, GET_HEALTH_INFO, health_status, m)

CXL_EXPORT int cxl_cmd_health_info_get_maintenance_needed(struct cxl_cmd *cmd)
{
	cmd_health_get_status_field(cmd,
		CXL_CMD_HEALTH_INFO_STATUS_MAINTENANCE_NEEDED_MASK);
}

CXL_EXPORT int cxl_cmd_health_info_get_performance_degraded(struct cxl_cmd *cmd)
{
	cmd_health_get_status_field(cmd,
		CXL_CMD_HEALTH_INFO_STATUS_PERFORMANCE_DEGRADED_MASK);
}

CXL_EXPORT int cxl_cmd_health_info_get_hw_replacement_needed(struct cxl_cmd *cmd)
{
	cmd_health_get_status_field(cmd,
		CXL_CMD_HEALTH_INFO_STATUS_HW_REPLACEMENT_NEEDED_MASK);
}

#define cmd_health_check_media_field(cmd, f)					\
do {										\
	struct cxl_cmd_get_health_info *c =					\
		(struct cxl_cmd_get_health_info *)cmd->send_cmd->out.payload;	\
	int rc = cxl_cmd_validate_status(cmd,					\
			CXL_MEM_COMMAND_ID_GET_HEALTH_INFO);			\
	if (rc)									\
		return rc;							\
	return (c->media_status == f);						\
} while(0)

CXL_EXPORT int cxl_cmd_health_info_get_media_normal(struct cxl_cmd *cmd)
{
	cmd_health_check_media_field(cmd,
		CXL_CMD_HEALTH_INFO_MEDIA_STATUS_NORMAL);
}

CXL_EXPORT int cxl_cmd_health_info_get_media_not_ready(struct cxl_cmd *cmd)
{
	cmd_health_check_media_field(cmd,
		CXL_CMD_HEALTH_INFO_MEDIA_STATUS_NOT_READY);
}

CXL_EXPORT int
cxl_cmd_health_info_get_media_persistence_lost(struct cxl_cmd *cmd)
{
	cmd_health_check_media_field(cmd,
		CXL_CMD_HEALTH_INFO_MEDIA_STATUS_PERSISTENCE_LOST);
}

CXL_EXPORT int cxl_cmd_health_info_get_media_data_lost(struct cxl_cmd *cmd)
{
	cmd_health_check_media_field(cmd,
		CXL_CMD_HEALTH_INFO_MEDIA_STATUS_DATA_LOST);
}

CXL_EXPORT int
cxl_cmd_health_info_get_media_powerloss_persistence_loss(struct cxl_cmd *cmd)
{
	cmd_health_check_media_field(cmd,
		CXL_CMD_HEALTH_INFO_MEDIA_STATUS_POWERLOSS_PERSISTENCE_LOSS);
}

CXL_EXPORT int
cxl_cmd_health_info_get_media_shutdown_persistence_loss(struct cxl_cmd *cmd)
{
	cmd_health_check_media_field(cmd,
		CXL_CMD_HEALTH_INFO_MEDIA_STATUS_SHUTDOWN_PERSISTENCE_LOSS);
}

CXL_EXPORT int
cxl_cmd_health_info_get_media_persistence_loss_imminent(struct cxl_cmd *cmd)
{
	cmd_health_check_media_field(cmd,
		CXL_CMD_HEALTH_INFO_MEDIA_STATUS_PERSISTENCE_LOSS_IMMINENT);
}

CXL_EXPORT int
cxl_cmd_health_info_get_media_powerloss_data_loss(struct cxl_cmd *cmd)
{
	cmd_health_check_media_field(cmd,
		CXL_CMD_HEALTH_INFO_MEDIA_STATUS_POWERLOSS_DATA_LOSS);
}

CXL_EXPORT int
cxl_cmd_health_info_get_media_shutdown_data_loss(struct cxl_cmd *cmd)
{
	cmd_health_check_media_field(cmd,
		CXL_CMD_HEALTH_INFO_MEDIA_STATUS_SHUTDOWN_DATA_LOSS);
}

CXL_EXPORT int
cxl_cmd_health_info_get_media_data_loss_imminent(struct cxl_cmd *cmd)
{
	cmd_health_check_media_field(cmd,
		CXL_CMD_HEALTH_INFO_MEDIA_STATUS_DATA_LOSS_IMMINENT);
}

#define cmd_health_check_ext_field(cmd, fname, type)				\
do {										\
	struct cxl_cmd_get_health_info *c =					\
		(struct cxl_cmd_get_health_info *)cmd->send_cmd->out.payload;	\
	int rc = cxl_cmd_validate_status(cmd,					\
			CXL_MEM_COMMAND_ID_GET_HEALTH_INFO);			\
	if (rc)									\
		return rc;							\
	return (FIELD_GET(fname##_MASK, c->ext_status) ==			\
		fname##_##type);						\
} while(0)

CXL_EXPORT int
cxl_cmd_health_info_get_ext_life_used_normal(struct cxl_cmd *cmd)
{
	cmd_health_check_ext_field(cmd,
		CXL_CMD_HEALTH_INFO_EXT_LIFE_USED, NORMAL);
}

CXL_EXPORT int
cxl_cmd_health_info_get_ext_life_used_warning(struct cxl_cmd *cmd)
{
	cmd_health_check_ext_field(cmd,
		CXL_CMD_HEALTH_INFO_EXT_LIFE_USED, WARNING);
}

CXL_EXPORT int
cxl_cmd_health_info_get_ext_life_used_critical(struct cxl_cmd *cmd)
{
	cmd_health_check_ext_field(cmd,
		CXL_CMD_HEALTH_INFO_EXT_LIFE_USED, CRITICAL);
}

CXL_EXPORT int
cxl_cmd_health_info_get_ext_temperature_normal(struct cxl_cmd *cmd)
{
	cmd_health_check_ext_field(cmd,
		CXL_CMD_HEALTH_INFO_EXT_TEMPERATURE, NORMAL);
}

CXL_EXPORT int
cxl_cmd_health_info_get_ext_temperature_warning(struct cxl_cmd *cmd)
{
	cmd_health_check_ext_field(cmd,
		CXL_CMD_HEALTH_INFO_EXT_TEMPERATURE, WARNING);
}

CXL_EXPORT int
cxl_cmd_health_info_get_ext_temperature_critical(struct cxl_cmd *cmd)
{
	cmd_health_check_ext_field(cmd,
		CXL_CMD_HEALTH_INFO_EXT_TEMPERATURE, CRITICAL);
}

CXL_EXPORT int
cxl_cmd_health_info_get_ext_corrected_volatile_normal(struct cxl_cmd *cmd)
{
	cmd_health_check_ext_field(cmd,
		CXL_CMD_HEALTH_INFO_EXT_CORRECTED_VOLATILE, NORMAL);
}

CXL_EXPORT int
cxl_cmd_health_info_get_ext_corrected_volatile_warning(struct cxl_cmd *cmd)
{
	cmd_health_check_ext_field(cmd,
		CXL_CMD_HEALTH_INFO_EXT_CORRECTED_VOLATILE, WARNING);
}

CXL_EXPORT int
cxl_cmd_health_info_get_ext_corrected_persistent_normal(struct cxl_cmd *cmd)
{
	cmd_health_check_ext_field(cmd,
		CXL_CMD_HEALTH_INFO_EXT_CORRECTED_PERSISTENT, NORMAL);
}

CXL_EXPORT int
cxl_cmd_health_info_get_ext_corrected_persistent_warning(struct cxl_cmd *cmd)
{
	cmd_health_check_ext_field(cmd,
		CXL_CMD_HEALTH_INFO_EXT_CORRECTED_PERSISTENT, WARNING);
}

static int health_info_get_life_used_raw(struct cxl_cmd *cmd)
{
	cmd_get_field_u8(cmd, get_health_info, GET_HEALTH_INFO,
				life_used);
}

CXL_EXPORT int cxl_cmd_health_info_get_life_used(struct cxl_cmd *cmd)
{
	int rc = health_info_get_life_used_raw(cmd);

	if (rc < 0)
		return rc;
	if (rc == CXL_CMD_HEALTH_INFO_LIFE_USED_NOT_IMPL)
		return -EOPNOTSUPP;
	return rc;
}

static int health_info_get_temperature_raw(struct cxl_cmd *cmd)
{
	cmd_get_field_u16(cmd, get_health_info, GET_HEALTH_INFO,
				 temperature);
}

CXL_EXPORT int cxl_cmd_health_info_get_temperature(struct cxl_cmd *cmd)
{
	int rc = health_info_get_temperature_raw(cmd);

	if (rc < 0)
		return rc;
	if (rc == CXL_CMD_HEALTH_INFO_TEMPERATURE_NOT_IMPL)
		return -EOPNOTSUPP;
	return rc;
}

CXL_EXPORT int cxl_cmd_health_info_get_dirty_shutdowns(struct cxl_cmd *cmd)
{
	cmd_get_field_u32(cmd, get_health_info, GET_HEALTH_INFO,
				 dirty_shutdowns);
}

CXL_EXPORT int cxl_cmd_health_info_get_volatile_errors(struct cxl_cmd *cmd)
{
	cmd_get_field_u32(cmd, get_health_info, GET_HEALTH_INFO,
				 volatile_errors);
}

CXL_EXPORT int cxl_cmd_health_info_get_pmem_errors(struct cxl_cmd *cmd)
{
	cmd_get_field_u32(cmd, get_health_info, GET_HEALTH_INFO,
				 pmem_errors);
}

CXL_EXPORT struct cxl_cmd *cxl_cmd_new_identify(struct cxl_memdev *memdev)
{
	return cxl_cmd_new_generic(memdev, CXL_MEM_COMMAND_ID_IDENTIFY);
}

CXL_EXPORT int cxl_cmd_identify_get_fw_rev(struct cxl_cmd *cmd, char *fw_rev,
		int fw_len)
{
	struct cxl_cmd_identify *id =
			(struct cxl_cmd_identify *)cmd->send_cmd->out.payload;

	if (cmd->send_cmd->id != CXL_MEM_COMMAND_ID_IDENTIFY)
		return -EINVAL;
	if (cmd->status < 0)
		return cmd->status;

	if (fw_len > 0)
		memcpy(fw_rev, id->fw_revision,
			min(fw_len, CXL_CMD_IDENTIFY_FW_REV_LENGTH));
	return 0;
}

CXL_EXPORT unsigned long long cxl_cmd_identify_get_partition_align(
		struct cxl_cmd *cmd)
{
	struct cxl_cmd_identify *id =
			(struct cxl_cmd_identify *)cmd->send_cmd->out.payload;

	if (cmd->send_cmd->id != CXL_MEM_COMMAND_ID_IDENTIFY)
		return -EINVAL;
	if (cmd->status < 0)
		return cmd->status;

	return le64_to_cpu(id->partition_align);
}

CXL_EXPORT unsigned int cxl_cmd_identify_get_label_size(struct cxl_cmd *cmd)
{
	struct cxl_cmd_identify *id =
			(struct cxl_cmd_identify *)cmd->send_cmd->out.payload;

	if (cmd->send_cmd->id != CXL_MEM_COMMAND_ID_IDENTIFY)
		return -EINVAL;
	if (cmd->status < 0)
		return cmd->status;

	return le32_to_cpu(id->lsa_size);
}

CXL_EXPORT struct cxl_cmd *cxl_cmd_new_raw(struct cxl_memdev *memdev,
		int opcode)
{
	struct cxl_cmd *cmd;

	/* opcode '0' is reserved */
	if (opcode <= 0) {
		errno = EINVAL;
		return NULL;
	}

	cmd = cxl_cmd_new_generic(memdev, CXL_MEM_COMMAND_ID_RAW);
	if (!cmd)
		return NULL;

	cmd->send_cmd->raw.opcode = opcode;
	return cmd;
}

CXL_EXPORT struct cxl_cmd *cxl_cmd_new_read_label(struct cxl_memdev *memdev,
		unsigned int offset, unsigned int length)
{
	struct cxl_cmd_get_lsa_in *get_lsa;
	struct cxl_cmd *cmd;

	cmd = cxl_cmd_new_generic(memdev, CXL_MEM_COMMAND_ID_GET_LSA);
	if (!cmd)
		return NULL;

	get_lsa = (struct cxl_cmd_get_lsa_in *)cmd->send_cmd->in.payload;
	get_lsa->offset = cpu_to_le32(offset);
	get_lsa->length = cpu_to_le32(length);
	return cmd;
}

CXL_EXPORT ssize_t cxl_cmd_read_label_get_payload(struct cxl_cmd *cmd,
		void *buf, unsigned int length)
{
	struct cxl_cmd_get_lsa_in *get_lsa;
	void *payload;
	int rc;

	rc = cxl_cmd_validate_status(cmd, CXL_MEM_COMMAND_ID_GET_LSA);
	if (rc)
		return rc;

	get_lsa = (struct cxl_cmd_get_lsa_in *)cmd->send_cmd->in.payload;
	if (length > le32_to_cpu(get_lsa->length))
		return -EINVAL;

	payload = (void *)cmd->send_cmd->out.payload;
	memcpy(buf, payload, length);
	return length;
}

CXL_EXPORT int cxl_cmd_submit(struct cxl_cmd *cmd)
{
	struct cxl_memdev *memdev = cmd->memdev;
	const char *devname = cxl_memdev_get_devname(memdev);
	struct cxl_ctx *ctx = cxl_memdev_get_ctx(memdev);
	int rc;

	switch (cmd->query_status) {
	case CXL_CMD_QUERY_OK:
		break;
	case CXL_CMD_QUERY_UNSUPPORTED:
		return -EOPNOTSUPP;
	case CXL_CMD_QUERY_NOT_RUN:
		return -EINVAL;
	default:
		err(ctx, "%s: Unknown query_status %d\n",
			devname, cmd->query_status);
		return -EINVAL;
	}

	dbg(ctx, "%s: submitting SEND cmd: in: %d, out: %d\n", devname,
		cmd->send_cmd->in.size, cmd->send_cmd->out.size);
	rc = do_cmd(cmd, CXL_MEM_SEND_COMMAND);
	cmd->status = cmd->send_cmd->retval;
	dbg(ctx, "%s: got SEND cmd: in: %d, out: %d, retval: %d, status: %d\n",
		devname, cmd->send_cmd->in.size, cmd->send_cmd->out.size,
		rc, cmd->status);

	return rc;
}

CXL_EXPORT int cxl_cmd_get_mbox_status(struct cxl_cmd *cmd)
{
	return cmd->status;
}

CXL_EXPORT int cxl_cmd_get_out_size(struct cxl_cmd *cmd)
{
	return cmd->send_cmd->out.size;
}

CXL_EXPORT struct cxl_cmd *cxl_cmd_new_write_label(struct cxl_memdev *memdev,
		void *lsa_buf, unsigned int offset, unsigned int length)
{
	struct cxl_ctx *ctx = cxl_memdev_get_ctx(memdev);
	struct cxl_cmd_set_lsa *set_lsa;
	struct cxl_cmd *cmd;
	int rc;

	cmd = cxl_cmd_new_generic(memdev, CXL_MEM_COMMAND_ID_SET_LSA);
	if (!cmd)
		return NULL;

	/* this will allocate 'in.payload' */
	rc = cxl_cmd_set_input_payload(cmd, NULL, sizeof(*set_lsa) + length);
	if (rc) {
		err(ctx, "%s: cmd setup failed: %s\n",
			cxl_memdev_get_devname(memdev), strerror(-rc));
		goto out_fail;
	}
	set_lsa = (struct cxl_cmd_set_lsa *)cmd->send_cmd->in.payload;
	set_lsa->offset = cpu_to_le32(offset);
	memcpy(set_lsa->lsa_data, lsa_buf, length);

	return cmd;

out_fail:
	cxl_cmd_unref(cmd);
	return NULL;
}

enum lsa_op {
	LSA_OP_GET,
	LSA_OP_SET,
	LSA_OP_ZERO,
};

static int __lsa_op(struct cxl_memdev *memdev, int op, void *buf,
		size_t length, size_t offset)
{
	const char *devname = cxl_memdev_get_devname(memdev);
	struct cxl_ctx *ctx = cxl_memdev_get_ctx(memdev);
	void *zero_buf = NULL;
	struct cxl_cmd *cmd;
	ssize_t ret_len;
	int rc = 0;

	switch (op) {
	case LSA_OP_GET:
		cmd = cxl_cmd_new_read_label(memdev, offset, length);
		if (!cmd)
			return -ENOMEM;
		rc = cxl_cmd_set_output_payload(cmd, buf, length);
		if (rc) {
			err(ctx, "%s: cmd setup failed: %s\n",
			    cxl_memdev_get_devname(memdev), strerror(-rc));
			goto out;
		}
		break;
	case LSA_OP_ZERO:
		zero_buf = calloc(1, length);
		if (!zero_buf)
			return -ENOMEM;
		buf = zero_buf;
		/* fall through */
	case LSA_OP_SET:
		cmd = cxl_cmd_new_write_label(memdev, buf, offset, length);
		if (!cmd) {
			rc = -ENOMEM;
			goto out_free;
		}
		break;
	default:
		return -EOPNOTSUPP;
	}

	rc = cxl_cmd_submit(cmd);
	if (rc < 0) {
		err(ctx, "%s: cmd submission failed: %s\n",
			devname, strerror(-rc));
		goto out;
	}

	rc = cxl_cmd_get_mbox_status(cmd);
	if (rc != 0) {
		err(ctx, "%s: firmware status: %d\n",
			devname, rc);
		rc = -ENXIO;
		goto out;
	}

	if (op == LSA_OP_GET) {
		ret_len = cxl_cmd_read_label_get_payload(cmd, buf, length);
		if (ret_len < 0) {
			rc = ret_len;
			goto out;
		}
	}

out:
	cxl_cmd_unref(cmd);
out_free:
	free(zero_buf);
	return rc;

}

static int lsa_op(struct cxl_memdev *memdev, int op, void *buf,
		size_t length, size_t offset)
{
	const char *devname = cxl_memdev_get_devname(memdev);
	struct cxl_ctx *ctx = cxl_memdev_get_ctx(memdev);
	size_t remaining = length, cur_len, cur_off = 0;
	int label_iter_max, rc = 0;

	if (op != LSA_OP_ZERO && buf == NULL) {
		err(ctx, "%s: LSA buffer cannot be NULL\n", devname);
		return -EINVAL;
	}

	if (length == 0)
		return 0;

	label_iter_max = memdev->payload_max - sizeof(struct cxl_cmd_set_lsa);
	while (remaining) {
		cur_len = min((size_t)label_iter_max, remaining);
		rc = __lsa_op(memdev, op, buf + cur_off,
				cur_len, offset + cur_off);
		if (rc)
			break;

		remaining -= cur_len;
		cur_off += cur_len;
	}

	if (rc && (op == LSA_OP_SET))
		err(ctx, "%s: labels may be in an inconsistent state\n",
			devname);
	return rc;
}

CXL_EXPORT int cxl_memdev_zero_label(struct cxl_memdev *memdev, size_t length,
		size_t offset)
{
	return lsa_op(memdev, LSA_OP_ZERO, NULL, length, offset);
}

CXL_EXPORT int cxl_memdev_write_label(struct cxl_memdev *memdev, void *buf,
		size_t length, size_t offset)
{
	return lsa_op(memdev, LSA_OP_SET, buf, length, offset);
}

CXL_EXPORT int cxl_memdev_read_label(struct cxl_memdev *memdev, void *buf,
		size_t length, size_t offset)
{
	return lsa_op(memdev, LSA_OP_GET, buf, length, offset);
}
