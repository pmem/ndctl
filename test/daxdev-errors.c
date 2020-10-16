// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2015-2020 Intel Corporation. All rights reserved.
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <linux/fs.h>
#include <linux/fiemap.h>
#include <setjmp.h>
#include <limits.h>

#include <util/log.h>
#include <util/sysfs.h>
#include <daxctl/libdaxctl.h>
#include <ccan/array_size/array_size.h>
#include <ndctl/libndctl.h>
#include <ndctl.h>

#define fail() fprintf(stderr, "%s: failed at: %d\n", __func__, __LINE__)

struct check_cmd {
	struct ndctl_cmd *cmd;
	struct ndctl_test *test;
};

static sigjmp_buf sj_env;
static int sig_count;

static const char *NFIT_PROVIDER0 = "nfit_test.0";
static struct check_cmd *check_cmds;

static void sigbus_hdl(int sig, siginfo_t *siginfo, void *ptr)
{
	fprintf(stderr, "** Received a SIGBUS **\n");
	sig_count++;
	siglongjmp(sj_env, 1);
}

static int check_ars_cap(struct ndctl_bus *bus, uint64_t start,
		size_t size, struct check_cmd *check)
{
	struct ndctl_cmd *cmd;
	int rc;

	if (check->cmd != NULL) {
		fprintf(stderr, "%s: expected a NULL command, by default\n",
				__func__);
		return -EINVAL;
	}

	cmd = ndctl_bus_cmd_new_ars_cap(bus, start, size);
	if (!cmd) {
		fprintf(stderr, "%s: bus: %s failed to create cmd\n",
				__func__, ndctl_bus_get_provider(bus));
		return -ENOTTY;
	}

	rc = ndctl_cmd_submit(cmd);
	if (rc < 0) {
		fprintf(stderr, "%s: bus: %s failed to submit cmd: %d\n",
				__func__, ndctl_bus_get_provider(bus), rc);
		ndctl_cmd_unref(cmd);
		return rc;
	}

	if (ndctl_cmd_ars_cap_get_size(cmd) < sizeof(struct nd_cmd_ars_status)){
		fprintf(stderr, "%s: bus: %s expected size >= %zd got: %d\n",
				__func__, ndctl_bus_get_provider(bus),
				sizeof(struct nd_cmd_ars_status),
				ndctl_cmd_ars_cap_get_size(cmd));
		ndctl_cmd_unref(cmd);
		return -ENXIO;
	}

	check->cmd = cmd;
	return 0;
}

static int check_ars_start(struct ndctl_bus *bus, struct check_cmd *check)
{
	struct ndctl_cmd *cmd_ars_cap = check_cmds[ND_CMD_ARS_CAP].cmd;
	struct ndctl_cmd *cmd;
	int rc;

	if (check->cmd != NULL) {
		fprintf(stderr, "%s: expected a NULL command, by default\n",
				__func__);
		return -ENXIO;
	}

	cmd = ndctl_bus_cmd_new_ars_start(cmd_ars_cap, ND_ARS_PERSISTENT);
	if (!cmd) {
		fprintf(stderr, "%s: bus: %s failed to create cmd\n",
				__func__, ndctl_bus_get_provider(bus));
		return -ENOTTY;
	}

	rc = ndctl_cmd_submit(cmd);
	if (rc < 0) {
		fprintf(stderr, "%s: bus: %s failed to submit cmd: %d\n",
				__func__, ndctl_bus_get_provider(bus), rc);
		ndctl_cmd_unref(cmd);
		return rc;
	}

	check->cmd = cmd;
	return 0;
}

static int check_ars_status(struct ndctl_bus *bus, struct check_cmd *check)
{
	struct ndctl_cmd *cmd_ars_cap = check_cmds[ND_CMD_ARS_CAP].cmd;
	struct ndctl_cmd *cmd;
	unsigned long tmo = 5;
	unsigned int i;
	int rc;

	if (check->cmd != NULL) {
		fprintf(stderr, "%s: expected a NULL command, by default\n",
				__func__);
		return -ENXIO;
	}

 retry:
	cmd = ndctl_bus_cmd_new_ars_status(cmd_ars_cap);
	if (!cmd) {
		fprintf(stderr, "%s: bus: %s failed to create cmd\n",
				__func__, ndctl_bus_get_provider(bus));
		return -ENOTTY;
	}

	rc = ndctl_cmd_submit(cmd);
	if (rc < 0) {
		fprintf(stderr, "%s: bus: %s failed to submit cmd: %d\n",
				__func__, ndctl_bus_get_provider(bus), rc);
		ndctl_cmd_unref(cmd);
		return rc;
	}

	if (!tmo) {
		fprintf(stderr, "%s: bus: %s ars timeout\n", __func__,
				ndctl_bus_get_provider(bus));
		return -EIO;
	}

	if (ndctl_cmd_ars_in_progress(cmd)) {
		tmo--;
		sleep(1);
		goto retry;
	}

	for (i = 0; i < ndctl_cmd_ars_num_records(cmd); i++) {
		fprintf(stderr, "%s: record[%d].addr: 0x%llx\n", __func__, i,
				ndctl_cmd_ars_get_record_addr(cmd, i));
		fprintf(stderr, "%s: record[%d].length: 0x%llx\n", __func__, i,
				ndctl_cmd_ars_get_record_len(cmd, i));
	}

	check->cmd = cmd;
	return 0;
}

