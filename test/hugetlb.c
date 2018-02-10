#include <stdio.h>
#include <errno.h>

#include <ccan/array_size/array_size.h>
#include <util/size.h>
#include <test.h>

static int test_hugetlb(void)
{
	int rc, i;
	unsigned long aligns[] = { SZ_4K, SZ_2M, SZ_1G };

	for (i = 0; i < (int) ARRAY_SIZE(aligns); i++) {
		fprintf(stderr, "%s: page_size: %#lx\n", __func__, aligns[i]);
		rc = test_dax_directio(-1, aligns[i], NULL, 0);
		if (rc == -ENOENT && aligns[i] == SZ_1G)
			continue; /* system not configured for 1G pages */
		else if (rc)
			return 77;
	}
	return 0;
}

int __attribute__((weak)) main(int argc, char *argv[])
{
	return test_hugetlb();
}
