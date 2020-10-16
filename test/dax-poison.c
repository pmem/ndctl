// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2018-2020 Intel Corporation. All rights reserved.
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <setjmp.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <linux/fs.h>
#include <test.h>
#include <util/size.h>
#include <stdbool.h>
#include <linux/version.h>

#define fail() fprintf(stderr, "%s: failed at: %d (%s)\n", \
	__func__, __LINE__, strerror(errno))

static sigjmp_buf sj_env;
static int sig_mcerr_ao, sig_mcerr_ar, sig_count;

static void sigbus_hdl(int sig, siginfo_t *si, void *ptr)
{
	switch (si->si_code) {
	case BUS_MCEERR_AO:
		fprintf(stderr, "%s: BUS_MCEERR_AO addr: %p len: %d\n",
			__func__, si->si_addr, 1 << si->si_addr_lsb);
		sig_mcerr_ao++;
		break;
	case BUS_MCEERR_AR:
		fprintf(stderr, "%s: BUS_MCEERR_AR addr: %p len: %d\n",
			__func__, si->si_addr, 1 << si->si_addr_lsb);
		sig_mcerr_ar++;
		break;
	default:
		sig_count++;
		break;
	}

	siglongjmp(sj_env, 1);
}

int test_dax_poison(struct ndctl_test *test, int dax_fd, unsigned long align,
		void *dax_addr, off_t offset, bool fsdax)
{
	unsigned char *addr = MAP_FAILED;
	struct sigaction act;
	unsigned x = x;
	void *buf;
	int rc;

	if (!ndctl_test_attempt(test, KERNEL_VERSION(4, 19, 0)))
		return 77;

	/*
	 * MADV_HWPOISON must be page aligned, and this routine assumes
	 * align is >= 8K
	 */
	if (align < SZ_2M)
		return 0;

	if (posix_memalign(&buf, 4096, 4096) != 0)
		return -ENOMEM;

	memset(&act, 0, sizeof(act));
	act.sa_sigaction = sigbus_hdl;
	act.sa_flags = SA_SIGINFO;

	if (sigaction(SIGBUS, &act, 0)) {
		fail();
		rc = -errno;
		goto out;
	}

	/* dirty the block on disk to bypass the default zero page */
	if (fsdax) {
		rc = pwrite(dax_fd, buf, 4096, offset + align / 2);
		if (rc < 4096) {
			fail();
			rc = -ENXIO;
			goto out;
		}
		fsync(dax_fd);
	}

	addr = mmap(dax_addr, 2*align, PROT_READ|PROT_WRITE,
			MAP_SHARED_VALIDATE|MAP_POPULATE|MAP_SYNC, dax_fd, offset);
	if (addr == MAP_FAILED) {
		fail();
		rc = -errno;
		goto out;
	}

	if (sigsetjmp(sj_env, 1)) {
		if (sig_mcerr_ar) {
			fprintf(stderr, "madvise triggered 'action required' sigbus\n");
			goto clear_error;
		} else if (sig_count) {
			fail();
			return -ENXIO;
		}
	}

	rc = madvise(addr + align / 2, 4096, MADV_HWPOISON);
	if (rc) {
		fail();
		rc = -errno;
		goto out;
	}

	/* clear the error */
clear_error:
	if (!sig_mcerr_ar) {
		fail();
		rc = -ENXIO;
		goto out;
	}

	if (!fsdax) {
		rc = 0;
		goto out;
	}

	rc = fallocate(dax_fd, FALLOC_FL_PUNCH_HOLE|FALLOC_FL_KEEP_SIZE,
			offset + align / 2, 4096);
	if (rc) {
		fail();
		rc = -errno;
		goto out;
	}

	rc = pwrite(dax_fd, buf, 4096, offset + align / 2);
	if (rc < 4096) {
		fail();
		rc = -ENXIO;
		goto out;
	}
	fsync(dax_fd);

	/* check that we can fault in the poison page */
	x = *(volatile unsigned *) addr + align / 2;
	rc = 0;

out:
	if (addr != MAP_FAILED)
		munmap(addr, 2 * align);
	free(buf);
	return rc;
}
