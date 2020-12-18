/* SPDX-License-Identifier: LGPL-2.1 */
/* Copyright (C) 2016 Hewlett Packard Enterprise Development LP */
/* Copyright (C) 2014-2020, Intel Corporation */
#ifndef __NDCTL_HPE1_H__
#define __NDCTL_HPE1_H__

enum {
	NDN_HPE1_CMD_QUERY = 0,

	/* non-root commands */
	NDN_HPE1_CMD_SMART = 1,
	NDN_HPE1_CMD_SMART_THRESHOLD = 2,
	NDN_HPE1_CMD_GET_CONFIG_SIZE = 4,
	NDN_HPE1_CMD_GET_CONFIG_DATA = 5,
	NDN_HPE1_CMD_SET_CONFIG_DATA = 6,
	NDN_HPE1_CMD_GET_IDENT = 10,
	NDN_HPE1_CMD_GET_ES_IDENT = 11,
	NDN_HPE1_CMD_GET_LAST_BACKUP = 12,
	NDN_HPE1_CMD_SET_LIFE_THRESHOLD = 13,
	NDN_HPE1_CMD_ERRINJ_QUERY = 18,
	NDN_HPE1_CMD_ERRINJ_INJECT = 19,
	NDN_HPE1_CMD_ERRINJ_STATUS = 20,
};

/* NDN_HPE1_CMD_SMART */
/* ndn_hpe1_smart.in_valid_flags / ndn_hpe1_smart_data.out_valid_flags */
#define NDN_HPE1_SMART_HEALTH_VALID	(1 << 0)
#define NDN_HPE1_SMART_TEMP_VALID	(1 << 1)
#define NDN_HPE1_SMART_SPARES_VALID	(1 << 2)
#define NDN_HPE1_SMART_ALARM_VALID	(1 << 3)
#define NDN_HPE1_SMART_USED_VALID	(1 << 4)
#define NDN_HPE1_SMART_SHUTDOWN_VALID	(1 << 5)
#define NDN_HPE1_SMART_STATS_VALID	(1 << 6)
#define NDN_HPE1_SMART_DETAIL_VALID	(1 << 7)
#define NDN_HPE1_SMART_ENERGY_VALID	(1 << 8)
#define NDN_HPE1_SMART_VENDOR_VALID	(1 << 9)
#define NDN_HPE1_SMART_NOTIFIED		(1 << 31)

/* ndn_hpe1_smart_data.stat_summary */
#define NDN_HPE1_SMART_NONCRIT_HEALTH	(1 << 0)
#define NDN_HPE1_SMART_CRITICAL_HEALTH	(1 << 1)
#define NDN_HPE1_SMART_FATAL_HEALTH	(1 << 2)

/* ndn_hpe1_smart_data.alarm_trips */
#define NDN_HPE1_SMART_TEMP_TRIP	(1 << 0)
#define NDN_HPE1_SMART_SPARE_TRIP	(1 << 1)
#define NDN_HPE1_SMART_LIFEWARN_TRIP	(1 << 2)
#define NDN_HPE1_SMART_LIFEERR_TRIP	(1 << 3)
#define NDN_HPE1_SMART_ESLIFEWARN_TRIP	(1 << 4)
#define NDN_HPE1_SMART_ESLIFEERR_TRIP	(1 << 5)
#define NDN_HPE1_SMART_ESTEMPWARN_TRIP	(1 << 6)
#define NDN_HPE1_SMART_ESTEMPERR_TRIP	(1 << 7)

/* ndn_hpe1_smart_data.last_shutdown_stat */
#define NDN_HPE1_SMART_LASTSAVEGOOD	(1 << 1)

