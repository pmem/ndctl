// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2015-2020 Intel Corporation. All rights reserved.
#include <stdio.h>
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

#define fail() fprintf(stderr, "%s: failed at: %d\n", __func__, __LINE__)

static sigjmp_buf sj_env;
static int sig_count;
/* buf is global in order to avoid gcc memcpy optimization */
static void *buf;

static void sigbus_hdl(int sig, siginfo_t *siginfo, void *ptr)
{
	fprintf(stderr, "** Received a SIGBUS **\n");
	sig_count++;
	siglongjmp(sj_env, 1);
}

static int test_dax_read_err(int fd)
{
	void *base;
	int rc = 0;

	if (fd < 0) {
		fail();
		return -ENXIO;
	}

	if (posix_memalign(&buf, 4096, 4096) != 0)
		return -ENOMEM;

	base = mmap(NULL, 4096, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	if (base == MAP_FAILED) {
		perror("mmap");
		rc = -ENXIO;
		goto err_mmap;
	}

	if (sigsetjmp(sj_env, 1)) {
		if (sig_count == 1) {
			fprintf(stderr, "Failed to read from mapped file\n");
			free(buf);
			if (base) {
				if (munmap(base, 4096) < 0) {
					fail();
					return 1;
				}
			}
			return 1;
		}
		return sig_count;
	}

	/* read a page through DAX (should fail due to a bad block) */
	memcpy(buf, base, 4096);

 err_mmap:
	free(buf);
	return rc;
}

/* TODO: disabled till we get clear-on-write in the kernel */
#if 0
static int test_dax_write_clear(int fd)
{
	void *buf;
	int rc = 0;

	if (fd < 0) {
		fail();
		return -ENXIO;
	}

	if (posix_memalign(&buf, 4096, 4096) != 0)
		return -ENOMEM;
	memset(buf, 0, 4096);

	/*
	 * Attempt to write zeroes to the first page of the file using write()
	 * This should clear the pmem errors/bad blocks
	 */
	printf("Attempting to write\n");
	if (write(fd, buf, 4096) < 0)
		rc = errno;

	free(buf);
	return rc;
}
#endif

int main(int argc, char *argv[])
{
	int fd, rc;
	struct sigaction act;

	if (argc < 1)
		return -EINVAL;

	memset(&act, 0, sizeof(act));
	act.sa_sigaction = sigbus_hdl;
	act.sa_flags = SA_SIGINFO;

	if (sigaction(SIGBUS, &act, 0)) {
		fail();
		return 1;
	}

	fd = open(argv[1], O_RDWR | O_DIRECT);

	/* Start the test. First, we do an mmap-read, and expect it to fail */
	rc = test_dax_read_err(fd);
	if (rc == 0) {
		fprintf(stderr, "Expected read to fail, but it succeeded\n");
		rc = -ENXIO;
		goto out;
	}
	if (rc > 1) {
		fprintf(stderr, "Received a second SIGBUS, exiting.\n");
		rc = -ENXIO;
		goto out;
	}
	printf("  mmap-read failed as expected\n");
	rc = 0;

	/* Next, do a regular (O_DIRECT) write() */
	/* TODO: Disable this till we have clear-on-write in the kernel
	 * rc = test_dax_write_clear(fd);
	 *
	 * if (rc)
	 *	perror("write");
	 */

 out:
	if (fd >= 0)
		close(fd);
	return rc;
}
