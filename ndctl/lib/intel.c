// SPDX-License-Identifier: LGPL-2.1
// Copyright (C) 2016-2020, Intel Corporation. All rights reserved.
#include <stdlib.h>
#include <limits.h>
#include <util/log.h>
#include <ndctl/libndctl.h>
#include "private.h"

static unsigned int intel_cmd_get_firmware_status(struct ndctl_cmd *cmd)
{
	struct nd_pkg_intel *intel = cmd->intel;

	switch (intel->gen.nd_command) {
	case ND_INTEL_SMART:
		return intel->smart.status;
	case ND_INTEL_SMART_THRESHOLD:
		return intel->thresh.status;
	case ND_INTEL_SMART_SET_THRESHOLD:
		return intel->set_thresh.status;
	case ND_INTEL_SMART_INJECT:
		return intel->inject.status;
	case ND_INTEL_FW_GET_INFO:
		return intel->info.status;
	case ND_INTEL_FW_START_UPDATE:
		return intel->start.status;
	case ND_INTEL_FW_SEND_DATA: {
		    struct nd_intel_fw_send_data *send = &intel->send;
		    u32 status;

		    /* the last dword after the payload is reserved for status */
		    memcpy(&status, ((void *) send) + sizeof(*send) + send->length,
				    sizeof(status));
		    return status;
	}
	case ND_INTEL_FW_FINISH_UPDATE:
		return intel->finish.status;
	case ND_INTEL_FW_FINISH_STATUS_QUERY:
		return intel->fquery.status;
	case ND_INTEL_ENABLE_LSS_STATUS:
		return intel->lss.status;
	}
	return -1U;
}

static int intel_cmd_xlat_firmware_status(struct ndctl_cmd *cmd)
{
	struct nd_pkg_intel *pkg = cmd->intel;
	unsigned int status, ext_status;

	status = cmd->get_firmware_status(cmd) & ND_INTEL_STATUS_MASK;
	ext_status = cmd->get_firmware_status(cmd) & ND_INTEL_STATUS_EXTEND_MASK;

	/* Common statuses */
	switch (status) {
	case ND_INTEL_STATUS_SUCCESS:
		return 0;
	case ND_INTEL_STATUS_NOTSUPP:
		return -EOPNOTSUPP;
	case ND_INTEL_STATUS_NOTEXIST:
		return -ENXIO;
	case ND_INTEL_STATUS_INVALPARM:
		return -EINVAL;
	case ND_INTEL_STATUS_HWERR:
		return -EIO;
	case ND_INTEL_STATUS_RETRY:
		return -EAGAIN;
	case ND_INTEL_STATUS_EXTEND:
		/* refer to extended status, break out of this */
		break;
	case ND_INTEL_STATUS_NORES:
		return -EAGAIN;
	case ND_INTEL_STATUS_NOTREADY:
		return -EBUSY;
	}

	/* Extended status is command specific */
	switch (pkg->gen.nd_command) {
	case ND_INTEL_SMART:
	case ND_INTEL_SMART_THRESHOLD:
	case ND_INTEL_SMART_SET_THRESHOLD:
		/* ext status not specified */
		break;
	case ND_INTEL_SMART_INJECT:
		/* smart injection not enabled */
		if (ext_status == ND_INTEL_STATUS_INJ_DISABLED)
			return -ENXIO;
		break;
	}

	return -ENOMSG;
}

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

	size = sizeof(*cmd) + sizeof(struct nd_pkg_intel) + in_size + out_size;
	cmd = calloc(1, size);
	if (!cmd)
		return NULL;

	cmd->dimm = dimm;
	ndctl_cmd_ref(cmd);
	cmd->type = ND_CMD_CALL;
	cmd->size = size;
	cmd->status = 1;
	cmd->get_firmware_status = intel_cmd_get_firmware_status;

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
	int rc; \
	rc = intel_smart_valid(cmd); \
	if (rc < 0) { \
		errno = -rc; \
		return UINT_MAX; \
	} \
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
	int rc; \
	rc = intel_smart_threshold_valid(cmd); \
	if (rc < 0) { \
		errno = -rc; \
		return UINT_MAX; \
	} \
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

static int intel_smart_inject_valid(struct ndctl_cmd *cmd)
{
	struct nd_pkg_intel *pkg = cmd->intel;

	if (cmd->type != ND_CMD_CALL || cmd->status != 1
			|| pkg->gen.nd_family != NVDIMM_FAMILY_INTEL
			|| pkg->gen.nd_command != ND_INTEL_SMART_INJECT)
		return cmd->status < 0 ? cmd->status : -EINVAL;
	return 0;
}

