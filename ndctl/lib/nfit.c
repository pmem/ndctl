// SPDX-License-Identifier: LGPL-2.1
// Copyright (c) 2017, FUJITSU LIMITED. All rights reserved.
#include <stdlib.h>
#include <ndctl/libndctl.h>
#include "private.h"
#include <ndctl/libndctl-nfit.h>

static u32 bus_get_firmware_status(struct ndctl_cmd *cmd)
{
	struct nd_cmd_bus *cmd_bus = cmd->cmd_bus;

	switch (cmd_bus->gen.nd_command) {
	case NFIT_CMD_TRANSLATE_SPA:
		return cmd_bus->xlat_spa.status;
	case NFIT_CMD_ARS_INJECT_SET:
		return cmd_bus->err_inj.status;
	case NFIT_CMD_ARS_INJECT_CLEAR:
		return cmd_bus->err_inj_clr.status;
	case NFIT_CMD_ARS_INJECT_GET:
		return cmd_bus->err_inj_stat.status;
	}

	return -1U;
}

/**
 * ndctl_bus_is_nfit_cmd_supported - ask nfit command is supported on @bus.
 * @bus: ndctl_bus instance
 * @cmd: nfit command number (defined as NFIT_CMD_XXX in libndctl-nfit.h)
 *
 * Return 1: command is supported. Return 0: command is not supported.
 *
 */
NDCTL_EXPORT int ndctl_bus_is_nfit_cmd_supported(struct ndctl_bus *bus,
                int cmd)
{
        return !!(bus->nfit_dsm_mask & (1ULL << cmd));
}

static int bus_has_translate_spa(struct ndctl_bus *bus)
{
	if (!ndctl_bus_has_nfit(bus))
		return 0;

	return ndctl_bus_is_nfit_cmd_supported(bus, NFIT_CMD_TRANSLATE_SPA);
}

static struct ndctl_cmd *ndctl_bus_cmd_new_translate_spa(struct ndctl_bus *bus)
{
	struct ndctl_cmd *cmd;
	struct nd_cmd_pkg *pkg;
	struct nd_cmd_translate_spa *translate_spa;
	size_t size, spa_length;

	spa_length = sizeof(struct nd_cmd_translate_spa)
		+ sizeof(struct nd_nvdimm_device);
	size = sizeof(*cmd) + sizeof(*pkg) + spa_length;
	cmd = calloc(1, size);
	if (!cmd)
		return NULL;

	cmd->bus = bus;
	ndctl_cmd_ref(cmd);
	cmd->type = ND_CMD_CALL;
	cmd->get_firmware_status = bus_get_firmware_status;
	cmd->size = size;
	cmd->status = 1;
	pkg = &cmd->cmd_bus->gen;
	pkg->nd_command = NFIT_CMD_TRANSLATE_SPA;
	pkg->nd_size_in = sizeof(unsigned long long);
	pkg->nd_size_out = spa_length;
	pkg->nd_fw_size = spa_length;
	translate_spa = &cmd->cmd_bus->xlat_spa;
	translate_spa->translate_length = spa_length;

	return cmd;
}

static int ndctl_bus_cmd_get_translate_spa(struct ndctl_cmd *cmd,
					unsigned int *handle, unsigned long long *dpa)
{
	struct nd_cmd_pkg *pkg;
	struct nd_cmd_translate_spa *translate_spa;

	pkg = (struct nd_cmd_pkg *)&cmd->cmd_buf[0];
	translate_spa = (struct nd_cmd_translate_spa *)&pkg->nd_payload[0];

	if (translate_spa->status == ND_TRANSLATE_SPA_STATUS_INVALID_SPA)
		return -EINVAL;

	/*
	 * XXX: Currently NVDIMM mirroring is not supported.
	 * Even if ACPI returned plural dimms due to mirroring,
	 * this function returns just the first dimm.
	 */

	*handle = translate_spa->devices[0].nfit_device_handle;
	*dpa = translate_spa->devices[0].dpa;

	return 0;
}

static int is_valid_spa(struct ndctl_bus *bus, unsigned long long spa)
{
	return !!ndctl_bus_get_region_by_physical_address(bus, spa);
}

/**
 * ndctl_bus_nfit_translate_spa - call translate spa.
 * @bus: bus which belongs to.
 * @address: address (System Physical Address)
 * @handle: pointer to return dimm handle
 * @dpa: pointer to return Dimm Physical address
 *
 * If success, returns zero, store dimm's @handle, and @dpa.
 */
