#include <stdio.h>
#include <string.h>
#include "../config.h"

int main(void)
{
	char buf[1024];

	while (fgets(buf, sizeof(buf), stdin))
		if (strncmp("Version:", buf, 8) == 0)
			fprintf(stdout, "Version:        %s\n", VERSION);
		else
			fprintf(stdout, "%s", buf);

	return 0;
}
