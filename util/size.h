/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2015-2020 Intel Corporation. All rights reserved. */

#ifndef _NDCTL_SIZE_H_
#define _NDCTL_SIZE_H_
#include <stdbool.h>
#include <stdint.h>
#include <util/util.h>

#define SZ_1K     0x00000400
#define SZ_4K     0x00001000
#define SZ_8K     0x00002000
#define SZ_1M     0x00100000
#define SZ_2M     0x00200000
#define SZ_4M     0x00400000
#define SZ_16M    0x01000000
#define SZ_64M    0x04000000
#define SZ_256M	  0x10000000
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

/*
 * Helpers for struct_size() copied from include/linux/overflow.h (GPL-2.0)
 *
 * For simplicity and code hygiene, the fallback code below insists on
 * a, b and *d having the same type (similar to the min() and max()
 * macros), whereas gcc's type-generic overflow checkers accept
 * different types. Hence we don't just make check_add_overflow an
 * alias for __builtin_add_overflow, but add type checks similar to
 * below.
 */
#define check_add_overflow(a, b, d) (({	\
	typeof(a) __a = (a);			\
	typeof(b) __b = (b);			\
	typeof(d) __d = (d);			\
	(void) (&__a == &__b);			\
	(void) (&__a == __d);			\
	__builtin_add_overflow(__a, __b, __d);	\
}))

#define check_mul_overflow(a, b, d) (({	\
	typeof(a) __a = (a);			\
	typeof(b) __b = (b);			\
	typeof(d) __d = (d);			\
	(void) (&__a == &__b);			\
	(void) (&__a == __d);			\
	__builtin_mul_overflow(__a, __b, __d);	\
}))

/*
 * Compute a*b+c, returning SIZE_MAX on overflow. Internal helper for
 * struct_size() below.
 */
static inline size_t __ab_c_size(size_t a, size_t b, size_t c)
{
	size_t bytes;

	if (check_mul_overflow(a, b, &bytes))
		return SIZE_MAX;
	if (check_add_overflow(bytes, c, &bytes))
		return SIZE_MAX;

	return bytes;
}

/**
 * struct_size() - Calculate size of structure with trailing array.
 * @p: Pointer to the structure.
 * @member: Name of the array member.
 * @count: Number of elements in the array.
 *
 * Calculates size of memory needed for structure @p followed by an
 * array of @count number of @member elements.
 *
 * Return: number of bytes needed or SIZE_MAX on overflow.
 */
#define struct_size(p, member, count)					\
	__ab_c_size(count,						\
		    sizeof(*(p)->member) + __must_be_array((p)->member),\
		    sizeof(*(p)))

#endif /* _NDCTL_SIZE_H_ */
