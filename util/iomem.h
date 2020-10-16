/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2019-2020 Intel Corporation. All rights reserved. */
#ifndef _NDCTL_IOMEM_H_
#define _NDCTL_IOMEM_H_

struct log_ctx;
unsigned long long __iomem_get_dev_resource(struct log_ctx *ctx,
		const char *path);

#define iomem_get_dev_resource(c, p) __iomem_get_dev_resource(&(c)->ctx, (p))

#endif /* _NDCTL_IOMEM_H_ */
