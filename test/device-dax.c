// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2015-2020 Intel Corporation. All rights reserved.
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
#include <daxctl/libdaxctl.h>
#include <ccan/array_size/array_size.h>

#include <builtin.h>
#include <test.h>

static sigjmp_buf sj_env;

static int create_namespace(int argc, const char **argv, void *ctx)
{
	builtin_xaction_namespace_reset();
	return cmd_create_namespace(argc, argv, ctx);
}

static int reset_device_dax(struct ndctl_namespace *ndns)
{
	struct ndctl_ctx *ctx = ndctl_namespace_get_ctx(ndns);
	const char *argv[] = {
		"__func__", "-v", "-m", "raw", "-f", "-e", "",
	};
	int argc = ARRAY_SIZE(argv);

	argv[argc - 1] = ndctl_namespace_get_devname(ndns);
	return create_namespace(argc, argv, ctx);
}

static int setup_device_dax(struct ndctl_namespace *ndns, unsigned long __align)
{
	struct ndctl_ctx *ctx = ndctl_namespace_get_ctx(ndns);
	char align[32];
	const char *argv[] = {
		"__func__", "-v", "-m", "devdax", "-M", "dev", "-f", "-a", align,
		"-e", "",
	};
	int argc = ARRAY_SIZE(argv);

	argv[argc - 1] = ndctl_namespace_get_devname(ndns);
	sprintf(align, "%ld", __align);
	return create_namespace(argc, argv, ctx);
}

static int setup_pmem_fsdax_mode(struct ndctl_namespace *ndns,
		unsigned long __align)
{
	struct ndctl_ctx *ctx = ndctl_namespace_get_ctx(ndns);
	char align[32];
	const char *argv[] = {
		"__func__", "-v", "-m", "fsdax", "-M", "dev", "-f", "-a",
		align, "-e", "",
	};
	int argc = ARRAY_SIZE(argv);

	argv[argc - 1] = ndctl_namespace_get_devname(ndns);
	sprintf(align, "%ld", __align);
	return create_namespace(argc, argv, ctx);
}

static void sigbus(int sig, siginfo_t *siginfo, void *d)
{
	siglongjmp(sj_env, 1);
}

#define VERIFY_SIZE(x) (x * 2)
#define VERIFY_BUF_SIZE 4096

/*
 * This timeout value derived from an Intel(R) Xeon(R) CPU E5-2690 v2 @
 * 3.00GHz where the loop, for the align == 2M case, completes in 7500us
 * when cached and 200ms when uncached.
 */
#define VERIFY_TIME(x) (suseconds_t) ((ALIGN(x, SZ_2M) / SZ_4K) * 60)

static int verify_data(struct daxctl_dev *dev, char *dax_buf,
		unsigned long align, int salt, struct ndctl_test *test)
{
	struct timeval tv1, tv2, tv_diff;
	unsigned long i;

	if (!ndctl_test_attempt(test, KERNEL_VERSION(4, 9, 0)))
		return 0;

	/* verify data and cache mode */
	gettimeofday(&tv1, NULL);
	for (i = 0; i < VERIFY_SIZE(align); i += VERIFY_BUF_SIZE) {
		unsigned int *verify = (unsigned int *) (dax_buf + i), j;

		for (j = 0; j < VERIFY_BUF_SIZE / sizeof(int); j++)
			if (verify[j] != salt + i + j)
				break;
		if (j < VERIFY_BUF_SIZE / sizeof(int)) {
			fprintf(stderr, "%s: @ %#lx expected %#x got %#lx\n",
					daxctl_dev_get_devname(dev), i,
					verify[j], salt + i + j);
			return -ENXIO;
		}
	}
	gettimeofday(&tv2, NULL);
	timersub(&tv2, &tv1, &tv_diff);
	tv_diff.tv_usec += tv_diff.tv_sec * 1000000;
	if (tv_diff.tv_usec > VERIFY_TIME(align)) {
		/*
		 * Checks whether the kernel correctly mapped the
		 * device-dax range as cacheable.
		 */
		fprintf(stderr, "%s: verify loop took too long usecs: %ld\n",
				daxctl_dev_get_devname(dev), tv_diff.tv_usec);
		return -ENXIO;
	}
	return 0;
}

