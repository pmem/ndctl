/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2015-2020 Intel Corporation. All rights reserved. */
#ifndef __TEST_H__
#define __TEST_H__
#include <stdbool.h>

struct ndctl_test;
struct ndctl_ctx;
struct ndctl_test *ndctl_test_new(unsigned int kver);
int ndctl_test_result(struct ndctl_test *test, int rc);
int ndctl_test_get_skipped(struct ndctl_test *test);
int ndctl_test_get_attempted(struct ndctl_test *test);
int __ndctl_test_attempt(struct ndctl_test *test, unsigned int kver,
		const char *caller, int line);
#define ndctl_test_attempt(t, v) __ndctl_test_attempt(t, v, __func__, __LINE__)
void __ndctl_test_skip(struct ndctl_test *test, const char *caller, int line);
#define ndctl_test_skip(t) __ndctl_test_skip(t, __func__, __LINE__)
struct ndctl_namespace *ndctl_get_test_dev(struct ndctl_ctx *ctx);
void builtin_xaction_namespace_reset(void);

struct kmod_ctx;
struct kmod_module;
int nfit_test_init(struct kmod_ctx **ctx, struct kmod_module **mod,
		struct ndctl_ctx *nd_ctx, int log_level,
		struct ndctl_test *test);

struct ndctl_ctx;
int test_parent_uuid(int loglevel, struct ndctl_test *test, struct ndctl_ctx *ctx);
int test_multi_pmem(int loglevel, struct ndctl_test *test, struct ndctl_ctx *ctx);
int test_dax_directio(int dax_fd, unsigned long align, void *dax_addr, off_t offset);
int test_dax_remap(struct ndctl_test *test, int dax_fd, unsigned long align, void *dax_addr,
		off_t offset, bool fsdax);
#ifdef ENABLE_POISON
int test_dax_poison(struct ndctl_test *test, int dax_fd, unsigned long align,
		void *dax_addr, off_t offset, bool fsdax);
#else
static inline int test_dax_poison(struct ndctl_test *test, int dax_fd,
		unsigned long align, void *dax_addr, off_t offset, bool fsdax)
{
	return 0;
}
#endif
int test_dpa_alloc(int loglevel, struct ndctl_test *test, struct ndctl_ctx *ctx);
int test_dsm_fail(int loglevel, struct ndctl_test *test, struct ndctl_ctx *ctx);
int test_libndctl(int loglevel, struct ndctl_test *test, struct ndctl_ctx *ctx);
int test_blk_namespaces(int loglevel, struct ndctl_test *test, struct ndctl_ctx *ctx);
int test_pmem_namespaces(int loglevel, struct ndctl_test *test, struct ndctl_ctx *ctx);
#endif /* __TEST_H__ */
