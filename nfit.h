/*
 * NVDIMM Firmware Interface Table (v0.8s8)
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

enum {
	NFIT_TABLE_SPA = 0,
	NFIT_SPA_PM = 1,
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
 * @spa_type: NFIT_SPA_*
 */
struct nfit_spa {
	uint16_t type;
	uint16_t length;
	uint16_t spa_type;
	uint16_t spa_index;
	uint16_t flags;
	uint16_t reserved;
	uint32_t proximity_domain;
	uint64_t spa_base;
	uint64_t spa_length;
	uint64_t mem_attr;
} __attribute__((packed));

#endif /* __NFIT_H__ */
