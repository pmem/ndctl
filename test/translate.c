// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2024 Intel Corporation. All rights reserved.
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Short type definitions */
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint64_t u64;

/* Mimic kernel macros */
#define GENMASK(h, l) (((~0U) << (l)) & (~0U >> (32 - 1 - (h))))

#define BITS_PER_LONG_LONG 64
#define GENMASK_ULL(h, l) \
	(((~0ULL) << (l)) & (~0ULL >> (BITS_PER_LONG_LONG - 1 - (h))))

#define XOR_MATH 1

static int hweight64(u64 value)
{
	int count = 0;

	while (value) {
		count += value & 1;
		value >>= 1;
	}
	return count;
}

static u64 __apply_xor_maps(u64 hpa, u64 map)
{
	u64 val;
	int pos;

	if (!map)
		return hpa;

	/* XOR of all set bits */
	val = (hweight64(hpa & map)) & 1;

	/* Find the lowest set bit in the map */
	pos = ffs(map) - 1;

	/* Set bit at hpa[pos] to val */
	hpa = (hpa & ~(1ULL << pos)) | (val << pos);

	return hpa;
}

static u64 apply_xor_maps(u64 hpa_offset, u8 eiw)
{
	u64 temp_a, temp_b, temp_c;

	switch (eiw) {
	case 0: /* 1-way */
	case 8: /* 3-way */
		return hpa_offset;

	/*
	 * These map values were selected to match the samples in the
	 * CXL Drivers Writers Guide for Host Bridge Interleaves at
	 * HBIG 0: 0x2020900, 0x4041200, 0x1010400, 0x800
	 *
	 * TODO Add the xormaps as test parameters.
	 */
	case 1: /* 2-way */
		return __apply_xor_maps(hpa_offset, 0x2020900);

	case 2: /* 4-way */
		temp_a = __apply_xor_maps(hpa_offset, 0x2020900);
		return __apply_xor_maps(temp_a, 0x4041200);

	case 3: /* 8-way */
		temp_a = __apply_xor_maps(hpa_offset, 0x2020900);
		temp_b = __apply_xor_maps(temp_a, 0x4041200);
		return __apply_xor_maps(temp_b, 0x1010400);

	case 4: /* 16-way */
		temp_a = __apply_xor_maps(hpa_offset, 0x2020900);
		temp_b = __apply_xor_maps(temp_a, 0x4041200);
		temp_c = __apply_xor_maps(temp_b, 0x1010400);
		return __apply_xor_maps(temp_c, 0x800);

	case 9: /* 6-way */
		return __apply_xor_maps(hpa_offset, 0x2020900);

	case 10: /* 12-way */
		temp_a = __apply_xor_maps(hpa_offset, 0x2020900);
		return __apply_xor_maps(temp_a, 0x4041200);

	default:
		return ULLONG_MAX;
	}

	return ULLONG_MAX;
}

static u64 to_hpa(u64 dpa_offset, int pos, u8 eiw, u16 eig, u8 hb_eiw, u8 math)
{
	u64 mask_upper, mask_lower;
	u64 bits_upper, bits_lower;
	u64 hpa_offset;

	/*
	 * Translate DPA->HPA by reversing the HPA->DPA decoder logic
	 * defined in CXL Spec 3.0 Section 8.2.4.19.13  Implementation
	 * Note: Device Decode Logic
	 *
	 * Insert the 'pos' to construct the HPA.
	 */
	mask_upper = GENMASK_ULL(51, eig + 8);

	if (eiw < 8) {
		hpa_offset = (dpa_offset & mask_upper) << eiw;
		hpa_offset |= pos << (eig + 8);
	} else {
		bits_upper = (dpa_offset & mask_upper) >> (eig + 8);
		bits_upper = bits_upper * 3;
		hpa_offset = ((bits_upper << (eiw - 8)) + pos) << (eig + 8);
	}

	/* Lower bits don't change */
	mask_lower = (1 << (eig + 8)) - 1;
	bits_lower = dpa_offset & mask_lower;
	hpa_offset += bits_lower;

	u64 pre_xor_hpa_offset = hpa_offset;
	if (math == XOR_MATH)
		hpa_offset = apply_xor_maps(hpa_offset, hb_eiw);

	if (hpa_offset != pre_xor_hpa_offset) {
		printf("to_hpa: XOR Changed %lu to %lu \n", pre_xor_hpa_offset,
		       hpa_offset);
	}
	return hpa_offset;
}