static struct ndctl_cmd *intel_new_smart_inject(struct ndctl_dimm *dimm)
{
	struct ndctl_cmd *cmd;

	BUILD_ASSERT(sizeof(struct nd_intel_smart_inject) == 19);

	cmd = alloc_intel_cmd(dimm, ND_INTEL_SMART_INJECT,
			offsetof(struct nd_intel_smart_inject, status), 4);
	if (!cmd)
		return NULL;

	return cmd;
}

static int intel_cmd_smart_inject_media_temperature(struct ndctl_cmd *cmd,
		bool enable, unsigned int mtemp)
{
	struct nd_intel_smart_inject *inj;

	if (intel_smart_inject_valid(cmd) < 0)
		return -EINVAL;
	inj = &cmd->intel->inject;
	inj->flags |= ND_INTEL_SMART_INJECT_MTEMP;
	inj->mtemp_enable = enable == true;
	inj->media_temperature = mtemp;
	return 0;
}

static int intel_cmd_smart_inject_spares(struct ndctl_cmd *cmd,
		bool enable, unsigned int spares)
{
	struct nd_intel_smart_inject *inj;

	if (intel_smart_inject_valid(cmd) < 0)
		return -EINVAL;
	inj = &cmd->intel->inject;
	inj->flags |= ND_INTEL_SMART_INJECT_SPARE;
	inj->spare_enable = enable == true;
	inj->spares = spares;
	return 0;
}

static int intel_cmd_smart_inject_fatal(struct ndctl_cmd *cmd, bool enable)
{
	struct nd_intel_smart_inject *inj;

	if (intel_smart_inject_valid(cmd) < 0)
		return -EINVAL;
	inj = &cmd->intel->inject;
	inj->flags |= ND_INTEL_SMART_INJECT_FATAL;
	inj->fatal_enable = enable == true;
	return 0;
}

static int intel_cmd_smart_inject_unsafe_shutdown(struct ndctl_cmd *cmd,
		bool enable)
{
	struct nd_intel_smart_inject *inj;

	if (intel_smart_inject_valid(cmd) < 0)
		return -EINVAL;
	inj = &cmd->intel->inject;
	inj->flags |= ND_INTEL_SMART_INJECT_SHUTDOWN;
	inj->unsafe_shutdown_enable = enable == true;
	return 0;
}

static int intel_dimm_smart_inject_supported(struct ndctl_dimm *dimm)
{
	struct ndctl_ctx *ctx = ndctl_dimm_get_ctx(dimm);

	if (!ndctl_dimm_is_cmd_supported(dimm, ND_CMD_CALL)) {
		dbg(ctx, "unsupported cmd: %d\n", ND_CMD_CALL);
		return -EOPNOTSUPP;
	}

	if (!test_dimm_dsm(dimm, ND_INTEL_SMART_INJECT)) {
		dbg(ctx, "smart injection functions unsupported\n");
		return -EIO;
	}

	return 0;
}

static const char *intel_cmd_desc(int fn)
{
	static const char *descs[] = {
		[ND_INTEL_SMART] = "smart",
		[ND_INTEL_SMART_INJECT] = "smart_inject",
		[ND_INTEL_SMART_THRESHOLD] = "smart_thresh",
		[ND_INTEL_FW_GET_INFO] = "firmware_get_info",
		[ND_INTEL_FW_START_UPDATE] = "firmware_start_update",
		[ND_INTEL_FW_SEND_DATA] = "firmware_send_data",
		[ND_INTEL_FW_FINISH_UPDATE] = "firmware_finish_update",
		[ND_INTEL_FW_FINISH_STATUS_QUERY] = "firmware_finish_query",
		[ND_INTEL_SMART_SET_THRESHOLD] = "smart_set_thresh",
	};
	const char *desc = descs[fn];

	if (fn >= (int) ARRAY_SIZE(descs))
		return "unknown";
	if (!desc)
		return "unknown";
	return desc;
}

static struct ndctl_cmd *intel_dimm_cmd_new_fw_get_info(struct ndctl_dimm *dimm)
{
	struct ndctl_cmd *cmd;

	BUILD_ASSERT(sizeof(struct nd_intel_fw_info) == 44);

