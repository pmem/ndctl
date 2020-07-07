/*
 * ACPI Table Definitions
 *
 * Copyright(c) 2013-2017 Intel Corporation. All rights reserved.
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
#ifndef __ACPI_H__
#define __ACPI_H__
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
	SRAT_TABLE_MEM = 1,
	SRAT_MEM_ENABLED = (1<<0),
	SRAT_MEM_HOT_PLUGGABLE = (1<<1),
	SRAT_MEM_NON_VOLATILE = (1<<2),
};

/**
 * struct nfit - Nvdimm Firmware Interface Table
 * @signature: "ACPI"
 * @length: sum of size of this table plus all appended subtables
 */
struct acpi_header {
	uint8_t signature[4];
	uint32_t length;
	uint8_t revision;
	uint8_t checksum;
	uint8_t oemid[6];
	uint64_t oem_tbl_id;
	uint32_t oem_revision;
	uint32_t asl_id;
	uint32_t asl_revision;
} __attribute__((packed));

struct nfit {
	struct acpi_header h;
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

static inline unsigned char acpi_checksum(void *buf, size_t size)
{
        unsigned char sum, *data = buf;
        size_t i;

        for (sum = 0, i = 0; i < size; i++)
                sum += data[i];
        return 0 - sum;
}

static inline void writeq(uint64_t v, void *a)
{
	uint64_t *p = a;

	*p = htole64(v);
}

static inline void writel(uint32_t v, void *a)
{
	uint32_t *p = a;

	*p = htole32(v);
}

static inline void writew(unsigned short v, void *a)
{
	unsigned short *p = a;

	*p = htole16(v);
}

static inline void writeb(unsigned char v, void *a)
{
	unsigned char *p = a;

	*p = v;
}
#endif /* __ACPI_H__ */
