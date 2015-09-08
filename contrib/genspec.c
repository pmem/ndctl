#include <stdio.h>
#include <string.h>
#include "../config.h"

static char *lname[] = {
	"ndctl-libs", "libndctl3",
};

static char *dname[] = {
	"ndctl-devel", "libndctl-devel",
};

static int license[] = {
	1, 0,
};

int main(int argc, char **argv)
{
	const char *commit = argv[1];
	char buf[1024];
	int os;

	if (argc != 3) {
		fprintf(stderr, "commit id and OS must be specified\n");
		return 1;
	}

	if (strcmp(argv[2], "rhel") == 0)
		os = 0;
	else if (strcmp(argv[2], "sles") == 0)
		os = 1;
	else
		return 1;

	while (fgets(buf, sizeof(buf), stdin)) {
		if (strncmp("Version:", buf, 8) == 0)
			fprintf(stdout, "Version:        %s\n", &VERSION[1]);
		else if (strncmp("%global gitcommit", buf, 17) == 0)
			fprintf(stdout, "%%global gitcommit %s\n", commit);
		else if (strncmp("%define lname", buf, 12) == 0)
			fprintf(stdout, "%%define lname %s\n", lname[os]);
		else if (strncmp("%define dname", buf, 12) == 0)
			fprintf(stdout, "%%define dname %s\n", dname[os]);
		else if (strncmp("%license", buf, 8) == 0 && !license[os])
			/* skip */;
		else if (strncmp("echo \"\" > version", buf, 17) == 0)
			fprintf(stdout, "echo \"%s\" > version\n", VERSION);
		else
			fprintf(stdout, "%s", buf);
	}

	return 0;
}
