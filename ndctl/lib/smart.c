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
 * Define the wrappers around the ndctl_dimm_ops:
 */

NDCTL_EXPORT struct ndctl_cmd *ndctl_dimm_cmd_new_smart(
		struct ndctl_dimm *dimm)
{
	struct ndctl_dimm_ops *ops = dimm->ops;

	if (ops && ops->new_smart)
		return ops->new_smart(dimm);
	else
		return NULL;
}

NDCTL_EXPORT struct ndctl_cmd *ndctl_dimm_cmd_new_smart_threshold(
		struct ndctl_dimm *dimm)
{
	struct ndctl_dimm_ops *ops = dimm->ops;

	if (ops && ops->new_smart_threshold)
		return ops->new_smart_threshold(dimm);
	else
		return NULL;
}

/*
 * smart_set_threshold is a read-modify-write command it depends on a
 * successfully completed smart_threshold command for its defaults.
 */
NDCTL_EXPORT struct ndctl_cmd *ndctl_dimm_cmd_new_smart_set_threshold(
		struct ndctl_cmd *cmd)
{
	struct ndctl_dimm_ops *ops;

	if (!cmd || !cmd->dimm)
		return NULL;
	ops = cmd->dimm->ops;

	if (ops && ops->new_smart_set_threshold)
		return ops->new_smart_set_threshold(cmd);
	else
		return NULL;
}

#define smart_cmd_op(op, rettype, defretvalue) \
NDCTL_EXPORT rettype ndctl_cmd_##op(struct ndctl_cmd *cmd) \
{ \
	if (cmd->dimm) { \
		struct ndctl_dimm_ops *ops = cmd->dimm->ops; \
		if (ops && ops->op) \
			return ops->op(cmd); \
	} \
	return defretvalue; \
}

smart_cmd_op(smart_get_flags, unsigned int, 0)
smart_cmd_op(smart_get_health, unsigned int, 0)
smart_cmd_op(smart_get_media_temperature, unsigned int, 0)
smart_cmd_op(smart_get_ctrl_temperature, unsigned int, 0)
smart_cmd_op(smart_get_spares, unsigned int, 0)
smart_cmd_op(smart_get_alarm_flags, unsigned int, 0)
smart_cmd_op(smart_get_life_used, unsigned int, 0)
smart_cmd_op(smart_get_shutdown_state, unsigned int, 0)
smart_cmd_op(smart_get_shutdown_count, unsigned int, 0)
smart_cmd_op(smart_get_vendor_size, unsigned int, 0)
smart_cmd_op(smart_get_vendor_data, unsigned char *, NULL)
smart_cmd_op(smart_threshold_get_alarm_control, unsigned int, 0)
smart_cmd_op(smart_threshold_get_media_temperature, unsigned int, 0)
smart_cmd_op(smart_threshold_get_ctrl_temperature, unsigned int, 0)
smart_cmd_op(smart_threshold_get_spares, unsigned int, 0)

NDCTL_EXPORT unsigned int ndctl_cmd_smart_get_temperature(struct ndctl_cmd *cmd)
{
	return ndctl_cmd_smart_get_media_temperature(cmd);
}

NDCTL_EXPORT unsigned int ndctl_cmd_smart_threshold_get_temperature(
		struct ndctl_cmd *cmd)
{
	return ndctl_cmd_smart_threshold_get_media_temperature(cmd);
}

smart_cmd_op(smart_threshold_get_supported_alarms, unsigned int, 0);

#define smart_cmd_set_op(op) \
NDCTL_EXPORT int ndctl_cmd_##op(struct ndctl_cmd *cmd, unsigned int val) \
{ \
	if (cmd->dimm) { \
		struct ndctl_dimm_ops *ops = cmd->dimm->ops; \
		if (ops && ops->op) \
			return ops->op(cmd, val); \
	} \
	return -ENXIO; \
}

smart_cmd_set_op(smart_threshold_set_alarm_control)
smart_cmd_set_op(smart_threshold_set_media_temperature)
smart_cmd_set_op(smart_threshold_set_ctrl_temperature)
smart_cmd_set_op(smart_threshold_set_spares)
