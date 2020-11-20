// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2018, FUJITSU LIMITED. All rights reserved.
#include <stdio.h>
#include <errno.h>
#include <util/json.h>
#include <util/filter.h>
#include <json-c/json.h>
#include <ndctl/libndctl.h>
#include <util/parse-options.h>
#include <ndctl.h>

struct util_filter_params param;
static int did_fail;
static int jflag = JSON_C_TO_STRING_PRETTY;

#define fail(fmt, ...) \
do { \
	did_fail = 1; \
	fprintf(stderr, "ndctl-%s:%s:%d: " fmt, \
			VERSION, __func__, __LINE__, ##__VA_ARGS__); \
} while (0)

static bool filter_region(struct ndctl_region *region,
		struct util_filter_ctx *ctx)
{
	return true;
}

static void filter_dimm(struct ndctl_dimm *dimm, struct util_filter_ctx *ctx)
{
	struct list_filter_arg *lfa = ctx->list;
	struct json_object *jdimm;

	if (!ndctl_dimm_is_cmd_supported(dimm, ND_CMD_SMART))
		return;
	if (!ndctl_dimm_is_cmd_supported(dimm, ND_CMD_SMART_THRESHOLD))
		return;
	if (!ndctl_dimm_is_flag_supported(dimm, ND_SMART_ALARM_VALID))
		return;

	if (!lfa->jdimms) {
		lfa->jdimms = json_object_new_array();
		if (!lfa->jdimms) {
			fail("\n");
			return;
		}
	}

	jdimm = util_dimm_to_json(dimm, lfa->flags);
	if (!jdimm) {
		fail("\n");
		return;
	}

	json_object_array_add(lfa->jdimms, jdimm);
}

static bool filter_bus(struct ndctl_bus *bus, struct util_filter_ctx *ctx)
{
	return true;
}

static int list_display(struct list_filter_arg *lfa)
{
	struct json_object *jdimms = lfa->jdimms;

	if (jdimms)
		util_display_json_array(stdout, jdimms, jflag);
	return 0;
}

int main(int argc, const char *argv[])
{
	struct ndctl_ctx *ctx;
	int i, rc;
	const struct option options[] = {
		OPT_STRING('b', "bus", &param.bus, "bus-id", "filter by bus"),
		OPT_STRING('r', "region", &param.region, "region-id",
				"filter by region"),
		OPT_STRING('d', "dimm", &param.dimm, "dimm-id",
				"filter by dimm"),
		OPT_STRING('n', "namespace", &param.namespace, "namespace-id",
				"filter by namespace id"),
		OPT_END(),
	};
	const char * const u[] = {
		"list-smart-dimm [<options>]",
		NULL
	};
	struct util_filter_ctx fctx = { 0 };
	struct list_filter_arg lfa = { 0 };

	rc = ndctl_new(&ctx);
	if (rc < 0)
		return EXIT_FAILURE;
        argc = parse_options(argc, argv, options, u, 0);
	for (i = 0; i < argc; i++)
		error("unknown parameter \"%s\"\n", argv[i]);
	if (argc)
		usage_with_options(u, options);

	fctx.filter_bus = filter_bus;
	fctx.filter_dimm = filter_dimm;
	fctx.filter_region = filter_region;
	fctx.filter_namespace = NULL;
	fctx.list = &lfa;
	lfa.flags = 0;

	rc = util_filter_walk(ctx, &fctx, &param);
	if (rc)
		return rc;

	if (list_display(&lfa) || did_fail)
		return -ENOMEM;
	return 0;
}