	cmd = alloc_intel_cmd(dimm, ND_INTEL_FW_GET_INFO,
			0, sizeof(cmd->intel->info));
	if (!cmd)
		return NULL;

	return cmd;
}

static int intel_fw_get_info_valid(struct ndctl_cmd *cmd)
{
	struct nd_pkg_intel *pkg = cmd->intel;

	if (cmd->type != ND_CMD_CALL || cmd->status != 0
			|| pkg->gen.nd_family != NVDIMM_FAMILY_INTEL
			|| pkg->gen.nd_command != ND_INTEL_FW_GET_INFO)
		return -EINVAL;
	return 0;
}

#define intel_fw_info_get_field32(cmd, field) \
static unsigned int intel_cmd_fw_info_get_##field( \
			struct ndctl_cmd *cmd) \
{ \
	int rc; \
	rc = intel_fw_get_info_valid(cmd); \
	if (rc < 0) { \
		errno = -rc; \
		return UINT_MAX; \
	} \
	return cmd->intel->info.field; \
}

#define intel_fw_info_get_field64(cmd, field) \
static unsigned long long intel_cmd_fw_info_get_##field( \
			struct ndctl_cmd *cmd) \
{ \
	int rc; \
	rc = intel_fw_get_info_valid(cmd); \
	if (rc < 0) { \
		errno = -rc; \
		return ULLONG_MAX; \
	} \
	return cmd->intel->info.field; \
}

intel_fw_info_get_field32(cmd, storage_size)
intel_fw_info_get_field32(cmd, max_send_len)
intel_fw_info_get_field32(cmd, query_interval)
intel_fw_info_get_field32(cmd, max_query_time);
intel_fw_info_get_field64(cmd, run_version);

static unsigned long long intel_cmd_fw_info_get_updated_version(
		struct ndctl_cmd *cmd)
{
	int rc;

	rc = intel_fw_get_info_valid(cmd);
	if (rc < 0) {
		errno = -rc;
		return ULLONG_MAX;
	}
	return cmd->intel->info.updated_version;

}

static struct ndctl_cmd *intel_dimm_cmd_new_fw_start(struct ndctl_dimm *dimm)
{
	struct ndctl_cmd *cmd;

	BUILD_ASSERT(sizeof(struct nd_intel_fw_start) == 8);

	cmd = alloc_intel_cmd(dimm, ND_INTEL_FW_START_UPDATE, 0,
			sizeof(cmd->intel->start));
	if (!cmd)
		return NULL;

	return cmd;
}

static int intel_fw_start_valid(struct ndctl_cmd *cmd)
{
	struct nd_pkg_intel *pkg = cmd->intel;

	if (cmd->type != ND_CMD_CALL || cmd->status != 0
			|| pkg->gen.nd_family != NVDIMM_FAMILY_INTEL
			|| pkg->gen.nd_command != ND_INTEL_FW_START_UPDATE)
		return -EINVAL;
	return 0;
}

static unsigned int intel_cmd_fw_start_get_context(struct ndctl_cmd *cmd)
{
	int rc;

	rc = intel_fw_start_valid(cmd);
	if (rc < 0) {
		errno = -rc;
		return UINT_MAX;
	}
	return cmd->intel->start.context;
}

static struct ndctl_cmd *intel_dimm_cmd_new_fw_send(struct ndctl_cmd *start,
		unsigned int offset, unsigned int len, void *data)
{
	struct ndctl_cmd *cmd;

	BUILD_ASSERT(sizeof(struct nd_intel_fw_send_data) == 12);

	cmd = alloc_intel_cmd(start->dimm, ND_INTEL_FW_SEND_DATA,
		sizeof(cmd->intel->send) + len, 4);
	if (!cmd)
		return NULL;

	cmd->intel->send.context = start->intel->start.context;
	cmd->intel->send.offset = offset;
	cmd->intel->send.length = len;
	memcpy(cmd->intel->send.data, data, len);
	return cmd;
}

static struct ndctl_cmd *intel_dimm_cmd_new_fw_finish(struct ndctl_cmd *start)
{
	struct ndctl_cmd *cmd;

	BUILD_ASSERT(sizeof(struct nd_intel_fw_finish_update) == 12);

	cmd = alloc_intel_cmd(start->dimm, ND_INTEL_FW_FINISH_UPDATE,
			offsetof(struct nd_intel_fw_finish_update, status), 4);
	if (!cmd)
		return NULL;

