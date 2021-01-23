/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2015-2020 Intel Corporation. All rights reserved. */
#ifndef __TEST_H__
#define __TEST_H__
#include <stdbool.h>

struct test_ctx;
struct ndctl_ctx;
struct test_ctx *test_new(unsigned int kver);
int test_result(struct test_ctx *test, int rc);
int test_get_skipped(struct test_ctx *test);
int test_get_attempted(struct test_ctx *test);
int __test_attempt(struct test_ctx *test, unsigned int kver,
		const char *caller, int line);
#define test_attempt(t, v) __test_attempt(t, v, __func__, __LINE__)
void __test_skip(struct test_ctx *test, const char *caller, int line);
#define test_skip(t) __test_skip(t, __func__, __LINE__)
struct ndctl_namespace *ndctl_get_test_dev(struct ndctl_ctx *ctx);
void builtin_xaction_namespace_reset(void);

struct kmod_ctx;
struct kmod_module;
int ndctl_test_init(struct kmod_ctx **ctx, struct kmod_module **mod,
		struct ndctl_ctx *nd_ctx, int log_level,
		struct test_ctx *test);

struct ndctl_ctx;
int test_parent_uuid(int loglevel, struct test_ctx *test, struct ndctl_ctx *ctx);
int test_multi_pmem(int loglevel, struct test_ctx *test, struct ndctl_ctx *ctx);
int test_dax_directio(int dax_fd, unsigned long align, void *dax_addr, off_t offset);
int test_dax_remap(struct test_ctx *test, int dax_fd, unsigned long align, void *dax_addr,
		off_t offset, bool fsdax);
#ifdef ENABLE_POISON
int test_dax_poison(struct test_ctx *test, int dax_fd, unsigned long align,
		void *dax_addr, off_t offset, bool fsdax);
#else
static inline int test_dax_poison(struct test_ctx *test, int dax_fd,
		unsigned long align, void *dax_addr, off_t offset, bool fsdax)
{
	return 0;
}
#endif
int test_dpa_alloc(int loglevel, struct test_ctx *test, struct ndctl_ctx *ctx);
int test_dsm_fail(int loglevel, struct test_ctx *test, struct ndctl_ctx *ctx);
int test_libndctl(int loglevel, struct test_ctx *test, struct ndctl_ctx *ctx);
int test_blk_namespaces(int loglevel, struct test_ctx *test, struct ndctl_ctx *ctx);
int test_pmem_namespaces(int loglevel, struct test_ctx *test, struct ndctl_ctx *ctx);
#endif /* __TEST_H__ */
