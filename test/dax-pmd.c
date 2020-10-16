// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2015-2020 Intel Corporation. All rights reserved.
#include <stdio.h>
#include <unistd.h>
#include <setjmp.h>
#include <sys/mman.h>
#include <linux/mman.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <linux/fs.h>
#include <test.h>
#include <util/size.h>
#include <linux/fiemap.h>
#include <linux/version.h>

#define NUM_EXTENTS 5
#define fail() fprintf(stderr, "%s: failed at: %d (%s)\n", \
	__func__, __LINE__, strerror(errno))
#define faili(i) fprintf(stderr, "%s: failed at: %d: %d (%s)\n", \
	__func__, __LINE__, i, strerror(errno))
#define TEST_FILE "test_dax_data"

#define REGION_MEM_SIZE 4096*4
#define REGION_PM_SIZE        4096*512
#define REMAP_SIZE      4096

static sigjmp_buf sj_env;

static void sigbus(int sig, siginfo_t *siginfo, void *d)
{
	siglongjmp(sj_env, 1);
}

int test_dax_remap(struct ndctl_test *test, int dax_fd, unsigned long align, void *dax_addr,
		off_t offset, bool fsdax)
{
	void *anon, *remap, *addr;
	struct sigaction act;
	int rc, val;

	if ((fsdax || align == SZ_2M) && !ndctl_test_attempt(test, KERNEL_VERSION(5, 8, 0))) {
		/* kernel's prior to 5.8 may crash on this test */
		fprintf(stderr, "%s: SKIP mremap() test\n", __func__);
		return 0;
	}

	anon = mmap(NULL, REGION_MEM_SIZE, PROT_READ|PROT_WRITE,
			MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);

	addr = mmap(dax_addr, 2*align,
			PROT_READ|PROT_WRITE, MAP_SHARED, dax_fd, offset);

	fprintf(stderr, "%s: addr: %p size: %#lx\n", __func__, addr, 2*align);

	if (addr == MAP_FAILED) {
		rc = -errno;
		faili(0);
		return rc;
	}

	memset(anon, 'a', REGION_MEM_SIZE);
	memset(addr, 'i', align*2);

	remap = mremap(addr, REMAP_SIZE, REMAP_SIZE, MREMAP_MAYMOVE|MREMAP_FIXED, anon);

	if (remap != anon) {
		rc = -ENXIO;
		perror("mremap");
		faili(1);
		return rc;
	}

	fprintf(stderr, "%s: addr: %p size: %#x\n", __func__, remap, REMAP_SIZE);

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
		if (!fsdax && align > SZ_4K) {
			fprintf(stderr, "got expected SIGBUS after mremap() of device-dax\n");
			rc = 0;
		} else {
			fprintf(stderr, "unpexpected SIGBUS after mremap()\n");
			rc = -EIO;
		}
		goto out;
	}

	*(int *) anon = 0xAA;
	val = *(int *) anon;

	if (val != 0xAA) {
		faili(2);
		return -ENXIO;
	}

	rc = 0;
out:
	return rc;
}

int test_dax_directio(int dax_fd, unsigned long align, void *dax_addr, off_t offset)
{
	int i, rc = -ENXIO;
	void *buf;

	if (posix_memalign(&buf, 4096, 4096) != 0)
		return -ENOMEM;

	for (i = 0; i < 5; i++) {
		unsigned long flags;
		void *addr;
		int fd2;

		if (dax_fd >= 0)
			flags = MAP_SHARED;
		else {
			/* hugetlbfs instead of device-dax */
			const char *base = "/sys/kernel/mm/hugepages";
			FILE *f_nrhuge;
			char path[256];

			flags = MAP_SHARED | MAP_ANONYMOUS;
			if (align >= SZ_2M) {
				char setting[] = { "2\n" };

				sprintf(path, "%s/hugepages-%ldkB/nr_hugepages",
						base, align / 1024);
				f_nrhuge = fopen(path, "r+");
				if (!f_nrhuge) {
					rc = -errno;
					faili(i);
					return rc;
				}
				if (fwrite(setting, sizeof(setting), 1, f_nrhuge) != 1) {
					rc = -errno;
					faili(i);
					fclose(f_nrhuge);
					return rc;
				}
				fclose(f_nrhuge);

				/* FIXME: support non-x86 page sizes */
				if (align > SZ_2M)
					flags |= MAP_HUGETLB | MAP_HUGE_1GB;
				else
					flags |= MAP_HUGETLB | MAP_HUGE_2MB;
			}
		}
		addr = mmap(dax_addr, 2*align,
				PROT_READ|PROT_WRITE, flags, dax_fd, offset);

		if (addr == MAP_FAILED) {
			rc = -errno;
			faili(i);
			break;
		}
		rc = -ENXIO;

		fd2 = open(TEST_FILE, O_CREAT|O_TRUNC|O_DIRECT|O_RDWR,
				DEFFILEMODE);
		if (fd2 < 0) {
			faili(i);
			munmap(addr, 2*align);
			break;
		}

		fprintf(stderr, "%s: test: %d\n", __func__, i);
		rc = 0;
		switch (i) {
		case 0: /* test O_DIRECT read of unfaulted address */
			if (write(fd2, addr, 4096) != 4096) {
				faili(i);
				rc = -ENXIO;
			}

			/*
			 * test O_DIRECT write of pre-faulted read-only
			 * address
			 */
			if (pread(fd2, addr, 4096, 0) != 4096) {
				faili(i);
				rc = -ENXIO;
			}
			break;
		case 1: /* test O_DIRECT of pre-faulted address */
			sprintf(addr, "odirect data");
			if (pwrite(fd2, addr, 4096, 0) != 4096) {
				faili(i);
				rc = -ENXIO;
			}
			((char *) buf)[0] = 0;
			if (pread(fd2, buf, 4096, 0) != 4096) {
				faili(i);
				rc = -ENXIO;
			}
			if (strcmp(buf, "odirect data") != 0) {
				faili(i);
				rc = -ENXIO;
			}
			break;
		case 2: /* fork with pre-faulted pmd */
			sprintf(addr, "fork data");
			rc = fork();
			if (rc == 0) {
				/* child */
				if (strcmp(addr, "fork data") == 0)
					exit(EXIT_SUCCESS);
				else
					exit(EXIT_FAILURE);
			} else if (rc > 0) {
				/* parent */
				wait(&rc);
				rc = WEXITSTATUS(rc);
				if (rc != EXIT_SUCCESS) {
					faili(i);
				}
			} else
				faili(i);
			break;
		case 3: /* convert ro mapping to rw */
			rc = *(volatile int *) addr;
			*(volatile int *) addr = rc;
			rc = 0;
			break;
		case 4: /* test O_DIRECT write of unfaulted address */
			sprintf(buf, "O_DIRECT write of unfaulted address\n");
			if (pwrite(fd2, buf, 4096, 0) < 4096) {
				faili(i);
				rc = -ENXIO;
				break;
			}

			if (pread(fd2, addr, 4096, 0) < 4096) {
				faili(i);
				rc = -ENXIO;
				break;
			}
			rc = 0;
			break;
		default:
			faili(i);
			rc = -ENXIO;
			break;
		}

		munmap(addr, 2*align);
		addr = MAP_FAILED;
		unlink(TEST_FILE);
		close(fd2);
		fd2 = -1;
		if (rc)
			break;
	}

	free(buf);
	return rc;
}

