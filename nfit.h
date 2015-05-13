/*
 * NVDIMM Firmware Interface Table - NFIT
 *
 * Copyright(c) 2013-2014 Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */
#ifndef __NFIT_H__
#define __NFIT_H__
#include <stdint.h>
#include <linux/uuid.h>

static inline void nfit_spa_uuid_pm(void *uuid)
{
	uuid_le uuid_pm = UUID_LE(0x66f0d379, 0xb4f3, 0x4074, 0xac, 0x43, 0x0d,
			0x33, 0x18, 0xb7, 0x8c, 0xdb);
	memcpy(uuid, &uuid_pm, 16);
}

enum {
	NFIT_TABLE_SPA = 0,
};

/**
 * struct nfit - Nvdimm Firmware Interface Table
 * @signature: "NFIT"
 * @length: sum of size of this table plus all appended subtables
 */
struct nfit {
	uint8_t signature[4];
	uint32_t length;
	uint8_t revision;
	uint8_t checksum;
	uint8_t oemid[6];
	uint64_t oem_tbl_id;
	uint32_t oem_revision;
	uint32_t creator_id;
	uint32_t creator_revision;
	uint32_t reserved;
} __attribute__((packed));

/**
 * struct nfit_spa - System Physical Address Range Descriptor Table
 */
struct nfit_spa {
	uint16_t type;
	uint16_t length;
	uint16_t range_index;
	uint16_t flags;
	uint32_t reserved;
	uint32_t proximity_domain;
	uint8_t type_uuid[16];
	uint64_t spa_base;
	uint64_t spa_length;
	uint64_t mem_attr;
} __attribute__((packed));

#endif /* __NFIT_H__ */
