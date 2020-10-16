// SPDX-License-Identifier: LGPL-2.1
// Copyright (C) 2016 Hewlett Packard Enterprise Development LP
// Copyright (C) 2016-2020, Intel Corporation.
#include <stdlib.h>
#include <limits.h>
#include <util/log.h>
#include <ndctl/libndctl.h>
#include "private.h"

#include "hpe1.h"

#define CMD_HPE1(_c) ((_c)->hpe1)
#define CMD_HPE1_SMART(_c) (CMD_HPE1(_c)->u.smart.data)
#define CMD_HPE1_SMART_THRESH(_c) (CMD_HPE1(_c)->u.thresh.data)

static u32 hpe1_get_firmware_status(struct ndctl_cmd *cmd)
{
	switch (cmd->hpe1->gen.nd_command) {
	case NDN_HPE1_CMD_SMART:
		return cmd->hpe1->u.smart.status;
	case NDN_HPE1_CMD_SMART_THRESHOLD:
		return cmd->hpe1->u.thresh.status;
	}
	return -1U;
}

static struct ndctl_cmd *hpe1_dimm_cmd_new_smart(struct ndctl_dimm *dimm)
{
	struct ndctl_bus *bus = ndctl_dimm_get_bus(dimm);
	struct ndctl_ctx *ctx = ndctl_bus_get_ctx(bus);
	struct ndctl_cmd *cmd;
	size_t size;
	struct ndn_pkg_hpe1 *hpe1;

	if (!ndctl_dimm_is_cmd_supported(dimm, ND_CMD_CALL)) {
		dbg(ctx, "unsupported cmd\n");
		return NULL;
	}

	if (test_dimm_dsm(dimm, NDN_HPE1_CMD_SMART)
			== DIMM_DSM_UNSUPPORTED) {
		dbg(ctx, "unsupported function\n");
		return NULL;
	}

	size = sizeof(*cmd) + sizeof(struct ndn_pkg_hpe1);
	cmd = calloc(1, size);
	if (!cmd)
		return NULL;

	cmd->dimm = dimm;
	ndctl_cmd_ref(cmd);
	cmd->type = ND_CMD_CALL;
	cmd->size = size;
	cmd->status = 1;

	hpe1 = CMD_HPE1(cmd);
	hpe1->gen.nd_family = NVDIMM_FAMILY_HPE1;
	hpe1->gen.nd_command = NDN_HPE1_CMD_SMART;
	hpe1->gen.nd_fw_size = 0;
	hpe1->gen.nd_size_in = offsetof(struct ndn_hpe1_smart, status);
	hpe1->gen.nd_size_out = sizeof(hpe1->u.smart);
	hpe1->u.smart.status = 3;
	cmd->get_firmware_status = hpe1_get_firmware_status;

	hpe1->u.smart.in_valid_flags = 0;
	hpe1->u.smart.in_valid_flags |= NDN_HPE1_SMART_HEALTH_VALID;
	hpe1->u.smart.in_valid_flags |= NDN_HPE1_SMART_TEMP_VALID;
	hpe1->u.smart.in_valid_flags |= NDN_HPE1_SMART_SPARES_VALID;
	hpe1->u.smart.in_valid_flags |= NDN_HPE1_SMART_ALARM_VALID;
	hpe1->u.smart.in_valid_flags |= NDN_HPE1_SMART_USED_VALID;
	hpe1->u.smart.in_valid_flags |= NDN_HPE1_SMART_SHUTDOWN_VALID;
	hpe1->u.smart.in_valid_flags |= NDN_HPE1_SMART_VENDOR_VALID;

	return cmd;
}

static int hpe1_smart_valid(struct ndctl_cmd *cmd)
{
	if (cmd->type != ND_CMD_CALL ||
	    cmd->size != sizeof(*cmd) + sizeof(struct ndn_pkg_hpe1) ||
	    CMD_HPE1(cmd)->gen.nd_family != NVDIMM_FAMILY_HPE1 ||
	    CMD_HPE1(cmd)->gen.nd_command != NDN_HPE1_CMD_SMART ||
	    cmd->status != 0)
		return cmd->status < 0 ? cmd->status : -EINVAL;
	return 0;
}

