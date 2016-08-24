#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <syslog.h>
#include <string.h>
#include <signal.h>
#include <setjmp.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <linux/falloc.h>
#include <linux/version.h>
#include <ndctl/libndctl.h>
#include <daxctl/libdaxctl.h>
#include <ccan/array_size/array_size.h>

#include <ndctl/builtin.h>
#include <test.h>

static sigjmp_buf sj_env;

static int create_namespace(int argc, const char **argv, struct ndctl_ctx *ctx)
{
	builtin_xaction_namespace_reset();
	return cmd_create_namespace(argc, argv, ctx);
}

static int reset_device_dax(struct ndctl_namespace *ndns)
{
	struct ndctl_ctx *ctx = ndctl_namespace_get_ctx(ndns);
	const char *argv[] = {
		"__func__", "-v", "-m", "raw", "-f", "-e", "",
	};
	int argc = ARRAY_SIZE(argv);

	argv[argc - 1] = ndctl_namespace_get_devname(ndns);
	return create_namespace(argc, argv, ctx);
}

static int setup_device_dax(struct ndctl_namespace *ndns)
{
	struct ndctl_ctx *ctx = ndctl_namespace_get_ctx(ndns);
	const char *argv[] = {
		"__func__", "-v", "-m", "dax", "-M", "dev", "-f", "-e", "",
	};
	int argc = ARRAY_SIZE(argv);

	argv[argc - 1] = ndctl_namespace_get_devname(ndns);
	return create_namespace(argc, argv, ctx);
}

static void sigbus(int sig, siginfo_t *siginfo, void *d)
{
	siglongjmp(sj_env, 1);
}

static int test_device_dax(int loglevel, struct ndctl_test *test,
		struct ndctl_ctx *ctx)
{
	int fd, rc, *p;
	char *buf, path[100];
	struct sigaction act;
	struct ndctl_dax *dax;
	struct daxctl_dev *dev;
	struct ndctl_namespace *ndns;
	struct daxctl_region *dax_region;

	memset (&act, 0, sizeof(act));
	act.sa_sigaction = sigbus;
	act.sa_flags = SA_SIGINFO;

	if (sigaction(SIGBUS, &act, 0)) {
		perror("sigaction");
		return 1;
	}

	if (!ndctl_test_attempt(test, KERNEL_VERSION(4, 7, 0)))
		return 77;

	ndctl_set_log_priority(ctx, loglevel);

	ndns = ndctl_get_test_dev(ctx);
	if (!ndns) {
		fprintf(stderr, "%s: failed to find suitable namespace\n",
				__func__);
		return 77;
	}

	rc = setup_device_dax(ndns);
	if (rc < 0) {
		fprintf(stderr, "%s: failed device-dax setup\n",
				ndctl_namespace_get_devname(ndns));
		return rc;
	}

	dax = ndctl_namespace_get_dax(ndns);
	dax_region = ndctl_dax_get_daxctl_region(dax);
	dev = daxctl_dev_get_first(dax_region);
	if (!dev) {
		fprintf(stderr, "%s: failed to find device-dax instance\n",
				ndctl_namespace_get_devname(ndns));
		return -ENXIO;
	}

	sprintf(path, "/dev/%s", daxctl_dev_get_devname(dev));

	fd = open(path, O_RDWR);
	if (fd < 0) {
		fprintf(stderr, "%s: failed to open device-dax instance\n",
				daxctl_dev_get_devname(dev));
		return -ENXIO;
	}

	buf = mmap(NULL, 2UL << 20, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	p = (int *) (buf + (1UL << 20));
	*p = 0;

	if (ndctl_test_attempt(test, KERNEL_VERSION(4, 9, 0))) {
		/* prior to 4.8-final this crashes */
		rc = test_dax_directio(fd, NULL, 0);
		if (rc) {
			fprintf(stderr, "%s: failed dax direct-i/o\n",
					ndctl_namespace_get_devname(ndns));
			return rc;
		}
	}

	rc = reset_device_dax(ndns);
	if (rc < 0) {
		fprintf(stderr, "%s: failed to reset device-dax instance\n",
				ndctl_namespace_get_devname(ndns));
		return rc;
	}

	/* test fault after device-dax instance disabled */
	if (sigsetjmp(sj_env, 1)) {
		/* got sigbus, success */
		close(fd);
		return 0;
	}

	rc = EXIT_SUCCESS;
	*p = 0xff;
	if (ndctl_test_attempt(test, KERNEL_VERSION(4, 9, 0))) {
		/* after 4.9 this test will properly get sigbus above */
		rc = EXIT_FAILURE;
		fprintf(stderr, "%s: failed to unmap after reset\n",
				daxctl_dev_get_devname(dev));
	}
	close(fd);
	return rc;
}

int __attribute__((weak)) main(int argc, char *argv[])
{
	struct ndctl_test *test = ndctl_test_new(0);
	struct ndctl_ctx *ctx;
	int rc;

	if (!test) {
		fprintf(stderr, "failed to initialize test\n");
		return EXIT_FAILURE;
	}

	rc = ndctl_new(&ctx);
	if (rc < 0)
		return ndctl_test_result(test, rc);

	rc = test_device_dax(LOG_DEBUG, test, ctx);
	ndctl_unref(ctx);
	return ndctl_test_result(test, rc);
}
