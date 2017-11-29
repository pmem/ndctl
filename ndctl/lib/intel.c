/*
 * Copyright (c) 2016-2017, Intel Corporation.
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
#include <stdlib.h>
#include <limits.h>
#include <util/log.h>
#include <ndctl/libndctl.h>
#include "private.h"

static struct ndctl_cmd *alloc_intel_cmd(struct ndctl_dimm *dimm,
		unsigned func, size_t in_size, size_t out_size)
{
	struct ndctl_ctx *ctx = ndctl_dimm_get_ctx(dimm);
	struct ndctl_cmd *cmd;
	size_t size;

	if (!ndctl_dimm_is_cmd_supported(dimm, ND_CMD_CALL)) {
		dbg(ctx, "unsupported cmd: %d\n", ND_CMD_CALL);
		return NULL;
	}

	if (test_dimm_dsm(dimm, func) == DIMM_DSM_UNSUPPORTED) {
		dbg(ctx, "unsupported function: %d\n", func);
		return NULL;
	}

	size = sizeof(*cmd) + sizeof(struct nd_pkg_intel);
	cmd = calloc(1, size);
	if (!cmd)
		return NULL;

	cmd->dimm = dimm;
	ndctl_cmd_ref(cmd);
	cmd->type = ND_CMD_CALL;
	cmd->size = size;
	cmd->status = 1;

	*(cmd->intel) = (struct nd_pkg_intel) {
		.gen = {
			.nd_family = NVDIMM_FAMILY_INTEL,
			.nd_command = func,
			.nd_size_in = in_size,
			.nd_size_out = out_size,
		},
	};

	return cmd;
}

static struct ndctl_cmd *intel_dimm_cmd_new_smart(struct ndctl_dimm *dimm)
{
	struct ndctl_cmd *cmd;

	BUILD_ASSERT(sizeof(struct nd_intel_smart) == 132);

	cmd = alloc_intel_cmd(dimm, ND_INTEL_SMART,
			0, sizeof(cmd->intel->smart));
	if (!cmd)
		return NULL;
	cmd->firmware_status = &cmd->intel->smart.status;

	return cmd;
}

static int intel_smart_valid(struct ndctl_cmd *cmd)
{
	struct nd_pkg_intel *pkg = cmd->intel;

	if (cmd->type != ND_CMD_CALL || cmd->status != 0
			|| pkg->gen.nd_family != NVDIMM_FAMILY_INTEL
			|| pkg->gen.nd_command != ND_INTEL_SMART)
		return cmd->status < 0 ? cmd->status : -EINVAL;
	return 0;
}

#define intel_smart_get_field(cmd, field) \
static unsigned int intel_cmd_smart_get_##field(struct ndctl_cmd *cmd) \
{ \
	if (intel_smart_valid(cmd) < 0) \
		return UINT_MAX; \
	return cmd->intel->smart.field; \
}

static unsigned int intel_cmd_smart_get_flags(struct ndctl_cmd *cmd)
{
	unsigned int flags = 0;
	unsigned int intel_flags;

	if (intel_smart_valid(cmd) < 0)
		return 0;

	/* translate intel specific flags to libndctl api smart flags */
	intel_flags = cmd->intel->smart.flags;
	if (intel_flags & ND_INTEL_SMART_HEALTH_VALID)
		flags |= ND_SMART_HEALTH_VALID;
	if (intel_flags & ND_INTEL_SMART_SPARES_VALID)
		flags |= ND_SMART_SPARES_VALID;
	if (intel_flags & ND_INTEL_SMART_USED_VALID)
		flags |= ND_SMART_USED_VALID;
	if (intel_flags & ND_INTEL_SMART_MTEMP_VALID)
		flags |= ND_SMART_MTEMP_VALID;
	if (intel_flags & ND_INTEL_SMART_CTEMP_VALID)
		flags |= ND_SMART_CTEMP_VALID;
	if (intel_flags & ND_INTEL_SMART_SHUTDOWN_COUNT_VALID)
		flags |= ND_SMART_SHUTDOWN_COUNT_VALID;
	if (intel_flags & ND_INTEL_SMART_AIT_STATUS_VALID)
		flags |= ND_SMART_AIT_STATUS_VALID;
	if (intel_flags & ND_INTEL_SMART_PTEMP_VALID)
		flags |= ND_SMART_PTEMP_VALID;
	if (intel_flags & ND_INTEL_SMART_ALARM_VALID)
		flags |= ND_SMART_ALARM_VALID;
	if (intel_flags & ND_INTEL_SMART_SHUTDOWN_VALID)
		flags |= ND_SMART_SHUTDOWN_VALID;
	if (intel_flags & ND_INTEL_SMART_VENDOR_VALID)
		flags |= ND_SMART_VENDOR_VALID;
	return flags;
}

