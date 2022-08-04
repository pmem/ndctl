// SPDX-License-Identifier: GPL-2.0
/* Copyright (C) 2020-2021 Intel Corporation. All rights reserved. */
/* Copyright (C) 2005 Andreas Ericsson. All rights reserved. */

/* originally copied from perf and git */

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <ccan/array_size/array_size.h>
#include <ccan/endian/endian.h>
#include <ccan/short_types/short_types.h>
#include <cxl/libcxl.h>
#include <util/parse-options.h>
#include <ccan/array_size/array_size.h>

#include <util/strbuf.h>
#include <util/util.h>
#include <util/main.h>
#include <cxl/builtin.h>

const char cxl_usage_string[] = "cxl [--version] [--help] COMMAND [ARGS]";
const char cxl_more_info_string[] =
	"See 'cxl help COMMAND' for more information on a specific command.\n"
	" cxl --list-cmds to see all available commands";

static int cmd_version(int argc, const char **argv, struct cxl_ctx *ctx)
{
	printf("%s\n", VERSION);
	return 0;
}

static int cmd_help(int argc, const char **argv, struct cxl_ctx *ctx)
{
	const char * const builtin_help_subcommands[] = {
		"list",
		NULL,
	};
	struct option builtin_help_options[] = {
		OPT_END(),
	};
	const char *builtin_help_usage[] = {
		"cxl help [command]",
		NULL
	};

	argc = parse_options_subcommand(argc, argv, builtin_help_options,
			builtin_help_subcommands, builtin_help_usage, 0);

	if (!argv[0]) {
		printf("\n usage: %s\n\n", cxl_usage_string);
		printf("\n %s\n\n", cxl_more_info_string);
		return 0;
	}

	return help_show_man_page(argv[0], "cxl", "CXL_MAN_VIEWER");
}

