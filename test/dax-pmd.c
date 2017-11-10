/*
 * Copyright(c) 2015-2017 Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */
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
#include <test.h>
#include <util/size.h>
#include <linux/fiemap.h>

#define NUM_EXTENTS 5
#define fail() fprintf(stderr, "%s: failed at: %d (%s)\n", \
	__func__, __LINE__, strerror(errno))
#define faili(i) fprintf(stderr, "%s: failed at: %d: %d (%s)\n", \
	__func__, __LINE__, i, strerror(errno))
#define TEST_FILE "test_dax_data"

int test_dax_directio(int dax_fd, unsigned long align, void *dax_addr, off_t offset)
{
	int i, rc = -ENXIO;
	void *buf;

	if (posix_memalign(&buf, 4096, 4096) != 0)
		return -ENOMEM;

	for (i = 0; i < 5; i++) {
		void *addr = mmap(dax_addr, 2*align,
				PROT_READ|PROT_WRITE, MAP_SHARED, dax_fd,
				offset);
		int fd2;

		if (addr == MAP_FAILED) {
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
			pread(fd2, buf, 4096, 0);
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
static int test_pmd(int fd)
{
	unsigned long long m_align, p_align;
	struct fiemap_extent *ext;
	struct fiemap *map;
	int rc = -ENXIO;
	unsigned long i;
	void *base;

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
		fprintf(stderr, "[%ld]: l: %llx p: %llx len: %llx flags: %x\n",
				i, ext->fe_logical, ext->fe_physical,
				ext->fe_length, ext->fe_flags);
		if (ext->fe_length > 2 * HPAGE_SIZE) {
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

	rc = test_dax_directio(fd, HPAGE_SIZE, (char *) base + m_align,
			ext->fe_logical + p_align);

 err_extent:
 err_mmap:
	free(map);
	return rc;
}

int __attribute__((weak)) main(int argc, char *argv[])
{
	int fd, rc;

	if (argc < 1)
		return -EINVAL;

	fd = open(argv[1], O_RDWR);
	rc = test_pmd(fd);
	if (fd >= 0)
		close(fd);
	return rc;
}
