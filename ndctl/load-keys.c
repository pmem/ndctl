// SPDX-License-Identifier: GPL-2.0
/* Copyright (C) 2019-2020 Intel Corporation. All rights reserved. */

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <keyutils.h>
#include <util/json.h>
#include <util/filter.h>
#include <json-c/json.h>
#include <ndctl/libndctl.h>
#include <util/parse-options.h>
#include <ccan/array_size/array_size.h>
#include <util/keys.h>
#include <ndctl.h>

static struct parameters {
	const char *key_path;
	const char *tpm_handle;
} param;

static const char *key_names[] = {"user", "trusted", "encrypted"};

static struct loadkeys {
	enum key_type key_type;
	DIR *dir;
	int dirfd;
} loadkey_ctx;

static int load_master_key(struct loadkeys *lk_ctx, const char *keypath)
{
	key_serial_t key;
	char *blob;
	int size, rc;
	char path[PATH_MAX];
	enum key_type;

	rc = sprintf(path, "%s/nvdimm-master.blob", keypath);
	if (rc < 0)
		return -errno;

	if (param.tpm_handle)
		lk_ctx->key_type = KEY_TRUSTED;
	else
		lk_ctx->key_type = KEY_USER;

	key = keyctl_search(KEY_SPEC_USER_KEYRING,
			key_names[lk_ctx->key_type], "nvdimm-master", 0);
	if (key > 0)	/* check to see if key already loaded */
		return 0;

	if (key < 0 && errno != ENOKEY) {
		fprintf(stderr, "keyctl_search() failed: %s\n",
				strerror(errno));
		return -errno;
	}

	blob = ndctl_load_key_blob(path, &size, param.tpm_handle, -1,
			lk_ctx->key_type);
	if (!blob)
		return -ENOMEM;

	key = add_key(key_names[lk_ctx->key_type], "nvdimm-master",
			blob, size, KEY_SPEC_USER_KEYRING);
	free(blob);
	if (key < 0) {
		fprintf(stderr, "add_key failed: %s\n", strerror(errno));
		return -errno;
	}

	printf("nvdimm master key loaded.\n");

	return 0;
}

static int load_dimm_keys(struct loadkeys *lk_ctx)
{
	int rc;
	struct dirent *dent;
	char *fname = NULL, *id, *blob = NULL;
	char desc[ND_KEY_DESC_SIZE];
	int size, count = 0;
	key_serial_t key;

	while ((dent = readdir(lk_ctx->dir)) != NULL) {
		if (dent->d_type != DT_REG)
			continue;

		fname = strdup(dent->d_name);
		if (!fname) {
			fprintf(stderr, "Unable to strdup %s\n",
					dent->d_name);
			return -ENOMEM;
		}

		/*
		 * We want to pick up the second member of the file name
		 * as the nvdimm id.
		 */
		id = strtok(fname, "_");
		if (!id) {
			free(fname);
			continue;
		}
		if (strcmp(id, "nvdimm") != 0) {
			free(fname);
			continue;
		}
		id = strtok(NULL, "_");
		if (!id) {
			free(fname);
			continue;
		}

		blob = ndctl_load_key_blob(dent->d_name, &size, NULL,
				lk_ctx->dirfd, KEY_ENCRYPTED);
		if (!blob) {
			free(fname);
			continue;
		}

		rc = sprintf(desc, "nvdimm:%s", id);
		if (rc < 0) {
			free(fname);
			free(blob);
			continue;
		}

		key = add_key("encrypted", desc, blob, size,
				KEY_SPEC_USER_KEYRING);
		if (key < 0)
			fprintf(stderr, "add_key failed: %s\n",
					strerror(errno));
		else
			count++;
		free(fname);
		free(blob);
	}

	printf("%d nvdimm keys loaded\n", count);

	return 0;
}

static int check_tpm_handle(struct loadkeys *lk_ctx)
{
	int fd, rc;
	FILE *fs;
	char *buf;

	fd = openat(lk_ctx->dirfd, "tpm.handle", O_RDONLY);
	if (fd < 0)
		return -errno;

	fs = fdopen(fd, "r");
	if (!fs) {
		fprintf(stderr, "Failed to open file stream: %s\n",
				strerror(errno));
		return -errno;
	}

	rc = fscanf(fs, "%ms", &buf);
	if (rc < 0) {
		rc = -errno;
		fprintf(stderr, "Failed to read file: %s\n", strerror(errno));
		fclose(fs);
		return rc;
	}

	param.tpm_handle = buf;
	fclose(fs);
	return 0;
}

static int load_keys(struct loadkeys *lk_ctx, const char *keypath,
		const char *tpmhandle)
{
	int rc;

	rc = chdir(keypath);
	if (rc < 0) {
		fprintf(stderr, "Change current work dir to %s failed: %s\n",
				param.key_path, strerror(errno));
		rc = -errno;
		goto erropen;
	}

	lk_ctx->dir = opendir(param.key_path);
	if (!lk_ctx->dir) {
		fprintf(stderr, "Unable to open dir %s: %s\n",
				param.key_path, strerror(errno));
		rc = -errno;
		goto erropen;
	}

	lk_ctx->dirfd = open(param.key_path, O_DIRECTORY);
	if (lk_ctx->dirfd < 0) {
		fprintf(stderr, "Unable to open dir %s: %s\n",
				param.key_path, strerror(errno));
		rc = -errno;
		goto erropen;
	}

	if (!tpmhandle) {
		rc = check_tpm_handle(lk_ctx);
		if (rc < 0)
			fprintf(stderr, "No TPM handle discovered.\n");
	}

	rc = load_master_key(lk_ctx, param.key_path);
	if (rc < 0)
		goto out;

	rc = load_dimm_keys(lk_ctx);
	if (rc < 0)
		goto out;

     out:
	close(lk_ctx->dirfd);
 erropen:
	closedir(lk_ctx->dir);
	return rc;
}

int cmd_load_keys(int argc, const char **argv, struct ndctl_ctx *ctx)
{
	const struct option options[] = {
		OPT_FILENAME('p', "key-path", &param.key_path, "key-path",
				"override the default key path"),
		OPT_STRING('t', "tpm-handle", &param.tpm_handle, "tpm-handle",
				"TPM handle for trusted key"),
		OPT_END(),
	};
	const char *const u[] = {
		"ndctl load-keys [<options>]",
		NULL
	};
	int i;

	argc = parse_options(argc, argv, options, u, 0);
	for (i = 0; i < argc; i++)
		error("unknown parameter \"%s\"\n", argv[i]);
	if (argc)
		usage_with_options(u, options);

	if (!param.key_path)
		param.key_path = strdup(NDCTL_KEYS_DIR);

	return load_keys(&loadkey_ctx, param.key_path, param.tpm_handle);
}