static struct cmd_struct commands[] = {
	{ "update-fw", .c_fn = cmd_update_fw },
	{ "get-fw-info", .c_fn = cmd_get_fw_info },
	{ "activate-fw", .c_fn = cmd_activate_fw },
	{ "device-info-get", .c_fn = cmd_device_info_get },
	{ "version", .c_fn = cmd_version },
	{ "list", .c_fn = cmd_list },
	{ "help", .c_fn = cmd_help },
	{ "zero-labels", .c_fn = cmd_zero_labels },
	{ "read-labels", .c_fn = cmd_read_labels },
	{ "write-labels", .c_fn = cmd_write_labels },
	{ "id-cmd", .c_fn = cmd_identify },
	{ "get-supported-logs", .c_fn = cmd_get_supported_logs },
	{ "get-cel-log", .c_fn = cmd_get_cel_log },
	{ "get-event-interrupt-policy", .c_fn = cmd_get_event_interrupt_policy },
	{ "set-event-interrupt-policy", .c_fn = cmd_set_event_interrupt_policy },
	{ "get-timestamp", .c_fn = cmd_get_timestamp },
	{ "set-timestamp", .c_fn = cmd_set_timestamp },
	{ "get-alert-config", .c_fn = cmd_get_alert_config },
	{ "set-alert-config", .c_fn = cmd_set_alert_config },
	{ "get-health-info", .c_fn = cmd_get_health_info },
	{ "get-event-records", .c_fn = cmd_get_event_records },
	{ "get-ld-info", .c_fn = cmd_get_ld_info },
	{ "clear-event-records", .c_fn = cmd_clear_event_records },
	{ "ddr-info", .c_fn = cmd_ddr_info },
	{ "hct-start-stop-trigger", .c_fn = cmd_hct_start_stop_trigger },
	{ "hct-get-buffer-status", .c_fn = cmd_hct_get_buffer_status },
	{ "hct-enable", .c_fn = cmd_hct_enable },
	{ "ltmon-capture-clear", .c_fn = cmd_ltmon_capture_clear },
	{ "ltmon-capture", .c_fn = cmd_ltmon_capture },
	{ "ltmon-capture-freeze-and-restore", .c_fn = cmd_ltmon_capture_freeze_and_restore },
	{ "ltmon-l2r-count-dump", .c_fn = cmd_ltmon_l2r_count_dump },
	{ "ltmon-l2r-count-clear", .c_fn = cmd_ltmon_l2r_count_clear },
	{ "ltmon-basic-cfg", .c_fn = cmd_ltmon_basic_cfg },
	{ "ltmon-watch", .c_fn = cmd_ltmon_watch },
	{ "ltmon-capture-stat", .c_fn = cmd_ltmon_capture_stat },
	{ "ltmon-capture-log-dmp", .c_fn = cmd_ltmon_capture_log_dmp },
	{ "ltmon-capture-trigger", .c_fn = cmd_ltmon_capture_trigger },
	{ "ltmon-enable", .c_fn = cmd_ltmon_enable },
	{ "osa-os-type-trig-cfg", .c_fn = cmd_osa_os_type_trig_cfg },
	{ "osa-cap-ctrl", .c_fn = cmd_osa_cap_ctrl },
	{ "osa-cfg-dump", .c_fn = cmd_osa_cfg_dump },
	{ "osa-ana-op", .c_fn = cmd_osa_ana_op },
	{ "osa-status-query", .c_fn = cmd_osa_status_query },
	{ "osa-access-rel", .c_fn = cmd_osa_access_rel },
	{ "perfcnt-mta-ltif-set", .c_fn = cmd_perfcnt_mta_ltif_set },
	{ "perfcnt-mta-get", .c_fn = cmd_perfcnt_mta_get },
	{ "perfcnt-mta-latch-val-get", .c_fn = cmd_perfcnt_mta_latch_val_get },
	{ "perfcnt-mta-counter-clear", .c_fn = cmd_perfcnt_mta_counter_clear },
	{ "perfcnt-mta-cnt-val-latch", .c_fn = cmd_perfcnt_mta_cnt_val_latch },
	{ "perfcnt-mta-hif-set", .c_fn = cmd_perfcnt_mta_hif_set },
	{ "perfcnt-mta-hif-cfg-get", .c_fn = cmd_perfcnt_mta_hif_cfg_get },
	{ "perfcnt-mta-hif-latch-val-get", .c_fn = cmd_perfcnt_mta_hif_latch_val_get },
	{ "perfcnt-mta-hif-counter-clear", .c_fn = cmd_perfcnt_mta_hif_counter_clear },
	{ "perfcnt-mta-hif-cnt-val-latch", .c_fn = cmd_perfcnt_mta_hif_cnt_val_latch },
	{ "perfcnt-ddr-generic-select", .c_fn = cmd_perfcnt_ddr_generic_select },
	{ "err-inj-drs-poison", .c_fn = cmd_err_inj_drs_poison },
	{ "err-inj-drs-ecc", .c_fn = cmd_err_inj_drs_ecc },
	{ "err-inj-rxflit-crc", .c_fn = cmd_err_inj_rxflit_crc },
	{ "err-inj-txflit-crc", .c_fn = cmd_err_inj_txflit_crc },
	{ "err-inj-viral", .c_fn = cmd_err_inj_viral },
	{ "eh-eye-cap-run", .c_fn = cmd_eh_eye_cap_run },
	{ "eh-eye-cap-read", .c_fn = cmd_eh_eye_cap_read },
	{ "eh-adapt-get", .c_fn = cmd_eh_adapt_get },
	{ "eh-adapt-oneoff", .c_fn = cmd_eh_adapt_oneoff },
	{ "eh-adapt-force", .c_fn = cmd_eh_adapt_force },
	{ "hbo-status", .c_fn = cmd_hbo_status },
	{ "hbo-transfer-fw", .c_fn = cmd_hbo_transfer_fw },
	{ "hbo-activate-fw", .c_fn = cmd_hbo_activate_fw },
	{ "health-counters-clear", .c_fn = cmd_health_counters_clear },
	{ "health-counters-get", .c_fn = cmd_health_counters_get },
	{ "hct-get-plat-param", .c_fn = cmd_hct_get_plat_param },
};

int main(int argc, const char **argv)
{
	struct cxl_ctx *ctx;
	int rc;

	/* Look for flags.. */
	argv++;
	argc--;
	main_handle_options(&argv, &argc, cxl_usage_string, commands,
			ARRAY_SIZE(commands));

	if (argc > 0) {
		if (!prefixcmp(argv[0], "--"))
			argv[0] += 2;
	} else {
		/* The user didn't specify a command; give them help */
		printf("\n usage: %s\n\n", cxl_usage_string);
		printf("\n %s\n\n", cxl_more_info_string);
		goto out;
	}

	rc = cxl_new(&ctx);
	if (rc)
		goto out;
	main_handle_internal_command(argc, argv, ctx, commands,
			ARRAY_SIZE(commands), PROG_CXL);
	cxl_unref(ctx);
	fprintf(stderr, "Unknown command: '%s'\n", argv[0]);
out:
	return 1;
}
