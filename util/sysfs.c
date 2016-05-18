/*
 * Copyright (c) 2014-2016, Intel Corporation.
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

#include <util/log.h>
#include <util/sysfs.h>

int __sysfs_read_attr(struct log_ctx *ctx, const char *path, char *buf)
{
	int fd = open(path, O_RDONLY|O_CLOEXEC);
	int n;

	if (fd < 0) {
		log_dbg(ctx, "failed to open %s: %s\n", path, strerror(errno));
		return -1;
	}
	n = read(fd, buf, SYSFS_ATTR_SIZE);
	close(fd);
	if (n < 0 || n >= SYSFS_ATTR_SIZE) {
		log_dbg(ctx, "failed to read %s: %s\n", path, strerror(errno));
		return -1;
	}
	buf[n] = 0;
	if (n && buf[n-1] == '\n')
		buf[n-1] = 0;
	return 0;
}

static int write_attr(struct log_ctx *ctx, const char *path,
		const char *buf, int quiet)
{
	int fd = open(path, O_WRONLY|O_CLOEXEC);
	int n, len = strlen(buf) + 1;

	if (fd < 0) {
		log_dbg(ctx, "failed to open %s: %s\n", path, strerror(errno));
		return -1;
	}
	n = write(fd, buf, len);
	close(fd);
	if (n < len) {
		if (!quiet)
			log_dbg(ctx, "failed to write %s to %s: %s\n", buf, path,
					strerror(errno));
		return -1;
	}
	return 0;
}

int __sysfs_write_attr(struct log_ctx *ctx, const char *path,
		const char *buf)
{
	return write_attr(ctx, path, buf, 0);
}

int __sysfs_write_attr_quiet(struct log_ctx *ctx, const char *path,
		const char *buf)
{
	return write_attr(ctx, path, buf, 1);
}

int __sysfs_device_parse(struct log_ctx *ctx, const char *base_path,
		const char *dev_name, void *parent, add_dev_fn add_dev)
{
	int add_errors = 0;
	struct dirent *de;
	DIR *dir;

	log_dbg(ctx, "base: %s dev: %s\n", base_path, dev_name);
	dir = opendir(base_path);
	if (!dir) {
		log_dbg(ctx, "no \"%s\" devices found\n", dev_name);
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
			log_err(ctx, "%s%d: path allocation failure\n",
					dev_name, id);
			continue;
		}

		rc = add_dev(parent, id, dev_path);
		free(dev_path);
		if (rc < 0) {
			add_errors++;
			log_err(ctx, "%s%d: add_dev() failed: %d\n",
					dev_name, id, rc);
		} else if (rc == 0) {
			log_dbg(ctx, "%s%d: added\n", dev_name, id);
		} else
			log_dbg(ctx, "%s%d: duplicate\n", dev_name, id);
	}
	closedir(dir);

	return add_errors;
}
