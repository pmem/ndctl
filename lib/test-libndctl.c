/*
 * libndctl: helper library for the nd (nvdimm, nfit-defined, persistent
 *           memory, ...) sub-system.
 *
 * Copyright (c) 2014, Intel Corporation.
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
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>

#include <ndctl/libndctl.h>

int main(int argc, char *argv[])
{
	struct ndctl_ctx *ctx;
	int err;

	err = ndctl_new(&ctx);
	if (err < 0)
		exit(EXIT_FAILURE);

	printf("version %s\n", VERSION);

	ndctl_unref(ctx);
	return EXIT_SUCCESS;
}
