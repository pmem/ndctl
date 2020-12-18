/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2015-2020 Intel Corporation. All rights reserved. */

#ifndef _NDCTL_SIZE_H_
#define _NDCTL_SIZE_H_
#include <stdbool.h>

#define SZ_1K     0x00000400
#define SZ_4K     0x00001000
#define SZ_8K     0x00002000
#define SZ_1M     0x00100000
#define SZ_2M     0x00200000
#define SZ_4M     0x00400000
#define SZ_16M    0x01000000
#define SZ_64M    0x04000000
#define SZ_1G     0x40000000
#define SZ_1T 0x10000000000ULL

unsigned long long parse_size64(const char *str);
unsigned long long __parse_size64(const char *str, unsigned long long *units);

static inline bool is_power_of_2(unsigned long long v)
{
	return v && ((v & (v - 1)) == 0);
}

#define ALIGN(x, a) ((((unsigned long long) x) + (a - 1)) & ~(a - 1))
#define ALIGN_DOWN(x, a) (((((unsigned long long) x) + a) & ~(a - 1)) - a)
#define IS_ALIGNED(x, a) (((x) & ((typeof(x))(a) - 1)) == 0)
#define BITS_PER_LONG (sizeof(unsigned long) * 8)
#define HPAGE_SIZE (2 << 20)

#endif /* _NDCTL_SIZE_H_ */