/* ndn_hpe1_smart_data.mod_hlth_stat */
#define NDN_HPE1_SMART_ES_FAILURE	(1 << 0)
#define NDN_HPE1_SMART_CTLR_FAILURE	(1 << 1)
#define NDN_HPE1_SMART_UE_TRIP		(1 << 2)
#define NDN_HPE1_SMART_CE_TRIP		(1 << 3)
#define NDN_HPE1_SMART_SAVE_FAILED	(1 << 4)
#define NDN_HPE1_SMART_RESTORE_FAILED	(1 << 5)
#define NDN_HPE1_SMART_ARM_FAILED	(1 << 6)
#define NDN_HPE1_SMART_ERASE_FAILED	(1 << 7)
#define NDN_HPE1_SMART_CONFIG_ERROR	(1 << 8)
#define NDN_HPE1_SMART_FW_ERROR		(1 << 9)
#define NDN_HPE1_SMART_VENDOR_ERROR	(1 << 10)

struct ndn_hpe1_smart_data {
	__u32	out_valid_flags;
	__u8	stat_summary;
	__u16	curr_temp;
	__u8	spare_blocks;
	__u16	alarm_trips;
	__u8	device_life;
	__u8	last_shutdown_stat;
	__u16	last_save_op_dur;
	__u16	last_restore_op_dur;
	__u16	last_erase_op_dur;
	__u16	res1;
	__u32	save_ops;
	__u32	restore_ops;
	__u32	erase_ops;
	__u32	life_save_ops;
	__u32	life_restore_ops;
	__u32	life_erase_ops;
	__u32	life_mod_pwr_cycles;
	__u32	mod_hlth_stat;
	__u32	energy_src_check;
	__u8	energy_src_life_percent;
	__u16	energy_src_curr_temp;
	__u8	res2;
	__u16	energy_src_total_runtime;
	__u16	vndr_spec_data_size;
	__u8	vnd_spec_data[60];
} __attribute__((packed));

struct ndn_hpe1_smart {
	__u32 in_valid_flags;
	__u32 status;
	union {
		__u8 buf[124];
		struct ndn_hpe1_smart_data data[1];
	};
} __attribute__((packed));

/* NDN_HPE1_CMD_SMART_THRESHOLD */
struct ndn_hpe1_smart_threshold_data {
	__u16	threshold_alarm_ctl;
	__u16	temp_threshold;
	__u8	spare_block_threshold;
	__u8	res1[3];
	__u8	dev_lifewarn_threshold;
	__u8	dev_lifeerr_threshold;
	__u8	res2[6];
	__u8	es_lifewarn_threshold;
	__u8	es_lifeerr_threshold;
	__u8	es_tempwarn_threshold;
	__u8	es_temperr_threshold;
	__u8	res3[4];
	__u64	res4;
} __attribute__((packed));

struct ndn_hpe1_smart_threshold {
	__u32 status;
	union {
		__u8 buf[32];
		struct ndn_hpe1_smart_threshold_data data[1];
	};
} __attribute__((packed));

/* NDN_HPE1_CMD_GET_CONFIG_SIZE */
struct ndn_hpe1_get_config_size {
	__u32 status;
	__u32 config_size;
	__u32 max_xfer;
} __attribute__((packed));

/* NDN_HPE1_CMD_GET_CONFIG_DATA */
struct ndn_hpe1_get_config_data_hdr {
	__u32 in_offset;
	__u32 in_length;
	__u32 status;
	__u8 out_buf[0];
} __attribute__((packed));

/* NDN_HPE1_CMD_SET_CONFIG_DATA */
struct ndn_hpe1_set_config_hdr {
	__u32 in_offset;
	__u32 in_length;
	__u8 in_buf[0];
} __attribute__((packed));


/* ndn_hpe1_get_id.sup_backup_trigger */
#define NDN_HPE1_BKUP_SUPPORT_CKE	(1 << 0)
#define NDN_HPE1_BKUP_SUPPORT_EXTERNAL	(1 << 1)
#define NDN_HPE1_BKUP_SUPPORT_12V	(1 << 2)
#define NDN_HPE1_BKUP_SUPPORT_I2C	(1 << 3)
#define NDN_HPE1_BKUP_SUPPORT_SAVEN	(1 << 4)