static unsigned int hpe1_cmd_smart_get_flags(struct ndctl_cmd *cmd)
{
	unsigned int hpe1flags;
	unsigned int flags;
	int rc;

	rc = hpe1_smart_valid(cmd);
	if (rc < 0) {
		errno = -rc;
		return UINT_MAX;
	}

	hpe1flags = CMD_HPE1_SMART(cmd)->out_valid_flags;
	flags = 0;
	if (hpe1flags & NDN_HPE1_SMART_HEALTH_VALID)
		flags |= ND_SMART_HEALTH_VALID;
	if (hpe1flags & NDN_HPE1_SMART_TEMP_VALID)
		flags |= ND_SMART_TEMP_VALID ;
	if (hpe1flags & NDN_HPE1_SMART_SPARES_VALID)
		flags |= ND_SMART_SPARES_VALID;
	if (hpe1flags & NDN_HPE1_SMART_ALARM_VALID)
		flags |= ND_SMART_ALARM_VALID;
	if (hpe1flags & NDN_HPE1_SMART_USED_VALID)
		flags |= ND_SMART_USED_VALID;
	if (hpe1flags & NDN_HPE1_SMART_SHUTDOWN_VALID)
		flags |= ND_SMART_SHUTDOWN_VALID;
	if (hpe1flags & NDN_HPE1_SMART_VENDOR_VALID)
		flags |= ND_SMART_VENDOR_VALID;

	return flags;
}

static unsigned int hpe1_cmd_smart_get_health(struct ndctl_cmd *cmd)
{
	unsigned char hpe1health;
	unsigned int health;
	int rc;

	rc = hpe1_smart_valid(cmd);
	if (rc < 0) {
		errno = -rc;
		return UINT_MAX;
	}

	hpe1health = CMD_HPE1_SMART(cmd)->stat_summary;
	health = 0;
	if (hpe1health & NDN_HPE1_SMART_NONCRIT_HEALTH)
		health |= ND_SMART_NON_CRITICAL_HEALTH;;
	if (hpe1health & NDN_HPE1_SMART_CRITICAL_HEALTH)
		health |= ND_SMART_CRITICAL_HEALTH;
	if (hpe1health & NDN_HPE1_SMART_FATAL_HEALTH)
		health |= ND_SMART_FATAL_HEALTH;

	return health;
}

static unsigned int hpe1_cmd_smart_get_media_temperature(struct ndctl_cmd *cmd)
{
	int rc;

	rc = hpe1_smart_valid(cmd);
	if (rc < 0) {
		errno = -rc;
		return UINT_MAX;
	}

	return CMD_HPE1_SMART(cmd)->curr_temp;
}

static unsigned int hpe1_cmd_smart_get_spares(struct ndctl_cmd *cmd)
{
	int rc;

	rc = hpe1_smart_valid(cmd);
	if (rc < 0) {
		errno = -rc;
		return UINT_MAX;
	}

	return CMD_HPE1_SMART(cmd)->spare_blocks;
}

static unsigned int hpe1_cmd_smart_get_alarm_flags(struct ndctl_cmd *cmd)
{
	unsigned int hpe1flags;
	unsigned int flags;
	int rc;

	rc = hpe1_smart_valid(cmd);
	if (rc < 0) {
		errno = -rc;
		return UINT_MAX;
	}

	hpe1flags = CMD_HPE1_SMART(cmd)->alarm_trips;
	flags = 0;
	if (hpe1flags & NDN_HPE1_SMART_TEMP_TRIP)
		flags |= ND_SMART_TEMP_TRIP;
	if (hpe1flags & NDN_HPE1_SMART_SPARE_TRIP)
		flags |= ND_SMART_SPARE_TRIP;

	return flags;
}

static unsigned int hpe1_cmd_smart_get_life_used(struct ndctl_cmd *cmd)
{
	int rc;

	rc = hpe1_smart_valid(cmd);
	if (rc < 0) {
		errno = -rc;
		return UINT_MAX;
	}

	return CMD_HPE1_SMART(cmd)->device_life;
}

static unsigned int hpe1_cmd_smart_get_shutdown_state(struct ndctl_cmd *cmd)
{
	unsigned int shutdown;
	int rc;

	rc = hpe1_smart_valid(cmd);
	if (rc < 0) {
		errno = -rc;
		return UINT_MAX;
	}

	shutdown = CMD_HPE1_SMART(cmd)->last_shutdown_stat;
	if (shutdown == NDN_HPE1_SMART_LASTSAVEGOOD)
		return 0;
	else
		return 1;
}

static unsigned int hpe1_cmd_smart_get_vendor_size(struct ndctl_cmd *cmd)
{
	int rc;

	rc = hpe1_smart_valid(cmd);
	if (rc < 0) {
		errno = -rc;
		return UINT_MAX;
	}

	return CMD_HPE1_SMART(cmd)->vndr_spec_data_size;
}

static unsigned char *hpe1_cmd_smart_get_vendor_data(struct ndctl_cmd *cmd)
{
	int rc;

	rc = hpe1_smart_valid(cmd);
	if (rc < 0) {
		errno = -rc;
		return NULL;
	}

	return CMD_HPE1_SMART(cmd)->vnd_spec_data;
}


static struct ndctl_cmd *hpe1_dimm_cmd_new_smart_threshold(struct ndctl_dimm *dimm)
{
	struct ndctl_bus *bus = ndctl_dimm_get_bus(dimm);
	struct ndctl_ctx *ctx = ndctl_bus_get_ctx(bus);
	struct ndctl_cmd *cmd;
	size_t size;
	struct ndn_pkg_hpe1 *hpe1;

