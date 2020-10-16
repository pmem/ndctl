// SPDX-License-Identifier: GPL-2.0
/* Copyright (C) 2019-2020 Intel Corporation. All rights reserved. */
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <daxctl/config.h>
#include <daxctl/libdaxctl.h>
#include <util/parse-options.h>
#include <ccan/array_size/array_size.h>

int cmd_migrate(int argc, const char **argv, struct daxctl_ctx *ctx)
{
	int i;
	static const struct option options[] = {
		OPT_END(),
	};
	const char * const u[] = {
		"daxctl migrate-device-model",
		NULL
	};

	argc = parse_options(argc, argv, options, u, 0);
	for (i = 0; i < argc; i++)
		error("unknown parameter \"%s\"\n", argv[i]);

	if (argc)
		usage_with_options(u, options);

	if (symlink(DAXCTL_MODPROBE_DATA, DAXCTL_MODPROBE_INSTALL) == 0) {
		fprintf(stderr, " success: installed %s\n",
				DAXCTL_MODPROBE_INSTALL);
		return EXIT_SUCCESS;
	}

	error("failed to install %s: %s\n", DAXCTL_MODPROBE_INSTALL,
			strerror(errno));

	return EXIT_FAILURE;
}