static unsigned int intel_cmd_smart_get_health(struct ndctl_cmd *cmd)
{
	unsigned int health = 0;
	unsigned int intel_health;

	if (intel_smart_valid(cmd) < 0)
		return 0;
	intel_health = cmd->intel->smart.health;
	if (intel_health & ND_INTEL_SMART_NON_CRITICAL_HEALTH)
		health |= ND_SMART_NON_CRITICAL_HEALTH;
	if (intel_health & ND_INTEL_SMART_CRITICAL_HEALTH)
		health |= ND_SMART_CRITICAL_HEALTH;
	if (intel_health & ND_INTEL_SMART_FATAL_HEALTH)
		health |= ND_SMART_FATAL_HEALTH;
	return health;
}

intel_smart_get_field(cmd, media_temperature)
intel_smart_get_field(cmd, ctrl_temperature)
intel_smart_get_field(cmd, spares)
intel_smart_get_field(cmd, alarm_flags)
intel_smart_get_field(cmd, life_used)
intel_smart_get_field(cmd, shutdown_state)
intel_smart_get_field(cmd, shutdown_count)
intel_smart_get_field(cmd, vendor_size)

static unsigned char *intel_cmd_smart_get_vendor_data(struct ndctl_cmd *cmd)
{
	if (intel_smart_valid(cmd) < 0)
		return NULL;
	return cmd->intel->smart.vendor_data;
}

static int intel_smart_threshold_valid(struct ndctl_cmd *cmd)
{
	struct nd_pkg_intel *pkg = cmd->intel;

	if (cmd->type != ND_CMD_CALL || cmd->status != 0
			|| pkg->gen.nd_family != NVDIMM_FAMILY_INTEL
			|| pkg->gen.nd_command != ND_INTEL_SMART_THRESHOLD)
		return cmd->status < 0 ? cmd->status : -EINVAL;
	return 0;
}

#define intel_smart_threshold_get_field(cmd, field) \
static unsigned int intel_cmd_smart_threshold_get_##field( \
			struct ndctl_cmd *cmd) \
{ \
	if (intel_smart_threshold_valid(cmd) < 0) \
		return UINT_MAX; \
	return cmd->intel->thresh.field; \
}

static unsigned int intel_cmd_smart_threshold_get_alarm_control(
		struct ndctl_cmd *cmd)
{
	struct nd_intel_smart_threshold *thresh;
	unsigned int flags = 0;

        if (intel_smart_threshold_valid(cmd) < 0)
		return 0;

	thresh = &cmd->intel->thresh;
	if (thresh->alarm_control & ND_INTEL_SMART_SPARE_TRIP)
		flags |= ND_SMART_SPARE_TRIP;
	if (thresh->alarm_control & ND_INTEL_SMART_TEMP_TRIP)
		flags |= ND_SMART_TEMP_TRIP;
	if (thresh->alarm_control & ND_INTEL_SMART_CTEMP_TRIP)
		flags |= ND_SMART_CTEMP_TRIP;

	return flags;
}

intel_smart_threshold_get_field(cmd, media_temperature)
intel_smart_threshold_get_field(cmd, ctrl_temperature)
intel_smart_threshold_get_field(cmd, spares)

static struct ndctl_cmd *intel_dimm_cmd_new_smart_threshold(
		struct ndctl_dimm *dimm)
{
	struct ndctl_cmd *cmd;

	BUILD_ASSERT(sizeof(struct nd_intel_smart_threshold) == 12);

	cmd = alloc_intel_cmd(dimm, ND_INTEL_SMART_THRESHOLD,
			0, sizeof(cmd->intel->thresh));
	if (!cmd)
		return NULL;
	cmd->firmware_status = &cmd->intel->thresh.status;

	return cmd;
}

static struct ndctl_cmd *intel_dimm_cmd_new_smart_set_threshold(
		struct ndctl_cmd *cmd_thresh)
{
	struct ndctl_cmd *cmd;
	struct nd_intel_smart_threshold *thresh;
	struct nd_intel_smart_set_threshold *set_thresh;

