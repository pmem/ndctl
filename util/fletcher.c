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
#include <stdlib.h>
#include <stdbool.h>
#include <util/fletcher.h>
#include <ccan/endian/endian.h>
#include <ccan/short_types/short_types.h>

/*
 * Note, fletcher64() is copied from drivers/nvdimm/label.c in the Linux kernel
 */
u64 fletcher64(void *addr, size_t len, bool le)
{
	u32 *buf = addr;
	u32 lo32 = 0;
	u64 hi32 = 0;
	size_t i;

	for (i = 0; i < len / sizeof(u32); i++) {
		lo32 += le ? le32_to_cpu((le32) buf[i]) : buf[i];
		hi32 += lo32;
	}

	return hi32 << 32 | lo32;
}
