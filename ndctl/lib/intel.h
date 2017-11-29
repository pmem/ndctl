/* SPDX-License-Identifier: LGPL-2.1 */
/* Copyright (c) 2017, Intel Corporation. All rights reserved. */

#ifndef __INTEL_H__
#define __INTEL_H__
#define ND_INTEL_SMART 1
#define ND_INTEL_SMART_THRESHOLD 2
#define ND_INTEL_SMART_SET_THRESHOLD 17

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

struct nd_pkg_intel {
	struct nd_cmd_pkg gen;
	union {
		struct nd_intel_smart smart;
		struct nd_intel_smart_threshold	thresh;
		struct nd_intel_smart_set_threshold set_thresh;
	};
};
#endif /* __INTEL_H__ */
