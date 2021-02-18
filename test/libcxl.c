// SPDX-License-Identifier: LGPL-2.1
/* Copyright (C) 2021, Intel Corporation. All rights reserved. */
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include <limits.h>
#include <syslog.h>
#include <libkmod.h>
#include <sys/wait.h>
#include <uuid/uuid.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <linux/version.h>

#include <util/size.h>
#include <util/hexdump.h>
#include <ccan/short_types/short_types.h>
#include <ccan/array_size/array_size.h>
#include <ccan/endian/endian.h>
#include <cxl/libcxl.h>
#include <cxl/cxl_mem.h>
#include <test.h>
#include "libcxl-expect.h"

#define TEST_SKIP 77

const char *mod_list[] = {
	"cxl_pci",
	"cxl_acpi",
	"cxl_core",
};

static int test_cxl_presence(struct cxl_ctx *ctx)
{
	struct cxl_memdev *memdev;
	int count = 0;

	cxl_memdev_foreach(ctx, memdev)
		count++;

	if (count == 0) {
		fprintf(stderr, "%s: no cxl memdevs found\n", __func__);
		return TEST_SKIP;
	}

	return 0;
}

/*
 * Only continue with tests if all CXL devices in the system are qemu-emulated
 * 'fake' devices. For now, use the firmware_version to check for this. Later,
 * this might need to be changed to a vendor specific command.
 *
 * TODO: Change this to produce a list of devices that are safe to run tests
 * against, and only run subsequent tests on this list. That will allow devices
 * from other, non-emulated sources to be present in the system, and still run
 * these unit tests safely.
 */
static int test_cxl_emulation_env(struct cxl_ctx *ctx)
{
	struct cxl_memdev *memdev;

	cxl_memdev_foreach(ctx, memdev) {
		const char *fw;

		fw = cxl_memdev_get_firmware_verison(memdev);
		if (!fw)
			return -ENXIO;
		if (strcmp(fw, EXPECT_FW_VER) != 0) {
			fprintf(stderr,
				"%s: found non-emulation device, aborting\n",
				__func__);
			return TEST_SKIP;
		}
	}
	return 0;
}

static int test_cxl_modules(struct cxl_ctx *ctx)
{
	int rc;
	unsigned int i;
	const char *name;
	struct kmod_module *mod;
	struct kmod_ctx *kmod_ctx;

	kmod_ctx = kmod_new(NULL, NULL);
	if (!kmod_ctx)
		return -ENXIO;
	kmod_set_log_priority(kmod_ctx, LOG_DEBUG);

	/* test module removal */
	for (i = 0; i < ARRAY_SIZE(mod_list); i++) {
		int state;

		name = mod_list[i];

		rc = kmod_module_new_from_name(kmod_ctx, name, &mod);
		if (rc) {
			fprintf(stderr, "%s: %s.ko: missing\n", __func__, name);
			break;
		}

		state = kmod_module_get_initstate(mod);
		if (state == KMOD_MODULE_LIVE) {
			rc = kmod_module_remove_module(mod, 0);
			if (rc) {
				fprintf(stderr,
					"%s: %s.ko: failed to remove: %d\n",
					__func__, name, rc);
				break;
			}
		} else if (state == KMOD_MODULE_BUILTIN) {
			fprintf(stderr,
				"%s: %s is builtin, skipping module removal test\n",
				__func__, name);
		} else {
			fprintf(stderr,
				"%s: warning: %s.ko: unexpected state (%d), trying to continue\n",
				__func__, name, state);
		}
	}

	if (rc)
		goto out;

	/* test module insertion */
	for (i = 0; i < ARRAY_SIZE(mod_list); i++) {
		name = mod_list[i];
		rc = kmod_module_new_from_name(kmod_ctx, name, &mod);
		if (rc) {
			fprintf(stderr, "%s: %s.ko: missing\n", __func__, name);
			break;
		}

		rc = kmod_module_probe_insert_module(mod,
				KMOD_PROBE_APPLY_BLACKLIST,
				NULL, NULL, NULL, NULL);
	}

out:
	kmod_unref(kmod_ctx);
	return rc;
}

