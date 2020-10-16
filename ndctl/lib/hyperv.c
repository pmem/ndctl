// SPDX-License-Identifier: GPL-2.0
/* Copyright (C) 2019, Microsoft Corporation. All rights reserved. */

#include <stdlib.h>
#include <limits.h>
#include <util/bitmap.h>
#include <util/log.h>
#include <ndctl/libndctl.h>
#include "private.h"
#include "hyperv.h"

static u32 hyperv_get_firmware_status(struct ndctl_cmd *cmd)
{
	return cmd->hyperv->u.status;
}

static bool hyperv_cmd_is_supported(struct ndctl_dimm *dimm, int cmd)
{
	/*
	 * "ndctl monitor" requires ND_CMD_SMART, which is not really supported
	 * by Hyper-V virtual NVDIMM. Nevertheless, ND_CMD_SMART can be emulated
	 * by ND_HYPERV_CMD_GET_HEALTH_INFO and ND_HYPERV_CMD_GET_SHUTDOWN_INFO.
	 */
	if (cmd == ND_CMD_SMART )
		return true;

	return !!(dimm->cmd_mask & (1ULL << cmd));
}

static struct ndctl_cmd *alloc_hyperv_cmd(struct ndctl_dimm *dimm,
		unsigned int command)
{
	struct ndctl_bus *bus = ndctl_dimm_get_bus(dimm);
	struct ndctl_ctx *ctx = ndctl_bus_get_ctx(bus);
	struct nd_pkg_hyperv *hyperv;
	struct ndctl_cmd *cmd;
	size_t size;

	if (!ndctl_dimm_is_cmd_supported(dimm, ND_CMD_CALL)) {
		dbg(ctx, "unsupported cmd\n");
		return NULL;
	}

	if (test_dimm_dsm(dimm, command) == DIMM_DSM_UNSUPPORTED) {
		dbg(ctx, "unsupported function\n");
		return NULL;
	}

	size = sizeof(*cmd) + sizeof(struct nd_pkg_hyperv);
	cmd = calloc(1, size);
	if (!cmd)
		return NULL;

	ndctl_cmd_ref(cmd);

	cmd->dimm = dimm;
	cmd->type = ND_CMD_CALL;
	cmd->get_firmware_status = hyperv_get_firmware_status;
	cmd->size = size;
	cmd->status = 1;

	hyperv = cmd->hyperv;
	hyperv->gen.nd_family = NVDIMM_FAMILY_HYPERV;
	hyperv->gen.nd_command = command;
	hyperv->gen.nd_size_out = sizeof(hyperv->u.health_info);

	return cmd;
}

static struct ndctl_cmd *hyperv_dimm_cmd_new_smart(struct ndctl_dimm *dimm)
{
	return alloc_hyperv_cmd(dimm, ND_HYPERV_CMD_GET_HEALTH_INFO);
}

static int hyperv_cmd_valid(struct ndctl_cmd *cmd, unsigned int command)
{
	if (cmd->type != ND_CMD_CALL ||
	    cmd->size != sizeof(*cmd) + sizeof(struct nd_pkg_hyperv) ||
	    cmd->hyperv->gen.nd_family != NVDIMM_FAMILY_HYPERV ||
	    cmd->hyperv->gen.nd_command != command ||
	    cmd->status != 0 ||
	    cmd->hyperv->u.status != 0)
		return cmd->status < 0 ? cmd->status : -EINVAL;

	return 0;
}

static int hyperv_valid_health_info(struct ndctl_cmd *cmd)
{
	return hyperv_cmd_valid(cmd, ND_HYPERV_CMD_GET_HEALTH_INFO);
}

static int hyperv_get_shutdown_count(struct ndctl_cmd *cmd, unsigned int *count)
{
	unsigned int command = ND_HYPERV_CMD_GET_SHUTDOWN_INFO;
	struct ndctl_cmd *cmd_get_shutdown_info;
	int rc;

	cmd_get_shutdown_info = alloc_hyperv_cmd(cmd->dimm, command);
	if (!cmd_get_shutdown_info)
		return -EINVAL;

	if (ndctl_cmd_submit_xlat(cmd_get_shutdown_info) < 0 ||
	    hyperv_cmd_valid(cmd_get_shutdown_info, command) < 0) {
		rc = -EINVAL;
		goto out;
	}

	*count = cmd_get_shutdown_info->hyperv->u.shutdown_info.count;
	rc = 0;
out:
	ndctl_cmd_unref(cmd_get_shutdown_info);
	return rc;
}

static unsigned int hyperv_cmd_get_flags(struct ndctl_cmd *cmd)
{
	unsigned int flags = 0;
	unsigned int count;
	int rc;

	rc = hyperv_valid_health_info(cmd);
	if (rc < 0) {
		errno = -rc;
		return 0;
	}
	flags |= ND_SMART_HEALTH_VALID;

	if (hyperv_get_shutdown_count(cmd, &count) == 0)
		flags |= ND_SMART_SHUTDOWN_COUNT_VALID;

	return flags;
}

static unsigned int hyperv_cmd_get_health(struct ndctl_cmd *cmd)
{
	unsigned int health = 0;
	__u32 num;
	int rc;

	rc = hyperv_valid_health_info(cmd);
	if (rc < 0) {
		errno = -rc;
		return UINT_MAX;
	}

	num = cmd->hyperv->u.health_info.health & 0x3F;

	if (num & (BIT(0) | BIT(1)))
		health |= ND_SMART_CRITICAL_HEALTH;

	if (num & BIT(2))
		health |= ND_SMART_FATAL_HEALTH;

	if (num & (BIT(3) | BIT(4) | BIT(5)))
		health |= ND_SMART_NON_CRITICAL_HEALTH;

	return health;
}

static unsigned int hyperv_cmd_get_shutdown_count(struct ndctl_cmd *cmd)
{
	unsigned int count;
	int rc;;

	rc = hyperv_get_shutdown_count(cmd, &count);

	if (rc < 0) {
		errno = -rc;
		return UINT_MAX;
	}

	return count;
}

static int hyperv_cmd_xlat_firmware_status(struct ndctl_cmd *cmd)
{
	return cmd->hyperv->u.status == 0 ? 0 : -EINVAL;
}

struct ndctl_dimm_ops * const hyperv_dimm_ops = &(struct ndctl_dimm_ops) {
	.cmd_is_supported = hyperv_cmd_is_supported,
	.new_smart = hyperv_dimm_cmd_new_smart,
	.smart_get_flags = hyperv_cmd_get_flags,
	.smart_get_health = hyperv_cmd_get_health,
	.smart_get_shutdown_count = hyperv_cmd_get_shutdown_count,
	.xlat_firmware_status = hyperv_cmd_xlat_firmware_status,
};
