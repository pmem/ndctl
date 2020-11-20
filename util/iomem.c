// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2019-2020 Intel Corporation. All rights reserved.
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <util/log.h>
#include <util/iomem.h>
#include <util/sysfs.h>

unsigned long long __iomem_get_dev_resource(struct log_ctx *ctx,
		const char *devpath)
{
	const char *devname = devpath_to_devname(devpath);
	FILE *fp = fopen("/proc/iomem", "r");
	unsigned long long res;
	char name[256];

	if (fp == NULL) {
		log_err(ctx, "%s: open /proc/iomem: %s\n", devname,
				strerror(errno));
		return 0;
	}

	while (fscanf(fp, "%llx-%*x : %254[^\n]\n", &res, name) == 2) {
		if (strcmp(name, devname) == 0) {
			log_dbg(ctx, "%s: got resource via iomem: %#llx\n",
					devname, res);
			fclose(fp);
			return res;
		}
	}

	log_dbg(ctx, "%s: not found in iomem\n", devname);
	fclose(fp);
	return 0;
}
