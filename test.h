#ifndef __TEST_H__
#define __TEST_H__
struct ndctl_test;
struct ndctl_test;
struct ndctl_test *ndctl_test_new(unsigned int kver);
int ndctl_test_result(struct ndctl_test *test, int rc);
int ndctl_test_get_skipped(struct ndctl_test *test);
int ndctl_test_get_attempted(struct ndctl_test *test);
int __ndctl_test_attempt(struct ndctl_test *test, unsigned int kver,
		const char *caller, int line);
#define ndctl_test_attempt(t, v) __ndctl_test_attempt(t, v, __func__, __LINE__)
void __ndctl_test_skip(struct ndctl_test *test, const char *caller, int line);
#define ndctl_test_skip(t) __ndctl_test_skip(t, __func__, __LINE__)

int test_parent_uuid(int loglevel, struct ndctl_test *test);
int test_direct_io(int loglevel, struct ndctl_test *test);
int test_dpa_alloc(int loglevel, struct ndctl_test *test);
int test_libndctl(int loglevel, struct ndctl_test *test);
int test_blk_namespaces(int loglevel, struct ndctl_test *test);
int test_pmem_namespaces(int loglevel, struct ndctl_test *test);
#endif /* __TEST_H__ */
