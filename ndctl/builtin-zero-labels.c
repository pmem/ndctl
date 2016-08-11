#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <syslog.h>
#include <util/filter.h>
#include <util/parse-options.h>
#include <ndctl/libndctl.h>

static int do_zero_dimm(struct ndctl_dimm *dimm, const char **argv, int argc,
		bool verbose)
{
	struct ndctl_ctx *ctx = ndctl_dimm_get_ctx(dimm);
	int i, rc, log;

	for (i = 0; i < argc; i++)
		if (util_dimm_filter(dimm, argv[i]))
			break;
	if (i >= argc)
		return -ENODEV;

	log = ndctl_get_log_priority(ctx);
	if (verbose)
		ndctl_set_log_priority(ctx, LOG_DEBUG);
	rc = ndctl_dimm_zero_labels(dimm);
	ndctl_set_log_priority(ctx, log);

	return rc;
}

int cmd_zero_labels(int argc, const char **argv, struct ndctl_ctx *ctx)
{
	const char *nmem_bus = NULL;
	bool verbose = false;
	const struct option nmem_options[] = {
		OPT_STRING('b', "bus", &nmem_bus, "bus-id",
				"<nmem> must be on a bus with an id/provider of <bus-id>"),
		OPT_BOOLEAN('v',"verbose", &verbose, "turn on debug"),
		OPT_END(),
	};
	const char * const u[] = {
		"ndctl zero-labels <nmem0> [<nmem1>..<nmemN>] [<options>]",
		NULL
	};
	struct ndctl_dimm *dimm;
	struct ndctl_bus *bus;
	int i, rc, count, err = 0;

        argc = parse_options(argc, argv, nmem_options, u, 0);

	if (argc == 0)
		usage_with_options(u, nmem_options);
	for (i = 0; i < argc; i++) {
		unsigned long id;

		if (strcmp(argv[i], "all") == 0)
			continue;
		if (sscanf(argv[i], "nmem%lu", &id) != 1) {
			fprintf(stderr, "unknown extra parameter \"%s\"\n",
					argv[i]);
			usage_with_options(u, nmem_options);
		}
	}

	count = 0;
        ndctl_bus_foreach(ctx, bus) {
		if (!util_bus_filter(bus, nmem_bus))
			continue;

		ndctl_dimm_foreach(bus, dimm) {
			rc = do_zero_dimm(dimm, argv, argc, verbose);
			if (rc == 0)
				count++;
			else if (rc && !err)
				err = rc;
		}
	}
	rc = err;

	fprintf(stderr, "zeroed %d nmem%s\n", count, count > 1 ? "s" : "");

	/*
	 * 0 if all dimms zeroed, count if at least 1 dimm zeroed, < 0
	 * if all errors
	 */
	if (rc == 0)
		return 0;
	if (count)
		return count;
	return rc;
}