/* test_pmd assumes that fd references a pre-allocated + dax-capable file */
static int test_pmd(struct ndctl_test *test, int fd)
{
	unsigned long long m_align, p_align, pmd_off;
	static const bool fsdax = true;
	struct fiemap_extent *ext;
	void *base, *pmd_addr;
	struct fiemap *map;
	int rc = -ENXIO;
	unsigned long i;

	if (fd < 0) {
		fail();
		return -ENXIO;
	}

	map = calloc(1, sizeof(struct fiemap)
			+ sizeof(struct fiemap_extent) * NUM_EXTENTS);
	if (!map) {
		fail();
		return -ENXIO;
	}

	base = mmap(NULL, 4*HPAGE_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	if (base == MAP_FAILED) {
		fail();
		goto err_mmap;
	}
	munmap(base, 4*HPAGE_SIZE);

	map->fm_start = 0;
	map->fm_length = -1;
	map->fm_extent_count = NUM_EXTENTS;
	rc = ioctl(fd, FS_IOC_FIEMAP, map);
	if (rc < 0) {
		fail();
		goto err_extent;
	}

	for (i = 0; i < map->fm_mapped_extents; i++) {
		ext = &map->fm_extents[i];
		p_align = ALIGN(ext->fe_physical, HPAGE_SIZE) - ext->fe_physical;
		fprintf(stderr, "[%ld]: l: %llx p: %llx len: %llx flags: %x\n",
				i, ext->fe_logical, ext->fe_physical,
				ext->fe_length, ext->fe_flags);
		if (ext->fe_length > 2 * HPAGE_SIZE && p_align == 0) {
			fprintf(stderr, "found potential huge extent\n");
			break;
		}
	}

	if (i >= map->fm_mapped_extents) {
		fail();
		goto err_extent;
	}

	m_align = ALIGN(base, HPAGE_SIZE) - ((unsigned long) base);
	p_align = ALIGN(ext->fe_physical, HPAGE_SIZE) - ext->fe_physical;

	pmd_addr = (char *) base + m_align;
	pmd_off =  ext->fe_logical + p_align;
	rc = test_dax_remap(test, fd, HPAGE_SIZE, pmd_addr, pmd_off, fsdax);
	if (rc)
		goto err_test;

	rc = test_dax_directio(fd, HPAGE_SIZE, pmd_addr, pmd_off);
	if (rc)
		goto err_test;

	rc = test_dax_poison(test, fd, HPAGE_SIZE, pmd_addr, pmd_off, fsdax);

err_test:
err_extent:
err_mmap:
	free(map);
	return rc;
}

int __attribute__((weak)) main(int argc, char *argv[])
{
	struct ndctl_test *test = ndctl_test_new(0);
	int fd, rc;

	if (!test) {
		fprintf(stderr, "failed to initialize test\n");
		return EXIT_FAILURE;
	}

	if (argc < 1)
		return -EINVAL;

	fd = open(argv[1], O_RDWR);
	rc = test_pmd(test, fd);
	if (fd >= 0)
		close(fd);
	return ndctl_test_result(test, rc);
}