static int __test_device_dax(unsigned long align, int loglevel,
		struct ndctl_test *test, struct ndctl_ctx *ctx)
{
	unsigned long i;
	struct sigaction act;
	struct ndctl_dax *dax;
	struct ndctl_pfn *pfn;
	struct daxctl_dev *dev;
	int fd, rc, *p, salt;
	struct ndctl_namespace *ndns;
	struct daxctl_region *dax_region;
	char *buf, path[100], data[VERIFY_BUF_SIZE];

	ndctl_set_log_priority(ctx, loglevel);

	ndns = ndctl_get_test_dev(ctx);
	if (!ndns) {
		fprintf(stderr, "%s: failed to find suitable namespace\n",
				__func__);
		return 77;
	}

	if (align > SZ_2M && !ndctl_test_attempt(test, KERNEL_VERSION(4, 11, 0)))
		return 77;

	if (!ndctl_test_attempt(test, KERNEL_VERSION(4, 7, 0)))
		return 77;

	/* setup up fsdax mode pmem device and seed with verification data */
	rc = setup_pmem_fsdax_mode(ndns, align);
	if (rc < 0 || !(pfn = ndctl_namespace_get_pfn(ndns))) {
		fprintf(stderr, "%s: failed device-dax setup\n",
				ndctl_namespace_get_devname(ndns));
		goto out;
	}

	sprintf(path, "/dev/%s", ndctl_pfn_get_block_device(pfn));
	fd = open(path, O_RDWR);
	if (fd < 0) {
		fprintf(stderr, "%s: failed to open pmem device\n", path);
		rc = -ENXIO;
		goto out;
	}

	srand(getpid());
	salt = rand();
	for (i = 0; i < VERIFY_SIZE(align); i += VERIFY_BUF_SIZE) {
		unsigned int *verify = (unsigned int *) data, j;

		for (j = 0; j < VERIFY_BUF_SIZE / sizeof(int); j++)
			verify[j] = salt + i + j;

		if (write(fd, data, sizeof(data)) != sizeof(data)) {
			fprintf(stderr, "%s: failed data setup\n",
					path);
			rc = -ENXIO;
			goto out;
		}
	}
	fsync(fd);
	close(fd);

	/* switch the namespace to device-dax mode and verify data via mmap */
	rc = setup_device_dax(ndns, align);
	if (rc < 0) {
		fprintf(stderr, "%s: failed device-dax setup\n",
				ndctl_namespace_get_devname(ndns));
		goto out;
	}

	dax = ndctl_namespace_get_dax(ndns);
	dax_region = ndctl_dax_get_daxctl_region(dax);
	dev = daxctl_dev_get_first(dax_region);
	if (!dev) {
		fprintf(stderr, "%s: failed to find device-dax instance\n",
				ndctl_namespace_get_devname(ndns));
		rc = -ENXIO;
		goto out;
	}

