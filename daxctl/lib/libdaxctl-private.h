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

#include <libkmod.h>

#define DAXCTL_EXPORT __attribute__ ((visibility("default")))

enum dax_subsystem {
	DAX_UNKNOWN,
	DAX_CLASS,
	DAX_BUS,
};

static const char *dax_subsystems[] = {
	[DAX_CLASS] = "/sys/class/dax",
	[DAX_BUS] = "/sys/bus/dax/devices",
};

enum daxctl_dev_mode {
	DAXCTL_DEV_MODE_DEVDAX = 0,
	DAXCTL_DEV_MODE_RAM,
	DAXCTL_DEV_MODE_END,
};

static const char *dax_modules[] = {
	[DAXCTL_DEV_MODE_DEVDAX] = "device_dax",
	[DAXCTL_DEV_MODE_RAM] = "kmem",
};

enum memory_op {
	MEM_SET_OFFLINE,
	MEM_SET_ONLINE,
	MEM_IS_ONLINE,
	MEM_COUNT,
};

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
	unsigned long align;
	unsigned long long size;
	struct daxctl_ctx *ctx;
	struct list_node list;
	struct list_head devices;
};

struct daxctl_dev {
	int id, major, minor;
	void *dev_buf;
	size_t buf_len;
	char *dev_path;
	struct list_node list;
	unsigned long long resource;
	unsigned long long size;
	struct kmod_module *module;
	struct kmod_list *kmod_list;
	struct daxctl_region *region;
	struct daxctl_memory *mem;
	int target_node;
};

struct daxctl_memory {
	struct daxctl_dev *dev;
	void *mem_buf;
	size_t buf_len;
	char *node_path;
	unsigned long block_size;
};


static inline int check_kmod(struct kmod_ctx *kmod_ctx)
{
	return kmod_ctx ? 0 : -ENXIO;
}

#endif /* _LIBDAXCTL_PRIVATE_H_ */
