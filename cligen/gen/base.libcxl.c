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

static void free_memdev(struct cxl_memdev *memdev, struct list_head *head)
{
	if (head)
		list_del_from(head, &memdev->list);
	kmod_module_unref(memdev->module);
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
	dbg(ctx, "%s: base: \'%s\'\n", __func__, cxlmem_base);

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

CXL_EXPORT size_t cxl_memdev_get_lsa_size(struct cxl_memdev *memdev)
{
	return memdev->lsa_size;
}

CXL_EXPORT int cxl_memdev_is_active(struct cxl_memdev *memdev)
{
	/*
	 * TODO: Currently memdevs are always considered inactive. Once we have
	 * cxl_bus drivers that are bound/unbound to memdevs, we'd use that to
	 * determine the active/inactive state.
	 */
	return 0;
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

	size = sizeof(struct cxl_mem_query_commands) +
			(num_cmds * sizeof(struct cxl_command_info));
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

	if (cinfo->size_out < 0)
		cinfo->size_out = cmd->memdev->payload_max; // -1 will require update

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

	return cmd;

fail:
	cxl_cmd_unref(cmd);
	return NULL;
}

CXL_EXPORT const char *cxl_cmd_get_devname(struct cxl_cmd *cmd)
{
	return cxl_memdev_get_devname(cmd->memdev);
}

#define cmd_get_int(cmd, n, N, field) \
do { \
	struct cxl_cmd_##n *c = (void *)cmd->send_cmd->out.payload; \
	if (cmd->send_cmd->id != CXL_MEM_COMMAND_ID_##N) \
		return EINVAL; \
	if (cmd->status < 0) \
		return cmd->status; \
	return le32_to_cpu(c->field); \
} while(0);

CXL_EXPORT struct cxl_cmd *cxl_cmd_new_get_health_info(
		struct cxl_memdev *memdev)
{
	return cxl_cmd_new_generic(memdev, CXL_MEM_COMMAND_ID_GET_HEALTH_INFO);
}

#define cmd_health_get_int(c, f) \
do { \
	cmd_get_int(c, get_health_info, GET_HEALTH_INFO, f); \
} while (0);

CXL_EXPORT int cxl_cmd_get_health_info_get_health_status(struct cxl_cmd *cmd)
{
	cmd_health_get_int(cmd, health_status);
}

CXL_EXPORT int cxl_cmd_get_health_info_get_media_status(struct cxl_cmd *cmd)
{
	cmd_health_get_int(cmd, media_status);
}

CXL_EXPORT int cxl_cmd_get_health_info_get_ext_status(struct cxl_cmd *cmd)
{
	cmd_health_get_int(cmd, ext_status);
}

CXL_EXPORT int cxl_cmd_get_health_info_get_life_used(struct cxl_cmd *cmd)
{
	cmd_health_get_int(cmd, life_used);
}

CXL_EXPORT int cxl_cmd_get_health_info_get_temperature(struct cxl_cmd *cmd)
{
	cmd_health_get_int(cmd, temperature);
}

CXL_EXPORT int cxl_cmd_get_health_info_get_dirty_shutdowns(struct cxl_cmd *cmd)
{
	cmd_health_get_int(cmd, dirty_shutdowns);
}

CXL_EXPORT int cxl_cmd_get_health_info_get_volatile_errors(struct cxl_cmd *cmd)
{
	cmd_health_get_int(cmd, volatile_errors);
}

CXL_EXPORT int cxl_cmd_get_health_info_get_pmem_errors(struct cxl_cmd *cmd)
{
	cmd_health_get_int(cmd, pmem_errors);
}

CXL_EXPORT struct cxl_cmd *cxl_cmd_new_identify(struct cxl_memdev *memdev)
{
	return cxl_cmd_new_generic(memdev, CXL_MEM_COMMAND_ID_IDENTIFY);
}

CXL_EXPORT int cxl_cmd_identify_get_fw_rev(struct cxl_cmd *cmd, char *fw_rev,
		int fw_len)
{
	struct cxl_cmd_identify *id = (void *)cmd->send_cmd->out.payload;

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
	struct cxl_cmd_identify *id = (void *)cmd->send_cmd->out.payload;

	if (cmd->send_cmd->id != CXL_MEM_COMMAND_ID_IDENTIFY)
		return -EINVAL;
	if (cmd->status < 0)
		return cmd->status;

	return le64_to_cpu(id->partition_align);
}

CXL_EXPORT unsigned int cxl_cmd_identify_get_lsa_size(struct cxl_cmd *cmd)
{
	struct cxl_cmd_identify *id = (void *)cmd->send_cmd->out.payload;

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

CXL_EXPORT struct cxl_cmd *cxl_cmd_new_get_lsa(struct cxl_memdev *memdev,
		unsigned int offset, unsigned int length)
{
	struct cxl_cmd_get_lsa_in *get_lsa;
	struct cxl_cmd *cmd;

	cmd = cxl_cmd_new_generic(memdev, CXL_MEM_COMMAND_ID_GET_LSA);
	if (!cmd)
		return NULL;

	get_lsa = (void *)cmd->send_cmd->in.payload;
	get_lsa->offset = cpu_to_le32(offset);
	get_lsa->length = cpu_to_le32(length);
	return cmd;
}

#define cmd_get_void(cmd, N) \
do { \
	void *p = (void *)cmd->send_cmd->out.payload; \
	if (cmd->send_cmd->id != CXL_MEM_COMMAND_ID_##N) \
		return NULL; \
	if (cmd->status < 0) \
		return NULL; \
	return p; \
} while(0);

CXL_EXPORT void *cxl_cmd_get_lsa_get_payload(struct cxl_cmd *cmd)
{
	cmd_get_void(cmd, GET_LSA);
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
	if (rc < 0)
		err(ctx, "%s: send command failed: %s\n",
			devname, strerror(-rc));
	cmd->status = cmd->send_cmd->retval;
	dbg(ctx, "%s: got SEND cmd: in: %d, out: %d, retval: %d\n", devname,
		cmd->send_cmd->in.size, cmd->send_cmd->out.size, cmd->status);

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

CXL_EXPORT struct cxl_cmd *cxl_cmd_new_set_lsa(struct cxl_memdev *memdev,
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
	set_lsa = (void *)cmd->send_cmd->in.payload;
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

static int lsa_op(struct cxl_memdev *memdev, int op, void **buf,
		size_t length, size_t offset)
{
	const char *devname = cxl_memdev_get_devname(memdev);
	struct cxl_ctx *ctx = cxl_memdev_get_ctx(memdev);
	struct cxl_cmd *cmd;
	void *zero_buf = NULL;
	int rc = 0;

	if (op != LSA_OP_ZERO && (buf == NULL || *buf == NULL)) {
		err(ctx, "%s: LSA buffer cannot be NULL\n", devname);
		return -EINVAL;
	}

	/* TODO: handle the case for offset + len > mailbox payload size */
	switch (op) {
	case LSA_OP_GET:
		if (length == 0)
			length = memdev->lsa_size;
		cmd = cxl_cmd_new_get_lsa(memdev, offset, length);
		if (!cmd)
			return -ENOMEM;
		rc = cxl_cmd_set_output_payload(cmd, *buf, length);
		if (rc) {
			err(ctx, "%s: cmd setup failed: %s\n",
			    cxl_memdev_get_devname(memdev), strerror(-rc));
			goto out;
		}
		break;
	case LSA_OP_ZERO:
		if (length == 0)
			length = memdev->lsa_size;
		zero_buf = calloc(1, length);
		if (!zero_buf)
			return -ENOMEM;
		buf = &zero_buf;
		/* fall through */
	case LSA_OP_SET:
		cmd = cxl_cmd_new_set_lsa(memdev, *buf, offset, length);
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

	if (op == LSA_OP_GET)
		memcpy(*buf, cxl_cmd_get_lsa_get_payload(cmd), length);
	/*
	 * TODO: If writing, the memdev may need to be disabled/re-enabled to
	 * refresh any cached LSA data in the kernel.
	 */

out:
	cxl_cmd_unref(cmd);
out_free:
	free(zero_buf);
	return rc;
}

CXL_EXPORT int cxl_memdev_zero_lsa(struct cxl_memdev *memdev)
{
	return lsa_op(memdev, LSA_OP_ZERO, NULL, 0, 0);
}

CXL_EXPORT int cxl_memdev_set_lsa(struct cxl_memdev *memdev, void *buf,
		size_t length, size_t offset)
{
	return lsa_op(memdev, LSA_OP_SET, &buf, length, offset);
}

CXL_EXPORT int cxl_memdev_get_lsa(struct cxl_memdev *memdev, void *buf,
		size_t length, size_t offset)
{
	return lsa_op(memdev, LSA_OP_GET, &buf, length, offset);
}

CXL_EXPORT int cxl_memdev_cmd_identify(struct cxl_memdev *memdev)
{
	struct cxl_cmd *cmd;
	struct cxl_cmd_identify *id;
	int rc = 0;

	printf("id: 0x%x\n", CXL_MEM_COMMAND_ID_IDENTIFY);
	cmd = cxl_cmd_new_identify(memdev);
	if (!cmd) {
		fprintf(stderr, "%s: cxl_cmd_new_identify returned Null output\n",
				cxl_memdev_get_devname(memdev));
		return -ENOMEM;
	}

	rc = cxl_cmd_submit(cmd);
	if (rc < 0) {
		fprintf(stderr, "%s: cmd submission failed: %d (%s)\n",
				cxl_memdev_get_devname(memdev), rc, strerror(-rc));
		goto out;
	}

	rc = cxl_cmd_get_mbox_status(cmd);
	if (rc != 0) {
		fprintf(stderr, "%s: firmware status: %d\n",
				cxl_memdev_get_devname(memdev), rc);
		rc = -ENXIO;
		goto out;
	}

	id = (void *)cmd->send_cmd->out.payload;
	fprintf(stderr, "size of payload: %ld\n", sizeof(*id));
	if (cmd->send_cmd->id != CXL_MEM_COMMAND_ID_IDENTIFY) {
		fprintf(stderr, "%s: invalid command id 0x%x (expecting 0x%x)\n",
				cxl_memdev_get_devname(memdev), cmd->send_cmd->id, CXL_MEM_COMMAND_ID_IDENTIFY);
		return -EINVAL;
	}

	fprintf(stdout, "%s info\n", cxl_memdev_get_devname(memdev));
	fprintf(stdout, "    fw revision: ");
	for (int i=0; i < CXL_CMD_IDENTIFY_FW_REV_LENGTH; ++i)
		fprintf(stdout, "%02x ", id->fw_revision[i]);
	fprintf(stdout, "\n");
	fprintf(stdout, "    total_capacity: %lu MB (%lu GB)\n",
	le64_to_cpu(id->total_capacity), (le64_to_cpu(id->total_capacity))/4);
	fprintf(stdout, "    volatile_capacity: %lu MB (%lu GB)\n",
	le64_to_cpu(id->volatile_capacity), (le64_to_cpu(id->volatile_capacity))/4);
	fprintf(stdout, "    persistent_capacity: %lu MB (%lu GB)\n",
	le64_to_cpu(id->persistent_capacity), (le64_to_cpu(id->persistent_capacity))/4);
	fprintf(stdout, "    partition_align: %lu MB (%lu GB)\n",
	le64_to_cpu(id->partition_align), (le64_to_cpu(id->partition_align))/4);
	fprintf(stdout, "    info_event_log_size: %d\n", le16_to_cpu(id->info_event_log_size));
	fprintf(stdout, "    warning_event_log_size: %d\n", le16_to_cpu(id->warning_event_log_size));
	fprintf(stdout, "    failure_event_log_size: %d\n", le16_to_cpu(id->failure_event_log_size));
	fprintf(stdout, "    fatal_event_log_size: %d\n", le16_to_cpu(id->fatal_event_log_size));
	fprintf(stdout, "    lsa_size: %d\n", le32_to_cpu(id->lsa_size));
	for (int i=0; i < 3; ++i)
	fprintf(stdout, "    poison_list_max_mer[%d]: %d\n", i, id->poison_list_max_mer[i]);
	fprintf(stdout, "    inject_poison_limit: %d\n", le16_to_cpu(id->inject_poison_limit));
	fprintf(stdout, "    poison_caps: %d\n", id->poison_caps);
	fprintf(stdout, "    qos_telemetry_caps: %d\n", id->qos_telemetry_caps);

out:
	cxl_cmd_unref(cmd);
	return rc;
}

struct cxl_mbox_get_supported_logs {
	__le16 entries;
	u8 rsvd[6];
	struct gsl_entry {
		uuid_t uuid;
		__le32 size;
	} __attribute__((packed)) entry[];
} __attribute__((packed));

CXL_EXPORT int cxl_memdev_get_supported_logs(struct cxl_memdev *memdev)
{
	struct cxl_cmd *cmd;
	struct cxl_mbox_get_supported_logs *gsl;
	int rc = 0;

	cmd = cxl_cmd_new_generic(memdev, CXL_MEM_COMMAND_ID_GET_SUPPORTED_LOGS);
	if (!cmd) {
		fprintf(stderr, "%s: cxl_cmd_new_identify returned Null output\n",
				cxl_memdev_get_devname(memdev));
		return -ENOMEM;
	}

	rc = cxl_cmd_submit(cmd);
	if (rc < 0) {
		fprintf(stderr, "%s: cmd submission failed: %d (%s)\n",
				cxl_memdev_get_devname(memdev), rc, strerror(-rc));
		goto out;
	}

	rc = cxl_cmd_get_mbox_status(cmd);
	if (rc != 0) {
		fprintf(stderr, "%s: firmware status: %d\n",
				cxl_memdev_get_devname(memdev), rc);
		rc = -ENXIO;
		goto out;
	}

	if (cmd->send_cmd->id != CXL_MEM_COMMAND_ID_GET_SUPPORTED_LOGS) {
		fprintf(stderr, "%s: invalid command id 0x%x (expecting 0x%x)\n",
				cxl_memdev_get_devname(memdev),
				cmd->send_cmd->id,
				CXL_MEM_COMMAND_ID_GET_SUPPORTED_LOGS);
		return -EINVAL;
	}

	gsl = (void *)cmd->send_cmd->out.payload;
	fprintf(stdout, "payload info\n");
	fprintf(stdout, "    out size: 0x%x\n", cmd->send_cmd->out.size);
	fprintf(stdout, "    entries: %d\n", gsl->entries);
	for (int e=0; e < gsl->entries; ++e) {
		char uuid[40];
		uuid_unparse(gsl->entry[e].uuid, uuid);
		fprintf(stdout, "        entries[%d] uuid: %s, size: %d\n", e, uuid, gsl->entry[e].size);
	}
out:
	cxl_cmd_unref(cmd);
	return rc;
}

#define CEL_UUID "0da9c0b5-bf41-4b78-8f79-96b1623b3f17"

struct cxl_mbox_get_log {
	uuid_t uuid;
	__le32 offset;
	__le32 length;
}  __attribute__((packed));

struct cel_entry {
	__le16 opcode;
	__le16 effect;
} __attribute__((packed));

CXL_EXPORT int cxl_memdev_get_cel_log(struct cxl_memdev *memdev)
{
	struct cxl_cmd *cmd;
	struct cxl_mbox_get_log *get_log_input;
	struct cel_entry *cel_entries;
	int no_cel_entries;
	int rc = 0;

	cmd = cxl_cmd_new_generic(memdev, CXL_MEM_COMMAND_ID_GET_LOG);
	if (!cmd) {
		fprintf(stderr, "%s: cxl_memdev_get_cel_log returned Null output\n",
				cxl_memdev_get_devname(memdev));
		return -ENOMEM;
	}

	fprintf(stderr, "in size: 0x%x\n", cmd->send_cmd->in.size);
	get_log_input = (void *) cmd->send_cmd->in.payload;
	uuid_parse(CEL_UUID, get_log_input->uuid);
	get_log_input->offset = 0;
	get_log_input->length = cmd->memdev->payload_max;

	rc = cxl_cmd_submit(cmd);
	if (rc < 0) {
		fprintf(stderr, "%s: cmd submission failed: %d (%s)\n",
				cxl_memdev_get_devname(memdev), rc, strerror(-rc));
		goto out;
	}

	rc = cxl_cmd_get_mbox_status(cmd);
	if (rc != 0) {
		fprintf(stderr, "%s: firmware status: %d\n",
				cxl_memdev_get_devname(memdev), rc);
		rc = -ENXIO;
		goto out;
	}

	if (cmd->send_cmd->id != CXL_MEM_COMMAND_ID_GET_LOG) {
		fprintf(stderr, "%s: invalid command id 0x%x (expecting 0x%x)\n",
				cxl_memdev_get_devname(memdev), cmd->send_cmd->id, CXL_MEM_COMMAND_ID_GET_LOG);
		return -EINVAL;
	}

	fprintf(stdout, "payload info\n");
	fprintf(stdout, "    out size: 0x%x\n", cmd->send_cmd->out.size);
	cel_entries = (void *)cmd->send_cmd->out.payload;
	no_cel_entries = (cmd->send_cmd->out.size)/sizeof(struct cel_entry);
	fprintf(stdout, "    no_cel_entries size: %d\n", no_cel_entries);
	for (int e = 0; e < no_cel_entries; ++e) {
		fprintf(stdout, "    cel_entry[%d] opcode: 0x%x, effect: 0x%x\n", e,
				le16_to_cpu(cel_entries[e].opcode),
				le16_to_cpu(cel_entries[e].effect));
	}
out:
	cxl_cmd_unref(cmd);
	return rc;
}

#define CXL_MEM_COMMAND_ID_GET_EVENT_INTERRUPT_POLICY CXL_MEM_COMMAND_ID_RAW
#define CXL_MEM_COMMAND_ID_GET_EVENT_INTERRUPT_POLICY_OPCODE 0x102

struct cxl_mbox_get_event_interrupt_policy {
	u8 info_event_log_int_settings;
	u8 warning_event_log_int_settings;
	u8 failure_event_log_int_settings;
	u8 fatal_event_log_int_settings;
}  __attribute__((packed));

CXL_EXPORT int cxl_memdev_get_event_interrupt_policy(struct cxl_memdev *memdev)
{
	struct cxl_cmd *cmd;
	struct cxl_mbox_get_event_interrupt_policy *event_interrupt_policy_out;
	int rc = 0;

	cmd = cxl_cmd_new_raw(memdev, CXL_MEM_COMMAND_ID_GET_EVENT_INTERRUPT_POLICY_OPCODE);
	if (!cmd) {
		fprintf(stderr, "%s: cxl_cmd_new_raw returned Null output\n",
				cxl_memdev_get_devname(memdev));
		return -ENOMEM;
	}

	rc = cxl_cmd_submit(cmd);
	if (rc < 0) {
		fprintf(stderr, "%s: cmd submission failed: %d (%s)\n",
				cxl_memdev_get_devname(memdev), rc, strerror(-rc));
		goto out;
	}

	rc = cxl_cmd_get_mbox_status(cmd);
	if (rc != 0) {
		fprintf(stderr, "%s: firmware status: %d\n",
				cxl_memdev_get_devname(memdev), rc);
		rc = -ENXIO;
		goto out;
	}

	if (cmd->send_cmd->id != CXL_MEM_COMMAND_ID_GET_EVENT_INTERRUPT_POLICY) {
		fprintf(stderr, "%s: invalid command id 0x%x (expecting 0x%x)\n",
				cxl_memdev_get_devname(memdev), cmd->send_cmd->id, CXL_MEM_COMMAND_ID_GET_EVENT_INTERRUPT_POLICY);
		return -EINVAL;
	}

	fprintf(stdout, "payload info\n");
	fprintf(stdout, "    out size: 0x%x\n", cmd->send_cmd->out.size);
	event_interrupt_policy_out = (void *)cmd->send_cmd->out.payload;
	fprintf(stdout, "    info_event_log_int_settings: 0x%x\n", event_interrupt_policy_out->info_event_log_int_settings);
	fprintf(stdout, "    warning_event_log_int_settings: 0x%x\n", event_interrupt_policy_out->warning_event_log_int_settings);
	fprintf(stdout, "    failure_event_log_int_settings: 0x%x\n", event_interrupt_policy_out->failure_event_log_int_settings);
	fprintf(stdout, "    fatal_event_log_int_settings: 0x%x\n", event_interrupt_policy_out->fatal_event_log_int_settings);
out:
	cxl_cmd_unref(cmd);
	return rc;
}

#define CXL_MEM_COMMAND_ID_SET_EVENT_INTERRUPT_POLICY CXL_MEM_COMMAND_ID_RAW
#define CXL_MEM_COMMAND_ID_SET_EVENT_INTERRUPT_POLICY_OPCODE 0x103
#define CXL_MEM_COMMAND_ID_SET_EVENT_INTERRUPT_POLICY_PAYLOAD_IN_SIZE 0x4

CXL_EXPORT int cxl_memdev_set_event_interrupt_policy(struct cxl_memdev *memdev, u32 int_policy)
{
	struct cxl_cmd *cmd;
	struct cxl_mem_query_commands *query;
    struct cxl_command_info *cinfo;
	struct cxl_mbox_get_event_interrupt_policy *interrupt_policy_in;
	int rc = 0;

	cmd = cxl_cmd_new_raw(memdev, CXL_MEM_COMMAND_ID_SET_EVENT_INTERRUPT_POLICY_OPCODE);
	if (!cmd) {
		fprintf(stderr, "%s: cxl_cmd_new_raw returned Null output\n",
				cxl_memdev_get_devname(memdev));
		return -ENOMEM;
	}

	query = cmd->query_cmd;
	cinfo = &query->commands[cmd->query_idx];

	/* this is hack to create right payload size */
	cinfo->size_in = CXL_MEM_COMMAND_ID_SET_EVENT_INTERRUPT_POLICY_PAYLOAD_IN_SIZE;
	if (cinfo->size_in > 0) {
        cmd->input_payload = calloc(1, cinfo->size_in);
        if (!cmd->input_payload)
            return -ENOMEM;
        cmd->send_cmd->in.payload = (u64)cmd->input_payload;
        cmd->send_cmd->in.size = cinfo->size_in;
    }

	fprintf(stdout, "in size: 0x%x\n", cmd->send_cmd->in.size);
	fprintf(stdout, "    int_policy: 0x%x\n", int_policy);
	interrupt_policy_in = (void *) cmd->send_cmd->in.payload;

	/* below is meant for readability, you don't really need this */
	int_policy = cpu_to_be32(int_policy);
	interrupt_policy_in->info_event_log_int_settings = (int_policy & 0xff);
	interrupt_policy_in->warning_event_log_int_settings = ((int_policy >> 8) & 0xff);
	interrupt_policy_in->failure_event_log_int_settings = ((int_policy >> 16) & 0xff);
	interrupt_policy_in->fatal_event_log_int_settings = ((int_policy >> 24) & 0xff);

	rc = cxl_cmd_submit(cmd);
	if (rc < 0) {
		fprintf(stderr, "%s: cmd submission failed: %d (%s)\n",
				cxl_memdev_get_devname(memdev), rc, strerror(-rc));
		goto out;
	}

	rc = cxl_cmd_get_mbox_status(cmd);
	if (rc != 0) {
		fprintf(stderr, "%s: firmware status: %d\n",
				cxl_memdev_get_devname(memdev), rc);
		rc = -ENXIO;
		goto out;
	}

	if (cmd->send_cmd->id != CXL_MEM_COMMAND_ID_SET_EVENT_INTERRUPT_POLICY) {
		fprintf(stderr, "%s: invalid command id 0x%x (expecting 0x%x)\n",
				cxl_memdev_get_devname(memdev), cmd->send_cmd->id, CXL_MEM_COMMAND_ID_SET_EVENT_INTERRUPT_POLICY);
		return -EINVAL;
	}

	fprintf(stdout, "command completed successfully\n");
out:
	cxl_cmd_unref(cmd);
	return rc;
	return 0;
}

#define CXL_MEM_COMMAND_ID_GET_TIMESTAMP CXL_MEM_COMMAND_ID_RAW
#define CXL_MEM_COMMAND_ID_GET_TIMESTAMP_OPCODE 0x0300

CXL_EXPORT int cxl_memdev_get_timestamp(struct cxl_memdev *memdev)
{
	struct cxl_cmd *cmd;
	__le64 *timestamp_out;
	int rc = 0;

	cmd = cxl_cmd_new_raw(memdev, CXL_MEM_COMMAND_ID_GET_TIMESTAMP_OPCODE);
	if (!cmd) {
		fprintf(stderr, "%s: cxl_cmd_new_raw returned Null output\n",
				cxl_memdev_get_devname(memdev));
		return -ENOMEM;
	}

	rc = cxl_cmd_submit(cmd);
	if (rc < 0) {
		fprintf(stderr, "%s: cmd submission failed: %d (%s)\n",
				cxl_memdev_get_devname(memdev), rc, strerror(-rc));
		goto out;
	}

	rc = cxl_cmd_get_mbox_status(cmd);
	if (rc != 0) {
		fprintf(stderr, "%s: firmware status: %d\n",
				cxl_memdev_get_devname(memdev), rc);
		rc = -ENXIO;
		goto out;
	}

	if (cmd->send_cmd->id != CXL_MEM_COMMAND_ID_GET_TIMESTAMP) {
		fprintf(stderr, "%s: invalid command id 0x%x (expecting 0x%x)\n",
				cxl_memdev_get_devname(memdev), cmd->send_cmd->id, CXL_MEM_COMMAND_ID_GET_TIMESTAMP);
		return -EINVAL;
	}

	timestamp_out = (void *)cmd->send_cmd->out.payload;
	fprintf(stdout, "timestamp: 0x%lx\n", le64_to_cpu(*timestamp_out));
out:
	cxl_cmd_unref(cmd);
	return rc;
}

#define CXL_MEM_COMMAND_ID_SET_TIMESTAMP CXL_MEM_COMMAND_ID_RAW
#define CXL_MEM_COMMAND_ID_SET_TIMESTAMP_OPCODE 0x0301
#define CXL_MEM_COMMAND_ID_SET_TIMESTAMP_PAYLOAD_IN_SIZE 8

CXL_EXPORT int cxl_memdev_set_timestamp(struct cxl_memdev *memdev, u64 timestamp)
{
	struct cxl_cmd *cmd;
	struct cxl_mem_query_commands *query;
    struct cxl_command_info *cinfo;
	__le64 *timestamp_in;
	int rc = 0;

	cmd = cxl_cmd_new_raw(memdev, CXL_MEM_COMMAND_ID_SET_TIMESTAMP_OPCODE);
	if (!cmd) {
		fprintf(stderr, "%s: cxl_cmd_new_raw returned Null output\n",
				cxl_memdev_get_devname(memdev));
		return -ENOMEM;
	}
	query = cmd->query_cmd;
	cinfo = &query->commands[cmd->query_idx];

	/* this is hack to create right payload size */
	cinfo->size_in = CXL_MEM_COMMAND_ID_SET_TIMESTAMP_PAYLOAD_IN_SIZE;
	if (cinfo->size_in > 0) {
        cmd->input_payload = calloc(1, cinfo->size_in);
        if (!cmd->input_payload)
            return -ENOMEM;
        cmd->send_cmd->in.payload = (u64)cmd->input_payload;
        cmd->send_cmd->in.size = cinfo->size_in;
    }

	timestamp_in = (void *) cmd->send_cmd->in.payload;
	*timestamp_in = cpu_to_le64(timestamp);
	fprintf(stdout, "setting timestamp to: 0x%lx\n", le64_to_cpu(*timestamp_in));

	rc = cxl_cmd_submit(cmd);
	if (rc < 0) {
		fprintf(stderr, "%s: cmd submission failed: %d (%s)\n",
				cxl_memdev_get_devname(memdev), rc, strerror(-rc));
		goto out;
	}

	rc = cxl_cmd_get_mbox_status(cmd);
	if (rc != 0) {
		fprintf(stderr, "%s: firmware status: %d\n",
				cxl_memdev_get_devname(memdev), rc);
		rc = -ENXIO;
		goto out;
	}

	if (cmd->send_cmd->id != CXL_MEM_COMMAND_ID_SET_TIMESTAMP) {
		fprintf(stderr, "%s: invalid command id 0x%x (expecting 0x%x)\n",
				cxl_memdev_get_devname(memdev), cmd->send_cmd->id, CXL_MEM_COMMAND_ID_SET_TIMESTAMP);
		return -EINVAL;
	}

	fprintf(stdout, "command completed successfully\n");
out:
	cxl_cmd_unref(cmd);
	return rc;
}

struct cxl_mbox_get_alert_config_out {
	u8 valid_alerts;
	u8 programmable_alerts;
	u8 life_used_critical_alert_threshold;
	u8 life_used_prog_warn_threshold;
	__le16 dev_over_temp_crit_alert_threshold;
	__le16 dev_under_temp_crit_alert_threshold;
	__le16 dev_over_temp_prog_warn_threshold;
	__le16 dev_under_temp_prog_warn_threshold;
	__le16 corr_vol_mem_err_prog_warn_thresold;
	__le16 corr_pers_mem_err_prog_warn_threshold;
}  __attribute__((packed));

CXL_EXPORT int cxl_memdev_get_alert_config(struct cxl_memdev *memdev)
{
	struct cxl_cmd *cmd;
	struct cxl_mbox_get_alert_config_out *alert_config_out;
	int rc = 0;

	cmd = cxl_cmd_new_generic(memdev, CXL_MEM_COMMAND_ID_GET_ALERT_CONFIG);
	if (!cmd) {
		fprintf(stderr, "%s: cxl_cmd_new_raw returned Null output\n",
				cxl_memdev_get_devname(memdev));
		return -ENOMEM;
	}

	rc = cxl_cmd_submit(cmd);
	if (rc < 0) {
		fprintf(stderr, "%s: cmd submission failed: %d (%s)\n",
				cxl_memdev_get_devname(memdev), rc, strerror(-rc));
		goto out;
	}

	rc = cxl_cmd_get_mbox_status(cmd);
	if (rc != 0) {
		fprintf(stderr, "%s: firmware status: %d\n",
				cxl_memdev_get_devname(memdev), rc);
		rc = -ENXIO;
		goto out;
	}

	if (cmd->send_cmd->id != CXL_MEM_COMMAND_ID_GET_ALERT_CONFIG) {
		fprintf(stderr, "%s: invalid command id 0x%x (expecting 0x%x)\n",
				cxl_memdev_get_devname(memdev), cmd->send_cmd->id, CXL_MEM_COMMAND_ID_GET_ALERT_CONFIG);
		return -EINVAL;
	}

	fprintf(stdout, "alert_config summary\n");
	//fprintf(stdout, "    out size: 0x%x\n", cmd->send_cmd->out.size);

	alert_config_out = (void *)cmd->send_cmd->out.payload;

	fprintf(stdout, "    valid_alerts: 0x%x\n", alert_config_out->valid_alerts);
	fprintf(stdout, "    programmable_alerts: 0x%x\n", alert_config_out->programmable_alerts);
	fprintf(stdout, "    life_used_critical_alert_threshold: 0x%x\n",
		alert_config_out->life_used_critical_alert_threshold);
	fprintf(stdout, "    life_used_prog_warn_threshold: 0x%x\n",
		alert_config_out->life_used_prog_warn_threshold);

	fprintf(stdout, "    dev_over_temp_crit_alert_threshold: 0x%x\n",
		le16_to_cpu(alert_config_out->dev_over_temp_crit_alert_threshold));
	fprintf(stdout, "    dev_under_temp_crit_alert_threshold: 0x%x\n",
		le16_to_cpu(alert_config_out->dev_under_temp_crit_alert_threshold));
	fprintf(stdout, "    dev_over_temp_prog_warn_threshold: 0x%x\n",
		le16_to_cpu(alert_config_out->dev_over_temp_prog_warn_threshold));
	fprintf(stdout, "    dev_under_temp_prog_warn_threshold: 0x%x\n",
		le16_to_cpu(alert_config_out->dev_under_temp_prog_warn_threshold));
	fprintf(stdout, "    corr_vol_mem_err_prog_warn_thresold: 0x%x\n",
		le16_to_cpu(alert_config_out->corr_vol_mem_err_prog_warn_thresold));
	fprintf(stdout, "    corr_pers_mem_err_prog_warn_threshold: 0x%x\n",
		le16_to_cpu(alert_config_out->corr_pers_mem_err_prog_warn_threshold));

out:
	cxl_cmd_unref(cmd);
	return rc;
}

struct cxl_mbox_set_alert_config_in {
    u8 valid_alert_actions;
    u8 enable_alert_actions;
    u8 life_used_prog_warn_threshold;
    u8 reserved;
    __le16 dev_over_temp_prog_warn_threshold;
    __le16 dev_under_temp_prog_warn_threshold;
    __le16 corr_vol_mem_err_prog_warn_thresold;
    __le16 corr_pers_mem_err_prog_warn_threshold;
}  __attribute__((packed));


CXL_EXPORT int cxl_memdev_set_alert_config(struct cxl_memdev *memdev, u32 alert_prog_threshold,
	u32 device_temp_threshold, u32 mem_error_threshold)
{
	struct cxl_cmd *cmd;
	struct cxl_mbox_set_alert_config_in *alert_config_in;
	int rc = 0;

	cmd = cxl_cmd_new_generic(memdev, CXL_MEM_COMMAND_ID_SET_ALERT_CONFIG);
	if (!cmd) {
		fprintf(stderr, "%s: cxl_cmd_new_raw returned Null output\n",
				cxl_memdev_get_devname(memdev));
		return -ENOMEM;
	}

	alert_config_in = (void *) cmd->send_cmd->in.payload;

	alert_prog_threshold = cpu_to_be32(alert_prog_threshold);
	device_temp_threshold = cpu_to_be32(device_temp_threshold);
	mem_error_threshold = cpu_to_be32(mem_error_threshold);

	alert_config_in->valid_alert_actions = ((alert_prog_threshold >> 8) & 0xff);
	alert_config_in->enable_alert_actions = ((alert_prog_threshold >> 16) & 0xff);
	alert_config_in->life_used_prog_warn_threshold = ((alert_prog_threshold >> 24) & 0xff);
	alert_config_in->reserved = 0;

	alert_config_in->dev_over_temp_prog_warn_threshold = cpu_to_le16(be16_to_cpu(((device_temp_threshold) & 0xffff)));
	alert_config_in->dev_under_temp_prog_warn_threshold = cpu_to_le16(be16_to_cpu((((device_temp_threshold) >> 16) & 0xffff)));

	alert_config_in->corr_vol_mem_err_prog_warn_thresold = cpu_to_le16(be16_to_cpu((mem_error_threshold & 0xffff)));
	alert_config_in->corr_pers_mem_err_prog_warn_threshold = cpu_to_le16(be16_to_cpu(((mem_error_threshold >> 16) & 0xffff)));

	fprintf(stdout, "alert_config settings\n");
	fprintf(stdout, "    valid_alert_actions: 0x%x\n", alert_config_in->valid_alert_actions);
	fprintf(stdout, "    enable_alert_actions: 0x%x\n", alert_config_in->enable_alert_actions);
	fprintf(stdout, "    life_used_prog_warn_threshold: 0x%x\n", alert_config_in->life_used_prog_warn_threshold);
	fprintf(stdout, "    dev_over_temp_prog_warn_threshold: 0x%x\n",
		le16_to_cpu(alert_config_in->dev_over_temp_prog_warn_threshold));
	fprintf(stdout, "    dev_under_temp_prog_warn_threshold: 0x%x\n",
		le16_to_cpu(alert_config_in->dev_under_temp_prog_warn_threshold));
	fprintf(stdout, "    corr_vol_mem_err_prog_warn_thresold: 0x%x\n",
		le16_to_cpu(alert_config_in->corr_vol_mem_err_prog_warn_thresold));
	fprintf(stdout, "    corr_pers_mem_err_prog_warn_threshold: 0x%x\n",
		le16_to_cpu(alert_config_in->corr_pers_mem_err_prog_warn_threshold));

	rc = cxl_cmd_submit(cmd);
	if (rc < 0) {
		fprintf(stderr, "%s: cmd submission failed: %d (%s)\n",
				cxl_memdev_get_devname(memdev), rc, strerror(-rc));
		goto out;
	}

	rc = cxl_cmd_get_mbox_status(cmd);
	if (rc != 0) {
		fprintf(stderr, "%s: firmware status: %d\n",
				cxl_memdev_get_devname(memdev), rc);
		rc = -ENXIO;
		goto out;
	}

	if (cmd->send_cmd->id != CXL_MEM_COMMAND_ID_SET_ALERT_CONFIG) {
		fprintf(stderr, "%s: invalid command id 0x%x (expecting 0x%x)\n",
				cxl_memdev_get_devname(memdev), cmd->send_cmd->id, CXL_MEM_COMMAND_ID_SET_ALERT_CONFIG);
		return -EINVAL;
	}

	fprintf(stdout, "command completed successfully\n");

out:
	cxl_cmd_unref(cmd);
	return rc;
}

struct cxl_health_info {
    u8 health_state;
    u8 media_status;
    u8 additional_status;
    u8 life_used;
    __le16 device_temp;
    __le32 dirty_shutdown_count;
    __le32 corr_vol_mem_err_count;
    __le32 corr_pers_mem_err_count;
}  __attribute__((packed));

CXL_EXPORT int cxl_memdev_get_health_info(struct cxl_memdev *memdev)
{
	struct cxl_cmd *cmd;
	struct cxl_health_info *health_info;
	int rc = 0;

	cmd = cxl_cmd_new_generic(memdev, CXL_MEM_COMMAND_ID_GET_HEALTH_INFO);
	if (!cmd) {
		fprintf(stderr, "%s: cxl_memdev_get_health_info returned Null output\n",
				cxl_memdev_get_devname(memdev));
		return -ENOMEM;
	}

	rc = cxl_cmd_submit(cmd);
	if (rc < 0) {
		fprintf(stderr, "%s: cmd submission failed: %d (%s)\n",
				cxl_memdev_get_devname(memdev), rc, strerror(-rc));
		goto out;
	}

	rc = cxl_cmd_get_mbox_status(cmd);
	if (rc != 0) {
		fprintf(stderr, "%s: firmware status: %d\n",
				cxl_memdev_get_devname(memdev), rc);
		rc = -ENXIO;
		goto out;
	}

	if (cmd->send_cmd->id != CXL_MEM_COMMAND_ID_GET_HEALTH_INFO) {
		fprintf(stderr, "%s: invalid command id 0x%x (expecting 0x%x)\n",
				cxl_memdev_get_devname(memdev), cmd->send_cmd->id, CXL_MEM_COMMAND_ID_GET_HEALTH_INFO);
		return -EINVAL;
	}

	if (cmd->send_cmd->out.size != sizeof(*health_info)) {
		fprintf(stderr, "%s: invalid payload output size (got: %d, required: %ld)\n",
				cxl_memdev_get_devname(memdev), cmd->send_cmd->out.size, sizeof(*health_info));
		return -EINVAL;
	}

	health_info = (void *)cmd->send_cmd->out.payload;

	fprintf(stdout, "Device Health Info\n");
	fprintf(stdout, "    out size: 0x%x\n", cmd->send_cmd->out.size);
	fprintf(stdout, "    health_state: 0x%x\n", health_info->health_state);
	fprintf(stdout, "    media_status: 0x%x\n", health_info->media_status);
	fprintf(stdout, "    additional_status: 0x%x\n", health_info->additional_status);
	fprintf(stdout, "    life_used: 0x%x\n", health_info->life_used);
	fprintf(stdout, "    device_temp: 0x%x\n", le16_to_cpu(health_info->device_temp));
	fprintf(stdout, "    dirty_shutdown_count: 0x%x\n", le32_to_cpu(health_info->dirty_shutdown_count));
	fprintf(stdout, "    corr_vol_mem_err_count: 0x%x\n", le32_to_cpu(health_info->corr_vol_mem_err_count));
	fprintf(stdout, "    corr_pers_mem_err_count: 0x%x\n", le32_to_cpu(health_info->corr_pers_mem_err_count));
out:
	cxl_cmd_unref(cmd);
	return rc;
}

#define CXL_MEM_COMMAND_ID_GET_EVENT_RECORDS CXL_MEM_COMMAND_ID_RAW
#define CXL_MEM_COMMAND_ID_GET_EVENT_RECORDS_OPCODE 0x100
#define CXL_MEM_COMMAND_ID_GET_EVENT_RECORDS_PAYLOAD_IN_SIZE 0x1
#define CXL_MAX_RECORDS_TO_DUMP 20

#define CXL_DRAM_EVENT_GUID "601dcbb3-9c06-4eab-b8af-4e9bfb5c9624"
#define CXL_MEM_MODULE_EVENT_GUID "fe927475-dd59-4339-a586-79bab113b774"

struct cxl_dram_event_record {
	__le64 physical_addr;
	u8 memory_event_descriptor;
	u8 memory_event_type;
	u8 transaction_type;
	__le16 validity_flags;
	u8 channel;
	u8 rank;
	u8 nibble_mask[3];
	u8 bank_group;
	u8 bank;
	u8 row[3];
	__le16 column;
	u8 correction_mask[0x20];
	u8 reserved[0x17];
} __attribute__((packed));

struct cxl_memory_module_record {
	u8 dev_event_type;
	u8 dev_health_info[0x12];
	u8 reserved[0x3d];
}__attribute__((packed));

struct cxl_event_record {
	uuid_t uuid;
	u8 event_record_length;
	u8 event_record_flags[3];
	__le16 event_record_handle;
	__le16 related_event_record_handle;
	__le64 event_record_ts;
	u8 reserved[0x10];
	union {
		struct cxl_dram_event_record dram_event_record;
		struct cxl_memory_module_record memory_module_record;
	} event_record;
} __attribute__((packed));

struct cxl_get_event_record_info {
    u8 flags;
    u8 reserved1;
    __le16 overflow_err_cnt;
    __le64 first_overflow_evt_ts;
    __le64 last_overflow_evt_ts;
    __le16 event_record_count;
	u8 reserved2[0xa];
	struct cxl_event_record event_records[];
}  __attribute__((packed));

CXL_EXPORT int cxl_memdev_get_event_records(struct cxl_memdev *memdev, u8 event_log_type)
{
	struct cxl_cmd *cmd;
	struct cxl_mem_query_commands *query;
    struct cxl_command_info *cinfo;
	struct cxl_get_event_record_info *event_info;
	int rc = 0;
	int rec;
	int indent = 2;

	cmd = cxl_cmd_new_raw(memdev, CXL_MEM_COMMAND_ID_GET_EVENT_RECORDS_OPCODE);
	if (!cmd) {
		fprintf(stderr, "%s: cxl_cmd_new_raw returned Null output\n",
				cxl_memdev_get_devname(memdev));
		return -ENOMEM;
	}

	query = cmd->query_cmd;
	cinfo = &query->commands[cmd->query_idx];

	/* this is hack to create right payload size */
	cinfo->size_in = CXL_MEM_COMMAND_ID_GET_EVENT_RECORDS_PAYLOAD_IN_SIZE;
	if (cinfo->size_in > 0) {
        cmd->input_payload = calloc(1, cinfo->size_in);
        if (!cmd->input_payload)
            return -ENOMEM;
        cmd->send_cmd->in.payload = (u64)cmd->input_payload;
        cmd->send_cmd->in.size = cinfo->size_in;
    }

	fprintf(stdout, "in size: 0x%x\n", cmd->send_cmd->in.size);
	fprintf(stdout, "Getting Event Records for %d type\n", event_log_type);
	* ((u8 *) cmd->send_cmd->in.payload) = event_log_type;

	rc = cxl_cmd_submit(cmd);
	if (rc < 0) {
		fprintf(stderr, "%s: cmd submission failed: %d (%s)\n",
				cxl_memdev_get_devname(memdev), rc, strerror(-rc));
		goto out;
	}

	rc = cxl_cmd_get_mbox_status(cmd);
	if (rc != 0) {
		fprintf(stderr, "%s: firmware status: %d\n",
				cxl_memdev_get_devname(memdev), rc);
		rc = -ENXIO;
		goto out;
	}

	if (cmd->send_cmd->id != CXL_MEM_COMMAND_ID_GET_EVENT_RECORDS) {
		fprintf(stderr, "%s: invalid command id 0x%x (expecting 0x%x)\n",
				cxl_memdev_get_devname(memdev), cmd->send_cmd->id, CXL_MEM_COMMAND_ID_GET_EVENT_RECORDS);
		return -EINVAL;
	}

	event_info = (void *)cmd->send_cmd->out.payload;

	fprintf(stdout, "cxl_dram_event_record size: 0x%lx\n", sizeof(struct cxl_dram_event_record));
	fprintf(stdout, "cxl_memory_module_record size: 0x%lx\n", sizeof(struct cxl_memory_module_record));
	fprintf(stdout, "cxl_event_record size: 0x%lx\n", sizeof(struct cxl_event_record));
	fprintf(stdout, "cxl_get_event_record_info size: 0x%lx\n", sizeof(struct cxl_get_event_record_info));
	fprintf(stdout, "========= Get Event Records Info =========\n");
	fprintf(stdout, "%*sout size: 0x%x\n", indent, "", cmd->send_cmd->out.size);
	fprintf(stdout, "%*sflags: 0x%x\n", indent, "", event_info->flags);
	fprintf(stdout, "%*soverflow_err_cnt: 0x%x\n", indent, "", le16_to_cpu(event_info->overflow_err_cnt));
	fprintf(stdout, "%*sfirst_overflow_evt_ts: 0x%lx\n", indent, "", le64_to_cpu(event_info->first_overflow_evt_ts));
	fprintf(stdout, "%*slast_overflow_evt_ts: 0x%lx\n", indent, "", le64_to_cpu(event_info->last_overflow_evt_ts));
	fprintf(stdout, "%*sevent_record_count: 0x%x\n", indent, "", le16_to_cpu(event_info->event_record_count));

	for (rec = 0; rec < min(CXL_MAX_RECORDS_TO_DUMP, le16_to_cpu(event_info->event_record_count)); ++rec) {
		char uuid[40];
		struct cxl_event_record *event_record = &event_info->event_records[rec];

		uuid_unparse(event_info->event_records[rec].uuid, uuid);

		if (strcmp(uuid, CXL_DRAM_EVENT_GUID) == 0)
			fprintf(stdout, "%*sEvent Record: %d (DRAM guid: %s)\n", indent, "", rec, uuid);
		else if (strcmp(uuid, CXL_MEM_MODULE_EVENT_GUID) == 0)
			fprintf(stdout, "%*sEvent Record: %d (Memory Module Event guid: %s)\n", indent, "", rec, uuid);
		else
			fprintf(stdout, "%*sEvent Record: %d (uuid: %s)\n", indent, "", rec, uuid);

		fprintf(stdout, "%*sevent_record_length: 0x%x\n", indent+2, "", event_record->event_record_length);
		fprintf(stdout, "%*sevent_record_flags: 0x%02x%02x%02x\n", indent+2, "", event_record->event_record_flags[0],
			event_record->event_record_flags[1], event_record->event_record_flags[2]);
		fprintf(stdout, "%*sevent_record_handle: 0x%x\n", indent+2, "", le16_to_cpu(event_record->event_record_handle));
		fprintf(stdout, "%*srelated_event_record_handle: 0x%x\n", indent+2, "",
			le16_to_cpu(event_record->related_event_record_handle));
		fprintf(stdout, "%*sevent_record_ts: 0x%lx\n", indent+2, "", le64_to_cpu(event_record->event_record_ts));

		if (strcmp(uuid, CXL_DRAM_EVENT_GUID) == 0){
			struct cxl_dram_event_record *dram_event = &event_record->event_record.dram_event_record;
			fprintf(stdout, "%*sphysical_addr: 0x%lx\n", indent+2, "", le64_to_cpu(dram_event->physical_addr));
			fprintf(stdout, "%*smemory_event_descriptor: 0x%x\n", indent+2, "", dram_event->memory_event_descriptor);
			fprintf(stdout, "%*smemory_event_type: 0x%x\n", indent+2, "", dram_event->memory_event_type);
			fprintf(stdout, "%*stransaction_type: 0x%x\n", indent+2, "", dram_event->transaction_type);
			fprintf(stdout, "%*svalidity_flags: 0x%x\n", indent+2, "", le16_to_cpu(dram_event->validity_flags));
			fprintf(stdout, "%*schannel: 0x%x\n", indent+2, "", dram_event->channel);
			fprintf(stdout, "%*srank: 0x%x\n", indent+2, "", dram_event->rank);
			fprintf(stdout, "%*snibble_mask: 0x%02x%02x%02x\n", indent+2, "",
				dram_event->nibble_mask[0], dram_event->nibble_mask[1],
				dram_event->nibble_mask[2]);
			fprintf(stdout, "%*sbank_group: 0x%x\n", indent+2, "", dram_event->bank_group);
			fprintf(stdout, "%*sbank: 0x%x\n", indent+2, "", dram_event->bank);
			fprintf(stdout, "%*srow: 0x%02x%02x%02x\n", indent+2, "", dram_event->row[0],
				dram_event->row[1], dram_event->row[2]);
			fprintf(stdout, "%*scolumn: 0x%x\n", indent+2, "", le16_to_cpu(dram_event->column));
		}
	}

out:
	cxl_cmd_unref(cmd);
	return rc;
	return 0;
}

// GET_LD_INFO START
#define CXL_MEM_COMMAND_ID_GET_LD_INFO CXL_MEM_COMMAND_ID_RAW
#define CXL_MEM_COMMAND_ID_GET_LD_INFO_OPCODE 0x5400
#define CXL_MEM_COMMAND_ID_GET_LD_INFO_PAYLOAD_OUT_SIZE 0xb

struct cxl_get_ld_info {
	__le64 mem_size;
	__le16 ld_cnt;
	u8 qos_telemetry_capa;
}  __attribute__((packed));

CXL_EXPORT int cxl_memdev_get_ld_info(struct cxl_memdev *memdev)
{
	struct cxl_cmd *cmd;
	struct cxl_get_ld_info *ld_info;
	int rc = 0;
	int rec;
	int indent = 2;

	cmd = cxl_cmd_new_raw(memdev, CXL_MEM_COMMAND_ID_GET_LD_INFO_OPCODE);
	if (!cmd) {
		fprintf(stderr, "%s: cxl_cmd_new_raw returned Null output\n",
				cxl_memdev_get_devname(memdev));
		return -ENOMEM;
	}
	cmd->send_cmd->in.size = 0;

	fprintf(stdout, "Getting LD info for memdev %d\n", cxl_memdev_get_devname(memdev));

	rc = cxl_cmd_submit(cmd);
	if (rc < 0) {
		fprintf(stderr, "%s: cmd submission failed: %d (%s)\n",
				cxl_memdev_get_devname(memdev), rc, strerror(-rc));
		goto out;
	}

	rc = cxl_cmd_get_mbox_status(cmd);
	if (rc != 0) {
		fprintf(stderr, "%s: firmware status: %d\n",
				cxl_memdev_get_devname(memdev), rc);
		rc = -ENXIO;
		goto out;
	}

	if (cmd->send_cmd->id != CXL_MEM_COMMAND_ID_GET_LD_INFO) {
		fprintf(stderr, "%s: invalid command id 0x%x (expecting 0x%x)\n",
				cxl_memdev_get_devname(memdev), cmd->send_cmd->id, CXL_MEM_COMMAND_ID_GET_LD_INFO);
		return -EINVAL;
	}

	ld_info = (void *)cmd->send_cmd->out.payload;

	fprintf(stdout, "========= Get LD Info =========\n");
	fprintf(stdout, "%*sout size: 0x%x\n", indent, "", cmd->send_cmd->out.size);
	fprintf(stdout, "%*smemory size: 0x%lu\n", indent, "", ld_info->mem_size);
	fprintf(stdout, "%*sld count: 0x%x\n", indent, "", le16_to_cpu(ld_info->ld_cnt));
	fprintf(stdout, "%*sqos telemetry capability: 0x%x\n", indent, "", ld_info->qos_telemetry_capa);

out:
	cxl_cmd_unref(cmd);
	return rc;
	return 0;
}

// GET_LD_INFO END

#define CXL_MEM_COMMAND_ID_DDR_INFO CXL_MEM_COMMAND_ID_RAW
#define CXL_MEM_COMMAND_ID_DDR_INFO_OPCODE 0xC500
#define CXL_MEM_COMMAND_ID_DDR_INFO_PAYLOAD_IN_SIZE 0x1
#define CXL_MEM_COMMAND_ID_DDR_INFO_PAYLOAD_OUT_SIZE 0x8

struct cxl_ddr_info {
	__le32 mstr_reg;
	__le32 dram_width;
}  __attribute__((packed));

const char* ddr_width[] = {"X4_DEVICE", "X8_DEVICE", "X16_DEVICE", "X32_DEVICE"};

CXL_EXPORT int cxl_memdev_ddr_info(struct cxl_memdev *memdev, u8 ddr_id)
{
	struct cxl_cmd *cmd;
	struct cxl_mem_query_commands *query;
    struct cxl_command_info *cinfo;
	struct cxl_ddr_info *ddr_info;
	int rc = 0;
	int rec;
	int indent = 2;

	cmd = cxl_cmd_new_raw(memdev, CXL_MEM_COMMAND_ID_DDR_INFO_OPCODE);
	if (!cmd) {
		fprintf(stderr, "%s: cxl_cmd_new_raw returned Null output\n",
				cxl_memdev_get_devname(memdev));
		return -ENOMEM;
	}

	query = cmd->query_cmd;
	cinfo = &query->commands[cmd->query_idx];

	/* this is hack to create right payload size */
	cinfo->size_in = CXL_MEM_COMMAND_ID_DDR_INFO_PAYLOAD_IN_SIZE;
	if (cinfo->size_in > 0) {
        cmd->input_payload = calloc(1, cinfo->size_in);
        if (!cmd->input_payload)
            return -ENOMEM;
		cmd->send_cmd->in.payload = (u64)cmd->input_payload;
        cmd->send_cmd->in.size = cinfo->size_in;
    }

	* ((u8 *) cmd->send_cmd->in.payload) = ddr_id;

	fprintf(stdout, "Getting ddr info for memdev %d\n", cxl_memdev_get_devname(memdev));

	rc = cxl_cmd_submit(cmd);
	if (rc < 0) {
		fprintf(stderr, "%s: cmd submission failed: %d (%s)\n",
				cxl_memdev_get_devname(memdev), rc, strerror(-rc));
		goto out;
	}

	rc = cxl_cmd_get_mbox_status(cmd);
	if (rc != 0) {
		fprintf(stderr, "%s: firmware status: %d\n",
				cxl_memdev_get_devname(memdev), rc);
		rc = -ENXIO;
		goto out;
	}

	if (cmd->send_cmd->id != CXL_MEM_COMMAND_ID_DDR_INFO) {
		fprintf(stderr, "%s: invalid command id 0x%x (expecting 0x%x)\n",
				cxl_memdev_get_devname(memdev), cmd->send_cmd->id, CXL_MEM_COMMAND_ID_DDR_INFO);
		return -EINVAL;
	}

	ddr_info = (void *)cmd->send_cmd->out.payload;

	fprintf(stdout, "========= DDR Info =========\n");
	fprintf(stdout, "%*sddr controller mstr register: 0x%d\n", indent, "", ddr_info->mstr_reg);
	fprintf(stdout, "%*sdram width derived from device config: 0x%s\n", indent, "", ddr_width[le32_to_cpu(ddr_info->dram_width)]);

out:
	cxl_cmd_unref(cmd);
	return rc;
	return 0;
}

#define CXL_MEM_COMMAND_ID_CLEAR_EVENT_RECORDS CXL_MEM_COMMAND_ID_RAW
#define CXL_MEM_COMMAND_ID_CLEAR_EVENT_RECORDS_OPCODE 0x101

struct cxl_clear_event_record_info {
    u8 event_log_type;
    u8 clear_event_flags;
    u8 no_event_record_handles;
	u8 reserved[3];
	__le16 event_record_handles[];
}  __attribute__((packed));

CXL_EXPORT int cxl_memdev_clear_event_records(struct cxl_memdev *memdev, u8 event_log_type,
	u8 clear_event_flags, u8 no_event_record_handles, u16 *event_record_handles)
{
	struct cxl_cmd *cmd;
	struct cxl_mem_query_commands *query;
    struct cxl_command_info *cinfo;
	struct cxl_clear_event_record_info *event_info;
	int rc = 0;
	int rec;

	cmd = cxl_cmd_new_raw(memdev, CXL_MEM_COMMAND_ID_CLEAR_EVENT_RECORDS_OPCODE);
	if (!cmd) {
		fprintf(stderr, "%s: cxl_cmd_new_raw returned Null output\n",
				cxl_memdev_get_devname(memdev));
		return -ENOMEM;
	}

	query = cmd->query_cmd;
	cinfo = &query->commands[cmd->query_idx];

	/* this is hack to create right payload size */
	cinfo->size_in = sizeof(*event_info) + (no_event_record_handles * sizeof(__le16));
	if (cinfo->size_in > 0) {
        cmd->input_payload = calloc(1, cinfo->size_in);
        if (!cmd->input_payload)
            return -ENOMEM;
        cmd->send_cmd->in.payload = (u64)cmd->input_payload;
        cmd->send_cmd->in.size = cinfo->size_in;
    }

	fprintf(stdout, "in size: 0x%x\n", cmd->send_cmd->in.size);
	if (clear_event_flags)
		fprintf(stdout, "Clearing 'All Event' Records for type %d\n", event_log_type);

	event_info = (struct cxl_clear_event_record_info *) cmd->send_cmd->in.payload;
	event_info->event_log_type = event_log_type;
	event_info->clear_event_flags = clear_event_flags;
	event_info->no_event_record_handles = no_event_record_handles;
	for (rec = 0; rec < event_info->no_event_record_handles; ++rec) {
		fprintf(stdout, "Clearing Event Record 0x%x for %d type\n", event_record_handles[rec], event_log_type);
		event_info->event_record_handles[rec] = cpu_to_le16(event_record_handles[rec]);
	}

	rc = cxl_cmd_submit(cmd);
	if (rc < 0) {
		fprintf(stderr, "%s: cmd submission failed: %d (%s)\n",
				cxl_memdev_get_devname(memdev), rc, strerror(-rc));
		goto out;
	}

	rc = cxl_cmd_get_mbox_status(cmd);
	if (rc != 0) {
		fprintf(stderr, "%s: firmware status: %d\n",
				cxl_memdev_get_devname(memdev), rc);
		rc = -ENXIO;
		goto out;
	}

	if (cmd->send_cmd->id != CXL_MEM_COMMAND_ID_CLEAR_EVENT_RECORDS) {
		fprintf(stderr, "%s: invalid command id 0x%x (expecting 0x%x)\n",
				cxl_memdev_get_devname(memdev), cmd->send_cmd->id, CXL_MEM_COMMAND_ID_CLEAR_EVENT_RECORDS);
		return -EINVAL;
	}

	fprintf(stdout, "Clear Event Records command completed successfully\n");

out:
	cxl_cmd_unref(cmd);
	return rc;
	return 0;
}

/* insert here */
