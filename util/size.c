// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2015-2020 Intel Corporation. All rights reserved.
#include <stdlib.h>
#include <limits.h>
#include <util/size.h>

unsigned long long __parse_size64(const char *str, unsigned long long *units)
{
	unsigned long long val, check;
	char *end;

	val = strtoull(str, &end, 0);
	if (val == ULLONG_MAX)
		return val;
	check = val;
	switch (*end) {
		case 'k':
		case 'K':
			if (units)
				*units = SZ_1K;
			val *= SZ_1K;
			end++;
			break;
		case 'm':
		case 'M':
			if (units)
				*units = SZ_1M;
			val *= SZ_1M;
			end++;
			break;
		case 'g':
		case 'G':
			if (units)
				*units = SZ_1G;
			val *= SZ_1G;
			end++;
			break;
		case 't':
		case 'T':
			if (units)
				*units = SZ_1T;
			val *= SZ_1T;
			end++;
			break;
		default:
			if (units)
				*units = 1;
			break;
	}

	if (val < check || *end != '\0')
		val = ULLONG_MAX;
	return val;
}

unsigned long long parse_size64(const char *str)
{
	if (!str)
		return 0;
	return __parse_size64(str, NULL);
}