	BUILD_ASSERT(sizeof(struct nd_intel_smart_set_threshold) == 11);

	if (intel_smart_threshold_valid(cmd_thresh) < 0)
		return NULL;

	cmd = alloc_intel_cmd(cmd_thresh->dimm, ND_INTEL_SMART_SET_THRESHOLD,
			offsetof(typeof(*set_thresh), status), 4);
	if (!cmd)
		return NULL;

	cmd->source = cmd_thresh;
	ndctl_cmd_ref(cmd_thresh);
	set_thresh = &cmd->intel->set_thresh;
	thresh = &cmd_thresh->intel->thresh;
	set_thresh->alarm_control = thresh->alarm_control;
	set_thresh->spares = thresh->spares;
	set_thresh->media_temperature = thresh->media_temperature;
	set_thresh->ctrl_temperature = thresh->ctrl_temperature;
	cmd->firmware_status = &set_thresh->status;

	return cmd;
}

static int intel_smart_set_threshold_valid(struct ndctl_cmd *cmd)
{
	struct nd_pkg_intel *pkg = cmd->intel;

	if (cmd->type != ND_CMD_CALL || cmd->status != 1
			|| pkg->gen.nd_family != NVDIMM_FAMILY_INTEL
			|| pkg->gen.nd_command != ND_INTEL_SMART_SET_THRESHOLD)
		return -EINVAL;
	return 0;
}

#define intel_smart_set_threshold_field(field) \
static int intel_cmd_smart_threshold_set_##field( \
			struct ndctl_cmd *cmd, unsigned int val) \
{ \
	if (intel_smart_set_threshold_valid(cmd) < 0) \
		return -EINVAL; \
	cmd->intel->set_thresh.field = val; \
	return 0; \
}

static unsigned int intel_cmd_smart_threshold_get_supported_alarms(
		struct ndctl_cmd *cmd)
{
	if (intel_smart_set_threshold_valid(cmd) < 0)
		return 0;
	return ND_SMART_SPARE_TRIP | ND_SMART_MTEMP_TRIP
		| ND_SMART_CTEMP_TRIP;
}

intel_smart_set_threshold_field(alarm_control)
intel_smart_set_threshold_field(spares)
intel_smart_set_threshold_field(media_temperature)
intel_smart_set_threshold_field(ctrl_temperature)

struct ndctl_smart_ops * const intel_smart_ops = &(struct ndctl_smart_ops) {
	.new_smart = intel_dimm_cmd_new_smart,
	.smart_get_flags = intel_cmd_smart_get_flags,
	.smart_get_health = intel_cmd_smart_get_health,
	.smart_get_media_temperature = intel_cmd_smart_get_media_temperature,
	.smart_get_ctrl_temperature = intel_cmd_smart_get_ctrl_temperature,
	.smart_get_spares = intel_cmd_smart_get_spares,
	.smart_get_alarm_flags = intel_cmd_smart_get_alarm_flags,
	.smart_get_life_used = intel_cmd_smart_get_life_used,
	.smart_get_shutdown_state = intel_cmd_smart_get_shutdown_state,
	.smart_get_shutdown_count = intel_cmd_smart_get_shutdown_count,
	.smart_get_vendor_size = intel_cmd_smart_get_vendor_size,
	.smart_get_vendor_data = intel_cmd_smart_get_vendor_data,
	.new_smart_threshold = intel_dimm_cmd_new_smart_threshold,
	.smart_threshold_get_alarm_control
		= intel_cmd_smart_threshold_get_alarm_control,
	.smart_threshold_get_media_temperature
		= intel_cmd_smart_threshold_get_media_temperature,
	.smart_threshold_get_ctrl_temperature
		= intel_cmd_smart_threshold_get_ctrl_temperature,
	.smart_threshold_get_spares = intel_cmd_smart_threshold_get_spares,
	.new_smart_set_threshold = intel_dimm_cmd_new_smart_set_threshold,
	.smart_threshold_get_supported_alarms
		= intel_cmd_smart_threshold_get_supported_alarms,
	.smart_threshold_set_alarm_control
		= intel_cmd_smart_threshold_set_alarm_control,
	.smart_threshold_set_media_temperature
		= intel_cmd_smart_threshold_set_media_temperature,
	.smart_threshold_set_ctrl_temperature
		= intel_cmd_smart_threshold_set_ctrl_temperature,
	.smart_threshold_set_spares = intel_cmd_smart_threshold_set_spares,
};
