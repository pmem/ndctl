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
#include <stdlib.h>
#include <linux/version.h>

#define err(msg)\
	fprintf(stderr, "%s:%d: %s (%s)\n", __func__, __LINE__, msg, strerror(errno))

int test_pcommit(struct ndctl_test *test)
{
	const char *pcommit = "pcommit";
	const char *flags = "flags";
	const int buffer_size = 1024;

	char buffer[buffer_size];
	FILE *cpuinfo;
	char *token;

	if (!ndctl_test_attempt(test, KERNEL_VERSION(4, 0, 0)))
		return 77;

	cpuinfo = fopen("/proc/cpuinfo", "r");
	if (!cpuinfo) {
		err("open");
		return -ENXIO;
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
	ndctl_test_skip(test);
	return 77;
}

int __attribute__((weak)) main(int argc, char *argv[])
{
	struct ndctl_test *test = ndctl_test_new(0);

	if (!test) {
		fprintf(stderr, "failed to initialize test\n");
		return EXIT_FAILURE;
	}

	return test_pcommit(test);
}
