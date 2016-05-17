#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <builtin.h>
#include <ccan/array_size/array_size.h>

#include <util/strbuf.h>
#include <util/util.h>

const char ndctl_usage_string[] = "ndctl [--version] [--help] COMMAND [ARGS]";
const char ndctl_more_info_string[] =
	"See 'ndctl help COMMAND' for more information on a specific command.\n"
	" ndctl --list-cmds to see all available commands";

static int cmd_version(int argc, const char **argv)
{
	printf("%s\n", VERSION);
	return 0;
}

static struct cmd_struct commands[] = {
	{ "version", cmd_version },
	{ "create-nfit", cmd_create_nfit },
	{ "enable-namespace", cmd_enable_namespace },
	{ "disable-namespace", cmd_disable_namespace },
	{ "create-namespace", cmd_create_namespace },
	{ "destroy-namespace", cmd_destroy_namespace },
	{ "enable-region", cmd_enable_region },
	{ "disable-region", cmd_disable_region },
	{ "zero-labels", cmd_zero_labels },
	{ "read-labels", cmd_read_labels },
	{ "list", cmd_list },
	{ "help", cmd_help },
	#ifdef ENABLE_TEST
	{ "test", cmd_test },
	#endif
	#ifdef ENABLE_DESTRUCTIVE
	{ "bat", cmd_bat },
	#endif
};

static int handle_options(const char ***argv, int *argc)
{
	int handled = 0;

	while (*argc > 0) {
		const char *cmd = (*argv)[0];
		if (cmd[0] != '-')
			break;

		if (!strcmp(cmd, "--version") || !strcmp(cmd, "--help"))
			break;

		/*
		 * Shortcut for '-h' and '-v' options to invoke help
		 * and version command.
		 */
		if (!strcmp(cmd, "-h")) {
			(*argv)[0] = "--help";
			break;
		}

		if (!strcmp(cmd, "-v")) {
			(*argv)[0] = "--version";
			break;
		}

		if (!strcmp(cmd, "--list-cmds")) {
			unsigned int i;

			for (i = 0; i < ARRAY_SIZE(commands); i++) {
				struct cmd_struct *p = commands+i;

				/* filter out commands from auto-complete */
				if (strcmp(p->cmd, "create-nfit") == 0)
					continue;
				if (strcmp(p->cmd, "test") == 0)
					continue;
				if (strcmp(p->cmd, "bat") == 0)
					continue;
				printf("%s\n", p->cmd);
			}
			exit(0);
		} else {
			fprintf(stderr, "Unknown option: %s\n", cmd);
			usage(ndctl_usage_string);
		}

		(*argv)++;
		(*argc)--;
		handled++;
	}
	return handled;
}

static int run_builtin(struct cmd_struct *p, int argc, const char **argv)
{
	int status;
	struct stat st;

	status = p->fn(argc, argv);

	if (status)
		return status & 0xff;

	/* Somebody closed stdout? */
	if (fstat(fileno(stdout), &st))
		return 0;
	/* Ignore write errors for pipes and sockets.. */
	if (S_ISFIFO(st.st_mode) || S_ISSOCK(st.st_mode))
		return 0;

	status = 1;
	/* Check for ENOSPC and EIO errors.. */
	if (fflush(stdout)) {
		fprintf(stderr, "write failure on standard output: %s", strerror(errno));
		goto out;
	}
	if (ferror(stdout)) {
		fprintf(stderr, "unknown write failure on standard output");
		goto out;
	}
	if (fclose(stdout)) {
		fprintf(stderr, "close failed on standard output: %s", strerror(errno));
		goto out;
	}
	status = 0;
out:
	return status;
}

static void handle_internal_command(int argc, const char **argv)
{
	const char *cmd = argv[0];
	unsigned int i;

	/* Turn "ndctl cmd --help" into "ndctl help cmd" */
	if (argc > 1 && !strcmp(argv[1], "--help")) {
		argv[1] = argv[0];
		argv[0] = cmd = "help";
	}

	for (i = 0; i < ARRAY_SIZE(commands); i++) {
		struct cmd_struct *p = commands+i;
		if (strcmp(p->cmd, cmd))
			continue;
		exit(run_builtin(p, argc, argv));
	}
}

int main(int argc, const char **argv)
{
	/* Look for flags.. */
	argv++;
	argc--;
	handle_options(&argv, &argc);

	if (argc > 0) {
		if (!prefixcmp(argv[0], "--"))
			argv[0] += 2;
	} else {
		/* The user didn't specify a command; give them help */
		printf("\n usage: %s\n\n", ndctl_usage_string);
		printf("\n %s\n\n", ndctl_more_info_string);
		goto out;
	}
	handle_internal_command(argc, argv);
	fprintf(stderr, "Unknown command: '%s'\n", argv[0]);
out:
	return 1;
}
