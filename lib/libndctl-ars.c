/*
 * Copyright (c) 2014-2016, Intel Corporation.
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
#include <ndctl/libndctl.h>
#include "libndctl-private.h"

NDCTL_EXPORT struct ndctl_cmd *ndctl_bus_cmd_new_ars_cap(struct ndctl_bus *bus,
		unsigned long long address, unsigned long long len)
{
	struct ndctl_ctx *ctx = ndctl_bus_get_ctx(bus);
	struct ndctl_cmd *cmd;
	size_t size;

	if (!ndctl_bus_is_cmd_supported(bus, ND_CMD_ARS_CAP)) {
		dbg(ctx, "unsupported cmd\n");
		return NULL;
	}

	size = sizeof(*cmd) + sizeof(struct nd_cmd_ars_cap);
	cmd = calloc(1, size);
	if (!cmd)
		return NULL;

	cmd->bus = bus;
	ndctl_cmd_ref(cmd);
	cmd->type = ND_CMD_ARS_CAP;
	cmd->size = size;
	cmd->status = 1;
	cmd->firmware_status = &cmd->ars_cap->status;
	cmd->ars_cap->address = address;
	cmd->ars_cap->length = len;

	return cmd;
}

NDCTL_EXPORT struct ndctl_cmd *ndctl_bus_cmd_new_ars_start(struct ndctl_cmd *ars_cap,
		int type)
{
	struct ndctl_bus *bus = ars_cap->bus;
	struct ndctl_ctx *ctx = ndctl_bus_get_ctx(bus);
	struct ndctl_cmd *cmd;
	size_t size;

	if (!ndctl_bus_is_cmd_supported(bus, ND_CMD_ARS_START)) {
		dbg(ctx, "unsupported cmd\n");
		return NULL;
	}
	if (ars_cap->status != 0) {
		dbg(ctx, "expected sucessfully completed ars_cap command\n");
		return NULL;
	}
	if ((*ars_cap->firmware_status & ARS_STATUS_MASK) != 0) {
		dbg(ctx, "expected sucessfully completed ars_cap command\n");
		return NULL;
	}
	if (!(*ars_cap->firmware_status >> ARS_EXT_STATUS_SHIFT & type)) {
		dbg(ctx, "ars_cap does not show requested type as supported\n");
		return NULL;
	}

	size = sizeof(*cmd) + sizeof(struct nd_cmd_ars_start);
	cmd = calloc(1, size);
	if (!cmd)
		return NULL;

	cmd->bus = bus;
	ndctl_cmd_ref(cmd);
	cmd->type = ND_CMD_ARS_START;
	cmd->size = size;
	cmd->status = 1;
	cmd->firmware_status = &cmd->ars_start->status;
	cmd->ars_start->address = ars_cap->ars_cap->address;
	cmd->ars_start->length = ars_cap->ars_cap->length;
	cmd->ars_start->type = type;

	return cmd;
}

NDCTL_EXPORT struct ndctl_cmd *ndctl_bus_cmd_new_ars_status(struct ndctl_cmd *ars_cap)
{
	struct nd_cmd_ars_cap *ars_cap_cmd = ars_cap->ars_cap;
	struct ndctl_bus *bus = ars_cap->bus;
	struct ndctl_ctx *ctx = ndctl_bus_get_ctx(bus);
	struct ndctl_cmd *cmd;
	size_t size;

	if (!ndctl_bus_is_cmd_supported(bus, ND_CMD_ARS_CAP)) {
		dbg(ctx, "unsupported cmd\n");
		return NULL;
	}
	if (ars_cap->status != 0) {
		dbg(ctx, "expected sucessfully completed ars_cap command\n");
		return NULL;
	}
	if ((*ars_cap->firmware_status & ARS_STATUS_MASK) != 0) {
		dbg(ctx, "expected sucessfully completed ars_cap command\n");
		return NULL;
	}
	if (ars_cap_cmd->max_ars_out == 0) {
		dbg(ctx, "invalid ars_cap\n");
		return NULL;
	}

	size = sizeof(*cmd) + ars_cap_cmd->max_ars_out;
	cmd = calloc(1, size);
	if (!cmd)
		return NULL;

	cmd->bus = bus;
	ndctl_cmd_ref(cmd);
	cmd->type = ND_CMD_ARS_STATUS;
	cmd->size = size;
	cmd->status = 1;
	cmd->firmware_status = &cmd->ars_status->status;
	cmd->ars_status->out_length = ars_cap_cmd->max_ars_out;

	return cmd;
}

NDCTL_EXPORT unsigned int ndctl_cmd_ars_cap_get_size(struct ndctl_cmd *ars_cap)
{
	struct ndctl_ctx *ctx = ndctl_bus_get_ctx(cmd_to_bus(ars_cap));

	if (ars_cap->type == ND_CMD_ARS_CAP && ars_cap->status == 0) {
		dbg(ctx, "max_ars_out: %d\n",
			ars_cap->ars_cap->max_ars_out);
		return ars_cap->ars_cap->max_ars_out;
	}

	dbg(ctx, "invalid ars_cap\n");
	return 0;
}

NDCTL_EXPORT int ndctl_cmd_ars_in_progress(struct ndctl_cmd *cmd)
{
	struct ndctl_ctx *ctx = ndctl_bus_get_ctx(cmd_to_bus(cmd));

	if (cmd->type == ND_CMD_ARS_STATUS && cmd->status == 0) {
		if (cmd->ars_status->status == 1 << 16) {
			/*
			 * If in-progress, invalidate the ndctl_cmd, so
			 * that if we're called again without a fresh
			 * ars_status command, we fail.
			 */
			cmd->status = 1;
			return 1;
		}
		return 0;
	}

	dbg(ctx, "invalid ars_status\n");
	return 0;
}

NDCTL_EXPORT unsigned int ndctl_cmd_ars_num_records(struct ndctl_cmd *ars_stat)
{
	struct ndctl_ctx *ctx = ndctl_bus_get_ctx(cmd_to_bus(ars_stat));

	if (ars_stat->type == ND_CMD_ARS_STATUS && ars_stat->status == 0)
		return ars_stat->ars_status->num_records;

	dbg(ctx, "invalid ars_status\n");
	return 0;
}

NDCTL_EXPORT unsigned long long ndctl_cmd_ars_get_record_addr(
		struct ndctl_cmd *ars_stat, unsigned int rec_index)
{
	struct ndctl_ctx *ctx = ndctl_bus_get_ctx(cmd_to_bus(ars_stat));

	if (rec_index >= ars_stat->ars_status->num_records) {
		dbg(ctx, "invalid record index\n");
		return 0;
	}

	if (ars_stat->type == ND_CMD_ARS_STATUS && ars_stat->status == 0)
		return ars_stat->ars_status->records[rec_index].err_address;

	dbg(ctx, "invalid ars_status\n");
	return 0;
}

NDCTL_EXPORT unsigned long long ndctl_cmd_ars_get_record_len(
		struct ndctl_cmd *ars_stat, unsigned int rec_index)
{
	struct ndctl_ctx *ctx = ndctl_bus_get_ctx(cmd_to_bus(ars_stat));

	if (rec_index >= ars_stat->ars_status->num_records) {
		dbg(ctx, "invalid record index\n");
		return 0;
	}

	if (ars_stat->type == ND_CMD_ARS_STATUS && ars_stat->status == 0)
		return ars_stat->ars_status->records[rec_index].length;

	dbg(ctx, "invalid ars_status\n");
	return 0;
}
