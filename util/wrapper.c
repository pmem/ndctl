// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2005 Junio C Hamano. All rights reserved.
// Copyright (C) 2005 Linus Torvalds. All rights reserved.

/* originally copied from perf and git */

/*
 * Various trivial helper wrappers around standard functions
 */
#include <string.h>
#include <stdlib.h>

#include <util/util.h>

char *xstrdup(const char *str)
{
	char *ret = strdup(str);
	if (!ret) {
		ret = strdup(str);
		if (!ret)
			die("Out of memory, strdup failed");
	}
	return ret;
}

void *xrealloc(void *ptr, size_t size)
{
	void *ret = realloc(ptr, size);
	if (!ret && !size)
		ret = realloc(ptr, 1);
	if (!ret) {
		ret = realloc(ptr, size);
		if (!ret && !size)
			ret = realloc(ptr, 1);
		if (!ret)
			die("Out of memory, realloc failed");
	}
	return ret;
}
