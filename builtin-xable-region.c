#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <util/parse-options.h>
#include <ndctl/libndctl.h>

static const char *region_bus;

static const struct option region_options[] = {
	OPT_STRING('b', "bus", &region_bus, "bus-id",
			"<region> must be on a bus with an id/provider of <bus-id>"),
	OPT_END(),
};

static const char *parse_region_options(int argc, const char **argv,
		char *xable_usage)
{
	const char * const u[] = {
		xable_usage,
		NULL
	};
	int i;

        argc = parse_options(argc, argv, region_options, u, 0);

	if (argc == 0)
		error("specify a region to delete, or \"all\"\n");
	for (i = 1; i < argc; i++)
		error("unknown extra parameter \"%s\"\n", argv[i]);
	if (argc == 0 || argc > 1) {
		usage_with_options(u, region_options);
		return NULL; /* we won't return from usage_with_options() */
	}
	return argv[0];
}

static int do_xable_region(const char *region_arg,
		int (*xable_fn)(struct ndctl_region *))
{
	unsigned long bus_id = ULONG_MAX, id;
	const char *provider, *region_name;
	int rc = -ENXIO, success = 0;
	struct ndctl_region *region;
	struct ndctl_ctx *ctx;
	struct ndctl_bus *bus;

	if (!region_arg)
		goto out;

	rc = ndctl_new(&ctx);
	if (rc < 0)
		goto out;

	if (region_bus) {
		char *end = NULL;

		bus_id = strtoul(region_bus, &end, 0);
		if (end)
			bus_id = ULONG_MAX;
	}

        ndctl_bus_foreach(ctx, bus) {
		provider = ndctl_bus_get_provider(bus);
		id = ndctl_bus_get_id(bus);

		if (bus_id < ULONG_MAX && bus_id != id)
			continue;
		else if (bus_id == ULONG_MAX && region_bus
				&& strcmp(region_bus, provider) != 0)
			continue;

		ndctl_region_foreach(bus, region) {
			region_name = ndctl_region_get_devname(region);

			if (strcmp(region_arg, "all") != 0
					&& strcmp(region_arg, region_name) != 0)
				continue;
			if (xable_fn(region) == 0)
				success++;
		}
	}

	rc = success;
	ndctl_unref(ctx);
 out:
	region_bus = NULL;
	return rc;
}

int cmd_disable_region(int argc, const char **argv)
{
	char *xable_usage = "ndctl disable-region <region> [<options>]";
	const char *region = parse_region_options(argc, argv, xable_usage);
	int disabled = do_xable_region(region, ndctl_region_disable_invalidate);

	if (disabled < 0) {
		fprintf(stderr, "error disabling regions: %s\n",
				strerror(-disabled));
		return disabled;
	} else if (disabled == 0) {
		fprintf(stderr, "disabled 0 regions\n");
		return -ENXIO;
	} else {
		fprintf(stderr, "disabled %d region%s\n", disabled,
				disabled > 1 ? "s" : "");
		return disabled;
	}
}

int cmd_enable_region(int argc, const char **argv)
{
	char *xable_usage = "ndctl enable-region <region> [<options>]";
	const char *region = parse_region_options(argc, argv, xable_usage);
	int enabled = do_xable_region(region, ndctl_region_enable);

	if (enabled < 0) {
		fprintf(stderr, "error enabling regions: %s\n",
				strerror(-enabled));
		return enabled;
	} else if (enabled == 0) {
		fprintf(stderr, "enabled 0 regions\n");
		return -ENXIO;
	} else {
		fprintf(stderr, "enabled %d region%s\n", enabled,
				enabled > 1 ? "s" : "");
		return enabled;
	}
}
