#include <syslog.h>
#include <test-libndctl.h>
#include <util/parse-options.h>

int cmd_test(int argc, const char **argv)
{
	int loglevel = LOG_DEBUG, i;
	const char * const u[] = {
		"ndctl test [<options>]",
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

	return test_libndctl(loglevel);
}
