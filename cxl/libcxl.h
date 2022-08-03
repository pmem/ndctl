/* SPDX-License-Identifier: LGPL-2.1 */
/* Copyright (C) 2020-2021, Intel Corporation. All rights reserved. */
#ifndef _LIBCXL_H_
#define _LIBCXL_H_

#include <stdarg.h>
#include <unistd.h>
#include <sys/types.h>
#include <ccan/short_types/short_types.h>

#ifdef HAVE_UUID
#include <uuid/uuid.h>
#else
typedef unsigned char uuid_t[16];
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct cxl_ctx;
struct cxl_ctx *cxl_ref(struct cxl_ctx *ctx);
void cxl_unref(struct cxl_ctx *ctx);
int cxl_new(struct cxl_ctx **ctx);
void cxl_set_log_fn(struct cxl_ctx *ctx,
		void (*log_fn)(struct cxl_ctx *ctx, int priority,
			const char *file, int line, const char *fn,
			const char *format, va_list args));
int cxl_get_log_priority(struct cxl_ctx *ctx);
void cxl_set_log_priority(struct cxl_ctx *ctx, int priority);
void cxl_set_userdata(struct cxl_ctx *ctx, void *userdata);
void *cxl_get_userdata(struct cxl_ctx *ctx);
void cxl_set_private_data(struct cxl_ctx *ctx, void *data);
void *cxl_get_private_data(struct cxl_ctx *ctx);

struct cxl_memdev;
struct cxl_memdev *cxl_memdev_get_first(struct cxl_ctx *ctx);
struct cxl_memdev *cxl_memdev_get_next(struct cxl_memdev *memdev);
int cxl_memdev_get_id(struct cxl_memdev *memdev);
const char *cxl_memdev_get_devname(struct cxl_memdev *memdev);
int cxl_memdev_get_major(struct cxl_memdev *memdev);
int cxl_memdev_get_minor(struct cxl_memdev *memdev);
struct cxl_ctx *cxl_memdev_get_ctx(struct cxl_memdev *memdev);
unsigned long long cxl_memdev_get_pmem_size(struct cxl_memdev *memdev);
unsigned long long cxl_memdev_get_ram_size(struct cxl_memdev *memdev);
const char *cxl_memdev_get_firmware_verison(struct cxl_memdev *memdev);
size_t cxl_memdev_get_lsa_size(struct cxl_memdev *memdev);
int cxl_memdev_is_active(struct cxl_memdev *memdev);
int cxl_memdev_zero_lsa(struct cxl_memdev *memdev);
int cxl_memdev_get_lsa(struct cxl_memdev *memdev, void *buf, size_t length,
		size_t offset);
int cxl_memdev_set_lsa(struct cxl_memdev *memdev, void *buf, size_t length,
		size_t offset);
int cxl_memdev_cmd_identify(struct cxl_memdev *memdev);
int cxl_memdev_device_info_get(struct cxl_memdev *memdev);
int cxl_memdev_get_supported_logs(struct cxl_memdev *memdev);
int cxl_memdev_get_cel_log(struct cxl_memdev *memdev);
int cxl_memdev_get_event_interrupt_policy(struct cxl_memdev *memdev);
int cxl_memdev_set_event_interrupt_policy(struct cxl_memdev *memdev, u32 int_policy);
int cxl_memdev_get_timestamp(struct cxl_memdev *memdev);
int cxl_memdev_set_timestamp(struct cxl_memdev *memdev, u64 timestamp);
int cxl_memdev_get_alert_config(struct cxl_memdev *memdev);
int cxl_memdev_set_alert_config(struct cxl_memdev *memdev, u32 alert_prog_threshold,
    u32 device_temp_threshold, u32 mem_error_threshold);
int cxl_memdev_get_health_info(struct cxl_memdev *memdev);
int cxl_memdev_get_event_records(struct cxl_memdev *memdev, u8 event_log_type);
int cxl_memdev_get_ld_info(struct cxl_memdev *memdev);
int cxl_memdev_ddr_info(struct cxl_memdev *memdev, u8 ddr_id);
int cxl_memdev_clear_event_records(struct cxl_memdev *memdev, u8 event_log_type,
    u8 clear_event_flags, u8 no_event_record_handles, u16 *event_record_handles);
int cxl_memdev_hct_start_stop_trigger(struct cxl_memdev *memdev,
	u8 hct_inst, u8 buf_control);
int cxl_memdev_hct_get_buffer_status(struct cxl_memdev *memdev,
	u8 hct_inst);
