#include <stdio.h>
#include <string.h>
#include "../config.h"

int main(int argc, char **argv)
{
	char buf[1024];

	if (argc != 2) {
		fprintf(stderr, "commit id must be specified\n");
		return 1;
	}

	while (fgets(buf, sizeof(buf), stdin))
		if (strncmp("Version:", buf, 8) == 0)
			fprintf(stdout, "Version:        %s\n", VERSION);
		else if (strncmp("%global gitcommit", buf, 17) == 0)
			fprintf(stdout, "%%global gitcommit %s\n", argv[1]);
		else
			fprintf(stdout, "%s", buf);

	return 0;
}
