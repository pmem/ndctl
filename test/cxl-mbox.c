// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2026 Nvidia Corporation. All rights reserved.
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <syslog.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <cxl/libcxl.h>
#include <cxl/cxl_mem.h>

static const char provider[] = "cxl_test";

/*
 * The cxl_test mock advertises a 4 KiB (SZ_4K) mailbox payload_size and
 * IDENTIFY returns a full struct cxl_mbox_identify. Post-fix the kernel
 * clamps the output allocation to payload_size and copies that many bytes
 * back into out.payload, so the buffer must be >= payload_size. 64 KiB is
 * comfortably above the mock's 4 KiB payload.
 */
#define OUT_BUF_SIZE	(64 * 1024)

/*
 * Regression for the unbounded kvzalloc() in cxl_mbox_cmd_ctor() driven by a
 * huge CXL_MEM_SEND_COMMAND out.size. The kernel fix CLAMPS the output
 * allocation to the mailbox payload_size; it does not reject the request.
 * Assert the ioctl SUCCEEDS (no -ENOMEM) -- do NOT assert -EINVAL.
 */
static int test_cxl_mbox_huge_out_size(struct cxl_memdev *memdev)
{
	struct cxl_send_command c = { 0 };
	const char *devname;
	char path[256];
	void *buf;
	int fd, rc;

	devname = cxl_memdev_get_devname(memdev);
	if (!devname)
		return -ENODEV;

	snprintf(path, sizeof(path), "/dev/cxl/%s", devname);

	fd = open(path, O_RDWR);
	if (fd < 0) {
		if (errno == ENOENT || errno == ENODEV)
			return -ENODEV;
		fprintf(stderr, "failed to open %s: %s\n", path,
			strerror(errno));
		return -errno;
	}

	buf = calloc(1, OUT_BUF_SIZE);
	if (!buf) {
		rc = -ENOMEM;
		goto out;
	}

	c.id = CXL_MEM_COMMAND_ID_IDENTIFY;
	/*
	 * 0x80000000 (2^31, > INT_MAX) is the proven reproducer that trips the
	 * size > INT_MAX kvmalloc() WARN. out.size is __s32 in this vendored
	 * UAPI; cast to avoid -Woverflow, the kernel reads the same 4 bytes
	 * (kernel UAPI declares it __u32).
	 */
	c.out.size = (typeof(c.out.size))0x80000000U;
	c.out.payload = (__u64)(uintptr_t)buf;

	rc = ioctl(fd, CXL_MEM_SEND_COMMAND, &c);

	/* Pass iff the kernel clamped (success), not rejected. */
	if (rc == 0 && c.retval == 0) {
		rc = 0;
		goto out;
	}

	fprintf(stderr,
		"CXL_MEM_SEND_COMMAND huge out.size mishandled: rc=%d errno=%d retval=%u\n",
		rc, errno, c.retval);
	rc = -ENXIO;

out:
	free(buf);
	close(fd);
	return rc;
}

static int test_cxl_mbox(struct cxl_ctx *ctx, struct cxl_bus *bus)
{
	struct cxl_memdev *memdev;

	cxl_memdev_foreach(ctx, memdev) {
		if (cxl_memdev_get_bus(memdev) != bus)
			continue;
		return test_cxl_mbox_huge_out_size(memdev);
	}

	return -ENODEV;
}

int main(int argc, char *argv[])
{
	struct cxl_ctx *ctx;
	struct cxl_bus *bus;
	int rc;

	rc = cxl_new(&ctx);
	if (rc < 0)
		return rc;

	cxl_set_log_priority(ctx, LOG_DEBUG);

	bus = cxl_bus_get_by_provider(ctx, provider);
	if (!bus) {
		fprintf(stderr, "%s: unable to find bus (%s)\n",
			argv[0], provider);
		rc = -ENODEV;
		goto out;
	}

	rc = test_cxl_mbox(ctx, bus);

out:
	cxl_unref(ctx);
	return rc;
}