#define expect(c, name, field, expect) \
do { \
	if (cxl_cmd_##name##_get_##field(c) != expect) { \
		fprintf(stderr, \
			"%s: %s: " #field " mismatch\n", \
			__func__, cxl_cmd_get_devname(c)); \
		cxl_cmd_unref(cmd); \
		return -ENXIO; \
	} \
} while (0)

static int test_cxl_cmd_identify(struct cxl_ctx *ctx)
{
	struct cxl_memdev *memdev;
	struct cxl_cmd *cmd;
	int rc;

	cxl_memdev_foreach(ctx, memdev) {
		char fw_rev[0x10];

		cmd = cxl_cmd_new_identify(memdev);
		if (!cmd)
			return -ENOMEM;
		rc = cxl_cmd_submit(cmd);
		if (rc < 0) {
			fprintf(stderr, "%s: %s: cmd submission failed: %s\n",
				__func__, cxl_memdev_get_devname(memdev),
				strerror(-rc));
			goto out_fail;
		}
		rc = cxl_cmd_get_mbox_status(cmd);
		if (rc) {
			fprintf(stderr,
				"%s: %s: cmd failed with firmware status: %d\n",
				__func__, cxl_memdev_get_devname(memdev), rc);
			rc = -ENXIO;
			goto out_fail;
		}

		rc = cxl_cmd_identify_get_fw_rev(cmd, fw_rev, 0x10);
		if (rc)
			goto out_fail;
		if (strncmp(fw_rev, EXPECT_FW_VER, 0x10) != 0) {
			fprintf(stderr,
				"%s: fw_rev mismatch. Expected %s, got %s\n",
				__func__, EXPECT_FW_VER, fw_rev);
			rc = -ENXIO;
			goto out_fail;
		}

		expect(cmd, identify, lsa_size, EXPECT_CMD_IDENTIFY_LSA_SIZE);
		expect(cmd, identify, partition_align,
			EXPECT_CMD_IDENTIFY_PARTITION_ALIGN);
		cxl_cmd_unref(cmd);
	}
	return 0;

out_fail:
	cxl_cmd_unref(cmd);
	return rc;
}

struct cmd_fuzzer {
	struct cxl_cmd *(*new_fn)(struct cxl_memdev *memdev);
	int in;		/* in size to set in cmd (INT_MAX = don't change) */
	int out;	/* out size to set in cmd (INT_MAX = don't change) */
	int e_out;	/* expected out size returned (INT_MAX = don't check) */
	int e_rc;	/* expected ioctl return (INT_MAX = don't check) */
	int e_hwrc;	/* expected 'mbox_status' (INT_MAX = don't check) */
} fuzz_set[] = {
	{ cxl_cmd_new_identify, INT_MAX, INT_MAX, 67, 0, 0 },
	{ cxl_cmd_new_identify, 64, INT_MAX, INT_MAX, -ENOMEM, INT_MAX },
	{ cxl_cmd_new_identify, INT_MAX, 1024, 67, 0, INT_MAX },
	{ cxl_cmd_new_identify, INT_MAX, 16, INT_MAX, -ENOMEM, INT_MAX },
};

static int do_one_cmd_size_test(struct cxl_memdev *memdev,
		struct cmd_fuzzer *test)
{
	const char *devname = cxl_memdev_get_devname(memdev);
	struct cxl_cmd *cmd;
	int rc;

	cmd = test->new_fn(memdev);
	if (!cmd)
		return -ENOMEM;

	if (test->in != INT_MAX) {
		rc = cxl_cmd_set_input_payload(cmd, NULL, test->in);
		if (rc) {
			fprintf(stderr,
				"%s: %s: failed to set in.size (%d): %s\n",
				__func__, devname, test->in, strerror(-rc));
			goto out_fail;
		}
	}
	if (test->out != INT_MAX) {
		rc = cxl_cmd_set_output_payload(cmd, NULL, test->out);
		if (rc) {
			fprintf(stderr,
				"%s: %s: failed to set out.size (%d): %s\n",
				__func__, devname, test->out, strerror(-rc));
			goto out_fail;
		}
	}

	rc = cxl_cmd_submit(cmd);
	if (test->e_rc != INT_MAX && rc != test->e_rc) {
		fprintf(stderr, "%s: %s: expected cmd rc %d, got %d\n",
			__func__, devname, test->e_rc, rc);
		rc = -ENXIO;
		goto out_fail;
	}

