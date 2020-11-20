// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2014-2020 Intel Corporation. All rights reserved.
#include <stdio.h>
#include <syslog.h>
#include <test.h>
#include <limits.h>
#include <util/parse-options.h>

int cmd_bat(int argc, const char **argv, struct ndctl_ctx *ctx)
{
	int loglevel = LOG_DEBUG, i, rc;
	struct ndctl_test *test;
	bool force = false;
	const char * const u[] = {
		"ndctl bat [<options>]",
		NULL
	};
	const struct option options[] = {
	OPT_INTEGER('l', "loglevel", &loglevel,
		"set the log level (default LOG_DEBUG)"),
	OPT_BOOLEAN('f', "force", &force,
		"force run all tests regardless of required kernel"),
	OPT_END(),
	};

        argc = parse_options(argc, argv, options, u, 0);

	for (i = 0; i < argc; i++)
		error("unknown parameter \"%s\"\n", argv[i]);

	if (argc)
		usage_with_options(u, options);

	if (force)
		test = ndctl_test_new(UINT_MAX);
	else
		test = ndctl_test_new(0);

	if (!test) {
		fprintf(stderr, "failed to initialize test\n");
		return EXIT_FAILURE;
	}

	rc = test_blk_namespaces(loglevel, test, ctx);
	fprintf(stderr, "test_blk_namespaces: %s\n", rc ? "FAIL" : "PASS");
	if (rc && rc != 77)
		return rc;

	rc = test_pmem_namespaces(loglevel, test, ctx);
	fprintf(stderr, "test_pmem_namespaces: %s\n", rc ? "FAIL" : "PASS");
	return ndctl_test_result(test, rc);
}
