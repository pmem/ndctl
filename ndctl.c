#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <ccan/array_size/array_size.h>

#include <util/strbuf.h>
#include <util/util.h>

static const char *ndctl_usage_string;

static const char ndctl_more_info_string[] =
	"See 'ndctl help COMMAND' for more information on a specific command.";

struct cmd_struct {
	const char *cmd;
	int (*fn)(int, const char **);
};

static int cmd_version(int argc, const char **argv)
{
	printf("%s\n", VERSION);
	return 0;
}

static int cmd_help(int argc, const char **argv)
{
	printf("\n%s\n\n", ndctl_usage_string);
	return 0;
}

int cmd_create_nfit(int argc, const char **argv);
int cmd_enable_namespace(int argc, const char **argv);
int cmd_disable_namespace(int argc, const char **argv);
int cmd_enable_region(int argc, const char **argv);
int cmd_disable_region(int argc, const char **argv);
int cmd_zero_labels(int argc, const char **argv);
#ifdef ENABLE_TEST
int cmd_test(int argc, const char **argv);
#endif
#ifdef ENABLE_DESTRUCTIVE
int cmd_bat(int argc, const char **argv);
#endif

static struct cmd_struct commands[] = {
	{ "version", cmd_version },
	{ "create-nfit", cmd_create_nfit },
	{ "enable-namespace", cmd_enable_namespace },
	{ "disable-namespace", cmd_disable_namespace },
	{ "enable-region", cmd_enable_region },
	{ "disable-region", cmd_disable_region },
	{ "zero-labels", cmd_zero_labels },
	#ifdef ENABLE_TEST
	{ "test", cmd_test },
	#endif
	#ifdef ENABLE_DESTRUCTIVE
	{ "bat", cmd_bat },
	#endif
};

/* place holder until help system is implemented */
static char *init_usage_string(void)
{
	char *def = "ndctl [--version] [--help] COMMAND [ARGS]";
	unsigned int len = strlen(def) + 1, i, p;
	char *u;

	for (i = 0; i < ARRAY_SIZE(commands); i++)
		len += strlen(commands[i].cmd) + 2;
	u = calloc(1, len);
	if (!u)
		return def;

	p = sprintf(u, "%s", "ndctl [--version] [--help] ");
	for (i = 0; i < ARRAY_SIZE(commands); i++) {
		p += sprintf(&u[p], "%s", commands[i].cmd);
		if ((i + 1) < ARRAY_SIZE(commands))
			p += sprintf(&u[p], "|");
	}
	p = sprintf(&u[p], "%s", " [ARGS]");
	return u;
}

static int handle_options(const char ***argv, int *argc)
{
	int handled = 0;

	while (*argc > 0) {
		const char *cmd = (*argv)[0];
		if (cmd[0] != '-')
			break;

               if (!strcmp(cmd, "--help"))
			exit(cmd_help(*argc, *argv));

		if (!strcmp(cmd, "--version"))
			break;

		if (!strcmp(cmd, "--list-cmds")) {
			unsigned int i;

			for (i = 0; i < ARRAY_SIZE(commands); i++) {
				struct cmd_struct *p = commands+i;
				printf("%s ", p->cmd);
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

	for (i = 0; i < ARRAY_SIZE(commands); i++) {
		struct cmd_struct *p = commands+i;
		if (strcmp(p->cmd, cmd))
			continue;
		exit(run_builtin(p, argc, argv));
	}
}

int main(int argc, const char **argv)
{
	ndctl_usage_string = init_usage_string();

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
		/* printf("\n %s\n\n", ndctl_more_info_string); TODO */
		goto out;
	}
	handle_internal_command(argc, argv);
	fprintf(stderr, "Failed to run command '%s': %s\n",
		argv[0], strerror(errno));
out:
	return 1;
}
