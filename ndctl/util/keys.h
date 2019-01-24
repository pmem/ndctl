// SPDX-License-Identifier: GPL-2.0
/* Copyright(c) 2019 Intel Corporation. All rights reserved. */

#ifndef _NDCTL_UTIL_KEYS_H_
#define _NDCTL_UTIL_KEYS_H_

enum ndctl_key_type {
	ND_USER_KEY,
	ND_USER_OLD_KEY,
};

#ifdef ENABLE_KEYUTILS
char *ndctl_load_key_blob(const char *path, int *size, const char *postfix,
		int dirfd);
int ndctl_dimm_setup_key(struct ndctl_dimm *dimm, const char *kek);
int ndctl_dimm_update_key(struct ndctl_dimm *dimm, const char *kek);
int ndctl_dimm_remove_key(struct ndctl_dimm *dimm);
int ndctl_dimm_secure_erase_key(struct ndctl_dimm *dimm);
int ndctl_dimm_overwrite_key(struct ndctl_dimm *dimm);
#else
char *ndctl_load_key_blob(const char *path, int *size, const char *postfix,
		int dirfd)
{
	return NULL;
}
static inline int ndctl_dimm_setup_key(struct ndctl_dimm *dimm,
		const char *kek)
{
	return -EOPNOTSUPP;
}

static inline int ndctl_dimm_update_key(struct ndctl_dimm *dimm,
		const char *kek)
{
	return -EOPNOTSUPP;
}

static inline int ndctl_dimm_remove_key(struct ndctl_dimm *dimm)
{
	return -EOPNOTSUPP;
}

static inline int ndctl_dimm_secure_erase_key(struct ndctl_dimm *dimm)
{
	return -EOPNOTSUPP;
}

static inline int ndctl_dimm_overwrite_key(struct ndctl_dimm *dimm)
{
	return -EOPNOTSUPP;
}
#endif /* ENABLE_KEYUTILS */

#endif
