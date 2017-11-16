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
#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/param.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <libgen.h>
#include <libpmem.h>
#include <util/json.h>
#include <util/filter.h>
#include <util/size.h>
#include <json-c/json.h>
#include <daxctl/libdaxctl.h>
#include <ccan/short_types/short_types.h>
#include <util/parse-options.h>
#include <ccan/array_size/array_size.h>
#include <ndctl/ndctl.h>

enum io_direction {
	IO_READ = 0,
	IO_WRITE,
};

struct io_dev {
	int fd;
	int major;
	int minor;
	void *mmap;
	const char *parm_path;
	char *real_path;
	uint64_t offset;
	enum io_direction direction;
	bool is_dax;
	bool is_char;
	bool is_new;
	bool need_trunc;
	struct ndctl_ctx *ndctl_ctx;
	struct ndctl_region *region;
	struct ndctl_dax *dax;
	uint64_t size;
};

static struct {
	struct io_dev dev[2];
	bool zero;
	uint64_t len;
	struct ndctl_cmd *ars_cap;
	struct ndctl_cmd *clear_err;
} io = {
	.dev[0].fd = -1,
	.dev[1].fd = -1,
};

#define fail(fmt, ...) \
do { \
	fprintf(stderr, "daxctl-%s:%s:%d: " fmt, \
			VERSION, __func__, __LINE__, ##__VA_ARGS__); \
} while (0)

static bool is_stdinout(struct io_dev *io_dev)
{
	return (io_dev->fd == STDIN_FILENO ||
			io_dev->fd == STDOUT_FILENO) ? true : false;
}

static int setup_device(struct io_dev *io_dev, size_t size)
{
	int flags, rc;

	if (is_stdinout(io_dev))
		return 0;

	if (io_dev->is_new)
		flags = O_CREAT|O_WRONLY|O_TRUNC;
	else if (io_dev->need_trunc)
		flags = O_RDWR | O_TRUNC;
	else
		flags = O_RDWR;

	io_dev->fd = open(io_dev->parm_path, flags, S_IRUSR|S_IWUSR);
	if (io_dev->fd == -1) {
		rc = -errno;
		perror("open");
		return rc;
	}

	if (!io_dev->is_dax)
		return 0;

	flags = (io_dev->direction == IO_READ) ? PROT_READ : PROT_WRITE;
	io_dev->mmap = mmap(NULL, size, flags, MAP_SHARED, io_dev->fd, 0);
	if (io_dev->mmap == MAP_FAILED) {
		rc = -errno;
		perror("mmap");
		return rc;
	}

	return 0;
}

static int match_device(struct io_dev *io_dev, struct daxctl_region *dregion)
{
	struct daxctl_dev *dev;

	daxctl_dev_foreach(dregion, dev) {
		if (io_dev->major == daxctl_dev_get_major(dev) &&
			io_dev->minor == daxctl_dev_get_minor(dev)) {
			io_dev->is_dax = true;
			io_dev->size = daxctl_dev_get_size(dev);
			return 1;
		}
	}

	return 0;
}

struct ndctl_dax *find_ndctl_dax(struct ndctl_ctx *ndctl_ctx,
		struct io_dev *io_dev)
{
	struct ndctl_bus *bus;
	struct ndctl_region *region;
	struct ndctl_dax *dax;
	struct daxctl_region *dregion;

	ndctl_bus_foreach(ndctl_ctx, bus)
		ndctl_region_foreach(bus, region)
			ndctl_dax_foreach(region, dax) {
				dregion = ndctl_dax_get_daxctl_region(dax);
				if (match_device(io_dev, dregion))
					return dax;
			}

	return NULL;
}

static int find_dax_device(struct io_dev *io_dev,
		struct daxctl_ctx *daxctl_ctx, struct ndctl_ctx *ndctl_ctx,
		enum io_direction dir)
{
	struct daxctl_region *dregion;
	struct stat st;
	int rc;

	if (is_stdinout(io_dev)) {
		io_dev->size = ULONG_MAX;
		return 0;
	}

	rc = stat(io_dev->parm_path, &st);
	if (rc == -1) {
		rc = -errno;
		if (rc == -ENOENT && dir == IO_WRITE) {
			io_dev->is_new = true;
			io_dev->size = ULONG_MAX;
			return 0;
		}
		perror("stat");
		return rc;
	}

	if (S_ISREG(st.st_mode)) {
		if (dir == IO_WRITE) {
			io_dev->need_trunc = true;
			io_dev->size = ULONG_MAX;
		} else
			io_dev->size = st.st_size;
		return 0;
	} else if (S_ISBLK(st.st_mode)) {
		io_dev->size = st.st_size;
		return 0;
	} else if (S_ISCHR(st.st_mode)) {
		io_dev->size = ULONG_MAX;
		io_dev->is_char = true;
		io_dev->major = major(st.st_rdev);
		io_dev->minor = minor(st.st_rdev);
	} else
		return -ENODEV;

	/* grab the ndctl matches if they exist */
	io_dev->dax = find_ndctl_dax(ndctl_ctx, io_dev);
	if (io_dev->dax) {
		io_dev->region = ndctl_dax_get_region(io_dev->dax);
		return 1;
	}

	daxctl_region_foreach(daxctl_ctx, dregion)
		if (match_device(io_dev, dregion))
			return 1;

	return 0;
}

static int send_clear_error(struct ndctl_bus *bus, uint64_t start, uint64_t size)
{
	uint64_t cleared;
	int rc;

	io.clear_err = ndctl_bus_cmd_new_clear_error(start, size, io.ars_cap);
	if (!io.clear_err) {
		fail("bus: %s failed to create cmd\n",
				ndctl_bus_get_provider(bus));
		return -ENXIO;
	}

	rc = ndctl_cmd_submit(io.clear_err);
	if (rc) {
		fail("bus: %s failed to submit cmd: %d\n",
				ndctl_bus_get_provider(bus), rc);
				ndctl_cmd_unref(io.clear_err);
		return rc;
	}

	cleared = ndctl_cmd_clear_error_get_cleared(io.clear_err);
	if (cleared != size) {
		fail("bus: %s expected to clear: %ld actual: %ld\n",
				ndctl_bus_get_provider(bus),
				size, cleared);
		return -ENXIO;
	}

	return 0;
}

static int get_ars_cap(struct ndctl_bus *bus, uint64_t start, uint64_t size)
{
	int rc;

	io.ars_cap = ndctl_bus_cmd_new_ars_cap(bus, start, size);
	if (!io.ars_cap) {
		fail("bus: %s failed to create cmd\n",
				ndctl_bus_get_provider(bus));
		return -ENOTTY;
	}

	rc = ndctl_cmd_submit(io.ars_cap);
	if (rc) {
		fail("bus: %s failed to submit cmd: %d\n",
				ndctl_bus_get_provider(bus), rc);
		ndctl_cmd_unref(io.ars_cap);
		return rc;
	}

	if (ndctl_cmd_ars_cap_get_size(io.ars_cap) <
			sizeof(struct nd_cmd_ars_status)) {
		fail("bus: %s expected size >= %zd got: %d\n",
				ndctl_bus_get_provider(bus),
				sizeof(struct nd_cmd_ars_status),
				ndctl_cmd_ars_cap_get_size(io.ars_cap));
		ndctl_cmd_unref(io.ars_cap);
		return -ENXIO;
	}

	return 0;
}

int clear_errors(struct ndctl_bus *bus, uint64_t start, uint64_t len)
{
	int rc;

	rc = get_ars_cap(bus, start, len);
	if (rc) {
		fail("get_ars_cap failed\n");
		return rc;
	}

	rc = send_clear_error(bus, start, len);
	if (rc) {
		fail("send_clear_error failed\n");
		return rc;
	}

	return 0;
}

static int clear_badblocks(struct io_dev *dev, uint64_t len)
{
	unsigned long long dax_begin, dax_size, dax_end;
	unsigned long long region_begin, offset;
	unsigned long long size, io_begin, io_end, io_len;
	struct badblock *bb;
	int rc;

	dax_begin = ndctl_dax_get_resource(dev->dax);
	if (dax_begin == ULLONG_MAX)
		return -ERANGE;

	dax_size = ndctl_dax_get_size(dev->dax);
	if (dax_size == ULLONG_MAX)
		return -ERANGE;

	dax_end = dax_begin + dax_size - 1;

	region_begin = ndctl_region_get_resource(dev->region);
	if (region_begin == ULLONG_MAX)
		return -ERANGE;

	ndctl_region_badblock_foreach(dev->region, bb) {
		unsigned long long bb_begin, bb_end, begin, end;

		bb_begin = region_begin + (bb->offset << 9);
		bb_end = bb_begin + (bb->len << 9) - 1;

		if (bb_end <= dax_begin || bb_begin >= dax_end)
			continue;

		if (bb_begin < dax_begin)
			begin = dax_begin;
		else
			begin = bb_begin;

		if (bb_end > dax_end)
			end = dax_end;
		else
			end = bb_end;

		offset = begin - dax_begin;
		size = end - begin + 1;

		/*
		 * If end of I/O is before badblock or the offset of the
		 * I/O is greater than the actual size of badblock range
		 */
		if (dev->offset + len - 1 < offset || dev->offset > size)
			continue;

		io_begin = (dev->offset < offset) ? offset : dev->offset;
		if ((dev->offset + len) < (offset + size))
			io_end = offset + len;
		else
			io_end = offset + size;

		io_len = io_end - io_begin;
		io_begin += dax_begin;
		rc = clear_errors(ndctl_region_get_bus(dev->region),
				io_begin, io_len);
		if (rc < 0)
			return rc;
	}

	return 0;
}

static int64_t __do_io(struct io_dev *dst_dev, struct io_dev *src_dev,
		uint64_t len, bool zero)
{
	void *src, *dst;
	ssize_t rc, count = 0;

	if (zero && dst_dev->is_dax) {
		dst = (uint8_t *)dst_dev->mmap + dst_dev->offset;
		memset(dst, 0, len);
		pmem_persist(dst, len);
		rc = len;
	} else if (dst_dev->is_dax && src_dev->is_dax) {
		src = (uint8_t *)src_dev->mmap + src_dev->offset;
		dst = (uint8_t *)dst_dev->mmap + dst_dev->offset;
		pmem_memcpy_persist(dst, src, len);
		rc = len;
	} else if (src_dev->is_dax) {
		src = (uint8_t *)src_dev->mmap + src_dev->offset;
		if (dst_dev->offset) {
			rc = lseek(dst_dev->fd, dst_dev->offset, SEEK_SET);
			if (rc < 0) {
				rc = -errno;
				perror("lseek");
				return rc;
			}
		}

		do {
			rc = write(dst_dev->fd, (uint8_t *)src + count,
					len - count);
			if (rc == -1) {
				rc = -errno;
				perror("write");
				return rc;
			}
			count += rc;
		} while (count != (ssize_t)len);
		rc = count;
		if (rc != (ssize_t)len)
			printf("Requested size %lu larger than source.\n",
					len);
	} else if (dst_dev->is_dax) {
		dst = (uint8_t *)dst_dev->mmap + dst_dev->offset;
		if (src_dev->offset) {
			rc = lseek(src_dev->fd, src_dev->offset, SEEK_SET);
			if (rc < 0) {
				rc = -errno;
				perror("lseek");
				return rc;
			}
		}

		do {
			rc = read(src_dev->fd, (uint8_t *)dst + count,
					len - count);
			if (rc == -1) {
				rc = -errno;
				perror("pread");
				return rc;
			}
			/* end of file */
			if (rc == 0)
				break;
			count += rc;
		} while (count != (ssize_t)len);
		pmem_persist(dst, count);
		rc = count;
		if (rc != (ssize_t)len)
			printf("Requested size %lu larger than destination.\n", len);
	} else
		return -EINVAL;

	return rc;
}

static int do_io(struct daxctl_ctx *daxctl_ctx, struct ndctl_ctx *ndctl_ctx)
{
	int i, dax_devs = 0;
	ssize_t rc;

	/* if we are zeroing the device, we just need output */
	i = io.zero ? 1 : 0;
	for (; i < 2; i++) {
		if (!io.dev[i].parm_path)
			continue;
		rc = find_dax_device(&io.dev[i], daxctl_ctx, ndctl_ctx, i);
		if (rc < 0)
			return rc;

		if (rc == 1)
			dax_devs++;
	}

	if (dax_devs == 0) {
		fail("No DAX devices for input or output, fail\n");
		return -ENODEV;
	}

	if (io.len == 0) {
		if (is_stdinout(&io.dev[0]))
			io.len = io.dev[1].size;
		else
			io.len = io.dev[0].size;
	}

	io.dev[1].direction = IO_WRITE;
	i = io.zero ? 1 : 0;
	for (; i < 2; i++) {
		if (!io.dev[i].parm_path)
			continue;
		rc = setup_device(&io.dev[i], io.len);
		if (rc < 0)
			return rc;
	}

	/* make sure we are DAX and we have ndctl related bits */
	if (io.dev[1].is_dax && io.dev[1].dax) {
		rc = clear_badblocks(&io.dev[1], io.len);
		if (rc < 0) {
			fail("Failed to clear badblocks on %s\n",
					io.dev[1].parm_path);
			return rc;
		}
	}

	rc = __do_io(&io.dev[1], &io.dev[0], io.len, io.zero);
	if (rc < 0) {
		fail("Failed to perform I/O: %ld\n", rc);
		return rc;
	}

	printf("Data copied %lu bytes to device %s\n",
			rc, io.dev[1].parm_path);

	return 0;
}

static void cleanup(void)
{
	int i;

	for (i = 0; i < 2; i++) {
		if (is_stdinout(&io.dev[i]))
			continue;
		close(io.dev[i].fd);
	}
}

int cmd_io(int argc, const char **argv, void *daxctl_ctx)
{
	const char *len_str;
	const struct option options[] = {
		OPT_STRING('i', "input", &io.dev[0].parm_path, "in device",
				"input device/file"),
		OPT_STRING('o', "output", &io.dev[1].parm_path, "out device",
				"output device/file"),
		OPT_BOOLEAN('z', "zero", &io.zero, "zeroing the device"),
		OPT_STRING('l', "len", &len_str, "I/O length", "total length to perform the I/O"),
		OPT_U64('s', "seek", &io.dev[1].offset, "seek offset for output"),
		OPT_U64('k', "skip", &io.dev[0].offset, "skip offset for input"),
	};
	const char * const u[] = {
		"daxctl io [<options>]",
		NULL
	};
	int i, rc;
	struct ndctl_ctx *ndctl_ctx;

	argc = parse_options(argc, argv, options, u, 0);
	for (i = 0; i < argc; i++)
		fail("Unknown parameter \"%s\"\n", argv[i]);

	if (argc)
		usage_with_options(u, options);

	if (!io.dev[0].parm_path && !io.dev[1].parm_path) {
		usage_with_options(u, options);
		return 0;
	}

	if (len_str) {
		io.len = parse_size64(len_str);
		if (io.len == ULLONG_MAX) {
			fail("Incorrect len param entered: %s\n", len_str);
			return -EINVAL;
		}
	} else
		io.len = 0;

	if (!io.dev[0].parm_path) {
		io.dev[0].fd = STDIN_FILENO;
		io.dev[0].offset = 0;
	}

	if (!io.dev[1].parm_path) {
		io.dev[1].fd = STDOUT_FILENO;
		io.dev[1].offset = 0;
	}

	rc = ndctl_new(&ndctl_ctx);
	if (rc)
		return -ENOMEM;

	rc = do_io(daxctl_ctx, ndctl_ctx);
	if (rc < 0)
		goto out;

	rc = 0;
out:
	cleanup();
	ndctl_unref(ndctl_ctx);
	return rc;
}