static int check_clear_error(struct ndctl_bus *bus, struct check_cmd *check)
{
	struct ndctl_cmd *ars_cap = check_cmds[ND_CMD_ARS_CAP].cmd;
	struct ndctl_cmd *clear_err;
	unsigned long long cleared;
	struct ndctl_range range;
	int rc;

	if (check->cmd != NULL) {
		fprintf(stderr, "%s: expected a NULL command, by default\n",
				__func__);
		return -ENXIO;
	}

	if (ndctl_cmd_ars_cap_get_range(ars_cap, &range)) {
		fprintf(stderr, "failed to get ars_cap range\n");
		return -ENXIO;
	}

	fprintf(stderr, "%s: clearing at %#llx for %llu bytes\n",
			__func__, range.address, range.length);

	clear_err = ndctl_bus_cmd_new_clear_error(range.address,
					range.length, ars_cap);
	if (!clear_err) {
		fprintf(stderr, "%s: bus: %s failed to create cmd\n",
				__func__, ndctl_bus_get_provider(bus));
		return -ENOTTY;
	}

	rc = ndctl_cmd_submit(clear_err);
	if (rc < 0) {
		fprintf(stderr, "%s: bus: %s failed to submit cmd: %d\n",
				__func__, ndctl_bus_get_provider(bus), rc);
		ndctl_cmd_unref(clear_err);
		return rc;
	}

	cleared = ndctl_cmd_clear_error_get_cleared(clear_err);
	if (cleared != range.length) {
		fprintf(stderr, "%s: bus: %s expected to clear: %lld actual: %lld\
n",
				__func__, ndctl_bus_get_provider(bus),
				range.length, cleared);
		return -ENXIO;
	}

	check->cmd = clear_err;
	return 0;
}

static struct ndctl_dax * get_dax_region(struct ndctl_region *region)
{
	struct ndctl_dax *dax;

	ndctl_dax_foreach(region, dax)
		if (ndctl_dax_is_enabled(dax) &&
			ndctl_dax_is_configured(dax))
			return dax;

	return NULL;
}

static int test_daxdev_clear_error(const char *bus_name,
		const char *region_name)
{
	int rc = 0, i;
	struct ndctl_ctx *ctx;
	struct ndctl_bus *bus;
	struct ndctl_region *region;
	struct ndctl_dax *dax = NULL;
	uint64_t base, start, offset, blocks, size;
        struct check_cmd __bus_cmds[] = {
                [ND_CMD_ARS_CAP] = {},
                [ND_CMD_ARS_START] = {},
                [ND_CMD_ARS_STATUS] = {},
                [ND_CMD_CLEAR_ERROR] = {},
        };
	char path[256];
	char buf[SYSFS_ATTR_SIZE];
	struct log_ctx log_ctx;

	log_init(&log_ctx, "test/init", "NDCTL_DAXDEV_TEST");
	rc = ndctl_new(&ctx);
	if (rc)
		return rc;

	bus = ndctl_bus_get_by_provider(ctx, NFIT_PROVIDER0);
	if (!bus) {
		rc = -ENODEV;
		goto cleanup;
	}

	ndctl_region_foreach(bus, region) {
		if (strncmp(region_name,
				ndctl_region_get_devname(region), 256) == 0) {
			/* find the dax region */
			dax = get_dax_region(region);
			break;
		}
	}

	if (!dax) {
		rc = -ENODEV;
		goto cleanup;
	}

	/* get badblocks */
	if (snprintf(path, 256,
			"/sys/devices/platform/%s/%s/%s/badblocks",
			NFIT_PROVIDER0,
			bus_name,
			ndctl_region_get_devname(region)) >= 256) {
		fprintf(stderr, "%s: buffer too small!\n",
				ndctl_region_get_devname(region));
		rc = -ENXIO;
		goto cleanup;
	}

	if (__sysfs_read_attr(&log_ctx, path, buf) < 0) {
		rc = -ENXIO;
		goto cleanup;
	}

	/* retrieve badblocks from buf */
	rc = sscanf(buf, "%lu %lu", &offset, &blocks);
	if (rc == EOF) {
		rc = -errno;
		goto cleanup;
	}

	/* get resource base */
	base = ndctl_region_get_resource(region);
	if (base == ULLONG_MAX) {
		rc = -ERANGE;
		goto cleanup;
	}

	check_cmds = __bus_cmds;
	start = base + offset * 512;
	size = 512 * blocks;

	rc = check_ars_cap(bus, start, size, &check_cmds[ND_CMD_ARS_CAP]);
	if (rc < 0)
		goto cleanup;

	rc = check_ars_start(bus, &check_cmds[ND_CMD_ARS_START]);
	if (rc < 0)
		goto cleanup;

	rc = check_ars_status(bus, &check_cmds[ND_CMD_ARS_STATUS]);
	if (rc < 0)
		goto cleanup;

	rc = check_clear_error(bus, &check_cmds[ND_CMD_CLEAR_ERROR]);
	if (rc < 0)
		goto cleanup;

	for (i = 1; i < (int)ARRAY_SIZE(__bus_cmds); i++) {
		if (__bus_cmds[i].cmd) {
			ndctl_cmd_unref(__bus_cmds[i].cmd);
			__bus_cmds[i].cmd = NULL;
                }
	}

cleanup:
	ndctl_unref(ctx);
	return rc;
}


int main(int argc, char *argv[])
{
	int rc;
	struct sigaction act;

	if (argc < 1 || argc > 4)
		return -EINVAL;

	memset(&act, 0, sizeof(act));
	act.sa_sigaction = sigbus_hdl;
	act.sa_flags = SA_SIGINFO;

	if (sigaction(SIGBUS, &act, 0)) {
		fail();
		return 1;
	}

	rc = test_daxdev_clear_error(argv[1], argv[2]);

	return rc;
}