static int eiw_to_mod3_ways(u8 eiw)
{
	if (eiw == 8)
		return 3;
	if (eiw == 9)
		return 6;

	return 12;
}

static void to_dpa_and_pos(u64 hpa_offset, u8 eiw, u16 eig, u8 hb_eiw, u8 math,
			   u64 *out_dpa, int *out_pos)
{
	u64 dpa_offset, bits_upper, bits_lower;
	u64 orig_hpa_offset = hpa_offset;
	u64 shifted, rem;
	int pos;

	if (math == XOR_MATH)
		hpa_offset = apply_xor_maps(hpa_offset, hb_eiw);

	if (hpa_offset != orig_hpa_offset) {
		printf("to_dpa_and_pos: XOR Changed %lu to %lu\n",
		       orig_hpa_offset, hpa_offset);
	}
	/*
	 * Extract interleave position based on CXL Spec 3.2 Section
	 * 8.2.4.20.13 Implementation Note: Device Decode Logic
	 */
	if (eiw < 8) {
		pos = (hpa_offset >> (eig + 8)) & GENMASK(eiw - 1, 0);
	} else {
		int ways = eiw_to_mod3_ways(eiw);
		pos = (hpa_offset >> (eig + 8)) % ways;
	}

	/*
	 * Extract DPA offset:
	 * Lower bits [IG+7:0] pass through unchanged
	 * Upper bits are processed according to eiw
	 */
	bits_lower = hpa_offset & GENMASK_ULL(eig + 7, 0);

	if (eiw < 8) {
		/* Clear the position bits to isolate upper section, then
		 * reverse the left shift by eiw done at DPA->HPA */
		hpa_offset &= ~GENMASK_ULL(eiw + eig + 8 - 1, 0);
		dpa_offset = hpa_offset >> eiw;
	} else {
		/* Extract upper bits from the correct bit range and divide
		 * by 3 to recover the original DPA upper bits */
		bits_upper = (hpa_offset >> (eig + eiw)) / 3;
		dpa_offset = bits_upper << (eig + 8);
	}

	dpa_offset |= bits_lower;
	*out_dpa = dpa_offset;
	*out_pos = pos;
}

int main(int argc, char *argv[])
{
	u8 region_eiw, hostbridge_eiw;
	u64 dpa, expect_hpa, hpa, reverse_dpa = 0;
	u16 region_eig;
	int math, pos, reverse_pos = 0;


	setvbuf(stdout, NULL, _IONBF, 0);
	setvbuf(stderr, NULL, _IONBF, 0);

	if (argc != 8) {
		printf("Usage: %s <dpa> <pos> <region_eiw> <region_eig> <host_eiw> <math> <hpa>\n",
		       argv[0]);
		return EXIT_FAILURE;
	}

	dpa = strtoull(argv[1], NULL, 0);
	pos = atoi(argv[2]);
	region_eiw = strtoul(argv[3], NULL, 0);
	region_eig = strtoul(argv[4], NULL, 0);
	hostbridge_eiw = strtoul(argv[5], NULL, 0);
	math = atoi(argv[6]);
	expect_hpa = strtoull(argv[7], NULL, 0);

	/* Test DPA->HPA->SPA translation */
	hpa = to_hpa(dpa, pos, region_eiw, region_eig, hostbridge_eiw, math);

	if (hpa != expect_hpa) {
		printf("Fail: expected_hpa %lu translated_hpa:%lu\n",
		       expect_hpa, hpa);
		return EXIT_FAILURE;
	}
	/* Test reverse SPA->HPA->DPA */
	to_dpa_and_pos(hpa, region_eiw, region_eig, hostbridge_eiw, math,
		       &reverse_dpa, &reverse_pos);

	if (reverse_pos != pos) {
		printf("PosFail: expected pos:%d, got pos:%d\n", pos,
		       reverse_pos);
		return EXIT_FAILURE;
	}
	if (reverse_dpa != dpa) {
		printf("DPAFail: expected dpa:%lu got dpa:%lu\n", dpa,
		       reverse_dpa);
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
