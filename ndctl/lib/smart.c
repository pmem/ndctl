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
smart_cmd_op(ndctl_cmd_smart_get_media_temperature, smart_get_media_temperature,
		unsigned int, 0)
smart_cmd_op(ndctl_cmd_smart_get_spares, smart_get_spares, unsigned int, 0)
smart_cmd_op(ndctl_cmd_smart_get_alarm_flags, smart_get_alarm_flags, unsigned int, 0)
smart_cmd_op(ndctl_cmd_smart_get_life_used, smart_get_life_used, unsigned int, 0)
smart_cmd_op(ndctl_cmd_smart_get_shutdown_state, smart_get_shutdown_state, unsigned int, 0)
smart_cmd_op(ndctl_cmd_smart_get_shutdown_count, smart_get_shutdown_count, unsigned int, 0)
smart_cmd_op(ndctl_cmd_smart_get_vendor_size, smart_get_vendor_size, unsigned int, 0)
smart_cmd_op(ndctl_cmd_smart_get_vendor_data, smart_get_vendor_data, unsigned char *, NULL)
smart_cmd_op(ndctl_cmd_smart_threshold_get_alarm_control, smart_threshold_get_alarm_control, unsigned int, 0)
smart_cmd_op(ndctl_cmd_smart_threshold_get_media_temperature,
		smart_threshold_get_media_temperature, unsigned int, 0)
smart_cmd_op(ndctl_cmd_smart_threshold_get_spares, smart_threshold_get_spares, unsigned int, 0)

NDCTL_EXPORT unsigned int ndctl_cmd_smart_get_temperature(struct ndctl_cmd *cmd)
{
	return ndctl_cmd_smart_get_media_temperature(cmd);
}

NDCTL_EXPORT unsigned int ndctl_cmd_smart_threshold_get_temperature(
		struct ndctl_cmd *cmd)
{
	return ndctl_cmd_smart_threshold_get_media_temperature(cmd);
}
