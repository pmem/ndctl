// SPDX-License-Identifier: LGPL-2.1
// Copyright (C) 2016-2017 Dell, Inc.
// Copyright (C) 2016 Hewlett Packard Enterprise Development LP
// Copyright (C) 2016-2020, Intel Corporation.
/* Copyright (C) 2022 iXsystems, Inc. */
#include <stdlib.h>
#include <limits.h>
#include <util/log.h>
#include <ndctl/libndctl.h>
#include "private.h"
#include "msft.h"

#define CMD_MSFT(_c) ((_c)->msft)
#define CMD_MSFT_SMART(_c) (CMD_MSFT(_c)->u.smart.data)

static const char *msft_cmd_desc(int fn)
{
	static const char * const descs[] = {
		[NDN_MSFT_CMD_CHEALTH] = "critical_health",
		[NDN_MSFT_CMD_NHEALTH] = "nvdimm_health",
		[NDN_MSFT_CMD_EHEALTH] = "es_health",
	};
	const char *desc;

	if (fn >= (int) ARRAY_SIZE(descs))
		return "unknown";
	desc = descs[fn];
	if (!desc)
		return "unknown";
	return desc;
}

static bool msft_cmd_is_supported(struct ndctl_dimm *dimm, int cmd)
{
	/* Handle this separately to support monitor mode */
	if (cmd == ND_CMD_SMART)
		return true;

	return !!(dimm->cmd_mask & (1ULL << cmd));
}

static u32 msft_get_firmware_status(struct ndctl_cmd *cmd)
{
	return cmd->msft->u.smart.status;
}

static struct ndctl_cmd *alloc_msft_cmd(struct ndctl_dimm *dimm,
		unsigned int func, size_t in_size, size_t out_size)
{
	struct ndctl_bus *bus = ndctl_dimm_get_bus(dimm);
	struct ndctl_ctx *ctx = ndctl_bus_get_ctx(bus);
	struct ndctl_cmd *cmd;
	size_t size;
	struct ndn_pkg_msft *msft;

	if (!ndctl_dimm_is_cmd_supported(dimm, ND_CMD_CALL)) {
		dbg(ctx, "unsupported cmd\n");
		return NULL;
	}

	if (test_dimm_dsm(dimm, func) == DIMM_DSM_UNSUPPORTED) {
		dbg(ctx, "unsupported function\n");
		return NULL;
	}

	size = sizeof(*cmd) + sizeof(struct nd_cmd_pkg) + in_size + out_size;
	cmd = calloc(1, size);
	if (!cmd)
		return NULL;

	cmd->dimm = dimm;
	ndctl_cmd_ref(cmd);
	cmd->type = ND_CMD_CALL;
	cmd->size = size;
	cmd->status = 1;

	msft = CMD_MSFT(cmd);
	msft->gen.nd_family = NVDIMM_FAMILY_MSFT;
	msft->gen.nd_command = func;
	msft->gen.nd_fw_size = 0;
	msft->gen.nd_size_in = in_size;
	msft->gen.nd_size_out = out_size;
	msft->u.smart.status = 0;
	cmd->get_firmware_status = msft_get_firmware_status;

	return cmd;
}

static struct ndctl_cmd *msft_dimm_cmd_new_smart(struct ndctl_dimm *dimm)
{
	return alloc_msft_cmd(dimm, NDN_MSFT_CMD_NHEALTH, 0,
	    sizeof(struct ndn_msft_smart));
}

static int msft_smart_valid(struct ndctl_cmd *cmd)
{
	if (cmd->type != ND_CMD_CALL ||
	    CMD_MSFT(cmd)->gen.nd_family != NVDIMM_FAMILY_MSFT ||
	    CMD_MSFT(cmd)->gen.nd_command != NDN_MSFT_CMD_NHEALTH ||
	    cmd->status != 0)
		return cmd->status < 0 ? cmd->status : -EINVAL;
	return 0;
}

static unsigned int msft_cmd_smart_get_flags(struct ndctl_cmd *cmd)
{
	int rc;

	rc = msft_smart_valid(cmd);
	if (rc < 0) {
		errno = -rc;
		return UINT_MAX;
	}

	/* below health data can be retrieved via MSFT _DSM function 11 */
	return ND_SMART_HEALTH_VALID | ND_SMART_TEMP_VALID |
	    ND_SMART_USED_VALID;
}

static unsigned int num_set_bit_health(__u16 num)
{
	int i;
	__u16 n = num & 0x7FFF;
	unsigned int count = 0;

	for (i = 0; i < 15; i++)
		if (!!(n & (1 << i)))
			count++;

	return count;
}

static unsigned int msft_cmd_smart_get_health(struct ndctl_cmd *cmd)
{
	unsigned int health;
	unsigned int num;
	int rc;

	rc = msft_smart_valid(cmd);
	if (rc < 0) {
		errno = -rc;
		return UINT_MAX;
	}

	num = num_set_bit_health(CMD_MSFT_SMART(cmd)->health);
	if (num == 0)
		health = 0;
	else if (num < 2)
		health = ND_SMART_NON_CRITICAL_HEALTH;
	else if (num < 3)
		health = ND_SMART_CRITICAL_HEALTH;
	else
		health = ND_SMART_FATAL_HEALTH;

	return health;
}

static unsigned int msft_cmd_smart_get_media_temperature(struct ndctl_cmd *cmd)
{
	int rc;

	rc = msft_smart_valid(cmd);
	if (rc < 0) {
		errno = -rc;
		return UINT_MAX;
	}

	return CMD_MSFT_SMART(cmd)->temp * 16;
}

static unsigned int msft_cmd_smart_get_life_used(struct ndctl_cmd *cmd)
{
	int rc;

	rc = msft_smart_valid(cmd);
	if (rc < 0) {
		errno = -rc;
		return UINT_MAX;
	}

	return 100 - CMD_MSFT_SMART(cmd)->nvm_lifetime;
}

static int msft_cmd_xlat_firmware_status(struct ndctl_cmd *cmd)
{
	unsigned int status;

	status = cmd->get_firmware_status(cmd) & NDN_MSFT_STATUS_MASK;

	/* Common statuses */
	switch (status) {
	case NDN_MSFT_STATUS_SUCCESS:
		return 0;
	case NDN_MSFT_STATUS_NOTSUPP:
		return -EOPNOTSUPP;
	case NDN_MSFT_STATUS_INVALPARM:
		return -EINVAL;
	case NDN_MSFT_STATUS_I2CERR:
		return -EIO;
	}

	return -ENOMSG;
}

struct ndctl_dimm_ops * const msft_dimm_ops = &(struct ndctl_dimm_ops) {
	.cmd_desc = msft_cmd_desc,
	.cmd_is_supported = msft_cmd_is_supported,
	.new_smart = msft_dimm_cmd_new_smart,
	.smart_get_flags = msft_cmd_smart_get_flags,
	.smart_get_health = msft_cmd_smart_get_health,
	.smart_get_media_temperature = msft_cmd_smart_get_media_temperature,
	.smart_get_life_used = msft_cmd_smart_get_life_used,
	.xlat_firmware_status = msft_cmd_xlat_firmware_status,
};
