#include <stdio.h>
#include <syslog.h>
#include <test-blk-namespaces.h>
#include <test-pmem-namespaces.h>
#include <util/parse-options.h>

int cmd_bat(int argc, const char **argv)
{
	int loglevel = LOG_DEBUG, i, rc;
	const char * const u[] = {
		"ndctl bat [<options>]",
		NULL
	};
	const struct option options[] = {
	OPT_INTEGER('l', "loglevel", &loglevel,
		"set the log level (default LOG_DEBUG)"),
	OPT_END(),
	};

        argc = parse_options(argc, argv, options, u, 0);

	for (i = 0; i < argc; i++)
		error("unknown parameter \"%s\"\n", argv[i]);

	if (argc)
		usage_with_options(u, options);

	rc = test_blk_namespaces(loglevel);
	fprintf(stderr, "test_blk_namespaces: %s\n", rc ? "FAIL" : "PASS");
	if (rc)
		return rc;

	rc = test_pmem_namespaces(loglevel);
	fprintf(stderr, "test_pmem_namespaces: %s\n", rc ? "FAIL" : "PASS");
	return rc;
}
