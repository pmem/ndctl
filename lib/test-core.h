struct ndctl_test;
struct ndctl_test *ndctl_test_new(unsigned int kver);
int ndctl_test_result(struct ndctl_test *test, int rc);
int ndctl_test_get_skipped(struct ndctl_test *test);
int ndctl_test_get_attempted(struct ndctl_test *test);
int __ndctl_test_attempt(struct ndctl_test *test, unsigned int kver,
		const char *caller, int line);
#define ndctl_test_attempt(t, v) __ndctl_test_attempt(t, v, __func__, __LINE__)