/* NDN_HPE1_CMD_GET_IDENT */
struct ndn_hpe1_get_id {
	__u32	status;
	__u8	spec_rev;
	__u8	num_stnd_pages;
	__u32	hw_rev;
	__u8	sup_backup_trigger;
	__u16	max_op_retries;
	__u8	__res1[3];
	__u32	backup_op_timeout;
	__u32	restore_op_timeout;
	__u32	erase_op_timeout;
	__u32	arm_op_timeout;
	__u32	fw_op_timeout;
	__u32	region_block_size;
	__u16	min_op_temp;
	__u16	max_op_temp;
	__u8	curr_fw_slot;
	__u8	res2[1];
	__u16	num_fw_slots;
	__u8	fw_slot_revision[0];
} __attribute__((packed));

/* ndn_hpe1_get_energy_src_id.attr */
#define NDN_HPE1_ES_ATTR_BUILTIN	(1 << 0)
#define NDN_HPE1_ES_ATTR_TETHERED	(1 << 1)
#define NDN_HPE1_ES_ATTR_SHARED		(1 << 2)

/* ndn_hpe1_get_energy_src_id.tech */
#define NDN_HPE1_ES_TECH_UNDEFINED	(1 << 0)
#define NDN_HPE1_ES_TECH_SUPERCAP	(1 << 1)
#define NDN_HPE1_ES_TECH_BATTERY	(1 << 2)
#define NDN_HPE1_ES_TECH_HYBRIDCAP	(1 << 3)

/* NDN_HPE1_CMD_GET_ES_IDENT */
struct ndn_hpe1_get_energy_src_id {
	__u32	status;
	__u8	energy_src_policy;
	__u8	attr;
	__u8	tech;
	__u8	reserved;
	__u16	hw_rev;
	__u16	fw_rev;
	__u32	charge_timeout;
	__u16	min_op_temp;
	__u16	max_op_temp;
} __attribute__((packed));

/* ndn_hpe1_last_backup_info.last_backup_initiation */
#define NDN_HPE1_LASTBKUP_SAVEN		(1 << 0)
#define NDN_HPE1_LASTBKUP_EXTERNAL	(1 << 1)
#define NDN_HPE1_LASTBKUP_CKE		(1 << 2)
#define NDN_HPE1_LASTBKUP_FW		(1 << 3)
#define NDN_HPE1_LASTBKUP_RESETN	(1 << 4)

/* ndn_hpe1_last_backup_info.ctlr_backup_stat */
#define NDN_HPE1_LASTBKUP_GTG		(1 << 0)
#define NDN_HPE1_LASTBKUP_SDRAM_FAULT	(1 << 1)
#define NDN_HPE1_LASTBKUP_GEN_FAULT	(1 << 2)

/* NDN_HPE1_CMD_GET_LAST_BACKUP */
struct ndn_hpe1_get_last_backup {
	__u32	status;
	__u32	last_backup_info;
} __attribute__((packed));

struct ndn_hpe1_last_backup_info {
	__u8	backup_image;
	__u8	backup_cmplt_stat;
	__u8	last_backup_initiation;
	__u8	ctlr_backup_stat;
} __attribute__((packed));


/* NDN_HPE1_CMD_SET_LIFE_THRESHOLD */
struct ndn_hpe1_set_lifetime_threshold {
	__u8	in_nvm_lifetime_warn_threshold;
	__u32	status;
} __attribute__((packed));


/* ndn_hpe1_inj_err.in_err_typ
 * ndn_hpe1_get_inj_err_status.err_inj_type
 * log2(ndn_hpe1_query_err_inj_cap.err_inj_cap)
 */
