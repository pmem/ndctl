#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/falloc.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
	char *buf;
	int fd;

	if (argc < 2) {
		perror("argc invalid");
		return -EINVAL;
	}

	fd = open(argv[1], O_RDWR);
	if (fd < 0) {
		perror("fd");
		return 1;
	}

	buf = mmap(NULL, 2UL << 20, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	*((int *) (buf + (1UL << 20))) = 0;

	close(fd);
	return 0;
}
