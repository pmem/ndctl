/*
 * test-pcommit: Make sure PCOMMIT is supported by the platform.
 *
 * Copyright (c) 2015, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU Lesser General Public License,
 * version 2.1, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
 */
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <test.h>

#define err(msg)\
	fprintf(stderr, "%s:%d: %s (%s)\n", __func__, __LINE__, msg, strerror(errno))

int test_pcommit(void)
{
	const char *pcommit = "pcommit";
	const char *flags = "flags";
	const int buffer_size = 1024;

	char buffer[buffer_size];
	FILE *cpuinfo;
	char *token;

	cpuinfo = fopen("/proc/cpuinfo", "r");
	if (!cpuinfo) {
		err("open");
		return EBADF;
	}

        while (fgets(buffer, buffer_size, cpuinfo)) {
		token = strtok(buffer, " :");

		if (token &&
		    strncmp(token, flags, strlen(flags)) != 0)
			continue;

		while (token != NULL) {
			token = strtok(NULL, " ");
			if (token &&
			    strncmp(token, pcommit, strlen(pcommit)) == 0) {
				fclose(cpuinfo);
				return 0;
			}
		}
        }

	fclose(cpuinfo);
	return 77;
}

int __attribute__((weak)) main(int argc, char *argv[])
{
	return test_pcommit();
}
