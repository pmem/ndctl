/*
 * Copyright (c) 2016, Intel Corporation.
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

/*
 * Define the wrappers around the ndctl_smart_ops:
 */

NDCTL_EXPORT struct ndctl_cmd *ndctl_dimm_cmd_new_smart(
		struct ndctl_dimm *dimm)
{
	struct ndctl_smart_ops *ops = ndctl_dimm_get_smart_ops(dimm);
	if (ops && ops->new_smart)
		return ops->new_smart(dimm);
	else
		return NULL;
}

NDCTL_EXPORT struct ndctl_cmd *ndctl_dimm_cmd_new_smart_threshold(
		struct ndctl_dimm *dimm)
{
	struct ndctl_smart_ops *ops = ndctl_dimm_get_smart_ops(dimm);
	if (ops && ops->new_smart_threshold)
		return ops->new_smart_threshold(dimm);
	else
		return NULL;
}

#define smart_cmd_op(name, op, rettype, defretvalue) \
NDCTL_EXPORT rettype name(struct ndctl_cmd *cmd) \
{ \
	if (cmd->dimm) { \
		struct ndctl_smart_ops *ops = ndctl_dimm_get_smart_ops(cmd->dimm); \
		if (ops && ops->op) \
			return ops->op(cmd); \
	} \
	return defretvalue; \
}

smart_cmd_op(ndctl_cmd_smart_get_flags, smart_get_flags, unsigned int, 0)
smart_cmd_op(ndctl_cmd_smart_get_health, smart_get_health, unsigned int, 0)
smart_cmd_op(ndctl_cmd_smart_get_temperature, smart_get_temperature, unsigned int, 0)
smart_cmd_op(ndctl_cmd_smart_get_spares, smart_get_spares, unsigned int, 0)
smart_cmd_op(ndctl_cmd_smart_get_alarm_flags, smart_get_alarm_flags, unsigned int, 0)
smart_cmd_op(ndctl_cmd_smart_get_life_used, smart_get_life_used, unsigned int, 0)
smart_cmd_op(ndctl_cmd_smart_get_shutdown_state, smart_get_shutdown_state, unsigned int, 0)
smart_cmd_op(ndctl_cmd_smart_get_shutdown_count, smart_get_shutdown_count, unsigned int, 0)
smart_cmd_op(ndctl_cmd_smart_get_vendor_size, smart_get_vendor_size, unsigned int, 0)
smart_cmd_op(ndctl_cmd_smart_get_vendor_data, smart_get_vendor_data, unsigned char *, NULL)
smart_cmd_op(ndctl_cmd_smart_threshold_get_alarm_control, smart_threshold_get_alarm_control, unsigned int, 0)
smart_cmd_op(ndctl_cmd_smart_threshold_get_temperature, smart_threshold_get_temperature, unsigned int, 0)
smart_cmd_op(ndctl_cmd_smart_threshold_get_spares, smart_threshold_get_spares, unsigned int, 0)

/*
 * The following intel_dimm_*() and intel_smart_*() functions implement
 * the ndctl_smart_ops for the Intel DSM family (NVDIMM_FAMILY_INTEL):
 */

static struct ndctl_cmd *intel_dimm_cmd_new_smart(struct ndctl_dimm *dimm)
{
	struct ndctl_bus *bus = ndctl_dimm_get_bus(dimm);
	struct ndctl_ctx *ctx = ndctl_bus_get_ctx(bus);
	struct ndctl_cmd *cmd;
	size_t size;

	BUILD_ASSERT(sizeof(struct nd_smart_payload) == 128);

	if (!ndctl_dimm_is_cmd_supported(dimm, ND_CMD_SMART)) {
		dbg(ctx, "unsupported cmd\n");
		return NULL;
	}

	size = sizeof(*cmd) + sizeof(struct nd_cmd_smart);
	cmd = calloc(1, size);
	if (!cmd)
		return NULL;

	cmd->dimm = dimm;
	ndctl_cmd_ref(cmd);
	cmd->type = ND_CMD_SMART;
	cmd->size = size;
	cmd->status = 1;
	cmd->firmware_status = &cmd->smart->status;

	return cmd;
}

static int intel_smart_valid(struct ndctl_cmd *cmd)
{
	if (cmd->type != ND_CMD_SMART || cmd->status != 0)
		return cmd->status < 0 ? cmd->status : -EINVAL;
	return 0;
}