	sprintf(path, "/dev/%s", daxctl_dev_get_devname(dev));
	fd = open(path, O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "%s: failed to open(O_RDONLY) device-dax instance\n",
				daxctl_dev_get_devname(dev));
		rc = -ENXIO;
		goto out;
	}

	buf = mmap(NULL, VERIFY_SIZE(align), PROT_READ, MAP_PRIVATE, fd, 0);
	if (buf != MAP_FAILED) {
		fprintf(stderr, "%s: expected MAP_PRIVATE failure\n", path);
		rc = -ENXIO;
		goto out;
	}

	buf = mmap(NULL, VERIFY_SIZE(align), PROT_READ, MAP_SHARED, fd, 0);
	if (buf == MAP_FAILED) {
		fprintf(stderr, "%s: expected MAP_SHARED success\n", path);
		return -ENXIO;
	}

	rc = verify_data(dev, buf, align, salt, test);
	if (rc)
		goto out;

	close(fd);
	munmap(buf, VERIFY_SIZE(align));

	/*
	 * Prior to 4.8-final these tests cause crashes, or are
	 * otherwise not supported.
	 */
	if (ndctl_test_attempt(test, KERNEL_VERSION(4, 9, 0))) {
		static const bool devdax = false;
		int fd2;

		fd = open(path, O_RDWR);
		if (fd < 0) {
			fprintf(stderr, "%s: failed to open for direct-io test\n",
					daxctl_dev_get_devname(dev));
			rc = -ENXIO;
			goto out;
		}
		rc = test_dax_directio(fd, align, NULL, 0);
		if (rc) {
			fprintf(stderr, "%s: failed dax direct-i/o\n",
					ndctl_namespace_get_devname(ndns));
			goto out;
		}

		rc = test_dax_remap(test, fd, align, NULL, 0, devdax);
		if (rc) {
			fprintf(stderr, "%s: failed dax remap\n",
					ndctl_namespace_get_devname(ndns));
			goto out;
		}
		close(fd);

		fprintf(stderr, "%s: test dax poison\n",
				ndctl_namespace_get_devname(ndns));

		fd = open(path, O_RDWR);
		if (fd < 0) {
			fprintf(stderr, "%s: failed to open for poison test\n",
					daxctl_dev_get_devname(dev));
			rc = -ENXIO;
			goto out;
		}

		rc = test_dax_poison(test, fd, align, NULL, 0, devdax);
		if (rc) {
			fprintf(stderr, "%s: failed dax poison\n",
					ndctl_namespace_get_devname(ndns));
			goto out;
		}
		close(fd);

		fd2 = open("/proc/self/smaps", O_RDONLY);
		if (fd2 < 0) {
			fprintf(stderr, "%s: failed smaps open\n",
					ndctl_namespace_get_devname(ndns));
			rc = -ENXIO;
			goto out;
		}

		do {
			rc = read(fd2, data, sizeof(data));
		} while (rc > 0);

		if (rc) {
			fprintf(stderr, "%s: failed smaps retrieval\n",
					ndctl_namespace_get_devname(ndns));
			rc = -ENXIO;
			goto out;
		}
	}

	/* establish a writable mapping */
	fd = open(path, O_RDWR);
	if (fd < 0) {
		fprintf(stderr, "%s: failed to open(O_RDWR) device-dax instance\n",
				daxctl_dev_get_devname(dev));
		rc = -ENXIO;
		goto out;
	}

	buf = mmap(NULL, VERIFY_SIZE(align), PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	if (buf == MAP_FAILED) {
		fprintf(stderr, "%s: expected PROT_WRITE + MAP_SHARED success\n",
				path);
		return -ENXIO;
	}

	rc = reset_device_dax(ndns);
	if (rc < 0) {
		fprintf(stderr, "%s: failed to reset device-dax instance\n",
				ndctl_namespace_get_devname(ndns));
		goto out;
	}

	memset(&act, 0, sizeof(act));
	act.sa_sigaction = sigbus;
	act.sa_flags = SA_SIGINFO;
	if (sigaction(SIGBUS, &act, 0)) {
		perror("sigaction");
		rc = EXIT_FAILURE;
		goto out;
	}

	/* test fault after device-dax instance disabled */
	if (sigsetjmp(sj_env, 1)) {
		/* got sigbus, success */
		close(fd);
		rc = 0;
		goto out;
	}

	rc = EXIT_SUCCESS;
	p = (int *) (buf + align);
	*p = 0xff;
	if (ndctl_test_attempt(test, KERNEL_VERSION(4, 9, 0))) {
		/* after 4.9 this test will properly get sigbus above */
		rc = EXIT_FAILURE;
		fprintf(stderr, "%s: failed to unmap after reset\n",
				daxctl_dev_get_devname(dev));
	}
	close(fd);
 out:
	reset_device_dax(ndns);
	return rc;
}

static int test_device_dax(int loglevel, struct ndctl_test *test,
		struct ndctl_ctx *ctx)
{
	unsigned long i, aligns[] = { SZ_4K, SZ_2M, SZ_1G };
	int rc;

	for (i = 0; i < ARRAY_SIZE(aligns); i++) {
		rc = __test_device_dax(aligns[i], loglevel, test, ctx);
		if (rc && rc != 77)
			break;
	}

	return rc;
}

int __attribute__((weak)) main(int argc, char *argv[])
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

	rc = test_device_dax(LOG_DEBUG, test, ctx);
	ndctl_unref(ctx);
	return ndctl_test_result(test, rc);
}
