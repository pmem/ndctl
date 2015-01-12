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

#include <nfit.h>

#define DEFAULT_NFIT "local_nfit.bin"
static const char *nfit_file = DEFAULT_NFIT;
static LIST_HEAD(spas);

struct spa {
	struct list_node list;
	unsigned long long size, offset;
};

#define SZ_1K     0x00000400
#define SZ_1M     0x00100000
#define SZ_1G     0x40000000
#define SZ_1T 0x10000000000ULL

static unsigned long long parse_size64(const char *str)
{
	unsigned long long val, check;
	char *end;

	val = strtoull(str, &end, 0);
	if (val == ULLONG_MAX)
		return val;
	check = val;
	switch (*end) {
	case 'k':
	case 'K':
		val *= SZ_1K;
		end++;
		break;
	case 'm':
	case 'M':
		val *= SZ_1M;
		end++;
		break;
	case 'g':
	case 'G':
		val *= SZ_1G;
		end++;
		break;
	case 't':
	case 'T':
		val *= SZ_1T;
		end++;
		break;
	default:
		break;
	}

	if (val < check || *end != '\0')
		val = ULLONG_MAX;
	return val;
}

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

static unsigned char nfit_checksum(void *buf, size_t size)
{
        unsigned char sum, *data = buf;
        size_t i;

        for (sum = 0, i = 0; i < size; i++)
                sum += data[i];
        return 0 - sum;
}

static void writeq(unsigned long long v, void *a)
{
	unsigned long long *p = a;

	*p = htole64(v);
}

static void writel(unsigned long v, void *a)
{
	unsigned long *p = a;

	*p = htole32(v);
}

static void writew(unsigned short v, void *a)
{
	unsigned short *p = a;

	*p = htole16(v);
}

static void writeb(unsigned char v, void *a)
{
	unsigned char *p = a;

	*p = v;
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
	nfit = (struct nfit *) buf;
	memcpy(nfit->signature, "NFIT", 4);
	writel(size, &nfit->length);
	writeb(1, &nfit->revision);
	memcpy(nfit->oemid, "LOCAL", 6);
	writew(1, &nfit->oem_tbl_id);
	writel(1, &nfit->oem_revision);
	writel(0x80860000, &nfit->creator_id);
	writel(1, &nfit->creator_revision);

	nfit_spa = (struct nfit_spa *) (buf + sizeof(*nfit));
	i = 1;
	list_for_each(spa_list, s, list) {
		writew(NFIT_TABLE_SPA, &nfit_spa->type);
		writew(sizeof(*nfit_spa), &nfit_spa->length);
		nfit_spa_uuid_pm(&nfit_spa->type_uuid);
		writew(i++, &nfit_spa->spa_index);
		writeq(s->offset, &nfit_spa->spa_base);
		writeq(s->size, &nfit_spa->spa_length);
		nfit_spa++;
	}

	writeb(nfit_checksum(buf, size), &nfit->checksum);

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

	rc = write(fd, nfit, le32toh(nfit->length));
	close(fd);
	return rc;
}

int cmd_create_nfit(int argc, const char **argv)
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
	if ((unsigned int) rc == le32toh(nfit->length)) {
		fprintf(stderr, "wrote %d bytes to %s\n",
				le32toh(nfit->length), nfit_file);
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