enum {
	NDN_HPE1_EINJ_DEV_NONCRIT = 1,
	NDN_HPE1_EINJ_DEV_CRIT = 2,
	NDN_HPE1_EINJ_DEV_FATAL = 3,
	NDN_HPE1_EINJ_UE_BACKUP = 4,
	NDN_HPE1_EINJ_UE_RESTORE = 5,
	NDN_HPE1_EINJ_UE_ERASE = 6,
	NDN_HPE1_EINJ_UE_ARM = 7,
	NDN_HPE1_EINJ_BADBLOCK = 8,
	NDN_HPE1_EINJ_ES_FAULT = 9,
	NDN_HPE1_EINJ_ES_LOWCHARGE = 10,
	NDN_HPE1_EINJ_ES_TEMPWARN = 11,
	NDN_HPE1_EINJ_ES_TEMPERR = 12,
	NDN_HPE1_EINJ_ES_LIFEWARN = 13,
	NDN_HPE1_EINJ_ES_LIFEERR = 14,
	NDN_HPE1_EINJ_DEV_LIFEWARN = 15,
	NDN_HPE1_EINJ_DEV_LIFEERR = 16,
	NDN_HPE1_EINJ_FWUPDATE_ERR = 17,
	NDN_HPE1_EINJ_CTRL_ERR = 18,
};

/* ndn_hpe1_inj_err.in_options / ndn_hpe1_get_inj_err_status.err_inj_opt */
enum {
	NDN_HPE1_EINJ_OPT_SINGLE = 0,
	NDN_HPE1_EINJ_OPT_PERSISTENT = 1,
	NDN_HPE1_EINJ_OPT_CLEAR = 2,
};

/* ndn_hpe1_get_inj_err_status.err_inj_stat_info */
enum {
	NDN_HPE1_EINJ_STAT_NONE = 0,
	NDN_HPE1_EINJ_STAT_INJECTED = 1,
	NDN_HPE1_EINJ_STAT_PENDING = 2,
};

/* NDN_HPE1_CMD_ERRINJ_QUERY */
struct ndn_hpe1_query_err_inj_cap {
	__u32	status;
	__u8	err_inj_cap[32];
} __attribute__((packed));


/* NDN_HPE1_CMD_ERRINJ_INJECT */
struct ndn_hpe1_inj_err {
	__u8	in_err_typ;
	__u8	in_options;
	__u32	status;
} __attribute__((packed));

/* NDN_HPE1_CMD_ERRINJ_STATUS */
struct ndn_hpe1_get_inj_err_status {
	__u32	status;
	__u8	err_inj_stat_info;
	__u8	err_inj_type;
	__u8	err_inj_opt;
} __attribute__((packed));

union ndn_hpe1_cmd {
	__u64					query;
	struct ndn_hpe1_smart			smart;
	struct ndn_hpe1_smart_threshold		thresh;
	struct ndn_hpe1_get_config_size		get_size;
	struct ndn_hpe1_get_config_data_hdr	get_data;
	struct ndn_hpe1_get_id			get_id;
	struct ndn_hpe1_get_energy_src_id	get_energy_src_id;
	struct ndn_hpe1_get_last_backup		get_last_backup;
	struct ndn_hpe1_last_backup_info	last_backup_info;
	struct ndn_hpe1_set_lifetime_threshold	set_life_thresh;
	struct ndn_hpe1_query_err_inj_cap	err_cap;
	struct ndn_hpe1_inj_err			inj_err;
	struct ndn_hpe1_get_inj_err_status	inj_err_stat;

	unsigned char				buf[128];
};

struct ndn_pkg_hpe1 {
	struct nd_cmd_pkg gen;
	union ndn_hpe1_cmd u;
} __attribute__((packed));

#define NDN_IOCTL_HPE1_PASSTHRU		_IOWR(ND_IOCTL, ND_CMD_CALL, \
					struct ndn_pkg_hpe1)

#endif /* __NDCTL_HPE1_H__ */
