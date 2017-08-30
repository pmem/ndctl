/*
 * Copyright (c) 2014-2017, Intel Corporation.
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
#include <ndctl/libndctl.h>
#include <util/sysfs.h>
#include <stdlib.h>
#include "private.h"

NDCTL_EXPORT struct ndctl_cmd *ndctl_dimm_read_labels(struct ndctl_dimm *dimm)
{
        struct ndctl_bus *bus = ndctl_dimm_get_bus(dimm);
        struct ndctl_cmd *cmd_size, *cmd_read;
        int rc;

        rc = ndctl_bus_wait_probe(bus);
        if (rc < 0)
                return NULL;

        cmd_size = ndctl_dimm_cmd_new_cfg_size(dimm);
        if (!cmd_size)
                return NULL;
        rc = ndctl_cmd_submit(cmd_size);
        if (rc || ndctl_cmd_get_firmware_status(cmd_size))
                goto out_size;

        cmd_read = ndctl_dimm_cmd_new_cfg_read(cmd_size);
        if (!cmd_read)
                goto out_size;
        rc = ndctl_cmd_submit(cmd_read);
        if (rc || ndctl_cmd_get_firmware_status(cmd_read))
                goto out_read;

        ndctl_cmd_unref(cmd_size);
        return cmd_read;

 out_read:
        ndctl_cmd_unref(cmd_read);
 out_size:
        ndctl_cmd_unref(cmd_size);
        return NULL;
}

NDCTL_EXPORT int ndctl_dimm_zero_labels(struct ndctl_dimm *dimm)
{
	struct ndctl_ctx *ctx = ndctl_dimm_get_ctx(dimm);
	struct ndctl_cmd *cmd_read, *cmd_write;
	int rc;

	cmd_read = ndctl_dimm_read_labels(dimm);
	if (!cmd_read)
		return -ENXIO;

	if (ndctl_dimm_is_active(dimm)) {
		dbg(ctx, "%s: regions active, abort label write\n",
			ndctl_dimm_get_devname(dimm));
		rc = -EBUSY;
		goto out_read;
	}

	cmd_write = ndctl_dimm_cmd_new_cfg_write(cmd_read);
	if (!cmd_write) {
		rc = -ENOTTY;
		goto out_read;
	}
	if (ndctl_cmd_cfg_write_zero_data(cmd_write) < 0) {
		rc = -ENXIO;
		goto out_write;
	}
	rc = ndctl_cmd_submit(cmd_write);
	if (rc || ndctl_cmd_get_firmware_status(cmd_write))
		goto out_write;

	/*
	 * If the dimm is already disabled the kernel is not holding a cached
	 * copy of the label space.
	 */
	if (!ndctl_dimm_is_enabled(dimm))
		goto out_write;

	rc = ndctl_dimm_disable(dimm);
	if (rc)
		goto out_write;
	rc = ndctl_dimm_enable(dimm);

 out_write:
	ndctl_cmd_unref(cmd_write);
 out_read:
	ndctl_cmd_unref(cmd_read);

	return rc;
}

NDCTL_EXPORT unsigned long ndctl_dimm_get_available_labels(
		struct ndctl_dimm *dimm)
{
	struct ndctl_ctx *ctx = ndctl_dimm_get_ctx(dimm);
	char *path = dimm->dimm_buf;
	int len = dimm->buf_len;
	char buf[20];

	if (snprintf(path, len, "%s/available_slots", dimm->dimm_path) >= len) {
		err(ctx, "%s: buffer too small!\n",
				ndctl_dimm_get_devname(dimm));
		return ULONG_MAX;
	}

	if (sysfs_read_attr(ctx, path, buf) < 0)
		return ULONG_MAX;

	return strtoul(buf, NULL, 0);
}
