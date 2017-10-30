/*
 * Copyright(c) 2015-2017 Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#ifndef _NDCTL_SIZE_H_
#define _NDCTL_SIZE_H_

#define SZ_1K     0x00000400
#define SZ_4K     0x00001000
#define SZ_1M     0x00100000
#define SZ_2M     0x00200000
#define SZ_4M     0x00400000
#define SZ_16M    0x01000000
#define SZ_64M    0x04000000
#define SZ_1G     0x40000000
#define SZ_1T 0x10000000000ULL

unsigned long long parse_size64(const char *str);
unsigned long long __parse_size64(const char *str, unsigned long long *units);

#define ALIGN(x, a) ((((unsigned long long) x) + (a - 1)) & ~(a - 1))
#define ALIGN_DOWN(x, a) (((((unsigned long long) x) + a) & ~(a - 1)) - a)
#define BITS_PER_LONG (sizeof(unsigned long) * 8)
#define HPAGE_SIZE (2 << 20)

#endif /* _NDCTL_SIZE_H_ */
