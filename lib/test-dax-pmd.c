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

#define NUM_EXTENTS 5
#define HPAGE_SIZE (2 << 20)
#define ALIGN(x, a) ((((unsigned long long) x) + (a - 1)) & ~(a - 1))
#define fail() fprintf(stderr, "%s: failed at: %d\n", __func__, __LINE__)
#define faili(i) fprintf(stderr, "%s: failed at: %d: %ld\n", __func__, __LINE__, i)
#define TEST_FILE "test_dax_data"

/* test_pmd assumes that fd references a pre-allocated + dax-capable file */
static int test_pmd(int fd)
{
	unsigned long long m_align, p_align;
	int fd2 = -1, rc = -ENXIO;
	struct fiemap_extent *ext;
	struct fiemap *map;
	void *addr, *buf;
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

	if (posix_memalign(&buf, 4096, 4096) != 0)
		goto err_memalign;

        addr = mmap(NULL, 4*HPAGE_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
        if (addr == MAP_FAILED) {
		fail();
		goto err_mmap;
	}
	munmap(addr, 4*HPAGE_SIZE);

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

	m_align = ALIGN(addr, HPAGE_SIZE) - ((unsigned long) addr);
	p_align = ALIGN(ext->fe_physical, HPAGE_SIZE) - ext->fe_physical;

	for (i = 0; i < 3; i++) {
		rc = -ENXIO;
		addr = mmap((char *) addr + m_align, 2*HPAGE_SIZE,
				PROT_READ|PROT_WRITE, MAP_SHARED, fd,
				ext->fe_logical + p_align);
		if (addr == MAP_FAILED) {
			faili(i);
			break;
		}

		fd2 = open(TEST_FILE, O_CREAT|O_TRUNC|O_DIRECT|O_RDWR);
		if (fd2 < 0) {
			faili(i);
			munmap(addr, 2*HPAGE_SIZE);
			break;
		}

		rc = 0;
		switch (i) {
		case 0: /* test O_DIRECT of unfaulted address */
			if (write(fd2, addr, 4096) != 4096) {
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
				if (rc != EXIT_SUCCESS)
					faili(i);
			} else
				faili(i);
			break;
		default:
			faili(i);
			rc = -ENXIO;
			break;
		}

		munmap(addr, 2*HPAGE_SIZE);
		addr = MAP_FAILED;
		unlink(TEST_FILE);
		close(fd2);
		fd2 = -1;
		if (rc)
			break;
	}

 err_extent:
 err_mmap:
	free(buf);
 err_memalign:
	free(map);
	return rc;
}

int main(int argc, char *argv[])
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
