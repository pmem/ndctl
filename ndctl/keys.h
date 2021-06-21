/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2019-2020 Intel Corporation. All rights reserved. */

#ifndef _NDCTL_UTIL_KEYS_H_
#define _NDCTL_UTIL_KEYS_H_

enum ndctl_key_type {
	ND_USER_KEY,
	ND_USER_OLD_KEY,
	ND_MASTER_KEY,
	ND_MASTER_OLD_KEY,
	ND_ZERO_KEY,
};

enum key_type {
	KEY_USER = 0,
	KEY_TRUSTED,
	KEY_ENCRYPTED,
};

#ifdef ENABLE_KEYUTILS
char *ndctl_load_key_blob(const char *path, int *size, const char *postfix,
		int dirfd, enum key_type key_type);
int ndctl_dimm_setup_key(struct ndctl_dimm *dimm, const char *kek,
				enum ndctl_key_type key_type);
int ndctl_dimm_update_key(struct ndctl_dimm *dimm, const char *kek,
				enum ndctl_key_type key_type);
int ndctl_dimm_remove_key(struct ndctl_dimm *dimm);
int ndctl_dimm_secure_erase_key(struct ndctl_dimm *dimm,
		enum ndctl_key_type key_type);
int ndctl_dimm_overwrite_key(struct ndctl_dimm *dimm);
#else
char *ndctl_load_key_blob(const char *path, int *size, const char *postfix,
		int dirfd, enum key_type key_type)
{
	return NULL;
}
static inline int ndctl_dimm_setup_key(struct ndctl_dimm *dimm,
		const char *kek, enum ndctl_key_type key_type)
{
	return -EOPNOTSUPP;
}

static inline int ndctl_dimm_update_key(struct ndctl_dimm *dimm,
		const char *kek, enum ndctl_key_type key_type)
{
	return -EOPNOTSUPP;
}

static inline int ndctl_dimm_remove_key(struct ndctl_dimm *dimm)
{
	return -EOPNOTSUPP;
}

static inline int ndctl_dimm_secure_erase_key(struct ndctl_dimm *dimm,
		enum ndctl_key_type key_type)
{
	return -EOPNOTSUPP;
}

static inline int ndctl_dimm_overwrite_key(struct ndctl_dimm *dimm)
{
	return -EOPNOTSUPP;
}
#endif /* ENABLE_KEYUTILS */

#endif
