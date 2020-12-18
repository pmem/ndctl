/* SPDX-License-Identifier: LGPL-2.1 */
/* Copyright (C) 2020-2021, Intel Corporation. All rights reserved. */
#ifndef _LIBCXL_PRIVATE_H_
#define _LIBCXL_PRIVATE_H_

#include <libkmod.h>

#define CXL_EXPORT __attribute__ ((visibility("default")))

struct cxl_memdev {
	int id, major, minor;
	void *dev_buf;
	size_t buf_len;
	char *dev_path;
	char *firmware_version;
	struct cxl_ctx *ctx;
	struct list_node list;
	unsigned long long pmem_size;
	unsigned long long ram_size;
	int payload_max;
	struct kmod_module *module;
};

static inline int check_kmod(struct kmod_ctx *kmod_ctx)
{
	return kmod_ctx ? 0 : -ENXIO;
}

#endif /* _LIBCXL_PRIVATE_H_ */
