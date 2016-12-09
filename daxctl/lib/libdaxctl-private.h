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
#ifndef _LIBDAXCTL_PRIVATE_H_
#define _LIBDAXCTL_PRIVATE_H_

#define DAXCTL_EXPORT __attribute__ ((visibility("default")))

/**
 * struct daxctl_region - container for dax_devices
 */
#define REGION_BUF_SIZE 50
struct daxctl_region {
	int id;
	uuid_t uuid;
	int refcount;
	char *devname;
	size_t buf_len;
	void *region_buf;
	int devices_init;
	char *region_path;
	unsigned int align;
	unsigned long long size;
	struct daxctl_ctx *ctx;
	struct list_head devices;
};

struct daxctl_dev {
	int id, major, minor;
	void *dev_buf;
	size_t buf_len;
	char *dev_path;
	struct list_node list;
	unsigned long long size;
	struct daxctl_region *region;
};
#endif /* _LIBDAXCTL_PRIVATE_H_ */
