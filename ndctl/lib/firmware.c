// SPDX-License-Identifier: GPL-2.0
/* Copyright (C) 2018-2020 Intel Corporation. All rights reserved. */
#include <stdlib.h>
#include <limits.h>
#include <util/log.h>
#include <ndctl/libndctl.h>
#include "private.h"

/*
 * Define the wrappers around the ndctl_dimm_ops for firmware update:
 */
NDCTL_EXPORT struct ndctl_cmd *
ndctl_dimm_cmd_new_fw_get_info(struct ndctl_dimm *dimm)
{
	struct ndctl_dimm_ops *ops = dimm->ops;

	if (ops && ops->new_fw_get_info)
		return ops->new_fw_get_info(dimm);
	else
		return NULL;
}

NDCTL_EXPORT struct ndctl_cmd *
ndctl_dimm_cmd_new_fw_start_update(struct ndctl_dimm *dimm)
{
	struct ndctl_dimm_ops *ops = dimm->ops;

	if (ops && ops->new_fw_start_update)
		return ops->new_fw_start_update(dimm);
	else
		return NULL;
}

NDCTL_EXPORT struct ndctl_cmd *
ndctl_dimm_cmd_new_fw_send(struct ndctl_cmd *start, unsigned int offset,
		unsigned int len, void *data)
{
	struct ndctl_dimm_ops *ops = start->dimm->ops;

	if (ops && ops->new_fw_send)
		return ops->new_fw_send(start, offset, len, data);
	else
		return NULL;
}

NDCTL_EXPORT struct ndctl_cmd *
ndctl_dimm_cmd_new_fw_finish(struct ndctl_cmd *start)
{
	struct ndctl_dimm_ops *ops = start->dimm->ops;

	if (ops && ops->new_fw_finish)
		return ops->new_fw_finish(start);
	else
		return NULL;
}

NDCTL_EXPORT struct ndctl_cmd *
ndctl_dimm_cmd_new_fw_abort(struct ndctl_cmd *start)
{
	struct ndctl_dimm_ops *ops = start->dimm->ops;

	if (ops && ops->new_fw_finish)
		return ops->new_fw_abort(start);
	else
		return NULL;
}

NDCTL_EXPORT struct ndctl_cmd *
ndctl_dimm_cmd_new_fw_finish_query(struct ndctl_cmd *start)
{
	struct ndctl_dimm_ops *ops = start->dimm->ops;

	if (ops && ops->new_fw_finish_query)
		return ops->new_fw_finish_query(start);
	else
		return NULL;
}


#define firmware_cmd_op(op, rettype, defretvalue) \
NDCTL_EXPORT rettype ndctl_cmd_##op(struct ndctl_cmd *cmd) \
{ \
	if (cmd->dimm) { \
		struct ndctl_dimm_ops *ops = cmd->dimm->ops; \
		if (ops && ops->op) \
			return ops->op(cmd); \
	} \
	return defretvalue; \
}

firmware_cmd_op(fw_info_get_storage_size, unsigned int, 0)
firmware_cmd_op(fw_info_get_max_send_len, unsigned int, 0)
firmware_cmd_op(fw_info_get_query_interval, unsigned int, 0)
firmware_cmd_op(fw_info_get_max_query_time, unsigned int, 0)
firmware_cmd_op(fw_info_get_run_version, unsigned long long, 0)
firmware_cmd_op(fw_info_get_updated_version, unsigned long long, 0)
firmware_cmd_op(fw_start_get_context, unsigned int, 0)
firmware_cmd_op(fw_fquery_get_fw_rev, unsigned long long, 0)

NDCTL_EXPORT enum ND_FW_STATUS
ndctl_cmd_fw_xlat_firmware_status(struct ndctl_cmd *cmd)
{
	struct ndctl_dimm_ops *ops = cmd->dimm->ops;

	if (ops && ops->fw_xlat_firmware_status)
		return ops->fw_xlat_firmware_status(cmd);
	else
		return FW_EUNKNOWN;
}

NDCTL_EXPORT int
ndctl_dimm_fw_update_supported(struct ndctl_dimm *dimm)
{
	struct ndctl_dimm_ops *ops = dimm->ops;

	if (ops && ops->fw_update_supported)
		return ops->fw_update_supported(dimm);
	else
		return -ENOTTY;
}