	rc = cxl_cmd_get_out_size(cmd);
	if (test->e_out != INT_MAX && rc != test->e_out) {
		fprintf(stderr, "%s: %s: expected response out.size %d, got %d\n",
			__func__, devname, test->e_out, rc);
		rc = -ENXIO;
		goto out_fail;
	}

	rc = cxl_cmd_get_mbox_status(cmd);
	if (test->e_hwrc != INT_MAX && rc != test->e_hwrc) {
		fprintf(stderr, "%s: %s: expected firmware status %d, got %d\n",
			__func__, devname, test->e_hwrc, rc);
		rc = -ENXIO;
		goto out_fail;
	}
	return 0;

out_fail:
	cxl_cmd_unref(cmd);
	return rc;

}

static void print_fuzz_test_status(struct cmd_fuzzer *t, const char *devname,
		unsigned long idx, const char *msg)
{
	fprintf(stderr,
		"%s: fuzz_set[%lu]: in: %d, out %d, e_out: %d, e_rc: %d, e_hwrc: %d, result: %s\n",
		devname, idx,
		(t->in == INT_MAX) ? -1 : t->in,
		(t->out == INT_MAX) ? -1 : t->out,
		(t->e_out == INT_MAX) ? -1 : t->e_out,
		(t->e_rc == INT_MAX) ? -1 : t->e_rc,
		(t->e_hwrc == INT_MAX) ? -1 : t->e_hwrc,
		msg);
}

static int test_cxl_cmd_fuzz_sizes(struct cxl_ctx *ctx)
{
	struct cxl_memdev *memdev;
	unsigned long i;
	int rc;

	cxl_memdev_foreach(ctx, memdev) {
		const char *devname = cxl_memdev_get_devname(memdev);

		for (i = 0; i < ARRAY_SIZE(fuzz_set); i++) {
			rc = do_one_cmd_size_test(memdev, &fuzz_set[i]);
			if (rc) {
				print_fuzz_test_status(&fuzz_set[i], devname,
					i, "FAIL");
				return rc;
			}
			print_fuzz_test_status(&fuzz_set[i], devname, i, "OK");
		}
	}
	return 0;
}

static int debugfs_write_raw_flag(char *str)
{
	char *path = "/sys/kernel/debug/cxl/mbox/raw_allow_all";
	int fd = open(path, O_WRONLY|O_CLOEXEC);
	int n, len = strlen(str) + 1, rc;

	if (fd < 0)
		return -errno;

	n = write(fd, str, len);
	rc = -errno;
	close(fd);
	if (n < len) {
		fprintf(stderr, "failed to write %s to %s: %s\n", str, path,
					strerror(errno));
		return rc;
	}
	return 0;
}

static char *test_lsa_data = "LIBCXL_TEST LSA DATA 01";
static int lsa_size = EXPECT_CMD_IDENTIFY_LSA_SIZE;

static int test_set_lsa(struct cxl_memdev *memdev)
{
	int data_size = strlen(test_lsa_data) + 1;
	struct cxl_cmd *cmd;
	struct {
		le32 offset;
		le32 rsvd;
		unsigned char data[lsa_size];
	} __attribute__((packed)) set_lsa;
	int rc;

	set_lsa.offset = cpu_to_le32(0);
	set_lsa.rsvd = cpu_to_le32(0);
	memcpy(set_lsa.data, test_lsa_data, data_size);

	cmd = cxl_cmd_new_raw(memdev, 0x4103);
	if (!cmd)
		return -ENOMEM;

	rc = cxl_cmd_set_input_payload(cmd, &set_lsa, sizeof(set_lsa));
	if (rc) {
		fprintf(stderr, "%s: %s: cmd setup failed: %s\n",
			__func__, cxl_memdev_get_devname(memdev),
			strerror(-rc));
		goto out_fail;
	}

	rc = debugfs_write_raw_flag("Y");
	if (rc < 0)
		return rc;

	rc = cxl_cmd_submit(cmd);
	if (rc < 0)
		fprintf(stderr, "%s: %s: cmd submission failed: %s\n",
			__func__, cxl_memdev_get_devname(memdev),
			strerror(-rc));

	rc = cxl_cmd_get_mbox_status(cmd);
	if (rc != 0) {
		fprintf(stderr, "%s: %s: firmware status: %d\n",
			__func__, cxl_memdev_get_devname(memdev), rc);
		return -ENXIO;
	}

	if(debugfs_write_raw_flag("N") < 0)
		fprintf(stderr, "%s: %s: failed to restore raw flag\n",
			__func__, cxl_memdev_get_devname(memdev));

out_fail:
	cxl_cmd_unref(cmd);
	return rc;
}

