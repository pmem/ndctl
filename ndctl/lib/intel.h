/* SPDX-License-Identifier: LGPL-2.1 */
/* Copyright (C) 2017-2020, Intel Corporation. All rights reserved. */

#ifndef __INTEL_H__
#define __INTEL_H__
#define ND_INTEL_SMART 1
#define ND_INTEL_SMART_THRESHOLD 2

#define ND_INTEL_ENABLE_LSS_STATUS 10
#define ND_INTEL_FW_GET_INFO 12
#define ND_INTEL_FW_START_UPDATE 13
#define ND_INTEL_FW_SEND_DATA 14
#define ND_INTEL_FW_FINISH_UPDATE 15
#define ND_INTEL_FW_FINISH_STATUS_QUERY 16
#define ND_INTEL_SMART_SET_THRESHOLD 17
#define ND_INTEL_SMART_INJECT 18

#define ND_INTEL_SMART_HEALTH_VALID             (1 << 0)
#define ND_INTEL_SMART_SPARES_VALID             (1 << 1)
#define ND_INTEL_SMART_USED_VALID               (1 << 2)
#define ND_INTEL_SMART_MTEMP_VALID              (1 << 3)
#define ND_INTEL_SMART_CTEMP_VALID              (1 << 4)
#define ND_INTEL_SMART_SHUTDOWN_COUNT_VALID     (1 << 5)
#define ND_INTEL_SMART_AIT_STATUS_VALID         (1 << 6)
#define ND_INTEL_SMART_PTEMP_VALID              (1 << 7)
#define ND_INTEL_SMART_ALARM_VALID              (1 << 9)
#define ND_INTEL_SMART_SHUTDOWN_VALID           (1 << 10)
#define ND_INTEL_SMART_VENDOR_VALID             (1 << 11)
#define ND_INTEL_SMART_SPARE_TRIP               (1 << 0)
#define ND_INTEL_SMART_TEMP_TRIP                (1 << 1)
#define ND_INTEL_SMART_CTEMP_TRIP               (1 << 2)
#define ND_INTEL_SMART_NON_CRITICAL_HEALTH      (1 << 0)
#define ND_INTEL_SMART_CRITICAL_HEALTH          (1 << 1)
#define ND_INTEL_SMART_FATAL_HEALTH             (1 << 2)
#define ND_INTEL_SMART_INJECT_MTEMP		(1 << 0)
#define ND_INTEL_SMART_INJECT_SPARE		(1 << 1)
#define ND_INTEL_SMART_INJECT_FATAL		(1 << 2)
#define ND_INTEL_SMART_INJECT_SHUTDOWN		(1 << 3)

struct nd_intel_smart {
        __u32 status;
	union {
		struct {
			__u32 flags;
			__u8 reserved0[4];
			__u8 health;
			__u8 spares;
			__u8 life_used;
			__u8 alarm_flags;
			__u16 media_temperature;
			__u16 ctrl_temperature;
			__u32 shutdown_count;
			__u8 ait_status;
			__u16 pmic_temperature;
			__u8 reserved1[8];
			__u8 shutdown_state;
			__u32 vendor_size;
			__u8 vendor_data[92];
		} __attribute__((packed));
		__u8 data[128];
	};
} __attribute__((packed));

struct nd_intel_smart_threshold {
        __u32 status;
	union {
		struct {
			__u16 alarm_control;
			__u8 spares;
			__u16 media_temperature;
			__u16 ctrl_temperature;
			__u8 reserved[1];
		} __attribute__((packed));
		__u8 data[8];
	};
} __attribute__((packed));

struct nd_intel_smart_set_threshold {
	__u16 alarm_control;
	__u8 spares;
	__u16 media_temperature;
	__u16 ctrl_temperature;
	__u32 status;
} __attribute__((packed));

struct nd_intel_smart_inject {
	__u64 flags;
	__u8 mtemp_enable;
	__u16 media_temperature;
	__u8 spare_enable;
	__u8 spares;
	__u8 fatal_enable;
	__u8 unsafe_shutdown_enable;
	__u32 status;
} __attribute__((packed));

struct nd_intel_fw_info {
	__u32 status;
	__u32 storage_size;
	__u32 max_send_len;
	__u32 query_interval;
	__u32 max_query_time;
	__u8 update_cap;
	__u8 reserved[3];
	__u32 fis_version;
	__u64 run_version;
	__u64 updated_version;
} __attribute__((packed));

struct nd_intel_fw_start {
	__u32 status;
	__u32 context;
} __attribute__((packed));

/* this one has the output first because the variable input data size */
struct nd_intel_fw_send_data {
	__u32 context;
	__u32 offset;
	__u32 length;
	__u8 data[0];
/* reserving last 4 bytes as status */
/*	__u32 status; */
} __attribute__((packed));

struct nd_intel_fw_finish_update {
	__u8 ctrl_flags;
	__u8 reserved[3];
	__u32 context;
	__u32 status;
} __attribute__((packed));

struct nd_intel_fw_finish_query {
	__u32 context;
	__u32 status;
	__u64 updated_fw_rev;
} __attribute__((packed));

struct nd_intel_lss {
	__u8 enable;
	__u32 status;
} __attribute__((packed));

struct nd_pkg_intel {
	struct nd_cmd_pkg gen;
	union {
		struct nd_intel_smart smart;
		struct nd_intel_smart_inject inject;
		struct nd_intel_smart_threshold	thresh;
		struct nd_intel_smart_set_threshold set_thresh;
		struct nd_intel_fw_info info;
		struct nd_intel_fw_start start;
		struct nd_intel_fw_send_data send;
		struct nd_intel_fw_finish_update finish;
		struct nd_intel_fw_finish_query fquery;
		struct nd_intel_lss lss;
	};
};

#define ND_INTEL_STATUS_MASK		0xffff
#define ND_INTEL_STATUS_SUCCESS		0
#define ND_INTEL_STATUS_NOTSUPP		1
#define ND_INTEL_STATUS_NOTEXIST	2
#define ND_INTEL_STATUS_INVALPARM	3
#define ND_INTEL_STATUS_HWERR		4
#define ND_INTEL_STATUS_RETRY		5
#define ND_INTEL_STATUS_UNKNOWN		6
#define ND_INTEL_STATUS_EXTEND		7
#define ND_INTEL_STATUS_NORES		8
#define ND_INTEL_STATUS_NOTREADY	9

#define ND_INTEL_STATUS_EXTEND_MASK	0xffff0000
#define ND_INTEL_STATUS_START_BUSY	0x10000
#define ND_INTEL_STATUS_SEND_CTXINVAL	0x10000
#define ND_INTEL_STATUS_FIN_CTXINVAL	0x10000
#define ND_INTEL_STATUS_FIN_DONE	0x20000
#define ND_INTEL_STATUS_FIN_BAD		0x30000
#define ND_INTEL_STATUS_FIN_ABORTED	0x40000
#define ND_INTEL_STATUS_FQ_CTXINVAL	0x10000
#define ND_INTEL_STATUS_FQ_BUSY		0x20000
#define ND_INTEL_STATUS_FQ_BAD		0x30000
#define ND_INTEL_STATUS_FQ_ORDER	0x40000
#define ND_INTEL_STATUS_INJ_DISABLED	0x10000

#endif /* __INTEL_H__ */
