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
#include "libndctl-private.h"

NDCTL_EXPORT struct ndctl_cmd *ndctl_dimm_cmd_new_smart(struct ndctl_dimm *dimm)
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

static int smart_valid(struct ndctl_cmd *cmd)
{
	if (cmd->type != ND_CMD_SMART || cmd->status != 0)
		return cmd->status < 0 ? cmd->status : -EINVAL;
	return 0;
}

#define smart_get_field(cmd, field) \
NDCTL_EXPORT unsigned int ndctl_cmd_smart_get_##field(struct ndctl_cmd *cmd) \
{ \
	struct nd_smart_payload *smart_data; \
	if (smart_valid(cmd) < 0) \
		return UINT_MAX; \
	smart_data = (struct nd_smart_payload *) cmd->smart->data; \
	return smart_data->field; \
}

smart_get_field(cmd, flags)
smart_get_field(cmd, health)
smart_get_field(cmd, temperature)
smart_get_field(cmd, spares)
smart_get_field(cmd, alarm_flags)
smart_get_field(cmd, life_used)
smart_get_field(cmd, shutdown_state)
smart_get_field(cmd, vendor_size)

NDCTL_EXPORT unsigned char *ndctl_cmd_smart_get_vendor_data(struct ndctl_cmd *cmd)
{
	struct nd_smart_payload *smart_data;

	if (smart_valid(cmd) < 0)
		return NULL;
	smart_data = (struct nd_smart_payload *) cmd->smart->data;
	return (unsigned char *) smart_data->vendor_data;
}

static int smart_threshold_valid(struct ndctl_cmd *cmd)
{
	if (cmd->type != ND_CMD_SMART_THRESHOLD || cmd->status != 0)
		return cmd->status < 0 ? cmd->status : -EINVAL;
	return 0;
}

#define smart_threshold_get_field(cmd, field) \
NDCTL_EXPORT unsigned int ndctl_cmd_smart_threshold_get_##field( \
			struct ndctl_cmd *cmd) \
{ \
	struct nd_smart_threshold_payload *smart_t_data; \
	if (smart_threshold_valid(cmd) < 0) \
		return UINT_MAX; \
	smart_t_data = (struct nd_smart_threshold_payload *) \
			cmd->smart_t->data; \
	return smart_t_data->field; \
}

smart_threshold_get_field(cmd, alarm_control)
smart_threshold_get_field(cmd, temperature)
smart_threshold_get_field(cmd, spares)

NDCTL_EXPORT struct ndctl_cmd *ndctl_dimm_cmd_new_smart_threshold(
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
