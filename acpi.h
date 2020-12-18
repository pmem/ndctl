/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2013-2020 Intel Corporation. All rights reserved. */
#ifndef __ACPI_H__
#define __ACPI_H__
#include <stdint.h>
#include <linux/uuid.h>

static const uuid_le uuid_pmem = UUID_LE(0x66f0d379, 0xb4f3, 0x4074, 0xac, 0x43, 0x0d,
			0x33, 0x18, 0xb7, 0x8c, 0xdb);

static inline void nfit_spa_uuid_pm(void *uuid)
{
	memcpy(uuid, &uuid_pmem, 16);
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

enum acpi_nfit_type {
	ACPI_NFIT_TYPE_SYSTEM_ADDRESS = 0,
	ACPI_NFIT_TYPE_MEMORY_MAP = 1,
	ACPI_NFIT_TYPE_INTERLEAVE = 2,
	ACPI_NFIT_TYPE_SMBIOS = 3,
	ACPI_NFIT_TYPE_CONTROL_REGION = 4,
	ACPI_NFIT_TYPE_DATA_REGION = 5,
	ACPI_NFIT_TYPE_FLUSH_ADDRESS = 6,
	ACPI_NFIT_TYPE_CAPABILITIES = 7,
	ACPI_NFIT_TYPE_RESERVED = 8     /* 8 and greater are reserved */
};

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

struct nfit_map {
	uint16_t type;
	uint16_t length;
	uint32_t device_handle;
	uint16_t physical_id;
	uint16_t region_id;
	uint16_t range_index;
	uint16_t region_index;
	uint64_t region_size;
	uint64_t region_offset;
	uint64_t address;
	uint16_t interleave_index;
	uint16_t interleave_ways;
	uint16_t flags;
	uint16_t reserved;           /* Reserved, must be zero */
} __attribute__((packed));

struct srat {
	struct acpi_header h;
	uint32_t revision;
	uint64_t reserved;
} __attribute__((packed));

enum acpi_srat_type {
	ACPI_SRAT_TYPE_CPU_AFFINITY = 0,
	ACPI_SRAT_TYPE_MEMORY_AFFINITY = 1,
	ACPI_SRAT_TYPE_X2APIC_CPU_AFFINITY = 2,
	ACPI_SRAT_TYPE_GICC_AFFINITY = 3,
	ACPI_SRAT_TYPE_GIC_ITS_AFFINITY = 4,    /* ACPI 6.2 */
	ACPI_SRAT_TYPE_GENERIC_AFFINITY = 5,    /* ACPI 6.3 */
	ACPI_SRAT_TYPE_RESERVED = 6     /* 5 and greater are reserved */
};

struct srat_cpu {
	uint8_t type;
	uint8_t length;
	uint8_t proximity_domain_lo;
	uint8_t apic_id;
	uint32_t flags;
	uint8_t local_sapic_eid;
	uint8_t proximity_domain_hi[3];
	uint32_t clock_domain;
} __attribute__((packed));

struct srat_generic {
	uint8_t type;
	uint8_t length;
	uint8_t reserved;
	uint8_t device_handle_type;
	uint32_t proximity_domain;
	uint8_t device_handle[16];
	uint32_t flags;
	uint32_t reserved1;
} __attribute__((packed));

struct srat_mem {
	uint8_t type;
	uint8_t length;
	uint32_t proximity_domain;
	uint16_t reserved;
	uint64_t spa_base;
	uint64_t spa_length;
	uint32_t reserved1;
	uint32_t flags;
	uint64_t reserved2;
} __attribute__((packed));

struct acpi_subtable8 {
	uint8_t type;
	uint8_t length;
	uint8_t buf[];
} __attribute__((packed));

struct acpi_subtable16 {
	uint16_t type;
	uint16_t length;
	uint8_t buf[];
} __attribute__((packed));

struct slit {
	struct acpi_header h;
	uint64_t count;
	uint8_t entry[]; /* size = count^2 */
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

static inline uint64_t readq(void *a)
{
	uint64_t *p = a;

	return le64toh(*p);
}

static inline uint32_t readl(void *a)
{
	uint32_t *p = a;

	return le32toh(*p);
}

static inline uint16_t readw(void *a)
{
	uint16_t *p = a;

	return le16toh(*p);
}

static inline uint8_t readb(void *a)
{
	uint8_t *p = a;

	return *p;
}
#endif /* __ACPI_H__ */
