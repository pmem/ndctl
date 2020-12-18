// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (C) 2020 Intel Corporation. All rights reserved. */
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <syslog.h>
#include <string.h>
#include <signal.h>
#include <setjmp.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/types.h>
#include <util/size.h>
#include <linux/falloc.h>
#include <linux/version.h>
#include <ndctl/libndctl.h>
#include <ccan/array_size/array_size.h>

#include <builtin.h>
#include <test.h>

static sigjmp_buf sj_env;

static void sigbus(int sig, siginfo_t *siginfo, void *d)
{
	siglongjmp(sj_env, 1);
}

#define err(fmt, ...) \
	fprintf(stderr, "%s: " fmt, __func__, ##__VA_ARGS__)

static int test_devmem(int loglevel, struct ndctl_test *test,
		struct ndctl_ctx *ctx)
{
	void *buf;
	int fd, rc;
	struct sigaction act;
	unsigned long long resource;
	struct ndctl_namespace *ndns;

	ndctl_set_log_priority(ctx, loglevel);

	/* iostrict devmem started in kernel 4.5 */
	if (!ndctl_test_attempt(test, KERNEL_VERSION(4, 5, 0)))
		return 77;

	ndns = ndctl_get_test_dev(ctx);
	if (!ndns) {
		err("failed to find suitable namespace\n");
		return 77;
	}

	resource = ndctl_namespace_get_resource(ndns);
	if (resource == ULLONG_MAX) {
		err("failed to retrieve resource base\n");
		return 77;
	}

	rc = ndctl_namespace_disable(ndns);
	if (rc) {
		err("failed to disable namespace\n");
		return rc;
	}

	/* establish a devmem mapping of the namespace memory */
	fd = open("/dev/mem", O_RDWR);
	if (fd < 0) {
		err("failed to open /dev/mem: %s\n", strerror(errno));
		rc = -errno;
		goto out_devmem;
	}

	buf = mmap(NULL, SZ_2M, PROT_READ|PROT_WRITE, MAP_SHARED, fd, resource);
	if (buf == MAP_FAILED) {
		err("failed to map /dev/mem: %s\n", strerror(errno));
		rc = -errno;
		goto out_mmap;
	}

	/* populate and write, should not fail */
	memset(buf, 0, SZ_2M);

	memset(&act, 0, sizeof(act));
	act.sa_sigaction = sigbus;
	act.sa_flags = SA_SIGINFO;
	if (sigaction(SIGBUS, &act, 0)) {
		perror("sigaction");
		rc = EXIT_FAILURE;
		goto out_sigaction;
	}

	/* test fault due to devmem revocation */
	if (sigsetjmp(sj_env, 1)) {
		/* got sigbus, success */
		fprintf(stderr, "devmem revoked!\n");
		rc = 0;
		goto out_sigaction;
	}

	rc = ndctl_namespace_enable(ndns);
	if (rc) {
		err("failed to enable namespace\n");
		goto out_sigaction;
	}

	/* write, should sigbus */
	memset(buf, 0, SZ_2M);

	err("kernel failed to prevent write after namespace enabled\n");
	rc = -ENXIO;

out_sigaction:
	munmap(buf, SZ_2M);
out_mmap:
	close(fd);
out_devmem:
	if (ndctl_namespace_enable(ndns) != 0)
		err("failed to re-enable namespace\n");
	return rc;
}

int main(int argc, char *argv[])
{
	struct ndctl_test *test = ndctl_test_new(0);
	struct ndctl_ctx *ctx;
	int rc;

	if (!test) {
		fprintf(stderr, "failed to initialize test\n");
		return EXIT_FAILURE;
	}

	rc = ndctl_new(&ctx);
	if (rc < 0)
		return ndctl_test_result(test, rc);

	rc = test_devmem(LOG_DEBUG, test, ctx);
	ndctl_unref(ctx);
	return ndctl_test_result(test, rc);
}
