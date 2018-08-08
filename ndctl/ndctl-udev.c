/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright(c) 2018 Intel Corporation. All rights reserved. */

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <ndctl/libndctl.h>

/**
 * mkdir_p
 *
 * Copied from util-linux lib/fileutils.c
 */
static int mkdir_p(const char *path, mode_t mode)
{
	char *p, *dir;
	int rc = 0;

	if (!path || !*path)
		return -EINVAL;

	dir = p = strdup(path);
	if (!dir)
		return -ENOMEM;

	if (*p == '/')
		p++;

	while (p && *p) {
		char *e = strchr(p, '/');
		if (e)
			*e = '\0';
		if (*p) {
			rc = mkdir(dir, mode);
			if (rc && errno != EEXIST)
				break;
			rc = 0;
		}
		if (!e)
			break;
		*e = '/';
		p = e + 1;
	}

	free(dir);
	return rc;
}

static struct ndctl_dimm *find_dimm(struct ndctl_ctx *ctx, const char *devname)
{
	struct ndctl_bus *bus;
	struct ndctl_dimm *dimm;

	ndctl_bus_foreach(ctx, bus) {
		ndctl_dimm_foreach(bus, dimm) {
			if (strcmp(ndctl_dimm_get_devname(dimm), devname) == 0)
				return dimm;
		}
	}
	return NULL;
}

static void ack_shutdown(struct ndctl_dimm *dimm)
{
	struct ndctl_cmd *cmd;

	cmd = ndctl_dimm_cmd_new_ack_shutdown_count(dimm);
	if (!cmd)
		return;
	ndctl_cmd_submit(cmd);
	ndctl_cmd_unref(cmd);
}

static void save_unsafe_shutdown_count(struct ndctl_dimm *dimm,
				       const char *devname)
{
	char *path, *usc, count[16];
	unsigned int shutdown;
	struct ndctl_cmd *cmd;
	int fd;

	cmd = ndctl_dimm_cmd_new_smart(dimm);
	if (!cmd)
		return;

	if (ndctl_cmd_submit(cmd))
		goto unref_cmd;

	shutdown = ndctl_cmd_smart_get_shutdown_count(cmd);
	if (shutdown == UINT_MAX)
		goto unref_cmd;

	if (asprintf(&path, DEF_TMPFS_DIR "/%s", devname) < 0)
		goto unref_cmd;

	if (mkdir_p(path, 0755))
		goto free_path;

	if (asprintf(&usc, "%s/usc", path) < 0)
		goto free_path;

	fd = open(usc, O_WRONLY | O_CREAT, 0644);
	if (fd < 0)
		goto free_usc;

	if (snprintf(count, sizeof(count), "%u\n", shutdown) < 0)
		goto free_usc;

	if (write(fd, count, strlen(count)) < 0)
		goto free_usc;
 free_usc:
	free(usc);
 free_path:
	free(path);
 unref_cmd:
	ndctl_cmd_unref(cmd);
}

int main(int argc, char *argv[])
{
	struct ndctl_ctx *ctx;
	struct ndctl_dimm *dimm = NULL;
	const char *devname;

	if (argc < 2)
		return EINVAL;

	devname = argv[1];
	if (ndctl_new(&ctx))
		return ENOMEM;

	dimm = find_dimm(ctx, devname);
	if (!dimm)
		return ENODEV;

	ack_shutdown(dimm);
	save_unsafe_shutdown_count(dimm, devname);

	ndctl_unref(ctx);
	return 0;
}