int cxl_memdev_hct_enable(struct cxl_memdev *memdev, u8 hct_inst);
int cxl_memdev_ltmon_capture_clear(struct cxl_memdev *memdev, u8 cxl_mem_id);
int cxl_memdev_ltmon_capture(struct cxl_memdev *memdev, u8 cxl_mem_id,
	u8 capt_mode, u16 ignore_sub_chg, u8 ignore_rxl0_chg, u8 trig_src_sel);
int cxl_memdev_ltmon_capture_freeze_and_restore(struct cxl_memdev *memdev,
	u8 cxl_mem_id, u8 freeze_restore);
int cxl_memdev_ltmon_l2r_count_dump(struct cxl_memdev *memdev,
	u8 cxl_mem_id);
int cxl_memdev_ltmon_l2r_count_clear(struct cxl_memdev *memdev,
	u8 cxl_mem_id);
int cxl_memdev_ltmon_basic_cfg(struct cxl_memdev *memdev, u8 cxl_mem_id,
	u8 tick_cnt, u8 global_ts);
int cxl_memdev_ltmon_watch(struct cxl_memdev *memdev, u8 cxl_mem_id,
	u8 watch_id, u8 watch_mode, u8 src_maj_st, u8 src_min_st, u8 src_l0_st,
	u8 dst_maj_st, u8 dst_min_st, u8 dst_l0_st);
int cxl_memdev_ltmon_capture_stat(struct cxl_memdev *memdev, u8 cxl_mem_id);
int cxl_memdev_ltmon_capture_log_dmp(struct cxl_memdev *memdev,
	u8 cxl_mem_id, u16 dump_idx, u16 dump_cnt);
int cxl_memdev_ltmon_capture_trigger(struct cxl_memdev *memdev,
	u8 cxl_mem_id, u8 trig_src);
int cxl_memdev_ltmon_enable(struct cxl_memdev *memdev, u8 cxl_mem_id,
	u8 enable);
int cxl_memdev_osa_os_type_trig_cfg(struct cxl_memdev *memdev,
	u8 cxl_mem_id, u16 lane_mask, u8 lane_dir_mask, u8 rate_mask, u16 os_type_mask);
int cxl_memdev_osa_cap_ctrl(struct cxl_memdev *memdev, u8 cxl_mem_id,
	u16 lane_mask, u8 lane_dir_mask, u8 drop_single_os, u8 stop_mode,
	u8 snapshot_mode, u16 post_trig_num, u16 os_type_mask);
int cxl_memdev_osa_cfg_dump(struct cxl_memdev *memdev, u8 cxl_mem_id);
int cxl_memdev_osa_ana_op(struct cxl_memdev *memdev, u8 cxl_mem_id,
	u8 op);
int cxl_memdev_osa_status_query(struct cxl_memdev *memdev, u8 cxl_mem_id);
int cxl_memdev_osa_access_rel(struct cxl_memdev *memdev, u8 cxl_mem_id);
int cxl_memdev_perfcnt_mta_ltif_set(struct cxl_memdev *memdev,
	u32 counter, u32 match_value, u32 opcode, u32 meta_field, u32 meta_value);
int cxl_memdev_perfcnt_mta_get(struct cxl_memdev *memdev, u8 type,
	u32 counter);
int cxl_memdev_perfcnt_mta_latch_val_get(struct cxl_memdev *memdev,
	u8 type, u32 counter);
int cxl_memdev_perfcnt_mta_counter_clear(struct cxl_memdev *memdev,
	u8 type, u32 counter);
int cxl_memdev_perfcnt_mta_cnt_val_latch(struct cxl_memdev *memdev,
	u8 type, u32 counter);
int cxl_memdev_perfcnt_mta_hif_set(struct cxl_memdev *memdev, u32 counter,
	u32 match_value, u32 addr, u32 req_ty, u32 sc_ty);
int cxl_memdev_perfcnt_mta_hif_cfg_get(struct cxl_memdev *memdev,
	u32 counter);
int cxl_memdev_perfcnt_mta_hif_latch_val_get(struct cxl_memdev *memdev,
	u32 counter);
int cxl_memdev_perfcnt_mta_hif_counter_clear(struct cxl_memdev *memdev,
	u32 counter);
int cxl_memdev_perfcnt_mta_hif_cnt_val_latch(struct cxl_memdev *memdev,
	u32 counter);
int cxl_memdev_perfcnt_ddr_generic_select(struct cxl_memdev *memdev,
	u8 ddr_id, u8 cid, u8 rank, u8 bank, u8 bankgroup, u8 *event);
int cxl_memdev_err_inj_drs_poison(struct cxl_memdev *memdev, u8 ch_id,
	u8 duration, u8 inj_mode, u16 tag);
int cxl_memdev_err_inj_drs_ecc(struct cxl_memdev *memdev, u8 ch_id,
	u8 duration, u8 inj_mode, u16 tag);
