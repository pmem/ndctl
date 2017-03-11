/*
 * Copyright(c) 2005 Junio C Hamano. All rights reserved.
 * Copyright(c) 2005 Linus Torvalds. All rights reserved.
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
