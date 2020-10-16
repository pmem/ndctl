// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2017-2020 Intel Corporation. All rights reserved.
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <syslog.h>
#include <stdlib.h>
#include <ndctl/libndctl.h>

static void do_notify(struct ndctl_dimm *dimm)
{
	struct ndctl_cmd *s_cmd = ndctl_dimm_cmd_new_smart(dimm);
	struct ndctl_cmd *st_cmd = NULL, *sst_cmd = NULL;
	unsigned int orig_mtemp, orig_ctemp, orig_spare;
	const char *name = ndctl_dimm_get_devname(dimm);
	unsigned int alarm, mtemp, ctemp, spare, valid;
	int rc, i;

	if (!s_cmd) {
		fprintf(stderr, "%s: no smart command support\n", name);
		goto out;
	}

	rc = ndctl_cmd_submit(s_cmd);
	if (rc < 0) {
		fprintf(stderr, "%s: smart command failed: %d %s\n", name,
				rc, strerror(errno));
		goto out;
	}

	valid = ndctl_cmd_smart_get_flags(s_cmd);
	alarm = ndctl_cmd_smart_get_alarm_flags(s_cmd);
	mtemp = ndctl_cmd_smart_get_media_temperature(s_cmd);
	ctemp = ndctl_cmd_smart_get_ctrl_temperature(s_cmd);
	spare = ndctl_cmd_smart_get_spares(s_cmd);
	fprintf(stderr, "%s: (smart) alarm%s: %#x mtemp%s: %.2f ctemp%s: %.2f spares%s: %d\n",
			name,
			valid & ND_SMART_ALARM_VALID ? "" : "(invalid)", alarm,
			valid & ND_SMART_MTEMP_VALID ? "" : "(invalid)",
			ndctl_decode_smart_temperature(mtemp),
			valid & ND_SMART_CTEMP_VALID ? "" : "(invalid)",
			ndctl_decode_smart_temperature(ctemp),
			valid & ND_SMART_SPARES_VALID ? "" : "(invalid)", spare);

	st_cmd = ndctl_dimm_cmd_new_smart_threshold(dimm);
	if (!st_cmd) {
		fprintf(stderr, "%s: no smart threshold command support\n", name);
		goto out;
	}

	rc = ndctl_cmd_submit(st_cmd);
	if (rc < 0) {
		fprintf(stderr, "%s: smart threshold command failed: %d %s\n",
				name, rc, strerror(errno));
		goto out;
	}

	alarm = ndctl_cmd_smart_threshold_get_alarm_control(st_cmd);
	mtemp = ndctl_cmd_smart_threshold_get_media_temperature(st_cmd);
	ctemp = ndctl_cmd_smart_threshold_get_ctrl_temperature(st_cmd);
	spare = ndctl_cmd_smart_threshold_get_spares(st_cmd);
	fprintf(stderr, "%s: (smart thresh) alarm: %#x mtemp: %.2f ctemp: %.2f spares: %d\n",
			name, alarm, ndctl_decode_smart_temperature(mtemp),
			ndctl_decode_smart_temperature(ctemp), spare);

	orig_mtemp = mtemp;
	orig_ctemp = ctemp;
	orig_spare = spare;

	sst_cmd = ndctl_dimm_cmd_new_smart_set_threshold(st_cmd);
	if (!sst_cmd) {
		fprintf(stderr, "%s: no smart set threshold command support\n", name);
		goto out;
	}

	alarm = ndctl_cmd_smart_threshold_get_supported_alarms(sst_cmd);
	if (!alarm) {
		fprintf(stderr, "%s: no smart set threshold command support\n", name);
		goto out;
	}


	fprintf(stderr, "%s: supported alarms: %#x\n", name, alarm);

	/*
	 * free the cmd now since we only needed the alarms and will
	 * create + issue a set_threshold test for each alarm
	 */
	ndctl_cmd_unref(sst_cmd);

	for (i = 0; i < 3; i++) {
		unsigned int set_alarm = 1 << i;

		if (!(alarm & set_alarm))
			continue;

		sst_cmd = ndctl_dimm_cmd_new_smart_set_threshold(st_cmd);
		if (!sst_cmd) {
			fprintf(stderr, "%s: alloc failed: smart set threshold\n",
					name);
			goto out;
		}

		switch (set_alarm) {
		case ND_SMART_SPARE_TRIP:
			fprintf(stderr, "%s: set spare threshold: 99\n", name);
			ndctl_cmd_smart_threshold_set_spares(sst_cmd, 99);
			ndctl_cmd_smart_threshold_set_media_temperature(
					sst_cmd, orig_mtemp);
			ndctl_cmd_smart_threshold_set_ctrl_temperature(
					sst_cmd, orig_ctemp);
			break;
		case ND_SMART_MTEMP_TRIP:
			mtemp = ndctl_cmd_smart_get_media_temperature(s_cmd);
			if (mtemp & (1 << 15))
				mtemp *= 2;
			else
				mtemp /= 2;
			fprintf(stderr, "%s: set mtemp threshold: %.2f\n", name,
					ndctl_decode_smart_temperature(mtemp));
			ndctl_cmd_smart_threshold_set_spares(
					sst_cmd, orig_spare);
			ndctl_cmd_smart_threshold_set_media_temperature(
					sst_cmd, mtemp);
			ndctl_cmd_smart_threshold_set_ctrl_temperature(
					sst_cmd, orig_ctemp);
			break;
		case ND_SMART_CTEMP_TRIP:
			ctemp = ndctl_cmd_smart_get_ctrl_temperature(s_cmd);
			if (ctemp & (1 << 15))
				ctemp *= 2;
			else
				ctemp /= 2;
			fprintf(stderr, "%s: set ctemp threshold: %.2f\n", name,
					ndctl_decode_smart_temperature(ctemp));

			ndctl_cmd_smart_threshold_set_spares(
					sst_cmd, orig_spare);
			ndctl_cmd_smart_threshold_set_media_temperature(
					sst_cmd, orig_mtemp);
			ndctl_cmd_smart_threshold_set_ctrl_temperature(
					sst_cmd, ctemp);

			break;
		default:
			break;
		}

		ndctl_cmd_smart_threshold_set_alarm_control(sst_cmd, set_alarm);
		rc = ndctl_cmd_submit(sst_cmd);
		if (rc < 0) {
			fprintf(stderr, "%s: smart set threshold command failed: %d %s\n",
					name, rc, strerror(errno));
			goto out;
		}

		ndctl_cmd_unref(sst_cmd);
	}

	fprintf(stderr, "%s: set thresholds back to defaults\n", name);
	sst_cmd = ndctl_dimm_cmd_new_smart_set_threshold(st_cmd);
	if (!sst_cmd) {
		fprintf(stderr, "%s: alloc failed: smart set threshold\n",
				name);
		goto out;
	}

	rc = ndctl_cmd_submit(sst_cmd);
	if (rc < 0) {
		fprintf(stderr, "%s: smart set threshold defaults failed: %d %s\n",
				name, rc, strerror(errno));
		goto out;
	}

out:
	ndctl_cmd_unref(sst_cmd);
	ndctl_cmd_unref(st_cmd);
	ndctl_cmd_unref(s_cmd);
}

static void test_dimm_notify(struct ndctl_bus *bus)
{
	struct ndctl_dimm *dimm;

	ndctl_dimm_foreach(bus, dimm)
		do_notify(dimm);
}

int main(int argc, char *argv[])
{
	struct ndctl_ctx *ctx;
	struct ndctl_bus *bus;
	int rc = EXIT_FAILURE;
	const char *provider;

	rc = ndctl_new(&ctx);
	if (rc < 0)
		return EXIT_FAILURE;

	if (argc != 2) {
		fprintf(stderr, "usage: smart-notify <nvdimm-bus-provider>\n");
		goto out;
	}

	ndctl_set_log_priority(ctx, LOG_DEBUG);

	provider = argv[1];
	bus = ndctl_bus_get_by_provider(ctx, provider);
	if (!bus) {
		fprintf(stderr, "smart-notify: unable to find bus (%s)\n",
				provider);
		goto out;
	}

	rc = EXIT_SUCCESS;
	test_dimm_notify(bus);
out:
	ndctl_unref(ctx);
	return rc;
}