int cxl_memdev_err_inj_rxflit_crc(struct cxl_memdev *memdev, u8 cxl_mem_id);
int cxl_memdev_err_inj_txflit_crc(struct cxl_memdev *memdev, u8 cxl_mem_id);
int cxl_memdev_err_inj_viral(struct cxl_memdev *memdev, u8 ld_id);
int cxl_memdev_eh_eye_cap_run(struct cxl_memdev *memdev, u8 depth,
	u32 lane_mask);
int cxl_memdev_eh_eye_cap_read(struct cxl_memdev *memdev, u8 lane_id,
	u8 bin_num);
int cxl_memdev_eh_adapt_get(struct cxl_memdev *memdev, u32 lane_id);
int cxl_memdev_eh_adapt_oneoff(struct cxl_memdev *memdev, u32 lane_id,
	u32 preload, u32 loops, u32 objects);
int cxl_memdev_eh_adapt_force(struct cxl_memdev *memdev, u32 lane_id,
	u32 rate, u32 vdd_bias, u32 ssc, u8 pga_gain, u8 pga_a0, u8 pga_off,
	u8 cdfe_a2, u8 cdfe_a3, u8 cdfe_a4, u8 cdfe_a5, u8 cdfe_a6, u8 cdfe_a7,
	u8 cdfe_a8, u8 cdfe_a9, u8 cdfe_a10, u16 dc_offset, u16 zobel_dc_offset,
	u16 udfe_thr_0, u16 udfe_thr_1, u16 median_amp, u8 zobel_a_gain,
	u8 ph_ofs_t);
int cxl_memdev_hbo_status(struct cxl_memdev *memdev);
int cxl_memdev_hbo_transfer_fw(struct cxl_memdev *memdev);
int cxl_memdev_hbo_activate_fw(struct cxl_memdev *memdev);
int cxl_memdev_health_counters_clear(struct cxl_memdev *memdev,
	u32 bitmask);
int cxl_memdev_health_counters_get(struct cxl_memdev *memdev);
int cxl_memdev_hct_get_plat_param(struct cxl_memdev *memdev);

#define cxl_memdev_foreach(ctx, memdev) \
        for (memdev = cxl_memdev_get_first(ctx); \
             memdev != NULL; \
             memdev = cxl_memdev_get_next(memdev))

struct cxl_cmd;
const char *cxl_cmd_get_devname(struct cxl_cmd *cmd);
struct cxl_cmd *cxl_cmd_new_raw(struct cxl_memdev *memdev, int opcode);
int cxl_cmd_set_input_payload(struct cxl_cmd *cmd, void *in, int size);
int cxl_cmd_set_output_payload(struct cxl_cmd *cmd, void *out, int size);
void cxl_cmd_ref(struct cxl_cmd *cmd);
void cxl_cmd_unref(struct cxl_cmd *cmd);
int cxl_cmd_submit(struct cxl_cmd *cmd);
int cxl_cmd_get_mbox_status(struct cxl_cmd *cmd);
int cxl_cmd_get_out_size(struct cxl_cmd *cmd);
struct cxl_cmd *cxl_cmd_new_identify(struct cxl_memdev *memdev);
int cxl_cmd_identify_get_fw_rev(struct cxl_cmd *cmd, char *fw_rev, int fw_len);
unsigned long long cxl_cmd_identify_get_partition_align(struct cxl_cmd *cmd);
unsigned int cxl_cmd_identify_get_lsa_size(struct cxl_cmd *cmd);
struct cxl_cmd *cxl_cmd_new_get_health_info(struct cxl_memdev *memdev);
int cxl_cmd_get_health_info_get_health_status(struct cxl_cmd *cmd);
int cxl_cmd_get_health_info_get_media_status(struct cxl_cmd *cmd);
int cxl_cmd_get_health_info_get_ext_status(struct cxl_cmd *cmd);
int cxl_cmd_get_health_info_get_life_used(struct cxl_cmd *cmd);
int cxl_cmd_get_health_info_get_temperature(struct cxl_cmd *cmd);
int cxl_cmd_get_health_info_get_dirty_shutdowns(struct cxl_cmd *cmd);
int cxl_cmd_get_health_info_get_volatile_errors(struct cxl_cmd *cmd);
int cxl_cmd_get_health_info_get_pmem_errors(struct cxl_cmd *cmd);
struct cxl_cmd *cxl_cmd_new_get_lsa(struct cxl_memdev *memdev,
		unsigned int offset, unsigned int length);
void *cxl_cmd_get_lsa_get_payload(struct cxl_cmd *cmd);
struct cxl_cmd *cxl_cmd_new_set_lsa(struct cxl_memdev *memdev,
		void *buf, unsigned int offset, unsigned int length);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
