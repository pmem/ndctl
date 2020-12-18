// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2015-2020 Intel Corporation. All rights reserved.
#include <stdio.h>
#include <limits.h>
#include <syslog.h>
#include <test.h>
#include <util/parse-options.h>

static char *result(int rc)
{
	if (rc == 77)
		return "SKIP";
	else if (rc)
		return "FAIL";
	else
		return "PASS";
}

int cmd_test(int argc, const char **argv, struct ndctl_ctx *ctx)
{
	struct ndctl_test *test;
	int loglevel = LOG_DEBUG, i, rc;
	const char * const u[] = {
		"ndctl test [<options>]",
		NULL
	};
	bool force = false;
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
	if (!test)
		return EXIT_FAILURE;

	rc = test_libndctl(loglevel, test, ctx);
	fprintf(stderr, "test-libndctl: %s\n", result(rc));
	if (rc && rc != 77)
		return rc;

	rc = test_dsm_fail(loglevel, test, ctx);
	fprintf(stderr, "test-dsm-fail: %s\n", result(rc));
	if (rc && rc != 77)
		return rc;

	rc = test_dpa_alloc(loglevel, test, ctx);
	fprintf(stderr, "test-dpa-alloc: %s\n", result(rc));
	if (rc && rc != 77)
		return rc;

	rc = test_parent_uuid(loglevel, test, ctx);
	fprintf(stderr, "test-parent-uuid: %s\n", result(rc));

	rc = test_multi_pmem(loglevel, test, ctx);
	fprintf(stderr, "test-multi-pmem: %s\n", result(rc));

	return ndctl_test_result(test, rc);
}
