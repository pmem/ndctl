#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <util/filter.h>
#include <util/parse-options.h>
#include <ndctl/libndctl.h>

static const char *dimm_bus;

static const struct option dimm_options[] = {
	OPT_STRING('b', "bus", &dimm_bus, "bus-id",
			"<dimm> must be on a bus with an id/provider of <bus-id>"),
	OPT_END(),
};

static const char *parse_dimm_options(int argc, const char **argv,
		char *xable_usage)
{
	const char * const u[] = {
		xable_usage,
		NULL
	};
	int i;

        argc = parse_options(argc, argv, dimm_options, u, 0);

	if (argc == 0)
		error("specify a dimm to disable, or \"all\"\n");
	for (i = 1; i < argc; i++)
		error("unknown extra parameter \"%s\"\n", argv[i]);
	if (argc == 0 || argc > 1) {
		usage_with_options(u, dimm_options);
		return NULL; /* we won't return from usage_with_options() */
	}
	return argv[0];
}

static int do_xable_dimm(const char *dimm_arg,
		int (*xable_fn)(struct ndctl_dimm *), struct ndctl_ctx *ctx)
{
	int rc = -ENXIO, skip = 0, success = 0;
	struct ndctl_dimm *dimm;
	struct ndctl_bus *bus;

	if (!dimm_arg)
		goto out;

        ndctl_bus_foreach(ctx, bus) {
		if (!util_bus_filter(bus, dimm_bus))
			continue;

		ndctl_dimm_foreach(bus, dimm) {
			if (!util_dimm_filter(dimm, dimm_arg))
				continue;
			if (xable_fn == ndctl_dimm_disable
					&& ndctl_dimm_is_active(dimm)) {
				fprintf(stderr, "%s is active, skipping...\n",
						ndctl_dimm_get_devname(dimm));
				skip++;
				continue;
			}
			if (xable_fn(dimm) == 0)
				success++;
		}
	}

	rc = success;
	if (!success && skip)
		rc = -EBUSY;
 out:
	dimm_bus = NULL;
	return rc;
}

int cmd_disable_dimm(int argc, const char **argv, struct ndctl_ctx *ctx)
{
	char *xable_usage = "ndctl disable-dimm <dimm> [<options>]";
	const char *dimm = parse_dimm_options(argc, argv, xable_usage);
	int disabled = do_xable_dimm(dimm, ndctl_dimm_disable,
			ctx);

	if (disabled < 0) {
		fprintf(stderr, "error disabling dimms: %s\n",
				strerror(-disabled));
		return disabled;
	} else if (disabled == 0) {
		fprintf(stderr, "disabled 0 dimms\n");
		return 0;
	} else {
		fprintf(stderr, "disabled %d dimm%s\n", disabled,
				disabled > 1 ? "s" : "");
		return 0;
	}
}

int cmd_enable_dimm(int argc, const char **argv, struct ndctl_ctx *ctx)
{
	char *xable_usage = "ndctl enable-dimm <dimm> [<options>]";
	const char *dimm = parse_dimm_options(argc, argv, xable_usage);
	int enabled = do_xable_dimm(dimm, ndctl_dimm_enable, ctx);

	if (enabled < 0) {
		fprintf(stderr, "error enabling dimms: %s\n",
				strerror(-enabled));
		return enabled;
	} else if (enabled == 0) {
		fprintf(stderr, "enabled 0 dimms\n");
		return 0;
	} else {
		fprintf(stderr, "enabled %d dimm%s\n", enabled,
				enabled > 1 ? "s" : "");
		return 0;
	}
}