#define intel_smart_get_field(cmd, field) \
static unsigned int intel_cmd_smart_get_##field(struct ndctl_cmd *cmd) \
{ \
	struct nd_smart_payload *smart_data; \
	if (intel_smart_valid(cmd) < 0) \
		return UINT_MAX; \
	smart_data = (struct nd_smart_payload *) cmd->smart->data; \
	return smart_data->field; \
}

intel_smart_get_field(cmd, flags)
intel_smart_get_field(cmd, health)
intel_smart_get_field(cmd, temperature)
intel_smart_get_field(cmd, spares)
intel_smart_get_field(cmd, alarm_flags)
intel_smart_get_field(cmd, life_used)
intel_smart_get_field(cmd, shutdown_state)
intel_smart_get_field(cmd, shutdown_count)
intel_smart_get_field(cmd, vendor_size)

static unsigned char *intel_cmd_smart_get_vendor_data(struct ndctl_cmd *cmd)
{
	struct nd_smart_payload *smart_data;

	if (intel_smart_valid(cmd) < 0)
		return NULL;
	smart_data = (struct nd_smart_payload *) cmd->smart->data;
	return (unsigned char *) smart_data->vendor_data;
}

static int intel_smart_threshold_valid(struct ndctl_cmd *cmd)
{
	if (cmd->type != ND_CMD_SMART_THRESHOLD || cmd->status != 0)
		return cmd->status < 0 ? cmd->status : -EINVAL;
	return 0;
}

#define intel_smart_threshold_get_field(cmd, field) \
static unsigned int intel_cmd_smart_threshold_get_##field( \
			struct ndctl_cmd *cmd) \
{ \
	struct nd_smart_threshold_payload *smart_t_data; \
	if (intel_smart_threshold_valid(cmd) < 0) \
		return UINT_MAX; \
	smart_t_data = (struct nd_smart_threshold_payload *) \
			cmd->smart_t->data; \
	return smart_t_data->field; \
}

intel_smart_threshold_get_field(cmd, alarm_control)
intel_smart_threshold_get_field(cmd, temperature)
intel_smart_threshold_get_field(cmd, spares)

static struct ndctl_cmd *intel_dimm_cmd_new_smart_threshold(
		struct ndctl_dimm *dimm)
{
	struct ndctl_bus *bus = ndctl_dimm_get_bus(dimm);
	struct ndctl_ctx *ctx = ndctl_bus_get_ctx(bus);
	struct ndctl_cmd *cmd;
	size_t size;

	BUILD_ASSERT(sizeof(struct nd_smart_threshold_payload) == 8);

	if (!ndctl_dimm_is_cmd_supported(dimm, ND_CMD_SMART_THRESHOLD)) {
		dbg(ctx, "unsupported cmd\n");
		return NULL;
	}

	size = sizeof(*cmd) + sizeof(struct nd_cmd_smart_threshold);
	cmd = calloc(1, size);
	if (!cmd)
		return NULL;

	cmd->dimm = dimm;
	ndctl_cmd_ref(cmd);
	cmd->type = ND_CMD_SMART_THRESHOLD;
	cmd->size = size;
	cmd->status = 1;
	cmd->firmware_status = &cmd->smart_t->status;

	return cmd;
}

struct ndctl_smart_ops * const intel_smart_ops = &(struct ndctl_smart_ops) {
	.new_smart = intel_dimm_cmd_new_smart,
	.smart_get_flags = intel_cmd_smart_get_flags,
	.smart_get_health = intel_cmd_smart_get_health,
	.smart_get_temperature = intel_cmd_smart_get_temperature,
	.smart_get_spares = intel_cmd_smart_get_spares,
	.smart_get_alarm_flags = intel_cmd_smart_get_alarm_flags,
	.smart_get_life_used = intel_cmd_smart_get_life_used,
	.smart_get_shutdown_state = intel_cmd_smart_get_shutdown_state,
	.smart_get_shutdown_count = intel_cmd_smart_get_shutdown_count,
	.smart_get_vendor_size = intel_cmd_smart_get_vendor_size,
	.smart_get_vendor_data = intel_cmd_smart_get_vendor_data,
	.new_smart_threshold = intel_dimm_cmd_new_smart_threshold,
	.smart_threshold_get_alarm_control = intel_cmd_smart_threshold_get_alarm_control,
	.smart_threshold_get_temperature = intel_cmd_smart_threshold_get_temperature,
	.smart_threshold_get_spares = intel_cmd_smart_threshold_get_spares,
};