	cmd->intel->finish.context = start->intel->start.context;
	cmd->intel->finish.ctrl_flags = 0;
	return cmd;
}

static struct ndctl_cmd *intel_dimm_cmd_new_fw_abort(struct ndctl_cmd *start)
{
	struct ndctl_cmd *cmd;

	BUILD_ASSERT(sizeof(struct nd_intel_fw_finish_update) == 12);

	cmd = alloc_intel_cmd(start->dimm, ND_INTEL_FW_FINISH_UPDATE,
			sizeof(cmd->intel->finish) - 4, 4);
	if (!cmd)
		return NULL;

	cmd->intel->finish.context = start->intel->start.context;
	cmd->intel->finish.ctrl_flags = 1;
	return cmd;
}

static struct ndctl_cmd *
intel_dimm_cmd_new_fw_finish_query(struct ndctl_cmd *start)
{
	struct ndctl_cmd *cmd;

	BUILD_ASSERT(sizeof(struct nd_intel_fw_finish_query) == 16);

	cmd = alloc_intel_cmd(start->dimm, ND_INTEL_FW_FINISH_STATUS_QUERY,
			4, sizeof(cmd->intel->fquery) - 4);
	if (!cmd)
		return NULL;

	cmd->intel->fquery.context = start->intel->start.context;
	return cmd;
}

static int intel_fw_fquery_valid(struct ndctl_cmd *cmd)
{
	struct nd_pkg_intel *pkg = cmd->intel;

	if (cmd->type != ND_CMD_CALL || cmd->status != 0
			|| pkg->gen.nd_family != NVDIMM_FAMILY_INTEL
			|| pkg->gen.nd_command != ND_INTEL_FW_FINISH_STATUS_QUERY)
		return -EINVAL;
	return 0;
}

static unsigned long long
intel_cmd_fw_fquery_get_fw_rev(struct ndctl_cmd *cmd)
{
	int rc;

	rc = intel_fw_fquery_valid(cmd);
	if (rc < 0) {
		errno = -rc;
		return ULLONG_MAX;
	}
	return cmd->intel->fquery.updated_fw_rev;
}

static enum ND_FW_STATUS
intel_cmd_fw_xlat_extend_firmware_status(struct ndctl_cmd *cmd,
		unsigned int status)
{
	/*
	 * Note: the cases commented out are identical to the ones that are
	 * not. They are there for reference.
	 */
	switch (status & ND_INTEL_STATUS_EXTEND_MASK) {
	case ND_INTEL_STATUS_START_BUSY:
	/* case ND_INTEL_STATUS_SEND_CTXINVAL: */
	/* case ND_INTEL_STATUS_FIN_CTXINVAL: */
	/* case ND_INTEL_STATUS_FQ_CTXINVAL: */
		if (cmd->intel->gen.nd_command == ND_INTEL_FW_START_UPDATE)
			return FW_EBUSY;
		else
			return FW_EINVAL_CTX;
	case ND_INTEL_STATUS_FIN_DONE:
	/* case ND_INTEL_STATUS_FQ_BUSY: */
		if (cmd->intel->gen.nd_command == ND_INTEL_FW_FINISH_UPDATE)
			return FW_ALREADY_DONE;
		else
			return FW_EBUSY;
	case ND_INTEL_STATUS_FIN_BAD:
	/* case ND_INTEL_STATUS_FQ_BAD: */
		return FW_EBADFW;
	case ND_INTEL_STATUS_FIN_ABORTED:
	/* case ND_INTEL_STATUS_FQ_ORDER: */
		if (cmd->intel->gen.nd_command == ND_INTEL_FW_FINISH_UPDATE)
			return FW_ABORTED;
		else
			return FW_ESEQUENCE;
	}

	return FW_EUNKNOWN;
}

static enum ND_FW_STATUS
intel_cmd_fw_xlat_firmware_status(struct ndctl_cmd *cmd)
{
	unsigned int status = intel_cmd_get_firmware_status(cmd);

	switch (status & ND_INTEL_STATUS_MASK) {
	case ND_INTEL_STATUS_SUCCESS:
		return FW_SUCCESS;
	case ND_INTEL_STATUS_NOTSUPP:
		return FW_ENOTSUPP;
	case ND_INTEL_STATUS_NOTEXIST:
		return FW_ENOTEXIST;
	case ND_INTEL_STATUS_INVALPARM:
		return FW_EINVAL;
	case ND_INTEL_STATUS_HWERR:
		return FW_EHWERR;
	case ND_INTEL_STATUS_RETRY:
		return FW_ERETRY;
	case ND_INTEL_STATUS_EXTEND:
		return intel_cmd_fw_xlat_extend_firmware_status(cmd, status);
	case ND_INTEL_STATUS_NORES:
		return FW_ENORES;
	case ND_INTEL_STATUS_NOTREADY:
		return FW_ENOTREADY;
	}

	return FW_EUNKNOWN;
}

