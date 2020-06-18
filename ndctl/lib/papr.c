// SPDX-License-Identifier: LGPL-2.1
/*
 * libndctl support for PAPR-SCM based NVDIMMs
 *
 * (C) Copyright IBM 2020
 *
 */

#include <stdint.h>
#include <stdlib.h>
#include <limits.h>
#include <util/log.h>
#include <ndctl.h>
#include <ndctl/libndctl.h>
#include <lib/private.h>
#include "papr.h"

/* Utility logging maros for simplify logging */
#define papr_dbg(_dimm, _format_str, ...) dbg(_dimm->bus->ctx,		\
					      "%s:" _format_str,	\
					      ndctl_dimm_get_devname(_dimm), \
					      ##__VA_ARGS__)

#define papr_err(_dimm, _format_str, ...) err(_dimm->bus->ctx,		\
					      "%s:" _format_str,	\
					      ndctl_dimm_get_devname(_dimm), \
					      ##__VA_ARGS__)

/* Convert a ndctl_cmd to pdsm package */
#define to_pdsm(C)  (&(C)->papr[0].pdsm)

/* Convert a ndctl_cmd to nd_cmd_pkg */
#define to_ndcmd(C)  (&(C)->papr[0].gen)

/* Return payload from a ndctl_cmd */
#define to_payload(C) (&(C)->papr[0].pdsm.payload)

/* return the pdsm command */
#define to_pdsm_cmd(C) ((enum papr_pdsm)to_ndcmd(C)->nd_command)

static bool papr_cmd_is_supported(struct ndctl_dimm *dimm, int cmd)
{
	/* Handle this separately to support monitor mode */
	if (cmd == ND_CMD_SMART)
		return true;

	return !!(dimm->cmd_mask & (1ULL << cmd));
}

static u32 papr_get_firmware_status(struct ndctl_cmd *cmd)
{
	const struct nd_pkg_pdsm *pcmd = to_pdsm(cmd);

	return (u32) pcmd->cmd_status;
}

static int papr_xlat_firmware_status(struct ndctl_cmd *cmd)
{
	const struct nd_pkg_pdsm *pcmd = to_pdsm(cmd);

	return pcmd->cmd_status;
}

/* Verify if the given command is supported and valid */
static bool cmd_is_valid(struct ndctl_cmd *cmd)
{
	const struct nd_cmd_pkg  *ncmd = NULL;

	if (cmd == NULL)
		return false;

	ncmd = to_ndcmd(cmd);

	/* Verify the command family */
	if (ncmd->nd_family != NVDIMM_FAMILY_PAPR) {
		papr_err(cmd->dimm, "Invalid command family:0x%016llx\n",
			 ncmd->nd_family);
		return false;
	}

	/* Verify the PDSM */
	if (ncmd->nd_command <= PAPR_PDSM_MIN ||
	    ncmd->nd_command >= PAPR_PDSM_MAX) {
		papr_err(cmd->dimm, "Invalid command :0x%016llx\n",
			 ncmd->nd_command);
		return false;
	}

	return true;
}

/* Allocate a struct ndctl_cmd for given pdsm request with payload size */
static struct ndctl_cmd *allocate_cmd(struct ndctl_dimm *dimm,
				      enum papr_pdsm pdsm_cmd,
				      size_t payload_size)
{
	struct ndctl_cmd *cmd;

	/* Verify that payload size is within acceptable range */
	if (payload_size > ND_PDSM_PAYLOAD_MAX_SIZE) {
		papr_err(dimm, "Requested payload size too large %lu bytes\n",
			 payload_size);
		return NULL;
	}

	cmd = calloc(1, sizeof(struct ndctl_cmd) + sizeof(struct nd_pkg_papr));
	if (!cmd)
		return NULL;

	ndctl_cmd_ref(cmd);
	cmd->dimm = dimm;
	cmd->type = ND_CMD_CALL;
	cmd->status = 0;
	cmd->get_firmware_status = &papr_get_firmware_status;

	/* Populate the nd_cmd_pkg contained in nd_pkg_pdsm */
	*to_ndcmd(cmd) =  (struct nd_cmd_pkg) {
		.nd_family = NVDIMM_FAMILY_PAPR,
		.nd_command = pdsm_cmd,
		.nd_size_in = 0,
		.nd_size_out = ND_PDSM_HDR_SIZE + payload_size,
		.nd_fw_size = 0,
	};
	return cmd;
}

/* Validate the ndctl_cmd and return applicable flags */
static unsigned int papr_smart_get_flags(struct ndctl_cmd *cmd)
{
	struct nd_pkg_pdsm *pcmd;

	if (!cmd_is_valid(cmd))
		return 0;

	pcmd = to_pdsm(cmd);
	/* If error reported then return empty flags */
	if (pcmd->cmd_status) {
		papr_err(cmd->dimm, "PDSM(0x%x) reported error:%d\n",
			 to_pdsm_cmd(cmd), pcmd->cmd_status);
		return 0;
	}

	/* return empty flags for now */
	return 0;
}


struct ndctl_dimm_ops * const papr_dimm_ops = &(struct ndctl_dimm_ops) {
	.cmd_is_supported = papr_cmd_is_supported,
	.smart_get_flags = papr_smart_get_flags,
	.get_firmware_status =  papr_get_firmware_status,
	.xlat_firmware_status = papr_xlat_firmware_status,
};
