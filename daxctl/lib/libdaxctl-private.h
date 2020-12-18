/* SPDX-License-Identifier: LGPL-2.1 */
/* Copyright (C) 2014-2020, Intel Corporation. All rights reserved. */
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
	MEM_SET_ONLINE_NO_MOVABLE,
	MEM_IS_ONLINE,
	MEM_COUNT,
	MEM_GET_ZONE,
};

/* OR-able flags, 1, 2, 4, 8 etc */
enum memory_op_status {
	MEM_ST_OK = 0,
	MEM_ST_ZONE_INCONSISTENT = 1,
};

enum memory_zones {
	MEM_ZONE_UNKNOWN = 1,
	MEM_ZONE_MOVABLE,
	MEM_ZONE_NORMAL,
};

static const char *zone_strings[] = {
	[MEM_ZONE_UNKNOWN] = "mixed",
	[MEM_ZONE_NORMAL] = "Normal",
	[MEM_ZONE_MOVABLE] = "Movable",
};

static const char *state_strings[] = {
	[MEM_ZONE_NORMAL] = "online",
	[MEM_ZONE_MOVABLE] = "online_movable",
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

struct daxctl_mapping {
	struct daxctl_dev *dev;
	unsigned long long pgoff, start, end;
	struct list_node list;
};

struct daxctl_dev {
	int id, major, minor;
	void *dev_buf;
	size_t buf_len;
	char *dev_path;
	struct list_node list;
	unsigned long long resource;
	unsigned long long size;
	unsigned long align;
	struct kmod_module *module;
	struct daxctl_region *region;
	struct daxctl_memory *mem;
	int target_node;
	int num_mappings;
	struct list_head mappings;
};

struct daxctl_memory {
	struct daxctl_dev *dev;
	void *mem_buf;
	size_t buf_len;
	char *node_path;
	unsigned long block_size;
	enum memory_zones zone;
};


static inline int check_kmod(struct kmod_ctx *kmod_ctx)
{
	return kmod_ctx ? 0 : -ENXIO;
}

#endif /* _LIBDAXCTL_PRIVATE_H_ */