static int test_cxl_cmd_lsa(struct cxl_ctx *ctx)
{
	int data_size = strlen(test_lsa_data) + 1;
	struct cxl_memdev *memdev;
	struct cxl_cmd *cmd;
	unsigned char *buf;
	int rc;

	cxl_memdev_foreach(ctx, memdev) {
		rc = test_set_lsa(memdev);
		if (rc)
			return rc;

		cmd = cxl_cmd_new_get_lsa(memdev, 0, lsa_size);
		if (!cmd)
			return -ENOMEM;
		rc = cxl_cmd_set_output_payload(cmd, NULL, lsa_size);
		if (rc) {
			fprintf(stderr, "%s: output buffer allocation: %s\n",
				__func__, strerror(-rc));
			return rc;
		}
		rc = cxl_cmd_submit(cmd);
		if (rc < 0) {
			fprintf(stderr, "%s: %s: cmd submission failed: %s\n",
				__func__, cxl_memdev_get_devname(memdev),
				strerror(-rc));
			goto out_fail;
		}

		rc = cxl_cmd_get_mbox_status(cmd);
		if (rc != 0) {
			fprintf(stderr, "%s: %s: firmware status: %d\n",
				__func__, cxl_memdev_get_devname(memdev), rc);
			return -ENXIO;
		}

		buf = cxl_cmd_get_lsa_get_payload(cmd);
		if (rc < 0)
			goto out_fail;

		if (memcmp(buf, test_lsa_data, data_size) != 0) {
			fprintf(stderr, "%s: LSA data mismatch.\n", __func__);
			hex_dump_buf(buf, data_size);
			rc = -EIO;
			goto out_fail;
		}
		cxl_cmd_unref(cmd);
	}
	return 0;

out_fail:
	cxl_cmd_unref(cmd);
	return rc;
}

typedef int (*do_test_fn)(struct cxl_ctx *ctx);

static do_test_fn do_test[] = {
	test_cxl_modules,
	test_cxl_presence,
	test_cxl_emulation_env,
	test_cxl_cmd_identify,
	test_cxl_cmd_lsa,
	test_cxl_cmd_fuzz_sizes,
};

static int test_libcxl(int loglevel, struct test_ctx *test, struct cxl_ctx *ctx)
{
	unsigned int i;
	int err, result = EXIT_FAILURE;

	if (!test_attempt(test, KERNEL_VERSION(5, 12, 0)))
		return 77;

	cxl_set_log_priority(ctx, loglevel);
	cxl_set_private_data(ctx, test);

	for (i = 0; i < ARRAY_SIZE(do_test); i++) {
		err = do_test[i](ctx);
		if (err < 0) {
			fprintf(stderr, "test[%d] failed: %d\n", i, err);
			break;
		} else if (err == TEST_SKIP) {
			fprintf(stderr, "test[%d]: SKIP\n", i);
			test_skip(test);
			result = TEST_SKIP;
			break;
		}
		fprintf(stderr, "test[%d]: PASS\n", i);
	}

	if (i >= ARRAY_SIZE(do_test))
		result = EXIT_SUCCESS;
	return result;
}

int __attribute__((weak)) main(int argc, char *argv[])
{
	struct test_ctx *test = test_new(0);
	struct cxl_ctx *ctx;
	int rc;

	if (!test) {
		fprintf(stderr, "failed to initialize test\n");
		return EXIT_FAILURE;
	}

	rc = cxl_new(&ctx);
	if (rc)
		return test_result(test, rc);
	rc = test_libcxl(LOG_DEBUG, test, ctx);
	cxl_unref(ctx);
	return test_result(test, rc);
}
