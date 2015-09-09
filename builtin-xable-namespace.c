#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <util/parse-options.h>
#include <ndctl/libndctl.h>

static const char *namespace_bus;
static const char *namespace_region;

static const struct option namespace_options[] = {
	OPT_STRING('b', "bus", &namespace_bus, "bus-id",
			"<namespace> must be on a bus with an id/provider of <bus-id>"),
	OPT_STRING('r', "region", &namespace_region, "region-id",
			"<namespace> must be a child of a region with an id/name of <region-id>"),
	OPT_END(),
};

static const char *parse_namespace_options(int argc, const char **argv,
		char *xable_usage)
{
	const char * const u[] = {
		xable_usage,
		NULL
	};
	int i;

        argc = parse_options(argc, argv, namespace_options, u, 0);

	if (argc == 0)
		error("specify a namespace to delete, or \"all\"\n");
	for (i = 1; i < argc; i++)
		error("unknown extra parameter \"%s\"\n", argv[i]);
	if (argc == 0 || argc > 1) {
		usage_with_options(u, namespace_options);
		return NULL; /* we won't return from usage_with_options() */
	}
	return argv[0];
}

static int do_xable_namespace(const char *namespace,
		int (*xable_fn)(struct ndctl_namespace *))
{
	unsigned long bus_id = ULONG_MAX, region_id = ULONG_MAX, id;
	const char *provider, *region_name, *ndns_name;
	int rc = -ENXIO, success = 0;
	struct ndctl_namespace *ndns;
	struct ndctl_region *region;
	struct ndctl_ctx *ctx;
	struct ndctl_bus *bus;

	if (!namespace)
		goto out;

	rc = ndctl_new(&ctx);
	if (rc < 0)
		goto out;

	if (namespace_bus) {
		char *end = NULL;

		bus_id = strtoul(namespace_bus, &end, 0);
		if (end)
			bus_id = ULONG_MAX;
	}

	if (namespace_region) {
		char *end = NULL;

		region_id = strtoul(namespace_region, &end, 0);
		if (end)
			region_id = ULONG_MAX;
	}

        ndctl_bus_foreach(ctx, bus) {
		provider = ndctl_bus_get_provider(bus);
		id = ndctl_bus_get_id(bus);

		if (bus_id < ULONG_MAX && bus_id != id)
			continue;
		else if (bus_id == ULONG_MAX && namespace_bus
				&& strcmp(namespace_bus, provider) != 0)
			continue;

		ndctl_region_foreach(bus, region) {
			region_name = ndctl_region_get_devname(region);
			id = ndctl_region_get_id(region);

			if (region_id < ULONG_MAX && region_id != id)
				continue;
			else if (region_id == ULONG_MAX && namespace_region
					&& strcmp(namespace_region, region_name) != 0)
				continue;
			ndctl_namespace_foreach(region, ndns) {
				ndns_name = ndctl_namespace_get_devname(ndns);

				if (strcmp(namespace, "all") != 0
						&& strcmp(namespace, ndns_name) != 0)
					continue;
				if (xable_fn(ndns) == 0)
					success++;
			}
		}
	}

	rc = success;
	ndctl_unref(ctx);
 out:
	namespace_bus = NULL;
	namespace_region = NULL;
	return rc;
}

int cmd_disable_namespace(int argc, const char **argv)
{
	char *xable_usage = "ndctl disable-namespace <namespace> [<options>]";
	const char *namespace = parse_namespace_options(argc, argv, xable_usage);
	int disabled = do_xable_namespace(namespace, ndctl_namespace_disable_invalidate);

	if (disabled < 0) {
		fprintf(stderr, "error disabling namespaces: %s\n",
				strerror(-disabled));
		return disabled;
	} else if (disabled == 0) {
		fprintf(stderr, "disabled 0 namespaces\n");
		return -ENXIO;
	} else {
		fprintf(stderr, "disabled %d namespace%s\n", disabled,
				disabled > 1 ? "s" : "");
		return disabled;
	}
}

int cmd_enable_namespace(int argc, const char **argv)
{
	char *xable_usage = "ndctl enable-namespace <namespace> [<options>]";
	const char *namespace = parse_namespace_options(argc, argv, xable_usage);
	int enabled = do_xable_namespace(namespace, ndctl_namespace_enable);

	if (enabled < 0) {
		fprintf(stderr, "error enabling namespaces: %s\n",
				strerror(-enabled));
		return enabled;
	} else if (enabled == 0) {
		fprintf(stderr, "enabled 0 namespaces\n");
		return -ENXIO;
	} else {
		fprintf(stderr, "enabled %d namespace%s\n", enabled,
				enabled > 1 ? "s" : "");
		return enabled;
	}
}
