// SPDX-License-Identifier: GPL-2.0
/* Copyright (C) 2020-2021 Intel Corporation. All rights reserved. */
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <util/log.h>
#include <util/filter.h>
#include <util/parse-options.h>
#include <ccan/list/list.h>
#include <ccan/minmax/minmax.h>
#include <ccan/array_size/array_size.h>
#include <ccan/endian/endian.h>
#include <ccan/short_types/short_types.h>
#include <cxl/libcxl.h>



struct action_context {
	FILE *f_out;
	FILE *f_in;
};

static struct parameters {
	const char *outfile;
	const char *infile;
	unsigned len;
	unsigned offset;
	bool verbose;
} param;

#define fail(fmt, ...) \
do { \
	fprintf(stderr, "cxl-%s:%s:%d: " fmt, \
			VERSION, __func__, __LINE__, ##__VA_ARGS__); \
} while (0)

#define BASE_OPTIONS() \
OPT_BOOLEAN('v',"verbose", &param.verbose, "turn on debug")

#define READ_OPTIONS() \
OPT_STRING('o', "output", &param.outfile, "output-file", \
	"filename to write label area contents")

#define WRITE_OPTIONS() \
OPT_STRING('i', "input", &param.infile, "input-file", \
	"filename to read label area data")

#define LABEL_OPTIONS() \
OPT_UINTEGER('s', "size", &param.len, "number of label bytes to operate"), \
OPT_UINTEGER('O', "offset", &param.offset, \
	"offset into the label area to start operation")

static const struct option read_options[] = {
	BASE_OPTIONS(),
	LABEL_OPTIONS(),
	READ_OPTIONS(),
	OPT_END(),
};

static const struct option write_options[] = {
	BASE_OPTIONS(),
	LABEL_OPTIONS(),
	WRITE_OPTIONS(),
	OPT_END(),
};

static const struct option zero_options[] = {
	BASE_OPTIONS(),
	LABEL_OPTIONS(),
	OPT_END(),
};

static const struct option cmd_identify_options[] = {
	BASE_OPTIONS(),
	OPT_END(),
};

static const struct option cmd_get_supported_logs_options[] = {
	BASE_OPTIONS(),
	OPT_END(),
};

static const struct option cmd_get_cel_log_options[] = {
	BASE_OPTIONS(),
	OPT_END(),
};

static const struct option cmd_get_event_interrupt_policy_options[] = {
	BASE_OPTIONS(),
	OPT_END(),
};

static struct _interrupt_policy_params {
	u32 policy;
	bool verbose;
} interrupt_policy_params;

#define INT_POLICY_BASE_OPTIONS() \
OPT_BOOLEAN('v',"verbose", &interrupt_policy_params.verbose, "turn on debug")

#define SET_INTERRUPT_POLICY_OPTIONS() \
OPT_UINTEGER('i', "int_policy", &interrupt_policy_params.policy, "Set event interrupt policy. Fields: Informational Event Log Interrupt Settings (1B), Warning Event Log Interrupt Settings (1B), Failure Event Log Interrupt Settings (1B), Fatal Event Log Interrupt Settings (1B)")

static const struct option cmd_set_event_interrupt_policy_options[] = {
	INT_POLICY_BASE_OPTIONS(),
	SET_INTERRUPT_POLICY_OPTIONS(),
	OPT_END(),
};

static const struct option cmd_get_timestamp_options[] = {
	BASE_OPTIONS(),
	OPT_END(),
};

static struct _ts_params {
	u64 timestamp;
	bool verbose;
} ts_params;

#define TS_BASE_OPTIONS() \
OPT_BOOLEAN('v',"verbose", &ts_params.verbose, "turn on debug")

#define SET_TIMESTAMP_OPTIONS() \
OPT_U64('t', "timestamp", &ts_params.timestamp, "Set the timestamp on the device")

static const struct option cmd_set_timestamp_options[] = {
	TS_BASE_OPTIONS(),
	SET_TIMESTAMP_OPTIONS(),
	OPT_END(),
};

static struct _update_fw_params {
	const char *filepath;
	u32 slot;
	bool hbo;
	bool verbose;
} update_fw_params;

#define UPDATE_FW_BASE_OPTIONS() \
OPT_BOOLEAN('v',"verbose", &update_fw_params.verbose, "turn on debug")

#define UPDATE_FW_OPTIONS() \
OPT_FILENAME('f', "file", &update_fw_params.filepath, "rom-file", \
	"filepath to read ROM for firmware update"), \
OPT_UINTEGER('s', "slot", &update_fw_params.slot, "slot to use for firmware loading"), \
OPT_BOOLEAN('b', "background", &update_fw_params.hbo, "runs as hidden background option")

static const struct option cmd_update_fw_options[] = {
	UPDATE_FW_BASE_OPTIONS(),
	UPDATE_FW_OPTIONS(),
	OPT_END(),
};

static struct _device_info_get_params {
	bool verbose;
} device_info_get_params;

#define DEVICE_INFO_GET_BASE_OPTIONS() \
OPT_BOOLEAN('v',"verbose", &device_info_get_params.verbose, "turn on debug")


static const struct option cmd_device_info_get_options[] = {
	DEVICE_INFO_GET_BASE_OPTIONS(),
	OPT_END(),
};

static struct _get_fw_info_params {
	bool verbose;
} get_fw_info_params;

#define GET_FW_INFO_BASE_OPTIONS() \
OPT_BOOLEAN('v',"verbose", &get_fw_info_params.verbose, "turn on debug")


static const struct option cmd_get_fw_info_options[] = {
	GET_FW_INFO_BASE_OPTIONS(),
	OPT_END(),
};

static struct _activate_fw_params {
	u32 action;
	u32 slot;
	bool verbose;
} activate_fw_params;

#define ACTIVATE_FW_BASE_OPTIONS() \
OPT_BOOLEAN('v',"verbose", &activate_fw_params.verbose, "turn on debug")

#define ACTIVATE_FW_OPTIONS() \
OPT_UINTEGER('a', "action", &activate_fw_params.action, "Action"), \
OPT_UINTEGER('s', "slot", &activate_fw_params.slot, "Slot")

static const struct option cmd_activate_fw_options[] = {
	ACTIVATE_FW_BASE_OPTIONS(),
	ACTIVATE_FW_OPTIONS(),
	OPT_END(),
};

static const struct option cmd_get_alert_config_options[] = {
	BASE_OPTIONS(),
	OPT_END(),
};

static struct _alert_config_params {
	u32 alert_prog_threshold;
	u32 device_temp_threshold;
	u32 mem_error_threshold;
	bool verbose;
} alert_config_params;

#define SET_ALERT_CONFIG_BASE_OPTIONS() \
OPT_BOOLEAN('v',"verbose", &interrupt_policy_params.verbose, "turn on debug")

#define SET_ALERT_CONFIG_OPTIONS() \
OPT_UINTEGER('a', "alert_prog_threshold", &alert_config_params.alert_prog_threshold, "Set valid, enable alert actions and life used programmable threshold. Fields: Valid Alert Actions (1B), Enable Alert Actions (1B), Life Used Programmable Warning Threshold (1B)"), \
OPT_UINTEGER('d', "device_temp_threshold", &alert_config_params.device_temp_threshold, "Set device over/under temp thresholds. Fields: Device Over-Temperature Programmable Warning Threshold (2B), Device Under-Temperature Programmable Warning Threshold (2B)"), \
OPT_UINTEGER('m', "mem_error_threshold", &alert_config_params.mem_error_threshold, "Set memory corrected thresholds. Fields: Corrected Volatile Memory Error Programmable Warning Threshold (2B), Corrected Persistent Memory Error Programmable Warning Threshold (2B)")

static const struct option cmd_set_alert_config_options[] = {
	SET_ALERT_CONFIG_BASE_OPTIONS(),
	SET_ALERT_CONFIG_OPTIONS(),
	OPT_END(),
};

static const struct option cmd_get_health_info_options[] = {
	BASE_OPTIONS(),
	OPT_END(),
};

static struct _get_ld_info_params {
	bool verbose;
} get_ld_info_params;

#define GET_LD_INFO_BASE_OPTIONS() \
OPT_BOOLEAN('v',"verbose", &get_ld_info_params.verbose, "turn on debug")

static const struct option cmd_get_ld_info_options[] = {
	GET_LD_INFO_BASE_OPTIONS(),
	OPT_END(),
};

static struct _ddr_info_params {
	bool verbose;
	int ddr_id;
} ddr_info_params;

#define DDR_INFO_BASE_OPTIONS() \
OPT_BOOLEAN('v',"verbose", &ddr_info_params.verbose, "turn on debug")

#define DDR_INFO_OPTIONS() \
OPT_INTEGER('i', "ddr_id", &ddr_info_params.ddr_id, "DDR instance id")


static const struct option cmd_ddr_info_options[] = {
	DDR_INFO_BASE_OPTIONS(),
	DDR_INFO_OPTIONS(),
	OPT_END(),
};

static struct _get_event_records_params {
	int event_log_type; /* 00 - information, 01 - warning, 02 - failure, 03 - fatal */
	bool verbose;
} get_event_records_params;

#define GET_EVENT_RECORDS_BASE_OPTIONS() \
OPT_BOOLEAN('v',"verbose", &get_event_records_params.verbose, "turn on debug")

#define GET_EVENT_RECORDS_OPTIONS() \
OPT_INTEGER('t', "log_type", &get_event_records_params.event_log_type, "Event log type (00 - information (default), 01 - warning, 02 - failure, 03 - fatal)")

static const struct option cmd_get_event_records_options[] = {
	GET_EVENT_RECORDS_BASE_OPTIONS(),
	GET_EVENT_RECORDS_OPTIONS(),
	OPT_END(),
};

static struct _clear_event_records_params {
	int event_log_type; /* 00 - information, 01 - warning, 02 - failure, 03 - fatal */
	int clear_event_flags; /* bit 0 - when set, clears all events */
	unsigned event_record_handle; /* only one is supported */
	bool verbose;
} clear_event_records_params;

#define CLEAR_EVENT_RECORDS_BASE_OPTIONS() \
OPT_BOOLEAN('v',"verbose", &clear_event_records_params.verbose, "turn on debug")

#define CLEAR_EVENT_RECORDS_OPTIONS() \
OPT_INTEGER('t', "log_type", &clear_event_records_params.event_log_type, "Event log type (00 - information (default), 01 - warning, 02 - failure, 03 - fatal)"), \
OPT_INTEGER('f', "event_flag", &clear_event_records_params.clear_event_flags, "Clear Event Flags: 1 - clear all events, 0 (default) - clear specific event record"), \
OPT_UINTEGER('i', "event_record_handle", &clear_event_records_params.event_record_handle, "Clear Specific Event specific by Event Record Handle")

static const struct option cmd_clear_event_records_options[] = {
	CLEAR_EVENT_RECORDS_BASE_OPTIONS(),
	CLEAR_EVENT_RECORDS_OPTIONS(),
	OPT_END(),
};

static struct _hct_start_stop_trigger_params {
	u32 hct_inst;
	u32 buf_control;
	bool verbose;
} hct_start_stop_trigger_params;

#define HCT_START_STOP_TRIGGER_BASE_OPTIONS() \
OPT_BOOLEAN('v',"verbose", &hct_start_stop_trigger_params.verbose, "turn on debug")

#define HCT_START_STOP_TRIGGER_OPTIONS() \
OPT_UINTEGER('h', "hct_inst", &hct_start_stop_trigger_params.hct_inst, "HCT Instance"), \
OPT_UINTEGER('b', "buf_control", &hct_start_stop_trigger_params.buf_control, "Buffer Control")

static const struct option cmd_hct_start_stop_trigger_options[] = {
	HCT_START_STOP_TRIGGER_BASE_OPTIONS(),
	HCT_START_STOP_TRIGGER_OPTIONS(),
	OPT_END(),
};

static struct _hct_get_buffer_status_params {
	u32 hct_inst;
	bool verbose;
} hct_get_buffer_status_params;

#define HCT_GET_BUFFER_STATUS_BASE_OPTIONS() \
OPT_BOOLEAN('v',"verbose", &hct_get_buffer_status_params.verbose, "turn on debug")

#define HCT_GET_BUFFER_STATUS_OPTIONS() \
OPT_UINTEGER('h', "hct_inst", &hct_get_buffer_status_params.hct_inst, "HCT Instance")

static const struct option cmd_hct_get_buffer_status_options[] = {
	HCT_GET_BUFFER_STATUS_BASE_OPTIONS(),
	HCT_GET_BUFFER_STATUS_OPTIONS(),
	OPT_END(),
};

static struct _hct_enable_params {
	u32 hct_inst;
	bool verbose;
} hct_enable_params;

#define HCT_ENABLE_BASE_OPTIONS() \
OPT_BOOLEAN('v',"verbose", &hct_enable_params.verbose, "turn on debug")

#define HCT_ENABLE_OPTIONS() \
OPT_UINTEGER('h', "hct_inst", &hct_enable_params.hct_inst, "HCT Instance")

static const struct option cmd_hct_enable_options[] = {
	HCT_ENABLE_BASE_OPTIONS(),
	HCT_ENABLE_OPTIONS(),
	OPT_END(),
};

static struct _ltmon_capture_clear_params {
	u32 cxl_mem_id;
	bool verbose;
} ltmon_capture_clear_params;

#define LTMON_CAPTURE_CLEAR_BASE_OPTIONS() \
OPT_BOOLEAN('v',"verbose", &ltmon_capture_clear_params.verbose, "turn on debug")

#define LTMON_CAPTURE_CLEAR_OPTIONS() \
OPT_UINTEGER('c', "cxl_mem_id", &ltmon_capture_clear_params.cxl_mem_id, "CXL.MEM ID")

static const struct option cmd_ltmon_capture_clear_options[] = {
	LTMON_CAPTURE_CLEAR_BASE_OPTIONS(),
	LTMON_CAPTURE_CLEAR_OPTIONS(),
	OPT_END(),
};

static struct _ltmon_capture_params {
	u32 cxl_mem_id;
	u32 capt_mode;
	u32 ignore_sub_chg;
	u32 ignore_rxl0_chg;
	u32 trig_src_sel;
	bool verbose;
} ltmon_capture_params;

#define LTMON_CAPTURE_BASE_OPTIONS() \
OPT_BOOLEAN('v',"verbose", &ltmon_capture_params.verbose, "turn on debug")

#define LTMON_CAPTURE_OPTIONS() \
OPT_UINTEGER('c', "cxl_mem_id", &ltmon_capture_params.cxl_mem_id, "CXL.MEM ID"), \
OPT_UINTEGER('d', "capt_mode", &ltmon_capture_params.capt_mode, "Capture Mode"), \
OPT_UINTEGER('i', "ignore_sub_chg", &ltmon_capture_params.ignore_sub_chg, "Ignore Sub Change"), \
OPT_UINTEGER('j', "ignore_rxl0_chg", &ltmon_capture_params.ignore_rxl0_chg, "Ignore Receiver L0 Change"), \
OPT_UINTEGER('t', "trig_src_sel", &ltmon_capture_params.trig_src_sel, "Trigger Source Selection")

static const struct option cmd_ltmon_capture_options[] = {
	LTMON_CAPTURE_BASE_OPTIONS(),
	LTMON_CAPTURE_OPTIONS(),
	OPT_END(),
};

static struct _ltmon_capture_freeze_and_restore_params {
	u32 cxl_mem_id;
	u32 freeze_restore;
	bool verbose;
} ltmon_capture_freeze_and_restore_params;

#define LTMON_CAPTURE_FREEZE_AND_RESTORE_BASE_OPTIONS() \
OPT_BOOLEAN('v',"verbose", &ltmon_capture_freeze_and_restore_params.verbose, "turn on debug")

#define LTMON_CAPTURE_FREEZE_AND_RESTORE_OPTIONS() \
OPT_UINTEGER('c', "cxl_mem_id", &ltmon_capture_freeze_and_restore_params.cxl_mem_id, "CXL.MEM ID"), \
OPT_UINTEGER('f', "freeze_restore", &ltmon_capture_freeze_and_restore_params.freeze_restore, "Freeze Restore")

static const struct option cmd_ltmon_capture_freeze_and_restore_options[] = {
	LTMON_CAPTURE_FREEZE_AND_RESTORE_BASE_OPTIONS(),
	LTMON_CAPTURE_FREEZE_AND_RESTORE_OPTIONS(),
	OPT_END(),
};

static struct _ltmon_l2r_count_dump_params {
	u32 cxl_mem_id;
	bool verbose;
} ltmon_l2r_count_dump_params;

#define LTMON_L2R_COUNT_DUMP_BASE_OPTIONS() \
OPT_BOOLEAN('v',"verbose", &ltmon_l2r_count_dump_params.verbose, "turn on debug")

#define LTMON_L2R_COUNT_DUMP_OPTIONS() \
OPT_UINTEGER('c', "cxl_mem_id", &ltmon_l2r_count_dump_params.cxl_mem_id, "CXL.MEM ID")

static const struct option cmd_ltmon_l2r_count_dump_options[] = {
	LTMON_L2R_COUNT_DUMP_BASE_OPTIONS(),
	LTMON_L2R_COUNT_DUMP_OPTIONS(),
	OPT_END(),
};

static struct _ltmon_l2r_count_clear_params {
	u32 cxl_mem_id;
	bool verbose;
} ltmon_l2r_count_clear_params;

#define LTMON_L2R_COUNT_CLEAR_BASE_OPTIONS() \
OPT_BOOLEAN('v',"verbose", &ltmon_l2r_count_clear_params.verbose, "turn on debug")

#define LTMON_L2R_COUNT_CLEAR_OPTIONS() \
OPT_UINTEGER('c', "cxl_mem_id", &ltmon_l2r_count_clear_params.cxl_mem_id, "CXL.MEM ID")

static const struct option cmd_ltmon_l2r_count_clear_options[] = {
	LTMON_L2R_COUNT_CLEAR_BASE_OPTIONS(),
	LTMON_L2R_COUNT_CLEAR_OPTIONS(),
	OPT_END(),
};

static struct _ltmon_basic_cfg_params {
	u32 cxl_mem_id;
	u32 tick_cnt;
	u32 global_ts;
	bool verbose;
} ltmon_basic_cfg_params;

#define LTMON_BASIC_CFG_BASE_OPTIONS() \
OPT_BOOLEAN('v',"verbose", &ltmon_basic_cfg_params.verbose, "turn on debug")

#define LTMON_BASIC_CFG_OPTIONS() \
OPT_UINTEGER('c', "cxl_mem_id", &ltmon_basic_cfg_params.cxl_mem_id, "CXL.MEM ID"), \
OPT_UINTEGER('t', "tick_cnt", &ltmon_basic_cfg_params.tick_cnt, "Tick Count"), \
OPT_UINTEGER('g', "global_ts", &ltmon_basic_cfg_params.global_ts, "Global Time Stamp")

static const struct option cmd_ltmon_basic_cfg_options[] = {
	LTMON_BASIC_CFG_BASE_OPTIONS(),
	LTMON_BASIC_CFG_OPTIONS(),
	OPT_END(),
};

static struct _ltmon_watch_params {
	u32 cxl_mem_id;
	u32 watch_id;
	u32 watch_mode;
	u32 src_maj_st;
	u32 src_min_st;
	u32 src_l0_st;
	u32 dst_maj_st;
	u32 dst_min_st;
	u32 dst_l0_st;
	bool verbose;
} ltmon_watch_params;

#define LTMON_WATCH_BASE_OPTIONS() \
OPT_BOOLEAN('v',"verbose", &ltmon_watch_params.verbose, "turn on debug")

#define LTMON_WATCH_OPTIONS() \
OPT_UINTEGER('c', "cxl_mem_id", &ltmon_watch_params.cxl_mem_id, "CXL.MEM ID"), \
OPT_UINTEGER('w', "watch_id", &ltmon_watch_params.watch_id, "Watch ID"), \
OPT_UINTEGER('x', "watch_mode", &ltmon_watch_params.watch_mode, "Watch Mode"), \
OPT_UINTEGER('s', "src_maj_st", &ltmon_watch_params.src_maj_st, "Source Maj State"), \
OPT_UINTEGER('t', "src_min_st", &ltmon_watch_params.src_min_st, "Source Min State"), \
OPT_UINTEGER('u', "src_l0_st", &ltmon_watch_params.src_l0_st, "Source L0 State"), \
OPT_UINTEGER('d', "dst_maj_st", &ltmon_watch_params.dst_maj_st, "Destination Maj State"), \
OPT_UINTEGER('e', "dst_min_st", &ltmon_watch_params.dst_min_st, "Destination Min State"), \
OPT_UINTEGER('f', "dst_l0_st", &ltmon_watch_params.dst_l0_st, "Destination L0 State")

static const struct option cmd_ltmon_watch_options[] = {
	LTMON_WATCH_BASE_OPTIONS(),
	LTMON_WATCH_OPTIONS(),
	OPT_END(),
};

static struct _ltmon_capture_stat_params {
	u32 cxl_mem_id;
	bool verbose;
} ltmon_capture_stat_params;

#define LTMON_CAPTURE_STAT_BASE_OPTIONS() \
OPT_BOOLEAN('v',"verbose", &ltmon_capture_stat_params.verbose, "turn on debug")

#define LTMON_CAPTURE_STAT_OPTIONS() \
OPT_UINTEGER('c', "cxl_mem_id", &ltmon_capture_stat_params.cxl_mem_id, "CXL.MEM ID")

static const struct option cmd_ltmon_capture_stat_options[] = {
	LTMON_CAPTURE_STAT_BASE_OPTIONS(),
	LTMON_CAPTURE_STAT_OPTIONS(),
	OPT_END(),
};

static struct _ltmon_capture_log_dmp_params {
	u32 cxl_mem_id;
	u32 dump_idx;
	u32 dump_cnt;
	bool verbose;
} ltmon_capture_log_dmp_params;

#define LTMON_CAPTURE_LOG_DMP_BASE_OPTIONS() \
OPT_BOOLEAN('v',"verbose", &ltmon_capture_log_dmp_params.verbose, "turn on debug")

#define LTMON_CAPTURE_LOG_DMP_OPTIONS() \
OPT_UINTEGER('c', "cxl_mem_id", &ltmon_capture_log_dmp_params.cxl_mem_id, "CXL.MEM ID"), \
OPT_UINTEGER('d', "dump_idx", &ltmon_capture_log_dmp_params.dump_idx, "Dump Index"), \
OPT_UINTEGER('e', "dump_cnt", &ltmon_capture_log_dmp_params.dump_cnt, "Dump Count")

static const struct option cmd_ltmon_capture_log_dmp_options[] = {
	LTMON_CAPTURE_LOG_DMP_BASE_OPTIONS(),
	LTMON_CAPTURE_LOG_DMP_OPTIONS(),
	OPT_END(),
};

static struct _ltmon_capture_trigger_params {
	u32 cxl_mem_id;
	u32 trig_src;
	bool verbose;
} ltmon_capture_trigger_params;

#define LTMON_CAPTURE_TRIGGER_BASE_OPTIONS() \
OPT_BOOLEAN('v',"verbose", &ltmon_capture_trigger_params.verbose, "turn on debug")

#define LTMON_CAPTURE_TRIGGER_OPTIONS() \
OPT_UINTEGER('c', "cxl_mem_id", &ltmon_capture_trigger_params.cxl_mem_id, "CXL.MEM ID"), \
OPT_UINTEGER('t', "trig_src", &ltmon_capture_trigger_params.trig_src, "Trigger Source")

static const struct option cmd_ltmon_capture_trigger_options[] = {
	LTMON_CAPTURE_TRIGGER_BASE_OPTIONS(),
	LTMON_CAPTURE_TRIGGER_OPTIONS(),
	OPT_END(),
};

static struct _ltmon_enable_params {
	u32 cxl_mem_id;
	u32 enable;
	bool verbose;
} ltmon_enable_params;

#define LTMON_ENABLE_BASE_OPTIONS() \
OPT_BOOLEAN('v',"verbose", &ltmon_enable_params.verbose, "turn on debug")

#define LTMON_ENABLE_OPTIONS() \
OPT_UINTEGER('c', "cxl_mem_id", &ltmon_enable_params.cxl_mem_id, "CXL.MEM ID"), \
OPT_UINTEGER('e', "enable", &ltmon_enable_params.enable, "Enable")

static const struct option cmd_ltmon_enable_options[] = {
	LTMON_ENABLE_BASE_OPTIONS(),
	LTMON_ENABLE_OPTIONS(),
	OPT_END(),
};

static struct _osa_os_type_trig_cfg_params {
	u32 cxl_mem_id;
	u32 lane_mask;
	u32 lane_dir_mask;
	u32 rate_mask;
	u32 os_type_mask;
	bool verbose;
} osa_os_type_trig_cfg_params;

#define OSA_OS_TYPE_TRIG_CFG_BASE_OPTIONS() \
OPT_BOOLEAN('v',"verbose", &osa_os_type_trig_cfg_params.verbose, "turn on debug")

#define OSA_OS_TYPE_TRIG_CFG_OPTIONS() \
OPT_UINTEGER('c', "cxl_mem_id", &osa_os_type_trig_cfg_params.cxl_mem_id, "CXL.MEM ID"), \
OPT_UINTEGER('l', "lane_mask", &osa_os_type_trig_cfg_params.lane_mask, "Lane Mask"), \
OPT_UINTEGER('m', "lane_dir_mask", &osa_os_type_trig_cfg_params.lane_dir_mask, "Lane Direction Mask (see OSA_LANE_DIR_BITMSK_*)"), \
OPT_UINTEGER('r', "rate_mask", &osa_os_type_trig_cfg_params.rate_mask, "Link Rate mask (see OSA_LINK_RATE_BITMSK_*)"), \
OPT_UINTEGER('o', "os_type_mask", &osa_os_type_trig_cfg_params.os_type_mask, "OS Type mask (see OSA_OS_TYPE_TRIG_BITMSK_*)")

static const struct option cmd_osa_os_type_trig_cfg_options[] = {
	OSA_OS_TYPE_TRIG_CFG_BASE_OPTIONS(),
	OSA_OS_TYPE_TRIG_CFG_OPTIONS(),
	OPT_END(),
};

static struct _osa_cap_ctrl_params {
	u32 cxl_mem_id;
	u32 lane_mask;
	u32 lane_dir_mask;
	u32 drop_single_os;
	u32 stop_mode;
	u32 snapshot_mode;
	u32 post_trig_num;
	u32 os_type_mask;
	bool verbose;
} osa_cap_ctrl_params;

#define OSA_CAP_CTRL_BASE_OPTIONS() \
OPT_BOOLEAN('v',"verbose", &osa_cap_ctrl_params.verbose, "turn on debug")

#define OSA_CAP_CTRL_OPTIONS() \
OPT_UINTEGER('c', "cxl_mem_id", &osa_cap_ctrl_params.cxl_mem_id, "CXL.MEM ID"), \
OPT_UINTEGER('l', "lane_mask", &osa_cap_ctrl_params.lane_mask, "Lane Mask"), \
OPT_UINTEGER('m', "lane_dir_mask", &osa_cap_ctrl_params.lane_dir_mask, "Lane Direction Mask (see OSA_LANE_DIR_BITMSK_*)"), \
OPT_UINTEGER('d', "drop_single_os", &osa_cap_ctrl_params.drop_single_os, "Drop Single OS's (TS1/TS2/FTS/CTL_SKP)"), \
OPT_UINTEGER('s', "stop_mode", &osa_cap_ctrl_params.stop_mode, "Capture Stop Mode (see osa_cap_stop_mode_enum)"), \
OPT_UINTEGER('t', "snapshot_mode", &osa_cap_ctrl_params.snapshot_mode, "Snapshot Mode Enable"), \
OPT_UINTEGER('p', "post_trig_num", &osa_cap_ctrl_params.post_trig_num, "Number of post-trigger entries"), \
OPT_UINTEGER('o', "os_type_mask", &osa_cap_ctrl_params.os_type_mask, "OS Type mask (see OSA_OS_TYPE_CAP_BITMSK_*)")

static const struct option cmd_osa_cap_ctrl_options[] = {
	OSA_CAP_CTRL_BASE_OPTIONS(),
	OSA_CAP_CTRL_OPTIONS(),
	OPT_END(),
};

static struct _osa_cfg_dump_params {
	u32 cxl_mem_id;
	bool verbose;
} osa_cfg_dump_params;

#define OSA_CFG_DUMP_BASE_OPTIONS() \
OPT_BOOLEAN('v',"verbose", &osa_cfg_dump_params.verbose, "turn on debug")

#define OSA_CFG_DUMP_OPTIONS() \
OPT_UINTEGER('c', "cxl_mem_id", &osa_cfg_dump_params.cxl_mem_id, "CXL.MEM ID")

static const struct option cmd_osa_cfg_dump_options[] = {
	OSA_CFG_DUMP_BASE_OPTIONS(),
	OSA_CFG_DUMP_OPTIONS(),
	OPT_END(),
};

static struct _osa_ana_op_params {
	u32 cxl_mem_id;
	u32 op;
	bool verbose;
} osa_ana_op_params;

#define OSA_ANA_OP_BASE_OPTIONS() \
OPT_BOOLEAN('v',"verbose", &osa_ana_op_params.verbose, "turn on debug")

#define OSA_ANA_OP_OPTIONS() \
OPT_UINTEGER('c', "cxl_mem_id", &osa_ana_op_params.cxl_mem_id, "CXL.MEM ID"), \
OPT_UINTEGER('o', "op", &osa_ana_op_params.op, "Operation (see osa_op_enum)")

static const struct option cmd_osa_ana_op_options[] = {
	OSA_ANA_OP_BASE_OPTIONS(),
	OSA_ANA_OP_OPTIONS(),
	OPT_END(),
};

static struct _osa_status_query_params {
	u32 cxl_mem_id;
	bool verbose;
} osa_status_query_params;

#define OSA_STATUS_QUERY_BASE_OPTIONS() \
OPT_BOOLEAN('v',"verbose", &osa_status_query_params.verbose, "turn on debug")

#define OSA_STATUS_QUERY_OPTIONS() \
OPT_UINTEGER('c', "cxl_mem_id", &osa_status_query_params.cxl_mem_id, "CXL.MEM ID")

static const struct option cmd_osa_status_query_options[] = {
	OSA_STATUS_QUERY_BASE_OPTIONS(),
	OSA_STATUS_QUERY_OPTIONS(),
	OPT_END(),
};

static struct _osa_access_rel_params {
	u32 cxl_mem_id;
	bool verbose;
} osa_access_rel_params;

#define OSA_ACCESS_REL_BASE_OPTIONS() \
OPT_BOOLEAN('v',"verbose", &osa_access_rel_params.verbose, "turn on debug")

#define OSA_ACCESS_REL_OPTIONS() \
OPT_UINTEGER('c', "cxl_mem_id", &osa_access_rel_params.cxl_mem_id, "CXL.MEM ID")

static const struct option cmd_osa_access_rel_options[] = {
	OSA_ACCESS_REL_BASE_OPTIONS(),
	OSA_ACCESS_REL_OPTIONS(),
	OPT_END(),
};

static struct _perfcnt_mta_ltif_set_params {
	u32 counter;
	u32 match_value;
	u32 opcode;
	u32 meta_field;
	u32 meta_value;
	bool verbose;
} perfcnt_mta_ltif_set_params;

#define PERFCNT_MTA_LTIF_SET_BASE_OPTIONS() \
OPT_BOOLEAN('v',"verbose", &perfcnt_mta_ltif_set_params.verbose, "turn on debug")

#define PERFCNT_MTA_LTIF_SET_OPTIONS() \
OPT_UINTEGER('c', "counter", &perfcnt_mta_ltif_set_params.counter, "Counter"), \
OPT_UINTEGER('m', "match_value", &perfcnt_mta_ltif_set_params.match_value, "Match Value"), \
OPT_UINTEGER('o', "opcode", &perfcnt_mta_ltif_set_params.opcode, "Opcode"), \
OPT_UINTEGER('n', "meta_field", &perfcnt_mta_ltif_set_params.meta_field, "Meta Field"), \
OPT_UINTEGER('p', "meta_value", &perfcnt_mta_ltif_set_params.meta_value, "Meta Value")

static const struct option cmd_perfcnt_mta_ltif_set_options[] = {
	PERFCNT_MTA_LTIF_SET_BASE_OPTIONS(),
	PERFCNT_MTA_LTIF_SET_OPTIONS(),
	OPT_END(),
};

static struct _perfcnt_mta_get_params {
	u32 type;
	u32 counter;
	bool verbose;
} perfcnt_mta_get_params;

#define PERFCNT_MTA_GET_BASE_OPTIONS() \
OPT_BOOLEAN('v',"verbose", &perfcnt_mta_get_params.verbose, "turn on debug")

#define PERFCNT_MTA_GET_OPTIONS() \
OPT_UINTEGER('t', "type", &perfcnt_mta_get_params.type, "Type"), \
OPT_UINTEGER('c', "counter", &perfcnt_mta_get_params.counter, "Counter")

static const struct option cmd_perfcnt_mta_get_options[] = {
	PERFCNT_MTA_GET_BASE_OPTIONS(),
	PERFCNT_MTA_GET_OPTIONS(),
	OPT_END(),
};

static struct _perfcnt_mta_latch_val_get_params {
	u32 type;
	u32 counter;
	bool verbose;
} perfcnt_mta_latch_val_get_params;

#define PERFCNT_MTA_LATCH_VAL_GET_BASE_OPTIONS() \
OPT_BOOLEAN('v',"verbose", &perfcnt_mta_latch_val_get_params.verbose, "turn on debug")

#define PERFCNT_MTA_LATCH_VAL_GET_OPTIONS() \
OPT_UINTEGER('t', "type", &perfcnt_mta_latch_val_get_params.type, "Type"), \
OPT_UINTEGER('c', "counter", &perfcnt_mta_latch_val_get_params.counter, "Counter")

static const struct option cmd_perfcnt_mta_latch_val_get_options[] = {
	PERFCNT_MTA_LATCH_VAL_GET_BASE_OPTIONS(),
	PERFCNT_MTA_LATCH_VAL_GET_OPTIONS(),
	OPT_END(),
};

static struct _perfcnt_mta_counter_clear_params {
	u32 type;
	u32 counter;
	bool verbose;
} perfcnt_mta_counter_clear_params;

#define PERFCNT_MTA_COUNTER_CLEAR_BASE_OPTIONS() \
OPT_BOOLEAN('v',"verbose", &perfcnt_mta_counter_clear_params.verbose, "turn on debug")

#define PERFCNT_MTA_COUNTER_CLEAR_OPTIONS() \
OPT_UINTEGER('t', "type", &perfcnt_mta_counter_clear_params.type, "Type"), \
OPT_UINTEGER('c', "counter", &perfcnt_mta_counter_clear_params.counter, "Counter")

static const struct option cmd_perfcnt_mta_counter_clear_options[] = {
	PERFCNT_MTA_COUNTER_CLEAR_BASE_OPTIONS(),
	PERFCNT_MTA_COUNTER_CLEAR_OPTIONS(),
	OPT_END(),
};

static struct _perfcnt_mta_cnt_val_latch_params {
	u32 type;
	u32 counter;
	bool verbose;
} perfcnt_mta_cnt_val_latch_params;

#define PERFCNT_MTA_CNT_VAL_LATCH_BASE_OPTIONS() \
OPT_BOOLEAN('v',"verbose", &perfcnt_mta_cnt_val_latch_params.verbose, "turn on debug")

#define PERFCNT_MTA_CNT_VAL_LATCH_OPTIONS() \
OPT_UINTEGER('t', "type", &perfcnt_mta_cnt_val_latch_params.type, "Type"), \
OPT_UINTEGER('c', "counter", &perfcnt_mta_cnt_val_latch_params.counter, "Counter")

static const struct option cmd_perfcnt_mta_cnt_val_latch_options[] = {
	PERFCNT_MTA_CNT_VAL_LATCH_BASE_OPTIONS(),
	PERFCNT_MTA_CNT_VAL_LATCH_OPTIONS(),
	OPT_END(),
};

static struct _perfcnt_mta_hif_set_params {
	u32 counter;
	u32 match_value;
	u32 addr;
	u32 req_ty;
	u32 sc_ty;
	bool verbose;
} perfcnt_mta_hif_set_params;

#define PERFCNT_MTA_HIF_SET_BASE_OPTIONS() \
OPT_BOOLEAN('v',"verbose", &perfcnt_mta_hif_set_params.verbose, "turn on debug")

#define PERFCNT_MTA_HIF_SET_OPTIONS() \
OPT_UINTEGER('c', "counter", &perfcnt_mta_hif_set_params.counter, "Counter"), \
OPT_UINTEGER('m', "match_value", &perfcnt_mta_hif_set_params.match_value, "Match Value"), \
OPT_UINTEGER('a', "addr", &perfcnt_mta_hif_set_params.addr, "Address"), \
OPT_UINTEGER('r', "req_ty", &perfcnt_mta_hif_set_params.req_ty, "Req Type"), \
OPT_UINTEGER('s', "sc_ty", &perfcnt_mta_hif_set_params.sc_ty, "Scrub Req")

static const struct option cmd_perfcnt_mta_hif_set_options[] = {
	PERFCNT_MTA_HIF_SET_BASE_OPTIONS(),
	PERFCNT_MTA_HIF_SET_OPTIONS(),
	OPT_END(),
};

static struct _perfcnt_mta_hif_cfg_get_params {
	u32 counter;
	bool verbose;
} perfcnt_mta_hif_cfg_get_params;

#define PERFCNT_MTA_HIF_CFG_GET_BASE_OPTIONS() \
OPT_BOOLEAN('v',"verbose", &perfcnt_mta_hif_cfg_get_params.verbose, "turn on debug")

#define PERFCNT_MTA_HIF_CFG_GET_OPTIONS() \
OPT_UINTEGER('c', "counter", &perfcnt_mta_hif_cfg_get_params.counter, "Counter")

static const struct option cmd_perfcnt_mta_hif_cfg_get_options[] = {
	PERFCNT_MTA_HIF_CFG_GET_BASE_OPTIONS(),
	PERFCNT_MTA_HIF_CFG_GET_OPTIONS(),
	OPT_END(),
};

static struct _perfcnt_mta_hif_latch_val_get_params {
	u32 counter;
	bool verbose;
} perfcnt_mta_hif_latch_val_get_params;

#define PERFCNT_MTA_HIF_LATCH_VAL_GET_BASE_OPTIONS() \
OPT_BOOLEAN('v',"verbose", &perfcnt_mta_hif_latch_val_get_params.verbose, "turn on debug")

#define PERFCNT_MTA_HIF_LATCH_VAL_GET_OPTIONS() \
OPT_UINTEGER('c', "counter", &perfcnt_mta_hif_latch_val_get_params.counter, "Counter")

static const struct option cmd_perfcnt_mta_hif_latch_val_get_options[] = {
	PERFCNT_MTA_HIF_LATCH_VAL_GET_BASE_OPTIONS(),
	PERFCNT_MTA_HIF_LATCH_VAL_GET_OPTIONS(),
	OPT_END(),
};

static struct _perfcnt_mta_hif_counter_clear_params {
	u32 counter;
	bool verbose;
} perfcnt_mta_hif_counter_clear_params;

#define PERFCNT_MTA_HIF_COUNTER_CLEAR_BASE_OPTIONS() \
OPT_BOOLEAN('v',"verbose", &perfcnt_mta_hif_counter_clear_params.verbose, "turn on debug")

#define PERFCNT_MTA_HIF_COUNTER_CLEAR_OPTIONS() \
OPT_UINTEGER('c', "counter", &perfcnt_mta_hif_counter_clear_params.counter, "Counter")

static const struct option cmd_perfcnt_mta_hif_counter_clear_options[] = {
	PERFCNT_MTA_HIF_COUNTER_CLEAR_BASE_OPTIONS(),
	PERFCNT_MTA_HIF_COUNTER_CLEAR_OPTIONS(),
	OPT_END(),
};

static struct _perfcnt_mta_hif_cnt_val_latch_params {
	u32 counter;
	bool verbose;
} perfcnt_mta_hif_cnt_val_latch_params;

#define PERFCNT_MTA_HIF_CNT_VAL_LATCH_BASE_OPTIONS() \
OPT_BOOLEAN('v',"verbose", &perfcnt_mta_hif_cnt_val_latch_params.verbose, "turn on debug")

#define PERFCNT_MTA_HIF_CNT_VAL_LATCH_OPTIONS() \
OPT_UINTEGER('c', "counter", &perfcnt_mta_hif_cnt_val_latch_params.counter, "Counter")

static const struct option cmd_perfcnt_mta_hif_cnt_val_latch_options[] = {
	PERFCNT_MTA_HIF_CNT_VAL_LATCH_BASE_OPTIONS(),
	PERFCNT_MTA_HIF_CNT_VAL_LATCH_OPTIONS(),
	OPT_END(),
};

static struct _perfcnt_ddr_generic_select_params {
	u32 ddr_id;
	u32 cid;
	u32 rank;
	u32 bank;
	u32 bankgroup;
	u64 event;
	bool verbose;
} perfcnt_ddr_generic_select_params;

#define PERFCNT_DDR_GENERIC_SELECT_BASE_OPTIONS() \
OPT_BOOLEAN('v',"verbose", &perfcnt_ddr_generic_select_params.verbose, "turn on debug")

#define PERFCNT_DDR_GENERIC_SELECT_OPTIONS() \
OPT_UINTEGER('d', "ddr_id", &perfcnt_ddr_generic_select_params.ddr_id, "DDR instance"), \
OPT_UINTEGER('c', "cid", &perfcnt_ddr_generic_select_params.cid, "CID selection"), \
OPT_UINTEGER('r', "rank", &perfcnt_ddr_generic_select_params.rank, "Rank selection"), \
OPT_UINTEGER('b', "bank", &perfcnt_ddr_generic_select_params.bank, "Bank selection"), \
OPT_UINTEGER('e', "bankgroup", &perfcnt_ddr_generic_select_params.bankgroup, "Bank Group selection"), \
OPT_U64('f', "event", &perfcnt_ddr_generic_select_params.event, "Events selection")

static const struct option cmd_perfcnt_ddr_generic_select_options[] = {
	PERFCNT_DDR_GENERIC_SELECT_BASE_OPTIONS(),
	PERFCNT_DDR_GENERIC_SELECT_OPTIONS(),
	OPT_END(),
};

static struct _err_inj_drs_poison_params {
	u32 ch_id;
	u32 duration;
	u32 inj_mode;
	u32 tag;
	bool verbose;
} err_inj_drs_poison_params;

#define ERR_INJ_DRS_POISON_BASE_OPTIONS() \
OPT_BOOLEAN('v',"verbose", &err_inj_drs_poison_params.verbose, "turn on debug")

#define ERR_INJ_DRS_POISON_OPTIONS() \
OPT_UINTEGER('c', "ch_id", &err_inj_drs_poison_params.ch_id, "DRS channel"), \
OPT_UINTEGER('d', "duration", &err_inj_drs_poison_params.duration, "Duration"), \
OPT_UINTEGER('i', "inj_mode", &err_inj_drs_poison_params.inj_mode, "Injection mode"), \
OPT_UINTEGER('t', "tag", &err_inj_drs_poison_params.tag, "Tag")

static const struct option cmd_err_inj_drs_poison_options[] = {
	ERR_INJ_DRS_POISON_BASE_OPTIONS(),
	ERR_INJ_DRS_POISON_OPTIONS(),
	OPT_END(),
};

static struct _err_inj_drs_ecc_params {
	u32 ch_id;
	u32 duration;
	u32 inj_mode;
	u32 tag;
	bool verbose;
} err_inj_drs_ecc_params;

#define ERR_INJ_DRS_ECC_BASE_OPTIONS() \
OPT_BOOLEAN('v',"verbose", &err_inj_drs_ecc_params.verbose, "turn on debug")

#define ERR_INJ_DRS_ECC_OPTIONS() \
OPT_UINTEGER('c', "ch_id", &err_inj_drs_ecc_params.ch_id, "DRS channel"), \
OPT_UINTEGER('d', "duration", &err_inj_drs_ecc_params.duration, "Duration"), \
OPT_UINTEGER('i', "inj_mode", &err_inj_drs_ecc_params.inj_mode, "Injection mode"), \
OPT_UINTEGER('t', "tag", &err_inj_drs_ecc_params.tag, "Tag")

static const struct option cmd_err_inj_drs_ecc_options[] = {
	ERR_INJ_DRS_ECC_BASE_OPTIONS(),
	ERR_INJ_DRS_ECC_OPTIONS(),
	OPT_END(),
};

static struct _err_inj_rxflit_crc_params {
	u32 cxl_mem_id;
	bool verbose;
} err_inj_rxflit_crc_params;

#define ERR_INJ_RXFLIT_CRC_BASE_OPTIONS() \
OPT_BOOLEAN('v',"verbose", &err_inj_rxflit_crc_params.verbose, "turn on debug")

#define ERR_INJ_RXFLIT_CRC_OPTIONS() \
OPT_UINTEGER('c', "cxl_mem_id", &err_inj_rxflit_crc_params.cxl_mem_id, "CXL.mem instance")

static const struct option cmd_err_inj_rxflit_crc_options[] = {
	ERR_INJ_RXFLIT_CRC_BASE_OPTIONS(),
	ERR_INJ_RXFLIT_CRC_OPTIONS(),
	OPT_END(),
};

static struct _err_inj_txflit_crc_params {
	u32 cxl_mem_id;
	bool verbose;
} err_inj_txflit_crc_params;

#define ERR_INJ_TXFLIT_CRC_BASE_OPTIONS() \
OPT_BOOLEAN('v',"verbose", &err_inj_txflit_crc_params.verbose, "turn on debug")

#define ERR_INJ_TXFLIT_CRC_OPTIONS() \
OPT_UINTEGER('c', "cxl_mem_id", &err_inj_txflit_crc_params.cxl_mem_id, "CXL.mem instance")

static const struct option cmd_err_inj_txflit_crc_options[] = {
	ERR_INJ_TXFLIT_CRC_BASE_OPTIONS(),
	ERR_INJ_TXFLIT_CRC_OPTIONS(),
	OPT_END(),
};

static struct _err_inj_viral_params {
	u32 ld_id;
	bool verbose;
} err_inj_viral_params;

#define ERR_INJ_VIRAL_BASE_OPTIONS() \
OPT_BOOLEAN('v',"verbose", &err_inj_viral_params.verbose, "turn on debug")

#define ERR_INJ_VIRAL_OPTIONS() \
OPT_UINTEGER('l', "ld_id", &err_inj_viral_params.ld_id, "ld_id")

static const struct option cmd_err_inj_viral_options[] = {
	ERR_INJ_VIRAL_BASE_OPTIONS(),
	ERR_INJ_VIRAL_OPTIONS(),
	OPT_END(),
};

static struct _eh_eye_cap_run_params {
	u32 depth;
	u32 lane_mask;
	bool verbose;
} eh_eye_cap_run_params;

#define EH_EYE_CAP_RUN_BASE_OPTIONS() \
OPT_BOOLEAN('v',"verbose", &eh_eye_cap_run_params.verbose, "turn on debug")

#define EH_EYE_CAP_RUN_OPTIONS() \
OPT_UINTEGER('d', "depth", &eh_eye_cap_run_params.depth, "capture depth (BT_DEPTH_MIN to BT_DEPTH_MAX)"), \
OPT_UINTEGER('l', "lane_mask", &eh_eye_cap_run_params.lane_mask, "lane mask")

static const struct option cmd_eh_eye_cap_run_options[] = {
	EH_EYE_CAP_RUN_BASE_OPTIONS(),
	EH_EYE_CAP_RUN_OPTIONS(),
	OPT_END(),
};

static struct _eh_eye_cap_read_params {
	u32 lane_id;
	u32 bin_num;
	bool verbose;
} eh_eye_cap_read_params;

#define EH_EYE_CAP_READ_BASE_OPTIONS() \
OPT_BOOLEAN('v',"verbose", &eh_eye_cap_read_params.verbose, "turn on debug")

#define EH_EYE_CAP_READ_OPTIONS() \
OPT_UINTEGER('l', "lane_id", &eh_eye_cap_read_params.lane_id, "lane ID"), \
OPT_UINTEGER('b', "bin_num", &eh_eye_cap_read_params.bin_num, "bin number [0 .. BT_BIN_TOT - 1]")

static const struct option cmd_eh_eye_cap_read_options[] = {
	EH_EYE_CAP_READ_BASE_OPTIONS(),
	EH_EYE_CAP_READ_OPTIONS(),
	OPT_END(),
};

static struct _eh_adapt_get_params {
	u32 lane_id;
	bool verbose;
} eh_adapt_get_params;

#define EH_ADAPT_GET_BASE_OPTIONS() \
OPT_BOOLEAN('v',"verbose", &eh_adapt_get_params.verbose, "turn on debug")

#define EH_ADAPT_GET_OPTIONS() \
OPT_UINTEGER('l', "lane_id", &eh_adapt_get_params.lane_id, "lane id")

static const struct option cmd_eh_adapt_get_options[] = {
	EH_ADAPT_GET_BASE_OPTIONS(),
	EH_ADAPT_GET_OPTIONS(),
	OPT_END(),
};

static struct _eh_adapt_oneoff_params {
	u32 lane_id;
	u32 preload;
	u32 loops;
	u32 objects;
	bool verbose;
} eh_adapt_oneoff_params;

#define EH_ADAPT_ONEOFF_BASE_OPTIONS() \
OPT_BOOLEAN('v',"verbose", &eh_adapt_oneoff_params.verbose, "turn on debug")

#define EH_ADAPT_ONEOFF_OPTIONS() \
OPT_UINTEGER('l', "lane_id", &eh_adapt_oneoff_params.lane_id, "lane id"), \
OPT_UINTEGER('p', "preload", &eh_adapt_oneoff_params.preload, "Adaption objects preload enable"), \
OPT_UINTEGER('m', "loops", &eh_adapt_oneoff_params.loops, "Adaptions loop"), \
OPT_UINTEGER('o', "objects", &eh_adapt_oneoff_params.objects, "Adaption objects enable")

static const struct option cmd_eh_adapt_oneoff_options[] = {
	EH_ADAPT_ONEOFF_BASE_OPTIONS(),
	EH_ADAPT_ONEOFF_OPTIONS(),
	OPT_END(),
};

static struct _eh_adapt_force_params {
	u32 lane_id;
	u32 rate;
	u32 vdd_bias;
	u32 ssc;
	u32 pga_gain;
	u32 pga_a0;
	u32 pga_off;
	u32 cdfe_a2;
	u32 cdfe_a3;
	u32 cdfe_a4;
	u32 cdfe_a5;
	u32 cdfe_a6;
	u32 cdfe_a7;
	u32 cdfe_a8;
	u32 cdfe_a9;
	u32 cdfe_a10;
	u32 dc_offset;
	u32 zobel_dc_offset;
	u32 udfe_thr_0;
	u32 udfe_thr_1;
	u32 median_amp;
	u32 zobel_a_gain;
	u32 ph_ofs_t;
	bool verbose;
} eh_adapt_force_params;

#define EH_ADAPT_FORCE_BASE_OPTIONS() \
OPT_BOOLEAN('v',"verbose", &eh_adapt_force_params.verbose, "turn on debug")

#define EH_ADAPT_FORCE_OPTIONS() \
OPT_UINTEGER('l', "lane_id", &eh_adapt_force_params.lane_id, "lane id"), \
OPT_UINTEGER('r', "rate", &eh_adapt_force_params.rate, "PCIe rate (0 - Gen1, 1 - Gen2, 2 - Gen3, 3 - Gen4, 4 - Gen5)"), \
OPT_UINTEGER('v', "vdd_bias", &eh_adapt_force_params.vdd_bias, "vdd bias (0 = 0.82V, 1 = 0.952V)"), \
OPT_UINTEGER('s', "ssc", &eh_adapt_force_params.ssc, "spread spectrum clocking enable (0 - SSC enable, 1 - SSC disable)"), \
OPT_UINTEGER('p', "pga_gain", &eh_adapt_force_params.pga_gain, "used to set the value of the PGA_GAIN object when preloading is enabled"), \
OPT_UINTEGER('q', "pga_a0", &eh_adapt_force_params.pga_a0, "used to set the value of the PGA_A0 object when preloading is enabled"), \
OPT_UINTEGER('t', "pga_off", &eh_adapt_force_params.pga_off, "PGA Stage1,2 offset preload value, split evenly between PGA Stage1 & Stage2 DC offset"), \
OPT_UINTEGER('c', "cdfe_a2", &eh_adapt_force_params.cdfe_a2, "used to set the value of CDFE_A2 (DFE Tap2) when preloading (CDFE_GRP0) is enabled"), \
OPT_UINTEGER('d', "cdfe_a3", &eh_adapt_force_params.cdfe_a3, "used to set the value of CDFE_A3 (DFE Tap3) when preloading (CDFE_GRP0) is enabled"), \
OPT_UINTEGER('e', "cdfe_a4", &eh_adapt_force_params.cdfe_a4, "used to set the value of CDFE_A4 (DFE Tap4) when preloading (CDFE_GRP0) is enabled"), \
OPT_UINTEGER('f', "cdfe_a5", &eh_adapt_force_params.cdfe_a5, "used to set the value of CDFE_A5 (DFE Tap5) when preloading (CDFE_GRP1) is enabled"), \
OPT_UINTEGER('g', "cdfe_a6", &eh_adapt_force_params.cdfe_a6, "used to set the value of CDFE_A6 (DFE Tap6) when preloading (CDFE_GRP1) is enabled"), \
OPT_UINTEGER('h', "cdfe_a7", &eh_adapt_force_params.cdfe_a7, "used to set the value of CDFE_A7 (DFE Tap7) when preloading (CDFE_GRP1) is enabled"), \
OPT_UINTEGER('i', "cdfe_a8", &eh_adapt_force_params.cdfe_a8, "used to set the value of CDFE_A8 (DFE Tap8) when preloading (CDFE_GRP2) is enabled"), \
OPT_UINTEGER('j', "cdfe_a9", &eh_adapt_force_params.cdfe_a9, "used to set the value of CDFE_A9 (DFE Tap9) when preloading (CDFE_GRP2) is enabled"), \
OPT_UINTEGER('k', "cdfe_a10", &eh_adapt_force_params.cdfe_a10, "used to set the value of CDFE_A10 (DFE Tap10) when preloading (CDFE_GRP2) is enabled"), \
OPT_UINTEGER('m', "dc_offset", &eh_adapt_force_params.dc_offset, "used to set the value of the DC_OFFSET object when preloading is enabled"), \
OPT_UINTEGER('z', "zobel_dc_offset", &eh_adapt_force_params.zobel_dc_offset, "Zobel DC offset preload value"), \
OPT_UINTEGER('u', "udfe_thr_0", &eh_adapt_force_params.udfe_thr_0, "used to set the value of the UDFE_THR_0 object when preloading is enabled"), \
OPT_UINTEGER('w', "udfe_thr_1", &eh_adapt_force_params.udfe_thr_1, "used to set the value of the UDFE_THR_1 object when preloading is enabled"), \
OPT_UINTEGER('n', "median_amp", &eh_adapt_force_params.median_amp, "used to set the value of the MEDIAN_AMP object when preloading is enabled"), \
OPT_UINTEGER('A', "zobel_a_gain", &eh_adapt_force_params.zobel_a_gain, "Zobel a_gain preload"), \
OPT_UINTEGER('x', "ph_ofs_t", &eh_adapt_force_params.ph_ofs_t, "Timing phase offset preload")

static const struct option cmd_eh_adapt_force_options[] = {
	EH_ADAPT_FORCE_BASE_OPTIONS(),
	EH_ADAPT_FORCE_OPTIONS(),
	OPT_END(),
};

static struct _hbo_status_params {
	bool verbose;
} hbo_status_params;

#define HBO_STATUS_BASE_OPTIONS() \
OPT_BOOLEAN('v',"verbose", &hbo_status_params.verbose, "turn on debug")

static const struct option cmd_hbo_status_options[] = {
	HBO_STATUS_BASE_OPTIONS(),
	OPT_END(),
};

static struct _hbo_transfer_fw_params {
	bool verbose;
} hbo_transfer_fw_params;

#define HBO_TRANSFER_FW_BASE_OPTIONS() \
OPT_BOOLEAN('v',"verbose", &hbo_transfer_fw_params.verbose, "turn on debug")

static const struct option cmd_hbo_transfer_fw_options[] = {
	HBO_TRANSFER_FW_BASE_OPTIONS(),
	OPT_END(),
};

static struct _hbo_activate_fw_params {
	bool verbose;
} hbo_activate_fw_params;

#define HBO_ACTIVATE_FW_BASE_OPTIONS() \
OPT_BOOLEAN('v',"verbose", &hbo_activate_fw_params.verbose, "turn on debug")

static const struct option cmd_hbo_activate_fw_options[] = {
	HBO_ACTIVATE_FW_BASE_OPTIONS(),
	OPT_END(),
};

static struct _health_counters_clear_params {
	u32 bitmask;
	bool verbose;
} health_counters_clear_params;

#define HEALTH_COUNTERS_CLEAR_BASE_OPTIONS() \
OPT_BOOLEAN('v',"verbose", &health_counters_clear_params.verbose, "turn on debug")

#define HEALTH_COUNTERS_CLEAR_OPTIONS() \
OPT_UINTEGER('b', "bitmask", &health_counters_clear_params.bitmask, "health counters bitmask")

static const struct option cmd_health_counters_clear_options[] = {
	HEALTH_COUNTERS_CLEAR_BASE_OPTIONS(),
	HEALTH_COUNTERS_CLEAR_OPTIONS(),
	OPT_END(),
};

static struct _health_counters_get_params {
	bool verbose;
} health_counters_get_params;

#define HEALTH_COUNTERS_GET_BASE_OPTIONS() \
OPT_BOOLEAN('v',"verbose", &health_counters_get_params.verbose, "turn on debug")

static const struct option cmd_health_counters_get_options[] = {
	HEALTH_COUNTERS_GET_BASE_OPTIONS(),
	OPT_END(),
};

static struct _hct_get_plat_param_params {
	bool verbose;
} hct_get_plat_param_params;

#define HCT_GET_PLAT_PARAM_BASE_OPTIONS() \
OPT_BOOLEAN('v',"verbose", &hct_get_plat_param_params.verbose, "turn on debug")

static const struct option cmd_hct_get_plat_param_options[] = {
	HCT_GET_PLAT_PARAM_BASE_OPTIONS(),
	OPT_END(),
};


static int action_cmd_clear_event_records(struct cxl_memdev *memdev, struct action_context *actx)
{
	u16 record_handle;
	if (cxl_memdev_is_active(memdev)) {
		fprintf(stderr, "%s: memdev active, abort clear_event_records\n",
			cxl_memdev_get_devname(memdev));
		return -EBUSY;
	}
	if (clear_event_records_params.clear_event_flags) {
		record_handle = 0;
		return cxl_memdev_clear_event_records(memdev, clear_event_records_params.event_log_type,
			clear_event_records_params.clear_event_flags, 0, &record_handle);
	}
	else {
		record_handle = (u16) clear_event_records_params.event_record_handle;
		return cxl_memdev_clear_event_records(memdev, clear_event_records_params.event_log_type,
			clear_event_records_params.clear_event_flags, 1, &record_handle);
	}
}

static int action_cmd_get_event_records(struct cxl_memdev *memdev, struct action_context *actx)
{
	if (cxl_memdev_is_active(memdev)) {
		fprintf(stderr, "%s: memdev active, abort get_event_records\n",
			cxl_memdev_get_devname(memdev));
		return -EBUSY;
	}
#if 0
	if (get_event_records_params.event_log_type < 0 || get_event_records_params.event_log_type > 3) {
		fprintf(stderr, "%s: Invalid Event Log type: %d, Allowed values Event log type "
			"(00 - information (default), 01 - warning, 02 - failure, 03 - fatal)\n",
			cxl_memdev_get_devname(memdev), get_event_records_params.event_log_type);
		return -EINVAL;
	}
#endif

	return cxl_memdev_get_event_records(memdev, get_event_records_params.event_log_type);
}

static int action_cmd_get_ld_info(struct cxl_memdev *memdev, struct action_context *actx)
{
	if (cxl_memdev_is_active(memdev)) {
		fprintf(stderr, "%s: memdev active, abort get_ld_info\n",
			cxl_memdev_get_devname(memdev));
		return -EBUSY;
	}

	return cxl_memdev_get_ld_info(memdev);
}

static int action_cmd_ddr_info(struct cxl_memdev *memdev, struct action_context *actx)
{
	if (cxl_memdev_is_active(memdev)) {
		fprintf(stderr, "%s: memdev active, abort ddr_info\n",
			cxl_memdev_get_devname(memdev));
		return -EBUSY;
	}
	fprintf(stdout, "memdev id: %d", cxl_memdev_get_id(memdev));
	return cxl_memdev_ddr_info(memdev, ddr_info_params.ddr_id);
}

static int action_cmd_get_health_info(struct cxl_memdev *memdev, struct action_context *actx)
{
	if (cxl_memdev_is_active(memdev)) {
		fprintf(stderr, "%s: memdev active, abort get_health_info\n",
			cxl_memdev_get_devname(memdev));
		return -EBUSY;
	}

	return cxl_memdev_get_health_info(memdev);
}

static int action_cmd_get_alert_config(struct cxl_memdev *memdev, struct action_context *actx)
{
	if (cxl_memdev_is_active(memdev)) {
		fprintf(stderr, "%s: memdev active, abort get_alert_config\n",
			cxl_memdev_get_devname(memdev));
		return -EBUSY;
	}

	return cxl_memdev_get_alert_config(memdev);
}

static int action_cmd_set_alert_config(struct cxl_memdev *memdev, struct action_context *actx)
{
	if (cxl_memdev_is_active(memdev)) {
		fprintf(stderr, "%s: memdev active, abort set_alert_config\n",
			cxl_memdev_get_devname(memdev));
		return -EBUSY;
	}

	return cxl_memdev_set_alert_config(memdev, alert_config_params.alert_prog_threshold,
		alert_config_params.device_temp_threshold, alert_config_params.mem_error_threshold);
}

static int action_cmd_get_timestamp(struct cxl_memdev *memdev, struct action_context *actx)
{
	if (cxl_memdev_is_active(memdev)) {
		fprintf(stderr, "%s: memdev active, get_timestamp\n",
			cxl_memdev_get_devname(memdev));
		return -EBUSY;
	}

	return cxl_memdev_get_timestamp(memdev);
}

static int action_cmd_set_timestamp(struct cxl_memdev *memdev, struct action_context *actx)
{
	if (cxl_memdev_is_active(memdev)) {
		fprintf(stderr, "%s: memdev active, set_timestamp\n",
			cxl_memdev_get_devname(memdev));
		return -EBUSY;
	}

	printf("timestamp: 0x%lx (%ld)\n", ts_params.timestamp, ts_params.timestamp);
	return cxl_memdev_set_timestamp(memdev, ts_params.timestamp);
}

#define INITIATE_TRANSFER 1
#define CONTINUE_TRANSFER 2
#define END_TRANSFER 3
#define ABORT_TRANSFER 4
static int action_cmd_update_fw(struct cxl_memdev *memdev, struct action_context *actx)
{
/*
Performs inband FW update through a series of successive calls to transfer-fw. The rom
is loaded into memory and transfered in 128 byte chunks. transfer-fw supports several
actions that are specified as part of the input payload. The first call sets the action
to initiate_transfer and includes the first chunk. The remaining chunks are then sent
with the continue_transfer action. Finally, the end_transfer action will cause the
device to validate the binary and transfer it to the indicated slot.

User must provide available FW slot as indicated from get-fw-info. This slot is provided
for every call to transfer-fw, but will only be read during the end_transfer call.
*/
	struct stat fileStat;
	int filesize;
	FILE *rom;
	int rc;
	int fd;
	int num_blocks;
	int num_read;
	const int max_retries = 25;
	int retry_count;
	u32 offset;
	fwblock *rom_buffer;
	u32 opcode;

	rom = fopen(update_fw_params.filepath, "rb");
	if (rom == NULL) {
		fprintf(stderr, "Error: File open returned %s\nCould not open file %s\n",
									strerror(errno), update_fw_params.filepath);
		return -ENOENT;
	}

	if (cxl_memdev_is_active(memdev)) {
		fprintf(stderr, "%s: memdev active, set_timestamp\n",
			cxl_memdev_get_devname(memdev));
		return -EBUSY;
	}

	printf("Rom filepath: %s\n", update_fw_params.filepath);
	fd = fileno(rom);
	rc = fstat(fd, &fileStat);
	if (rc != 0) {
		fprintf(stderr, "Could not read filesize");
		fclose(rom);
		return 1;
	}
    filesize = fileStat.st_size;
	printf("ROM size: %d bytes\n", filesize);
	num_blocks = filesize / FW_BLOCK_SIZE;
	rom_buffer = (fwblock*) malloc(filesize);
	num_read = fread(rom_buffer, FW_BLOCK_SIZE, num_blocks, rom);
	if (num_blocks != num_read)
	{
		fprintf(stderr, "Number of blocks read: %d\nNumber of blocks expected: %d\n", num_read, num_blocks);
		free(rom_buffer);
		fclose(rom);
		return -ENOENT;
	}
	offset = 0;
	if (&update_fw_params.hbo)
	{
		opcode = 0xCD01;
	}
	else
	{
		opcode = 0x0201;
	}
	rc = cxl_memdev_transfer_fw(memdev, INITIATE_TRANSFER, update_fw_params.slot, offset, rom_buffer[0], opcode);
	if (rc != 0)
	{
		fprintf(stderr, "transfer_fw failed to initiate, terminating...\n");
		free(rom_buffer);
		fclose(rom);
		goto abort;
	}
	for (int i = 1; i < num_blocks; i++)
	{
		printf("Transfering block %d of %d\n", i, num_blocks);
		offset = i;
		fflush(stdout);
		rc = cxl_memdev_transfer_fw(memdev, CONTINUE_TRANSFER, update_fw_params.slot, offset, rom_buffer[i], opcode);
		retry_count = 0;
		while (rc != 0)
		{
			if (retry_count > max_retries)
			{
				printf("Maximum %d retries exceeded while transferring block %d\n", max_retries, i);
				goto abort;
			}
			printf("Mailbox returned %d, retrying...\n", rc);
			sleep(0.25);
			rc = cxl_memdev_transfer_fw(memdev, CONTINUE_TRANSFER, update_fw_params.slot, offset, rom_buffer[i], opcode);
			retry_count++;
		}
		if (rc != 0)
		{
			fprintf(stderr, "transfer_fw failed on %d of %d\n", i, num_blocks);
			goto abort;
		}
	}
	printf("Transfer complete. Aborting...\n");
	// End transfer will be added here when fully debugged.
	// For now we will simply abort the fw update once transfer is complete.
abort:
	sleep(2.0);
	rc = cxl_memdev_transfer_fw(memdev, ABORT_TRANSFER, update_fw_params.slot, FW_BLOCK_SIZE, rom_buffer[0]);
	printf("Abort return status %d\n", rc);
	free(rom_buffer);
	fclose(rom);
	return 0;
}

static int action_cmd_get_event_interrupt_policy(struct cxl_memdev *memdev, struct action_context *actx)
{
	if (cxl_memdev_is_active(memdev)) {
		fprintf(stderr, "%s: memdev active, get_event_interrupt_policy\n",
			cxl_memdev_get_devname(memdev));
		return -EBUSY;
	}

	return cxl_memdev_get_event_interrupt_policy(memdev);
}

static int action_cmd_set_event_interrupt_policy(struct cxl_memdev *memdev, struct action_context *actx)
{
	if (cxl_memdev_is_active(memdev)) {
		fprintf(stderr, "%s: memdev active, set_event_interrupt_policy\n",
			cxl_memdev_get_devname(memdev));
		return -EBUSY;
	}

	return cxl_memdev_set_event_interrupt_policy(memdev, interrupt_policy_params.policy);
}

static int action_cmd_get_cel_log(struct cxl_memdev *memdev, struct action_context *actx)
{
	if (cxl_memdev_is_active(memdev)) {
		fprintf(stderr, "%s: memdev active, get_cel_log\n",
			cxl_memdev_get_devname(memdev));
		return -EBUSY;
	}

	return cxl_memdev_get_cel_log(memdev);
}

static int action_cmd_get_supported_logs(struct cxl_memdev *memdev, struct action_context *actx)
{
	if (cxl_memdev_is_active(memdev)) {
		fprintf(stderr, "%s: memdev active, get_supported_logs\n",
			cxl_memdev_get_devname(memdev));
		return -EBUSY;
	}

	return cxl_memdev_get_supported_logs(memdev);
}

static int action_cmd_identify(struct cxl_memdev *memdev, struct action_context *actx)
{
	if (cxl_memdev_is_active(memdev)) {
		fprintf(stderr, "%s: memdev active, cmd_identify\n",
			cxl_memdev_get_devname(memdev));
		return -EBUSY;
	}

	return cxl_memdev_cmd_identify(memdev);
}

static int action_cmd_hct_start_stop_trigger(struct cxl_memdev *memdev, struct action_context *actx)
{
	if (cxl_memdev_is_active(memdev)) {
		fprintf(stderr, "%s: memdev active, abort hct_start_stop_trigger\n",
			cxl_memdev_get_devname(memdev));
		return -EBUSY;
	}

	return cxl_memdev_hct_start_stop_trigger(memdev, hct_start_stop_trigger_params.hct_inst,
		hct_start_stop_trigger_params.buf_control);
}

static int action_cmd_hct_get_buffer_status(struct cxl_memdev *memdev, struct action_context *actx)
{
	if (cxl_memdev_is_active(memdev)) {
		fprintf(stderr, "%s: memdev active, abort hct_get_buffer_status\n",
			cxl_memdev_get_devname(memdev));
		return -EBUSY;
	}

	return cxl_memdev_hct_get_buffer_status(memdev, hct_get_buffer_status_params.hct_inst);
}

static int action_cmd_hct_enable(struct cxl_memdev *memdev, struct action_context *actx)
{
	if (cxl_memdev_is_active(memdev)) {
		fprintf(stderr, "%s: memdev active, abort hct_enable\n",
			cxl_memdev_get_devname(memdev));
		return -EBUSY;
	}

	return cxl_memdev_hct_enable(memdev, hct_enable_params.hct_inst);
}

static int action_cmd_ltmon_capture_clear(struct cxl_memdev *memdev, struct action_context *actx)
{
	if (cxl_memdev_is_active(memdev)) {
		fprintf(stderr, "%s: memdev active, abort ltmon_capture_clear\n",
			cxl_memdev_get_devname(memdev));
		return -EBUSY;
	}

	return cxl_memdev_ltmon_capture_clear(memdev, ltmon_capture_clear_params.cxl_mem_id);
}

static int action_cmd_ltmon_capture(struct cxl_memdev *memdev, struct action_context *actx)
{
	if (cxl_memdev_is_active(memdev)) {
		fprintf(stderr, "%s: memdev active, abort ltmon_capture\n",
			cxl_memdev_get_devname(memdev));
		return -EBUSY;
	}

	return cxl_memdev_ltmon_capture(memdev, ltmon_capture_params.cxl_mem_id,
		ltmon_capture_params.capt_mode, ltmon_capture_params.ignore_sub_chg,
		ltmon_capture_params.ignore_rxl0_chg, ltmon_capture_params.trig_src_sel);
}

static int action_cmd_device_info_get(struct cxl_memdev *memdev, struct action_context *actx)
{
	if (cxl_memdev_is_active(memdev)) {
		fprintf(stderr, "%s: memdev active, abort device_info_get\n",
			cxl_memdev_get_devname(memdev));
		return -EBUSY;
	}

	return cxl_memdev_device_info_get(memdev);
}

static int action_cmd_get_fw_info(struct cxl_memdev *memdev, struct action_context *actx)
{
	if (cxl_memdev_is_active(memdev)) {
		fprintf(stderr, "%s: memdev active, abort get_fw_info",
			cxl_memdev_get_devname(memdev));
		return -EBUSY;
	}

	return cxl_memdev_get_fw_info(memdev);
}

static int action_cmd_activate_fw(struct cxl_memdev *memdev, struct action_context *actx)
{
	if (cxl_memdev_is_active(memdev)) {
		fprintf(stderr, "%s: memdev active, abort activate_fw",
			cxl_memdev_get_devname(memdev));
		return -EBUSY;
	}

	return cxl_memdev_activate_fw(memdev, activate_fw_params.action, activate_fw_params.slot);
}

static int action_cmd_ltmon_capture_freeze_and_restore(struct cxl_memdev *memdev, struct action_context *actx)
{
	if (cxl_memdev_is_active(memdev)) {
		fprintf(stderr, "%s: memdev active, abort ltmon_capture_freeze_and_restore\n",
			cxl_memdev_get_devname(memdev));
		return -EBUSY;
	}

	return cxl_memdev_ltmon_capture_freeze_and_restore(memdev, ltmon_capture_freeze_and_restore_params.cxl_mem_id,
		ltmon_capture_freeze_and_restore_params.freeze_restore);
}

static int action_cmd_ltmon_l2r_count_dump(struct cxl_memdev *memdev, struct action_context *actx)
{
	if (cxl_memdev_is_active(memdev)) {
		fprintf(stderr, "%s: memdev active, abort ltmon_l2r_count_dump\n",
			cxl_memdev_get_devname(memdev));
		return -EBUSY;
	}

	return cxl_memdev_ltmon_l2r_count_dump(memdev, ltmon_l2r_count_dump_params.cxl_mem_id);
}

static int action_cmd_ltmon_l2r_count_clear(struct cxl_memdev *memdev, struct action_context *actx)
{
	if (cxl_memdev_is_active(memdev)) {
		fprintf(stderr, "%s: memdev active, abort ltmon_l2r_count_clear\n",
			cxl_memdev_get_devname(memdev));
		return -EBUSY;
	}

	return cxl_memdev_ltmon_l2r_count_clear(memdev, ltmon_l2r_count_clear_params.cxl_mem_id);
}

static int action_cmd_ltmon_basic_cfg(struct cxl_memdev *memdev, struct action_context *actx)
{
	if (cxl_memdev_is_active(memdev)) {
		fprintf(stderr, "%s: memdev active, abort ltmon_basic_cfg\n",
			cxl_memdev_get_devname(memdev));
		return -EBUSY;
	}

	return cxl_memdev_ltmon_basic_cfg(memdev, ltmon_basic_cfg_params.cxl_mem_id,
		ltmon_basic_cfg_params.tick_cnt, ltmon_basic_cfg_params.global_ts);
}

static int action_cmd_ltmon_watch(struct cxl_memdev *memdev, struct action_context *actx)
{
	if (cxl_memdev_is_active(memdev)) {
		fprintf(stderr, "%s: memdev active, abort ltmon_watch\n",
			cxl_memdev_get_devname(memdev));
		return -EBUSY;
	}

	return cxl_memdev_ltmon_watch(memdev, ltmon_watch_params.cxl_mem_id,
		ltmon_watch_params.watch_id, ltmon_watch_params.watch_mode, ltmon_watch_params.src_maj_st,
		ltmon_watch_params.src_min_st, ltmon_watch_params.src_l0_st, ltmon_watch_params.dst_maj_st,
		ltmon_watch_params.dst_min_st, ltmon_watch_params.dst_l0_st);
}

static int action_cmd_ltmon_capture_stat(struct cxl_memdev *memdev, struct action_context *actx)
{
	if (cxl_memdev_is_active(memdev)) {
		fprintf(stderr, "%s: memdev active, abort ltmon_capture_stat\n",
			cxl_memdev_get_devname(memdev));
		return -EBUSY;
	}

	return cxl_memdev_ltmon_capture_stat(memdev, ltmon_capture_stat_params.cxl_mem_id);
}

static int action_cmd_ltmon_capture_log_dmp(struct cxl_memdev *memdev, struct action_context *actx)
{
	if (cxl_memdev_is_active(memdev)) {
		fprintf(stderr, "%s: memdev active, abort ltmon_capture_log_dmp\n",
			cxl_memdev_get_devname(memdev));
		return -EBUSY;
	}

	return cxl_memdev_ltmon_capture_log_dmp(memdev, ltmon_capture_log_dmp_params.cxl_mem_id,
		ltmon_capture_log_dmp_params.dump_idx, ltmon_capture_log_dmp_params.dump_cnt);
}

static int action_cmd_ltmon_capture_trigger(struct cxl_memdev *memdev, struct action_context *actx)
{
	if (cxl_memdev_is_active(memdev)) {
		fprintf(stderr, "%s: memdev active, abort ltmon_capture_trigger\n",
			cxl_memdev_get_devname(memdev));
		return -EBUSY;
	}

	return cxl_memdev_ltmon_capture_trigger(memdev, ltmon_capture_trigger_params.cxl_mem_id,
		ltmon_capture_trigger_params.trig_src);
}

static int action_cmd_ltmon_enable(struct cxl_memdev *memdev, struct action_context *actx)
{
	if (cxl_memdev_is_active(memdev)) {
		fprintf(stderr, "%s: memdev active, abort ltmon_enable\n",
			cxl_memdev_get_devname(memdev));
		return -EBUSY;
	}

	return cxl_memdev_ltmon_enable(memdev, ltmon_enable_params.cxl_mem_id,
		ltmon_enable_params.enable);
}

static int action_cmd_osa_os_type_trig_cfg(struct cxl_memdev *memdev, struct action_context *actx)
{
	if (cxl_memdev_is_active(memdev)) {
		fprintf(stderr, "%s: memdev active, abort osa_os_type_trig_cfg\n",
			cxl_memdev_get_devname(memdev));
		return -EBUSY;
	}

	return cxl_memdev_osa_os_type_trig_cfg(memdev, osa_os_type_trig_cfg_params.cxl_mem_id,
		osa_os_type_trig_cfg_params.lane_mask, osa_os_type_trig_cfg_params.lane_dir_mask,
		osa_os_type_trig_cfg_params.rate_mask, osa_os_type_trig_cfg_params.os_type_mask);
}

static int action_cmd_osa_cap_ctrl(struct cxl_memdev *memdev, struct action_context *actx)
{
	if (cxl_memdev_is_active(memdev)) {
		fprintf(stderr, "%s: memdev active, abort osa_cap_ctrl\n",
			cxl_memdev_get_devname(memdev));
		return -EBUSY;
	}

	return cxl_memdev_osa_cap_ctrl(memdev, osa_cap_ctrl_params.cxl_mem_id,
		osa_cap_ctrl_params.lane_mask, osa_cap_ctrl_params.lane_dir_mask,
		osa_cap_ctrl_params.drop_single_os, osa_cap_ctrl_params.stop_mode,
		osa_cap_ctrl_params.snapshot_mode, osa_cap_ctrl_params.post_trig_num,
		osa_cap_ctrl_params.os_type_mask);
}

static int action_cmd_osa_cfg_dump(struct cxl_memdev *memdev, struct action_context *actx)
{
	if (cxl_memdev_is_active(memdev)) {
		fprintf(stderr, "%s: memdev active, abort osa_cfg_dump\n",
			cxl_memdev_get_devname(memdev));
		return -EBUSY;
	}

	return cxl_memdev_osa_cfg_dump(memdev, osa_cfg_dump_params.cxl_mem_id);
}

static int action_cmd_osa_ana_op(struct cxl_memdev *memdev, struct action_context *actx)
{
	if (cxl_memdev_is_active(memdev)) {
		fprintf(stderr, "%s: memdev active, abort osa_ana_op\n",
			cxl_memdev_get_devname(memdev));
		return -EBUSY;
	}

	return cxl_memdev_osa_ana_op(memdev, osa_ana_op_params.cxl_mem_id,
		osa_ana_op_params.op);
}

static int action_cmd_osa_status_query(struct cxl_memdev *memdev, struct action_context *actx)
{
	if (cxl_memdev_is_active(memdev)) {
		fprintf(stderr, "%s: memdev active, abort osa_status_query\n",
			cxl_memdev_get_devname(memdev));
		return -EBUSY;
	}

	return cxl_memdev_osa_status_query(memdev, osa_status_query_params.cxl_mem_id);
}

static int action_cmd_osa_access_rel(struct cxl_memdev *memdev, struct action_context *actx)
{
	if (cxl_memdev_is_active(memdev)) {
		fprintf(stderr, "%s: memdev active, abort osa_access_rel\n",
			cxl_memdev_get_devname(memdev));
		return -EBUSY;
	}

	return cxl_memdev_osa_access_rel(memdev, osa_access_rel_params.cxl_mem_id);
}

static int action_cmd_perfcnt_mta_ltif_set(struct cxl_memdev *memdev, struct action_context *actx)
{
	if (cxl_memdev_is_active(memdev)) {
		fprintf(stderr, "%s: memdev active, abort perfcnt_mta_ltif_set\n",
			cxl_memdev_get_devname(memdev));
		return -EBUSY;
	}

	return cxl_memdev_perfcnt_mta_ltif_set(memdev, perfcnt_mta_ltif_set_params.counter,
		perfcnt_mta_ltif_set_params.match_value, perfcnt_mta_ltif_set_params.opcode,
		perfcnt_mta_ltif_set_params.meta_field, perfcnt_mta_ltif_set_params.meta_value);
}

static int action_cmd_perfcnt_mta_get(struct cxl_memdev *memdev, struct action_context *actx)
{
	if (cxl_memdev_is_active(memdev)) {
		fprintf(stderr, "%s: memdev active, abort perfcnt_mta_get\n",
			cxl_memdev_get_devname(memdev));
		return -EBUSY;
	}

	return cxl_memdev_perfcnt_mta_get(memdev, perfcnt_mta_get_params.type,
		perfcnt_mta_get_params.counter);
}

static int action_cmd_perfcnt_mta_latch_val_get(struct cxl_memdev *memdev, struct action_context *actx)
{
	if (cxl_memdev_is_active(memdev)) {
		fprintf(stderr, "%s: memdev active, abort perfcnt_mta_latch_val_get\n",
			cxl_memdev_get_devname(memdev));
		return -EBUSY;
	}

	return cxl_memdev_perfcnt_mta_latch_val_get(memdev, perfcnt_mta_latch_val_get_params.type,
		perfcnt_mta_latch_val_get_params.counter);
}

static int action_cmd_perfcnt_mta_counter_clear(struct cxl_memdev *memdev, struct action_context *actx)
{
	if (cxl_memdev_is_active(memdev)) {
		fprintf(stderr, "%s: memdev active, abort perfcnt_mta_counter_clear\n",
			cxl_memdev_get_devname(memdev));
		return -EBUSY;
	}

	return cxl_memdev_perfcnt_mta_counter_clear(memdev, perfcnt_mta_counter_clear_params.type,
		perfcnt_mta_counter_clear_params.counter);
}

static int action_cmd_perfcnt_mta_cnt_val_latch(struct cxl_memdev *memdev, struct action_context *actx)
{
	if (cxl_memdev_is_active(memdev)) {
		fprintf(stderr, "%s: memdev active, abort perfcnt_mta_cnt_val_latch\n",
			cxl_memdev_get_devname(memdev));
		return -EBUSY;
	}

	return cxl_memdev_perfcnt_mta_cnt_val_latch(memdev, perfcnt_mta_cnt_val_latch_params.type,
		perfcnt_mta_cnt_val_latch_params.counter);
}

static int action_cmd_perfcnt_mta_hif_set(struct cxl_memdev *memdev, struct action_context *actx)
{
	if (cxl_memdev_is_active(memdev)) {
		fprintf(stderr, "%s: memdev active, abort perfcnt_mta_hif_set\n",
			cxl_memdev_get_devname(memdev));
		return -EBUSY;
	}

	return cxl_memdev_perfcnt_mta_hif_set(memdev, perfcnt_mta_hif_set_params.counter,
		perfcnt_mta_hif_set_params.match_value, perfcnt_mta_hif_set_params.addr,
		perfcnt_mta_hif_set_params.req_ty, perfcnt_mta_hif_set_params.sc_ty);
}

static int action_cmd_perfcnt_mta_hif_cfg_get(struct cxl_memdev *memdev, struct action_context *actx)
{
	if (cxl_memdev_is_active(memdev)) {
		fprintf(stderr, "%s: memdev active, abort perfcnt_mta_hif_cfg_get\n",
			cxl_memdev_get_devname(memdev));
		return -EBUSY;
	}

	return cxl_memdev_perfcnt_mta_hif_cfg_get(memdev, perfcnt_mta_hif_cfg_get_params.counter);
}

static int action_cmd_perfcnt_mta_hif_latch_val_get(struct cxl_memdev *memdev, struct action_context *actx)
{
	if (cxl_memdev_is_active(memdev)) {
		fprintf(stderr, "%s: memdev active, abort perfcnt_mta_hif_latch_val_get\n",
			cxl_memdev_get_devname(memdev));
		return -EBUSY;
	}

	return cxl_memdev_perfcnt_mta_hif_latch_val_get(memdev, perfcnt_mta_hif_latch_val_get_params.counter);
}

static int action_cmd_perfcnt_mta_hif_counter_clear(struct cxl_memdev *memdev, struct action_context *actx)
{
	if (cxl_memdev_is_active(memdev)) {
		fprintf(stderr, "%s: memdev active, abort perfcnt_mta_hif_counter_clear\n",
			cxl_memdev_get_devname(memdev));
		return -EBUSY;
	}

	return cxl_memdev_perfcnt_mta_hif_counter_clear(memdev, perfcnt_mta_hif_counter_clear_params.counter);
}

static int action_cmd_perfcnt_mta_hif_cnt_val_latch(struct cxl_memdev *memdev, struct action_context *actx)
{
	if (cxl_memdev_is_active(memdev)) {
		fprintf(stderr, "%s: memdev active, abort perfcnt_mta_hif_cnt_val_latch\n",
			cxl_memdev_get_devname(memdev));
		return -EBUSY;
	}

	return cxl_memdev_perfcnt_mta_hif_cnt_val_latch(memdev, perfcnt_mta_hif_cnt_val_latch_params.counter);
}

static int action_cmd_perfcnt_ddr_generic_select(struct cxl_memdev *memdev, struct action_context *actx)
{
	if (cxl_memdev_is_active(memdev)) {
		fprintf(stderr, "%s: memdev active, abort perfcnt_ddr_generic_select\n",
			cxl_memdev_get_devname(memdev));
		return -EBUSY;
	}

	return cxl_memdev_perfcnt_ddr_generic_select(memdev, perfcnt_ddr_generic_select_params.ddr_id,
		perfcnt_ddr_generic_select_params.cid, perfcnt_ddr_generic_select_params.rank,
		perfcnt_ddr_generic_select_params.bank, perfcnt_ddr_generic_select_params.bankgroup,
		(void *) perfcnt_ddr_generic_select_params.event);
}

static int action_cmd_err_inj_drs_poison(struct cxl_memdev *memdev, struct action_context *actx)
{
	if (cxl_memdev_is_active(memdev)) {
		fprintf(stderr, "%s: memdev active, abort err_inj_drs_poison\n",
			cxl_memdev_get_devname(memdev));
		return -EBUSY;
	}

	return cxl_memdev_err_inj_drs_poison(memdev, err_inj_drs_poison_params.ch_id,
		err_inj_drs_poison_params.duration, err_inj_drs_poison_params.inj_mode,
		err_inj_drs_poison_params.tag);
}

static int action_cmd_err_inj_drs_ecc(struct cxl_memdev *memdev, struct action_context *actx)
{
	if (cxl_memdev_is_active(memdev)) {
		fprintf(stderr, "%s: memdev active, abort err_inj_drs_ecc\n",
			cxl_memdev_get_devname(memdev));
		return -EBUSY;
	}

	return cxl_memdev_err_inj_drs_ecc(memdev, err_inj_drs_ecc_params.ch_id,
		err_inj_drs_ecc_params.duration, err_inj_drs_ecc_params.inj_mode,
		err_inj_drs_ecc_params.tag);
}

static int action_cmd_err_inj_rxflit_crc(struct cxl_memdev *memdev, struct action_context *actx)
{
	if (cxl_memdev_is_active(memdev)) {
		fprintf(stderr, "%s: memdev active, abort err_inj_rxflit_crc\n",
			cxl_memdev_get_devname(memdev));
		return -EBUSY;
	}

	return cxl_memdev_err_inj_rxflit_crc(memdev, err_inj_rxflit_crc_params.cxl_mem_id);
}

static int action_cmd_err_inj_txflit_crc(struct cxl_memdev *memdev, struct action_context *actx)
{
	if (cxl_memdev_is_active(memdev)) {
		fprintf(stderr, "%s: memdev active, abort err_inj_txflit_crc\n",
			cxl_memdev_get_devname(memdev));
		return -EBUSY;
	}

	return cxl_memdev_err_inj_txflit_crc(memdev, err_inj_txflit_crc_params.cxl_mem_id);
}

static int action_cmd_err_inj_viral(struct cxl_memdev *memdev, struct action_context *actx)
{
	if (cxl_memdev_is_active(memdev)) {
		fprintf(stderr, "%s: memdev active, abort err_inj_viral\n",
			cxl_memdev_get_devname(memdev));
		return -EBUSY;
	}

	return cxl_memdev_err_inj_viral(memdev, err_inj_viral_params.ld_id);
}

static int action_cmd_eh_eye_cap_run(struct cxl_memdev *memdev, struct action_context *actx)
{
	if (cxl_memdev_is_active(memdev)) {
		fprintf(stderr, "%s: memdev active, abort eh_eye_cap_run\n",
			cxl_memdev_get_devname(memdev));
		return -EBUSY;
	}

	return cxl_memdev_eh_eye_cap_run(memdev, eh_eye_cap_run_params.depth,
		eh_eye_cap_run_params.lane_mask);
}

static int action_cmd_eh_eye_cap_read(struct cxl_memdev *memdev, struct action_context *actx)
{
	if (cxl_memdev_is_active(memdev)) {
		fprintf(stderr, "%s: memdev active, abort eh_eye_cap_read\n",
			cxl_memdev_get_devname(memdev));
		return -EBUSY;
	}

	return cxl_memdev_eh_eye_cap_read(memdev, eh_eye_cap_read_params.lane_id,
		eh_eye_cap_read_params.bin_num);
}

static int action_cmd_eh_adapt_get(struct cxl_memdev *memdev, struct action_context *actx)
{
	if (cxl_memdev_is_active(memdev)) {
		fprintf(stderr, "%s: memdev active, abort eh_adapt_get\n",
			cxl_memdev_get_devname(memdev));
		return -EBUSY;
	}

	return cxl_memdev_eh_adapt_get(memdev, eh_adapt_get_params.lane_id);
}

static int action_cmd_eh_adapt_oneoff(struct cxl_memdev *memdev, struct action_context *actx)
{
	if (cxl_memdev_is_active(memdev)) {
		fprintf(stderr, "%s: memdev active, abort eh_adapt_oneoff\n",
			cxl_memdev_get_devname(memdev));
		return -EBUSY;
	}

	return cxl_memdev_eh_adapt_oneoff(memdev, eh_adapt_oneoff_params.lane_id,
		eh_adapt_oneoff_params.preload, eh_adapt_oneoff_params.loops, eh_adapt_oneoff_params.objects);
}

static int action_cmd_eh_adapt_force(struct cxl_memdev *memdev, struct action_context *actx)
{
	if (cxl_memdev_is_active(memdev)) {
		fprintf(stderr, "%s: memdev active, abort eh_adapt_force\n",
			cxl_memdev_get_devname(memdev));
		return -EBUSY;
	}

	return cxl_memdev_eh_adapt_force(memdev, eh_adapt_force_params.lane_id,
		eh_adapt_force_params.rate, eh_adapt_force_params.vdd_bias, eh_adapt_force_params.ssc,
		eh_adapt_force_params.pga_gain, eh_adapt_force_params.pga_a0, eh_adapt_force_params.pga_off,
		eh_adapt_force_params.cdfe_a2, eh_adapt_force_params.cdfe_a3, eh_adapt_force_params.cdfe_a4,
		eh_adapt_force_params.cdfe_a5, eh_adapt_force_params.cdfe_a6, eh_adapt_force_params.cdfe_a7,
		eh_adapt_force_params.cdfe_a8, eh_adapt_force_params.cdfe_a9, eh_adapt_force_params.cdfe_a10,
		eh_adapt_force_params.dc_offset, eh_adapt_force_params.zobel_dc_offset,
		eh_adapt_force_params.udfe_thr_0, eh_adapt_force_params.udfe_thr_1,
		eh_adapt_force_params.median_amp, eh_adapt_force_params.zobel_a_gain,
		eh_adapt_force_params.ph_ofs_t);
}

static int action_cmd_hbo_status(struct cxl_memdev *memdev, struct action_context *actx)
{
	if (cxl_memdev_is_active(memdev)) {
		fprintf(stderr, "%s: memdev active, abort hbo_status\n",
			cxl_memdev_get_devname(memdev));
		return -EBUSY;
	}

	return cxl_memdev_hbo_status(memdev);
}

static int action_cmd_hbo_transfer_fw(struct cxl_memdev *memdev, struct action_context *actx)
{
	if (cxl_memdev_is_active(memdev)) {
		fprintf(stderr, "%s: memdev active, abort hbo_transfer_fw\n",
			cxl_memdev_get_devname(memdev));
		return -EBUSY;
	}

	return cxl_memdev_hbo_transfer_fw(memdev);
}

static int action_cmd_hbo_activate_fw(struct cxl_memdev *memdev, struct action_context *actx)
{
	if (cxl_memdev_is_active(memdev)) {
		fprintf(stderr, "%s: memdev active, abort hbo_activate_fw\n",
			cxl_memdev_get_devname(memdev));
		return -EBUSY;
	}

	return cxl_memdev_hbo_activate_fw(memdev);
}

static int action_cmd_health_counters_clear(struct cxl_memdev *memdev, struct action_context *actx)
{
	if (cxl_memdev_is_active(memdev)) {
		fprintf(stderr, "%s: memdev active, abort health_counters_clear\n",
			cxl_memdev_get_devname(memdev));
		return -EBUSY;
	}

	return cxl_memdev_health_counters_clear(memdev, health_counters_clear_params.bitmask);
}

static int action_cmd_health_counters_get(struct cxl_memdev *memdev, struct action_context *actx)
{
	if (cxl_memdev_is_active(memdev)) {
		fprintf(stderr, "%s: memdev active, abort health_counters_get\n",
			cxl_memdev_get_devname(memdev));
		return -EBUSY;
	}

	return cxl_memdev_health_counters_get(memdev);
}

static int action_cmd_hct_get_plat_param(struct cxl_memdev *memdev, struct action_context *actx)
{
	if (cxl_memdev_is_active(memdev)) {
		fprintf(stderr, "%s: memdev active, abort hct_get_plat_param\n",
			cxl_memdev_get_devname(memdev));
		return -EBUSY;
	}

	return cxl_memdev_hct_get_plat_param(memdev);
}

static int action_zero(struct cxl_memdev *memdev, struct action_context *actx)
{
	int rc;

	if (cxl_memdev_is_active(memdev)) {
		fprintf(stderr, "%s: memdev active, abort label write\n",
			cxl_memdev_get_devname(memdev));
		return -EBUSY;
	}

	rc = cxl_memdev_zero_lsa(memdev);
	if (rc < 0)
		fprintf(stderr, "%s: label zeroing failed: %s\n",
			cxl_memdev_get_devname(memdev), strerror(-rc));

	return rc;
}

static int action_write(struct cxl_memdev *memdev, struct action_context *actx)
{
	size_t size = param.len, read_len;
	unsigned char *buf;
	int rc;

	if (cxl_memdev_is_active(memdev)) {
		fprintf(stderr, "%s is active, abort label write\n",
			cxl_memdev_get_devname(memdev));
		return -EBUSY;
	}

	if (!size) {
		size_t lsa_size = cxl_memdev_get_lsa_size(memdev);

		fseek(actx->f_in, 0L, SEEK_END);
		size = ftell(actx->f_in);
		fseek(actx->f_in, 0L, SEEK_SET);

		if (size > lsa_size) {
			fprintf(stderr,
				"File size (%zu) greater than LSA size (%zu), aborting\n",
				size, lsa_size);
			return -EINVAL;
		}
	}

	buf = calloc(1, size);
	if (!buf)
		return -ENOMEM;

	read_len = fread(buf, 1, size, actx->f_in);
	if (read_len != size) {
		rc = -ENXIO;
		goto out;
	}

	rc = cxl_memdev_set_lsa(memdev, buf, size, param.offset);
	if (rc < 0)
		fprintf(stderr, "%s: label write failed: %s\n",
			cxl_memdev_get_devname(memdev), strerror(-rc));

out:
	free(buf);
	return rc;
}

static int action_read(struct cxl_memdev *memdev, struct action_context *actx)
{
	size_t size = param.len, write_len;
	char *buf;
	int rc;

	if (!size)
		size = cxl_memdev_get_lsa_size(memdev);

	buf = calloc(1, size);
	if (!buf)
		return -ENOMEM;

	rc = cxl_memdev_get_lsa(memdev, buf, size, param.offset);
	if (rc < 0) {
		fprintf(stderr, "%s: label read failed: %s\n",
			cxl_memdev_get_devname(memdev), strerror(-rc));
		goto out;
	}

	write_len = fwrite(buf, 1, size, actx->f_out);
	if (write_len != size) {
		rc = -ENXIO;
		goto out;
	}
	fflush(actx->f_out);

out:
	free(buf);
	return rc;
}

static int memdev_action(int argc, const char **argv, struct cxl_ctx *ctx,
		int (*action)(struct cxl_memdev *memdev, struct action_context *actx),
		const struct option *options, const char *usage)
{
	struct cxl_memdev *memdev, *single = NULL;
	struct action_context actx = { 0 };
	int i, rc = 0, count = 0, err = 0;
	const char * const u[] = {
		usage,
		NULL
	};
	unsigned long id;

	argc = parse_options(argc, argv, options, u, 0);

	if (argc == 0)
		usage_with_options(u, options);
	for (i = 0; i < argc; i++) {
		if (strcmp(argv[i], "all") == 0) {
			argv[0] = "all";
			argc = 1;
			break;
		}

		if (sscanf(argv[i], "mem%lu", &id) != 1) {
			fprintf(stderr, "'%s' is not a valid memdev name\n",
					argv[i]);
			err++;
		}
	}

	if (err == argc) {
		usage_with_options(u, options);
		return -EINVAL;
	}

	if (!param.outfile)
		actx.f_out = stdout;
	else {
		actx.f_out = fopen(param.outfile, "w+");
		if (!actx.f_out) {
			fprintf(stderr, "failed to open: %s: (%s)\n",
					param.outfile, strerror(errno));
			rc = -errno;
			goto out;
		}
	}

	if (!param.infile) {
		actx.f_in = stdin;
	} else {
		actx.f_in = fopen(param.infile, "r");
		if (!actx.f_in) {
			fprintf(stderr, "failed to open: %s: (%s)\n",
					param.infile, strerror(errno));
			rc = -errno;
			goto out_close_fout;
		}
	}

	if (param.verbose)
		cxl_set_log_priority(ctx, LOG_DEBUG);

	rc = 0;
	err = 0;
	count = 0;

	for (i = 0; i < argc; i++) {
		if (sscanf(argv[i], "mem%lu", &id) != 1
				&& strcmp(argv[i], "all") != 0)
			continue;

		cxl_memdev_foreach (ctx, memdev) {
			if (!util_cxl_memdev_filter(memdev, argv[i]))
				continue;

			if (action == action_write) {
				single = memdev;
				rc = 0;
			} else
				rc = action(memdev, &actx);

			if (rc == 0)
				count++;
			else if (rc && !err)
				err = rc;
		}
	}
	rc = err;

	if (action == action_write) {
		if (count > 1) {
			error("write-labels only supports writing a single memdev\n");
			usage_with_options(u, options);
			return -EINVAL;
		} else if (single) {
			rc = action(single, &actx);
			if (rc)
				count = 0;
		}
	}

	if (actx.f_in != stdin)
		fclose(actx.f_in);

 out_close_fout:
	if (actx.f_out != stdout)
		fclose(actx.f_out);

 out:
	/*
	 * count if some actions succeeded, 0 if none were attempted,
	 * negative error code otherwise.
	 */
	if (count > 0)
		return count;
	return rc;
}

int cmd_write_labels(int argc, const char **argv, struct cxl_ctx *ctx)
{
	int count = memdev_action(argc, argv, ctx, action_write, write_options,
			"cxl write-labels <memdev> [-i <filename>]");

	fprintf(stderr, "wrote %d mem%s\n", count >= 0 ? count : 0,
			count > 1 ? "s" : "");
	return count >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_read_labels(int argc, const char **argv, struct cxl_ctx *ctx)
{
	int count = memdev_action(argc, argv, ctx, action_read, read_options,
			"cxl read-labels <mem0> [<mem1>..<memN>] [-o <filename>]");

	fprintf(stderr, "read %d mem%s\n", count >= 0 ? count : 0,
			count > 1 ? "s" : "");
	return count >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_zero_labels(int argc, const char **argv, struct cxl_ctx *ctx)
{
	int count = memdev_action(argc, argv, ctx, action_zero, zero_options,
			"cxl zero-labels <mem0> [<mem1>..<memN>] [<options>]");

	fprintf(stderr, "zeroed %d mem%s\n", count >= 0 ? count : 0,
			count > 1 ? "s" : "");
	return count >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_identify(int argc, const char **argv, struct cxl_ctx *ctx)
{
	int rc = memdev_action(argc, argv, ctx, action_cmd_identify, cmd_identify_options,
			"cxl id-cmd <mem0> [<mem1>..<memN>] [<options>]");

	return rc >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_get_supported_logs(int argc, const char **argv, struct cxl_ctx *ctx)
{
	int rc = memdev_action(argc, argv, ctx, action_cmd_get_supported_logs, cmd_get_supported_logs_options,
			"cxl get-supported-logs <mem0> [<mem1>..<memN>] [<options>]");

	return rc >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_get_cel_log(int argc, const char **argv, struct cxl_ctx *ctx)
{
	int rc = memdev_action(argc, argv, ctx, action_cmd_get_cel_log, cmd_get_cel_log_options,
			"cxl get-cel-log <mem0> [<mem1>..<memN>] [<options>]");

	return rc >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_get_event_interrupt_policy(int argc, const char **argv, struct cxl_ctx *ctx)
{
	int rc = memdev_action(argc, argv, ctx, action_cmd_get_event_interrupt_policy, cmd_get_event_interrupt_policy_options,
			"cxl get-event-interrupt-policy <mem0> [<mem1>..<memN>] [<options>]");

	return rc >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_set_event_interrupt_policy(int argc, const char **argv, struct cxl_ctx *ctx)
{
	int rc = memdev_action(argc, argv, ctx, action_cmd_set_event_interrupt_policy, cmd_set_event_interrupt_policy_options,
			"cxl set-event-interrupt-policy <mem0> [<mem1>..<memN>] [<options>]");

	return rc >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_get_timestamp(int argc, const char **argv, struct cxl_ctx *ctx)
{
	int rc = memdev_action(argc, argv, ctx, action_cmd_get_timestamp, cmd_get_timestamp_options,
			"cxl get-timestamp <mem0> [<mem1>..<memN>] [<options>]");

	return rc >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_device_info_get(int argc, const char **argv, struct cxl_ctx *ctx)
{
	int rc = memdev_action(argc, argv, ctx, action_cmd_device_info_get, cmd_device_info_get_options,
			"cxl device_info_get <mem0> [<mem1>..<memN>] [<options>]");

	return rc >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_get_fw_info(int argc, const char **argv, struct cxl_ctx *ctx)
{
	int rc = memdev_action(argc, argv, ctx, action_cmd_get_fw_info, cmd_get_fw_info_options,
			"cxl get_fw_info <mem0> [<mem1>..<memN>] [<options>]");

	return rc >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_activate_fw(int argc, const char **argv, struct cxl_ctx *ctx)
{
	int rc = memdev_action(argc, argv, ctx, action_cmd_activate_fw, cmd_activate_fw_options,
			"cxl activate_fw <mem0> [<mem1>..<memN>] [<options>]");

	return rc >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_set_timestamp(int argc, const char **argv, struct cxl_ctx *ctx)
{
	int rc = memdev_action(argc, argv, ctx, action_cmd_set_timestamp, cmd_set_timestamp_options,
			"cxl set-timestamp <mem0> [<mem1>..<memN>] [<options>]");

	return rc >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_get_alert_config(int argc, const char **argv, struct cxl_ctx *ctx)
{
	int rc = memdev_action(argc, argv, ctx, action_cmd_get_alert_config, cmd_get_alert_config_options,
			"cxl get-alert-config <mem0> [<mem1>..<memN>] [<options>]");

	return rc >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_update_fw(int argc, const char **argv, struct cxl_ctx *ctx)
{
	int rc = memdev_action(argc, argv, ctx, action_cmd_update_fw, cmd_update_fw_options,
			"cxl update-fw <mem0> [<mem1>..<memN>] [<options>]");

	return rc >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_set_alert_config(int argc, const char **argv, struct cxl_ctx *ctx)
{
	int rc = memdev_action(argc, argv, ctx, action_cmd_set_alert_config, cmd_set_alert_config_options,
			"cxl set-alert-config <mem0> [<mem1>..<memN>] [<options>]");

	return rc >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_get_health_info(int argc, const char **argv, struct cxl_ctx *ctx)
{
	int rc = memdev_action(argc, argv, ctx, action_cmd_get_health_info, cmd_get_health_info_options,
			"cxl get-health-info <mem0> [<mem1>..<memN>] [<options>]");

	return rc >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_get_event_records(int argc, const char **argv, struct cxl_ctx *ctx)
{
	int rc = memdev_action(argc, argv, ctx, action_cmd_get_event_records, cmd_get_event_records_options,
			"cxl get-event-records <mem0> [<mem1>..<memN>] [<options>]");

	return rc >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_get_ld_info(int argc, const char **argv, struct cxl_ctx *ctx)
{
	int rc = memdev_action(argc, argv, ctx, action_cmd_get_ld_info, cmd_get_ld_info_options,
			"cxl get-ld-info <mem0> [<mem1>..<memN>] [<options>]");

	return rc >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_ddr_info(int argc, const char **argv, struct cxl_ctx *ctx)
{
	int rc = memdev_action(argc, argv, ctx, action_cmd_ddr_info, cmd_ddr_info_options,
			"cxl ddr-info <mem0> [<mem1>..<memN>] [-i <ddr_instance_id>]");

	return rc >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_clear_event_records(int argc, const char **argv, struct cxl_ctx *ctx)
{
	int rc = memdev_action(argc, argv, ctx, action_cmd_clear_event_records, cmd_clear_event_records_options,
			"cxl clear-event-records <mem0> [<mem1>..<memN>] [<options>]");

	return rc >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_hct_start_stop_trigger(int argc, const char **argv, struct cxl_ctx *ctx)
{
	int rc = memdev_action(argc, argv, ctx, action_cmd_hct_start_stop_trigger, cmd_hct_start_stop_trigger_options,
			"cxl hct_start_stop_trigger <mem0> [<mem1>..<memN>] [<options>]");

	return rc >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_hct_get_buffer_status(int argc, const char **argv, struct cxl_ctx *ctx)
{
	int rc = memdev_action(argc, argv, ctx, action_cmd_hct_get_buffer_status, cmd_hct_get_buffer_status_options,
			"cxl hct_get_buffer_status <mem0> [<mem1>..<memN>] [<options>]");

	return rc >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_hct_enable(int argc, const char **argv, struct cxl_ctx *ctx)
{
	int rc = memdev_action(argc, argv, ctx, action_cmd_hct_enable, cmd_hct_enable_options,
			"cxl hct_enable <mem0> [<mem1>..<memN>] [<options>]");

	return rc >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_ltmon_capture_clear(int argc, const char **argv, struct cxl_ctx *ctx)
{
	int rc = memdev_action(argc, argv, ctx, action_cmd_ltmon_capture_clear, cmd_ltmon_capture_clear_options,
			"cxl ltmon_capture_clear <mem0> [<mem1>..<memN>] [<options>]");

	return rc >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_ltmon_capture(int argc, const char **argv, struct cxl_ctx *ctx)
{
	int rc = memdev_action(argc, argv, ctx, action_cmd_ltmon_capture, cmd_ltmon_capture_options,
			"cxl ltmon_capture <mem0> [<mem1>..<memN>] [<options>]");

	return rc >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_ltmon_capture_freeze_and_restore(int argc, const char **argv, struct cxl_ctx *ctx)
{
	int rc = memdev_action(argc, argv, ctx, action_cmd_ltmon_capture_freeze_and_restore, cmd_ltmon_capture_freeze_and_restore_options,
			"cxl ltmon_capture_freeze_and_restore <mem0> [<mem1>..<memN>] [<options>]");

	return rc >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_ltmon_l2r_count_dump(int argc, const char **argv, struct cxl_ctx *ctx)
{
	int rc = memdev_action(argc, argv, ctx, action_cmd_ltmon_l2r_count_dump, cmd_ltmon_l2r_count_dump_options,
			"cxl ltmon_l2r_count_dump <mem0> [<mem1>..<memN>] [<options>]");

	return rc >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_ltmon_l2r_count_clear(int argc, const char **argv, struct cxl_ctx *ctx)
{
	int rc = memdev_action(argc, argv, ctx, action_cmd_ltmon_l2r_count_clear, cmd_ltmon_l2r_count_clear_options,
			"cxl ltmon_l2r_count_clear <mem0> [<mem1>..<memN>] [<options>]");

	return rc >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_ltmon_basic_cfg(int argc, const char **argv, struct cxl_ctx *ctx)
{
	int rc = memdev_action(argc, argv, ctx, action_cmd_ltmon_basic_cfg, cmd_ltmon_basic_cfg_options,
			"cxl ltmon_basic_cfg <mem0> [<mem1>..<memN>] [<options>]");

	return rc >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_ltmon_watch(int argc, const char **argv, struct cxl_ctx *ctx)
{
	int rc = memdev_action(argc, argv, ctx, action_cmd_ltmon_watch, cmd_ltmon_watch_options,
			"cxl ltmon_watch <mem0> [<mem1>..<memN>] [<options>]");

	return rc >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_ltmon_capture_stat(int argc, const char **argv, struct cxl_ctx *ctx)
{
	int rc = memdev_action(argc, argv, ctx, action_cmd_ltmon_capture_stat, cmd_ltmon_capture_stat_options,
			"cxl ltmon_capture_stat <mem0> [<mem1>..<memN>] [<options>]");

	return rc >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_ltmon_capture_log_dmp(int argc, const char **argv, struct cxl_ctx *ctx)
{
	int rc = memdev_action(argc, argv, ctx, action_cmd_ltmon_capture_log_dmp, cmd_ltmon_capture_log_dmp_options,
			"cxl ltmon_capture_log_dmp <mem0> [<mem1>..<memN>] [<options>]");

	return rc >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_ltmon_capture_trigger(int argc, const char **argv, struct cxl_ctx *ctx)
{
	int rc = memdev_action(argc, argv, ctx, action_cmd_ltmon_capture_trigger, cmd_ltmon_capture_trigger_options,
			"cxl ltmon_capture_trigger <mem0> [<mem1>..<memN>] [<options>]");

	return rc >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_ltmon_enable(int argc, const char **argv, struct cxl_ctx *ctx)
{
	int rc = memdev_action(argc, argv, ctx, action_cmd_ltmon_enable, cmd_ltmon_enable_options,
			"cxl ltmon_enable <mem0> [<mem1>..<memN>] [<options>]");

	return rc >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_osa_os_type_trig_cfg(int argc, const char **argv, struct cxl_ctx *ctx)
{
	int rc = memdev_action(argc, argv, ctx, action_cmd_osa_os_type_trig_cfg, cmd_osa_os_type_trig_cfg_options,
			"cxl osa_os_type_trig_cfg <mem0> [<mem1>..<memN>] [<options>]");

	return rc >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_osa_cap_ctrl(int argc, const char **argv, struct cxl_ctx *ctx)
{
	int rc = memdev_action(argc, argv, ctx, action_cmd_osa_cap_ctrl, cmd_osa_cap_ctrl_options,
			"cxl osa_cap_ctrl <mem0> [<mem1>..<memN>] [<options>]");

	return rc >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_osa_cfg_dump(int argc, const char **argv, struct cxl_ctx *ctx)
{
	int rc = memdev_action(argc, argv, ctx, action_cmd_osa_cfg_dump, cmd_osa_cfg_dump_options,
			"cxl osa_cfg_dump <mem0> [<mem1>..<memN>] [<options>]");

	return rc >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_osa_ana_op(int argc, const char **argv, struct cxl_ctx *ctx)
{
	int rc = memdev_action(argc, argv, ctx, action_cmd_osa_ana_op, cmd_osa_ana_op_options,
			"cxl osa_ana_op <mem0> [<mem1>..<memN>] [<options>]");

	return rc >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_osa_status_query(int argc, const char **argv, struct cxl_ctx *ctx)
{
	int rc = memdev_action(argc, argv, ctx, action_cmd_osa_status_query, cmd_osa_status_query_options,
			"cxl osa_status_query <mem0> [<mem1>..<memN>] [<options>]");

	return rc >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_osa_access_rel(int argc, const char **argv, struct cxl_ctx *ctx)
{
	int rc = memdev_action(argc, argv, ctx, action_cmd_osa_access_rel, cmd_osa_access_rel_options,
			"cxl osa_access_rel <mem0> [<mem1>..<memN>] [<options>]");

	return rc >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_perfcnt_mta_ltif_set(int argc, const char **argv, struct cxl_ctx *ctx)
{
	int rc = memdev_action(argc, argv, ctx, action_cmd_perfcnt_mta_ltif_set, cmd_perfcnt_mta_ltif_set_options,
			"cxl perfcnt_mta_ltif_set <mem0> [<mem1>..<memN>] [<options>]");

	return rc >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_perfcnt_mta_get(int argc, const char **argv, struct cxl_ctx *ctx)
{
	int rc = memdev_action(argc, argv, ctx, action_cmd_perfcnt_mta_get, cmd_perfcnt_mta_get_options,
			"cxl perfcnt_mta_get <mem0> [<mem1>..<memN>] [<options>]");

	return rc >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_perfcnt_mta_latch_val_get(int argc, const char **argv, struct cxl_ctx *ctx)
{
	int rc = memdev_action(argc, argv, ctx, action_cmd_perfcnt_mta_latch_val_get, cmd_perfcnt_mta_latch_val_get_options,
			"cxl perfcnt_mta_latch_val_get <mem0> [<mem1>..<memN>] [<options>]");

	return rc >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_perfcnt_mta_counter_clear(int argc, const char **argv, struct cxl_ctx *ctx)
{
	int rc = memdev_action(argc, argv, ctx, action_cmd_perfcnt_mta_counter_clear, cmd_perfcnt_mta_counter_clear_options,
			"cxl perfcnt_mta_counter_clear <mem0> [<mem1>..<memN>] [<options>]");

	return rc >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_perfcnt_mta_cnt_val_latch(int argc, const char **argv, struct cxl_ctx *ctx)
{
	int rc = memdev_action(argc, argv, ctx, action_cmd_perfcnt_mta_cnt_val_latch, cmd_perfcnt_mta_cnt_val_latch_options,
			"cxl perfcnt_mta_cnt_val_latch <mem0> [<mem1>..<memN>] [<options>]");

	return rc >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_perfcnt_mta_hif_set(int argc, const char **argv, struct cxl_ctx *ctx)
{
	int rc = memdev_action(argc, argv, ctx, action_cmd_perfcnt_mta_hif_set, cmd_perfcnt_mta_hif_set_options,
			"cxl perfcnt_mta_hif_set <mem0> [<mem1>..<memN>] [<options>]");

	return rc >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_perfcnt_mta_hif_cfg_get(int argc, const char **argv, struct cxl_ctx *ctx)
{
	int rc = memdev_action(argc, argv, ctx, action_cmd_perfcnt_mta_hif_cfg_get, cmd_perfcnt_mta_hif_cfg_get_options,
			"cxl perfcnt_mta_hif_cfg_get <mem0> [<mem1>..<memN>] [<options>]");

	return rc >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_perfcnt_mta_hif_latch_val_get(int argc, const char **argv, struct cxl_ctx *ctx)
{
	int rc = memdev_action(argc, argv, ctx, action_cmd_perfcnt_mta_hif_latch_val_get, cmd_perfcnt_mta_hif_latch_val_get_options,
			"cxl perfcnt_mta_hif_latch_val_get <mem0> [<mem1>..<memN>] [<options>]");

	return rc >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_perfcnt_mta_hif_counter_clear(int argc, const char **argv, struct cxl_ctx *ctx)
{
	int rc = memdev_action(argc, argv, ctx, action_cmd_perfcnt_mta_hif_counter_clear, cmd_perfcnt_mta_hif_counter_clear_options,
			"cxl perfcnt_mta_hif_counter_clear <mem0> [<mem1>..<memN>] [<options>]");

	return rc >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_perfcnt_mta_hif_cnt_val_latch(int argc, const char **argv, struct cxl_ctx *ctx)
{
	int rc = memdev_action(argc, argv, ctx, action_cmd_perfcnt_mta_hif_cnt_val_latch, cmd_perfcnt_mta_hif_cnt_val_latch_options,
			"cxl perfcnt_mta_hif_cnt_val_latch <mem0> [<mem1>..<memN>] [<options>]");

	return rc >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_perfcnt_ddr_generic_select(int argc, const char **argv, struct cxl_ctx *ctx)
{
	int rc = memdev_action(argc, argv, ctx, action_cmd_perfcnt_ddr_generic_select, cmd_perfcnt_ddr_generic_select_options,
			"cxl perfcnt_ddr_generic_select <mem0> [<mem1>..<memN>] [<options>]");

	return rc >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_err_inj_drs_poison(int argc, const char **argv, struct cxl_ctx *ctx)
{
	int rc = memdev_action(argc, argv, ctx, action_cmd_err_inj_drs_poison, cmd_err_inj_drs_poison_options,
			"cxl err_inj_drs_poison <mem0> [<mem1>..<memN>] [<options>]");

	return rc >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_err_inj_drs_ecc(int argc, const char **argv, struct cxl_ctx *ctx)
{
	int rc = memdev_action(argc, argv, ctx, action_cmd_err_inj_drs_ecc, cmd_err_inj_drs_ecc_options,
			"cxl err_inj_drs_ecc <mem0> [<mem1>..<memN>] [<options>]");

	return rc >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_err_inj_rxflit_crc(int argc, const char **argv, struct cxl_ctx *ctx)
{
	int rc = memdev_action(argc, argv, ctx, action_cmd_err_inj_rxflit_crc, cmd_err_inj_rxflit_crc_options,
			"cxl err_inj_rxflit_crc <mem0> [<mem1>..<memN>] [<options>]");

	return rc >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_err_inj_txflit_crc(int argc, const char **argv, struct cxl_ctx *ctx)
{
	int rc = memdev_action(argc, argv, ctx, action_cmd_err_inj_txflit_crc, cmd_err_inj_txflit_crc_options,
			"cxl err_inj_txflit_crc <mem0> [<mem1>..<memN>] [<options>]");

	return rc >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_err_inj_viral(int argc, const char **argv, struct cxl_ctx *ctx)
{
	int rc = memdev_action(argc, argv, ctx, action_cmd_err_inj_viral, cmd_err_inj_viral_options,
			"cxl err_inj_viral <mem0> [<mem1>..<memN>] [<options>]");

	return rc >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_eh_eye_cap_run(int argc, const char **argv, struct cxl_ctx *ctx)
{
	int rc = memdev_action(argc, argv, ctx, action_cmd_eh_eye_cap_run, cmd_eh_eye_cap_run_options,
			"cxl eh_eye_cap_run <mem0> [<mem1>..<memN>] [<options>]");

	return rc >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_eh_eye_cap_read(int argc, const char **argv, struct cxl_ctx *ctx)
{
	int rc = memdev_action(argc, argv, ctx, action_cmd_eh_eye_cap_read, cmd_eh_eye_cap_read_options,
			"cxl eh_eye_cap_read <mem0> [<mem1>..<memN>] [<options>]");

	return rc >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_eh_adapt_get(int argc, const char **argv, struct cxl_ctx *ctx)
{
	int rc = memdev_action(argc, argv, ctx, action_cmd_eh_adapt_get, cmd_eh_adapt_get_options,
			"cxl eh_adapt_get <mem0> [<mem1>..<memN>] [<options>]");

	return rc >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_eh_adapt_oneoff(int argc, const char **argv, struct cxl_ctx *ctx)
{
	int rc = memdev_action(argc, argv, ctx, action_cmd_eh_adapt_oneoff, cmd_eh_adapt_oneoff_options,
			"cxl eh_adapt_oneoff <mem0> [<mem1>..<memN>] [<options>]");

	return rc >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_eh_adapt_force(int argc, const char **argv, struct cxl_ctx *ctx)
{
	int rc = memdev_action(argc, argv, ctx, action_cmd_eh_adapt_force, cmd_eh_adapt_force_options,
			"cxl eh_adapt_force <mem0> [<mem1>..<memN>] [<options>]");

	return rc >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_hbo_status(int argc, const char **argv, struct cxl_ctx *ctx)
{
	int rc = memdev_action(argc, argv, ctx, action_cmd_hbo_status, cmd_hbo_status_options,
			"cxl hbo_status <mem0> [<mem1>..<memN>] [<options>]");

	return rc >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_hbo_transfer_fw(int argc, const char **argv, struct cxl_ctx *ctx)
{
	int rc = memdev_action(argc, argv, ctx, action_cmd_hbo_transfer_fw, cmd_hbo_transfer_fw_options,
			"cxl hbo_transfer_fw <mem0> [<mem1>..<memN>] [<options>]");

	return rc >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_hbo_activate_fw(int argc, const char **argv, struct cxl_ctx *ctx)
{
	int rc = memdev_action(argc, argv, ctx, action_cmd_hbo_activate_fw, cmd_hbo_activate_fw_options,
			"cxl hbo_activate_fw <mem0> [<mem1>..<memN>] [<options>]");

	return rc >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_health_counters_clear(int argc, const char **argv, struct cxl_ctx *ctx)
{
	int rc = memdev_action(argc, argv, ctx, action_cmd_health_counters_clear, cmd_health_counters_clear_options,
			"cxl health_counters_clear <mem0> [<mem1>..<memN>] [<options>]");

	return rc >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_health_counters_get(int argc, const char **argv, struct cxl_ctx *ctx)
{
	int rc = memdev_action(argc, argv, ctx, action_cmd_health_counters_get, cmd_health_counters_get_options,
			"cxl health_counters_get <mem0> [<mem1>..<memN>] [<options>]");

	return rc >= 0 ? 0 : EXIT_FAILURE;
}

int cmd_hct_get_plat_param(int argc, const char **argv, struct cxl_ctx *ctx)
{
	int rc = memdev_action(argc, argv, ctx, action_cmd_hct_get_plat_param, cmd_hct_get_plat_param_options,
			"cxl hct-get-plat-params <mem0> [<mem1>..<memN>] [<options>]");

	return rc >= 0 ? 0 : EXIT_FAILURE;
}