int ndctl_bus_nfit_translate_spa(struct ndctl_bus *bus,
	unsigned long long address, unsigned int *handle, unsigned long long *dpa)
{

	struct ndctl_cmd *cmd;
	struct nd_cmd_pkg *pkg;
	struct nd_cmd_translate_spa *translate_spa;
	int rc;

	if (!bus || !handle || !dpa)
		return -EINVAL;

	if (!bus_has_translate_spa(bus))
		return -ENOTTY;

	if (!is_valid_spa(bus, address))
		return -EINVAL;

	cmd = ndctl_bus_cmd_new_translate_spa(bus);
	if (!cmd)
		return -ENOMEM;

	pkg = (struct nd_cmd_pkg *)&cmd->cmd_buf[0];
	translate_spa = (struct nd_cmd_translate_spa *)&pkg->nd_payload[0];
	translate_spa->spa = address;

	rc = ndctl_cmd_submit(cmd);
	if (rc < 0) {
		ndctl_cmd_unref(cmd);
		return rc;
	}

	rc = ndctl_bus_cmd_get_translate_spa(cmd, handle, dpa);
	ndctl_cmd_unref(cmd);

	return rc;
}

struct ndctl_cmd *ndctl_bus_cmd_new_err_inj(struct ndctl_bus *bus)
{
	size_t size, cmd_length;
	struct nd_cmd_pkg *pkg;
	struct ndctl_cmd *cmd;

	cmd_length = sizeof(struct nd_cmd_ars_err_inj);
	size = sizeof(*cmd) + sizeof(*pkg) + cmd_length;
	cmd = calloc(1, size);
	if (!cmd)
		return NULL;

	cmd->bus = bus;
	ndctl_cmd_ref(cmd);
	cmd->type = ND_CMD_CALL;
	cmd->get_firmware_status = bus_get_firmware_status;
	cmd->size = size;
	cmd->status = 1;
	pkg = (struct nd_cmd_pkg *)&cmd->cmd_buf[0];
	pkg->nd_command = NFIT_CMD_ARS_INJECT_SET;
	pkg->nd_size_in = offsetof(struct nd_cmd_ars_err_inj, status);
	pkg->nd_size_out = cmd_length - pkg->nd_size_in;
	pkg->nd_fw_size = pkg->nd_size_out;

	return cmd;
}

struct ndctl_cmd *ndctl_bus_cmd_new_err_inj_clr(struct ndctl_bus *bus)
{
	size_t size, cmd_length;
	struct nd_cmd_pkg *pkg;
	struct ndctl_cmd *cmd;

	cmd_length = sizeof(struct nd_cmd_ars_err_inj_clr);
	size = sizeof(*cmd) + sizeof(*pkg) + cmd_length;
	cmd = calloc(1, size);
	if (!cmd)
		return NULL;

	cmd->bus = bus;
	ndctl_cmd_ref(cmd);
	cmd->type = ND_CMD_CALL;
	cmd->get_firmware_status = bus_get_firmware_status;
	cmd->size = size;
	cmd->status = 1;
	pkg = (struct nd_cmd_pkg *)&cmd->cmd_buf[0];
	pkg->nd_command = NFIT_CMD_ARS_INJECT_CLEAR;
	pkg->nd_size_in = offsetof(struct nd_cmd_ars_err_inj_clr, status);
	pkg->nd_size_out = cmd_length - pkg->nd_size_in;
	pkg->nd_fw_size = pkg->nd_size_out;

	return cmd;
}

struct ndctl_cmd *ndctl_bus_cmd_new_err_inj_stat(struct ndctl_bus *bus,
	u32 buf_size)
{
	size_t size, cmd_length;
	struct nd_cmd_pkg *pkg;
	struct ndctl_cmd *cmd;


	cmd_length = sizeof(struct nd_cmd_ars_err_inj_stat);
	size = sizeof(*cmd) + sizeof(*pkg) + cmd_length + buf_size;
	cmd = calloc(1, size);
	if (!cmd)
		return NULL;

	cmd->bus = bus;
	ndctl_cmd_ref(cmd);
	cmd->type = ND_CMD_CALL;
	cmd->get_firmware_status = bus_get_firmware_status;
	cmd->size = size;
	cmd->status = 1;
	pkg = (struct nd_cmd_pkg *)&cmd->cmd_buf[0];
	pkg->nd_command = NFIT_CMD_ARS_INJECT_GET;
	pkg->nd_size_in = 0;
	pkg->nd_size_out = cmd_length + buf_size;
	pkg->nd_fw_size = pkg->nd_size_out;

	return cmd;
}