static struct ndctl_cmd *
intel_dimm_cmd_new_lss(struct ndctl_dimm *dimm)
{
	struct ndctl_cmd *cmd;

	BUILD_ASSERT(sizeof(struct nd_intel_lss) == 5);

	cmd = alloc_intel_cmd(dimm, ND_INTEL_ENABLE_LSS_STATUS, 1, 4);
	if (!cmd)
		return NULL;

	cmd->intel->lss.enable = 1;
	return cmd;
}

static int intel_dimm_fw_update_supported(struct ndctl_dimm *dimm)
{
	struct ndctl_ctx *ctx = ndctl_dimm_get_ctx(dimm);

	if (!ndctl_dimm_is_cmd_supported(dimm, ND_CMD_CALL)) {
		dbg(ctx, "unsupported cmd: %d\n", ND_CMD_CALL);
		return -EOPNOTSUPP;
	}

	if (test_dimm_dsm(dimm, ND_INTEL_FW_GET_INFO) ==
			DIMM_DSM_UNSUPPORTED ||
			test_dimm_dsm(dimm, ND_INTEL_FW_START_UPDATE) ==
			DIMM_DSM_UNSUPPORTED ||
			test_dimm_dsm(dimm, ND_INTEL_FW_SEND_DATA) ==
			DIMM_DSM_UNSUPPORTED ||
			test_dimm_dsm(dimm, ND_INTEL_FW_FINISH_UPDATE) ==
			DIMM_DSM_UNSUPPORTED ||
			test_dimm_dsm(dimm, ND_INTEL_FW_FINISH_STATUS_QUERY) ==
			DIMM_DSM_UNSUPPORTED) {
		dbg(ctx, "unsupported function: %d\n",
				ND_INTEL_FW_GET_INFO);
		return -EIO;
	}

	return 0;
}

struct ndctl_dimm_ops * const intel_dimm_ops = &(struct ndctl_dimm_ops) {
	.cmd_desc = intel_cmd_desc,
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
	.new_smart_inject = intel_new_smart_inject,
	.smart_inject_media_temperature = intel_cmd_smart_inject_media_temperature,
	.smart_inject_spares = intel_cmd_smart_inject_spares,
	.smart_inject_fatal = intel_cmd_smart_inject_fatal,
	.smart_inject_unsafe_shutdown = intel_cmd_smart_inject_unsafe_shutdown,
	.smart_inject_supported = intel_dimm_smart_inject_supported,
	.new_fw_get_info = intel_dimm_cmd_new_fw_get_info,
	.fw_info_get_storage_size = intel_cmd_fw_info_get_storage_size,
	.fw_info_get_max_send_len = intel_cmd_fw_info_get_max_send_len,
	.fw_info_get_query_interval = intel_cmd_fw_info_get_query_interval,
	.fw_info_get_max_query_time = intel_cmd_fw_info_get_max_query_time,
	.fw_info_get_run_version = intel_cmd_fw_info_get_run_version,
	.fw_info_get_updated_version = intel_cmd_fw_info_get_updated_version,
	.new_fw_start_update = intel_dimm_cmd_new_fw_start,
	.fw_start_get_context = intel_cmd_fw_start_get_context,
	.new_fw_send = intel_dimm_cmd_new_fw_send,
	.new_fw_finish = intel_dimm_cmd_new_fw_finish,
	.new_fw_abort = intel_dimm_cmd_new_fw_abort,
	.new_fw_finish_query = intel_dimm_cmd_new_fw_finish_query,
	.fw_fquery_get_fw_rev = intel_cmd_fw_fquery_get_fw_rev,
	.fw_xlat_firmware_status = intel_cmd_fw_xlat_firmware_status,
	.new_ack_shutdown_count = intel_dimm_cmd_new_lss,
	.fw_update_supported = intel_dimm_fw_update_supported,
	.xlat_firmware_status = intel_cmd_xlat_firmware_status,
};