	if (!ndctl_dimm_is_cmd_supported(dimm, ND_CMD_CALL)) {
		dbg(ctx, "unsupported cmd\n");
		return NULL;
	}

	if (test_dimm_dsm(dimm, NDN_HPE1_CMD_SMART_THRESHOLD)
			== DIMM_DSM_UNSUPPORTED) {
		dbg(ctx, "unsupported function\n");
		return NULL;
	}

	size = sizeof(*cmd) + sizeof(struct ndn_pkg_hpe1);
	cmd = calloc(1, size);
	if (!cmd)
		return NULL;

	cmd->dimm = dimm;
	ndctl_cmd_ref(cmd);
	cmd->type = ND_CMD_CALL;
	cmd->size = size;
	cmd->status = 1;

	hpe1 = CMD_HPE1(cmd);
	hpe1->gen.nd_family = NVDIMM_FAMILY_HPE1;
	hpe1->gen.nd_command = NDN_HPE1_CMD_SMART_THRESHOLD;
	hpe1->gen.nd_fw_size = 0;
	hpe1->gen.nd_size_in = offsetof(struct ndn_hpe1_smart_threshold, status);
	hpe1->gen.nd_size_out = sizeof(hpe1->u.smart);
	hpe1->u.thresh.status = 3;
	cmd->get_firmware_status = hpe1_get_firmware_status;

	return cmd;
}

static int hpe1_smart_threshold_valid(struct ndctl_cmd *cmd)
{
	if (cmd->type != ND_CMD_CALL ||
	    cmd->size != sizeof(*cmd) + sizeof(struct ndn_pkg_hpe1) ||
	    CMD_HPE1(cmd)->gen.nd_family != NVDIMM_FAMILY_HPE1 ||
	    CMD_HPE1(cmd)->gen.nd_command != NDN_HPE1_CMD_SMART_THRESHOLD ||
	    cmd->status != 0)
		return cmd->status < 0 ? cmd->status : -EINVAL;
	return 0;
}

static unsigned int hpe1_cmd_smart_threshold_get_alarm_control(struct ndctl_cmd *cmd)
{
	unsigned int hpe1flags;
	unsigned int flags;
	int rc;

	rc = hpe1_smart_threshold_valid(cmd);
	if (rc < 0) {
		errno = -rc;
		return UINT_MAX;
	}

	hpe1flags = CMD_HPE1_SMART_THRESH(cmd)->threshold_alarm_ctl;
	flags = 0;
	if (hpe1flags & NDN_HPE1_SMART_TEMP_TRIP)
		flags |= ND_SMART_TEMP_TRIP;
	if (hpe1flags & NDN_HPE1_SMART_SPARE_TRIP)
		flags |= ND_SMART_SPARE_TRIP;

	return flags;
}

static unsigned int hpe1_cmd_smart_threshold_get_media_temperature(
		struct ndctl_cmd *cmd)
{
	int rc;

	rc = hpe1_smart_threshold_valid(cmd);
	if (rc < 0) {
		errno = -rc;
		return UINT_MAX;
	}

	return CMD_HPE1_SMART_THRESH(cmd)->temp_threshold;
}

static unsigned int hpe1_cmd_smart_threshold_get_spares(struct ndctl_cmd *cmd)
{
	int rc;

	rc = hpe1_smart_threshold_valid(cmd);
	if (rc < 0) {
		errno = -rc;
		return UINT_MAX;
	}

	return CMD_HPE1_SMART_THRESH(cmd)->spare_block_threshold;
}

struct ndctl_dimm_ops * const hpe1_dimm_ops = &(struct ndctl_dimm_ops) {
	.new_smart = hpe1_dimm_cmd_new_smart,
	.smart_get_flags = hpe1_cmd_smart_get_flags,
	.smart_get_health = hpe1_cmd_smart_get_health,
	.smart_get_media_temperature = hpe1_cmd_smart_get_media_temperature,
	.smart_get_spares = hpe1_cmd_smart_get_spares,
	.smart_get_alarm_flags = hpe1_cmd_smart_get_alarm_flags,
	.smart_get_life_used = hpe1_cmd_smart_get_life_used,
	.smart_get_shutdown_state = hpe1_cmd_smart_get_shutdown_state,
	.smart_get_vendor_size = hpe1_cmd_smart_get_vendor_size,
	.smart_get_vendor_data = hpe1_cmd_smart_get_vendor_data,
	.new_smart_threshold = hpe1_dimm_cmd_new_smart_threshold,
	.smart_threshold_get_alarm_control = hpe1_cmd_smart_threshold_get_alarm_control,
	.smart_threshold_get_media_temperature =
		hpe1_cmd_smart_threshold_get_media_temperature,
	.smart_threshold_get_spares = hpe1_cmd_smart_threshold_get_spares,
};
