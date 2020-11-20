// SPDX-License-Identifier: GPL-2.0
// Copyright(C) 2014-2020 Intel Corporation. All rights reserved.
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <endian.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ccan/list/list.h>
#include <util/parse-options.h>
#include <util/size.h>

#include <acpi.h>

#define DEFAULT_NFIT "local_nfit.bin"
static const char *nfit_file = DEFAULT_NFIT;
static LIST_HEAD(spas);

struct spa {
	struct list_node list;
	unsigned long long size, offset;
};

static int parse_add_spa(const struct option *option, const char *__arg, int unset)
{
	struct spa *s = calloc(1, sizeof(struct spa));
	char *arg = strdup(__arg);
	char *size, *offset;
	int rc = -ENOMEM;

	if (!s || !arg)
		goto err;
	rc = -EINVAL;

	size = arg;
	offset = strchr(arg, ',');
	if (!offset)
		goto err;
	*offset++ = '\0';
	s->size = parse_size64(size);
	if (s->size == ULLONG_MAX)
		goto err;
	s->offset = parse_size64(offset);
	if (s->offset == ULLONG_MAX)
		goto err;

	list_add_tail(&spas, &s->list);
	free(arg);

	return 0;

 err:
	error("failed to parse --add-spa=%s\n", __arg);
	free(arg);
	free(s);
	return rc;
}

static struct nfit *create_nfit(struct list_head *spa_list)
{
	struct nfit_spa *nfit_spa;
	struct nfit *nfit;
	struct spa *s;
	size_t size;
	char *buf;
	int i;

	size = sizeof(struct nfit);
	list_for_each(spa_list, s, list)
		size += sizeof(struct nfit_spa);

	buf = calloc(1, size);
	if (!buf)
		return NULL;

	/* nfit header */
	nfit = (typeof(nfit)) buf;
	memcpy(nfit->h.signature, "NFIT", 4);
	writel(size, &nfit->h.length);
	writeb(1, &nfit->h.revision);
	memcpy(nfit->h.oemid, "LOCAL", 6);
	writew(1, &nfit->h.oem_tbl_id);
	writel(1, &nfit->h.oem_revision);
	writel(0x80860000, &nfit->h.asl_id);
	writel(1, &nfit->h.asl_revision);

	nfit_spa = (struct nfit_spa *) (buf + sizeof(*nfit));
	i = 1;
	list_for_each(spa_list, s, list) {
		writew(NFIT_TABLE_SPA, &nfit_spa->type);
		writew(sizeof(*nfit_spa), &nfit_spa->length);
		nfit_spa_uuid_pm(&nfit_spa->type_uuid);
		writew(i++, &nfit_spa->range_index);
		writeq(s->offset, &nfit_spa->spa_base);
		writeq(s->size, &nfit_spa->spa_length);
		nfit_spa++;
	}

	writeb(acpi_checksum(buf, size), &nfit->h.checksum);

	return nfit;
}

static int write_nfit(struct nfit *nfit, const char *file, int force)
{
	int fd;
	ssize_t rc;
	mode_t mode = S_IRUSR|S_IRGRP|S_IWUSR|S_IWGRP;

	fd = open(file, O_RDWR|O_CREAT|O_EXCL, mode);
	if (fd < 0 && !force && errno == EEXIST) {
		error("\"%s\" exists, overwrite with --force\n", file);
		return -EEXIST;
	} else if (fd < 0 && force && errno == EEXIST) {
		fd = open(file, O_RDWR|O_CREAT|O_TRUNC, mode);
	}

	if (fd < 0) {
		error("Failed to open \"%s\": %s\n", file, strerror(errno));
		return -errno;
	}

	rc = write(fd, nfit, le32toh(nfit->h.length));
	close(fd);
	return rc;
}

struct ndctl_ctx;
int cmd_create_nfit(int argc, const char **argv, struct ndctl_ctx *ctx)
{
	int i, rc = -ENXIO, force = 0;
	const char * const u[] = {
		"ndctl create-nfit [<options>]",
		NULL
	};
	const struct option options[] = {
        OPT_CALLBACK('a', "add-spa", NULL, "size,offset",
                        "add a system-physical-address range table entry",
                        parse_add_spa),
	OPT_STRING('o', NULL, &nfit_file, "file",
			"output to <file> (default: " DEFAULT_NFIT ")"),
	OPT_INCR('f', "force", &force, "overwrite <file> if it already exists"),
	OPT_END(),
	};
	struct spa *s, *_s;
	struct nfit *nfit = NULL;

        argc = parse_options(argc, argv, options, u, 0);

	for (i = 0; i < argc; i++)
		error("unknown parameter \"%s\"\n", argv[i]);
	if (list_empty(&spas))
		error("specify at least one --add-spa= option\n");

	if (argc || list_empty(&spas))
		usage_with_options(u, options);

	nfit = create_nfit(&spas);
	if (!nfit)
		goto out;

	rc = write_nfit(nfit, nfit_file, force);
	if ((unsigned int) rc == le32toh(nfit->h.length)) {
		fprintf(stderr, "wrote %d bytes to %s\n",
				le32toh(nfit->h.length), nfit_file);
		rc = 0;
	}

 out:
	free(nfit);
	list_for_each_safe(&spas, s, _s, list) {
		list_del(&s->list);
		free(s);
	}

	return rc;
}
