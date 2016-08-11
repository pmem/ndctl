/*
 * builtin-help.c
 *
 * Builtin help command
 */
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <builtin.h>
#include <util/strbuf.h>
#include <util/parse-options.h>

#define pr_err(x, ...) fprintf(stderr, x, ##__VA_ARGS__)
#define STRERR_BUFSIZE  128     /* For the buffer size of strerror_r */

static void exec_man_konqueror(const char *path, const char *page)
{
	const char *display = getenv("DISPLAY");

	if (display && *display) {
		struct strbuf man_page = STRBUF_INIT;
		const char *filename = "kfmclient";
		char sbuf[STRERR_BUFSIZE];

		/* It's simpler to launch konqueror using kfmclient. */
		if (path) {
			const char *file = strrchr(path, '/');
			if (file && !strcmp(file + 1, "konqueror")) {
				char *new = strdup(path);
				char *dest = strrchr(new, '/');

				/* strlen("konqueror") == strlen("kfmclient") */
				strcpy(dest + 1, "kfmclient");
				path = new;
			}
			if (file)
				filename = file;
		} else
			path = "kfmclient";
		strbuf_addf(&man_page, "man:%s(1)", page);
		execlp(path, filename, "newTab", man_page.buf, NULL);
		warning("failed to exec '%s': %s", path,
			strerror_r(errno, sbuf, sizeof(sbuf)));
	}
}

static void exec_man_man(const char *path, const char *page)
{
	char sbuf[STRERR_BUFSIZE];

	if (!path)
		path = "man";
	execlp(path, "man", page, NULL);
	warning("failed to exec '%s': %s", path,
		strerror_r(errno, sbuf, sizeof(sbuf)));
}

static char *cmd_to_page(const char *ndctl_cmd, char **page)
{
	int rc;

	if (!ndctl_cmd)
		rc = asprintf(page, "ndctl");
	else if (!prefixcmp(ndctl_cmd, "ndctl"))
		rc = asprintf(page, "%s", ndctl_cmd);
	else
		rc = asprintf(page, "ndctl-%s", ndctl_cmd);

	if (rc < 0)
		return NULL;
	return *page;
}

static int is_absolute_path(const char *path)
{
	return path[0] == '/';
}

static const char *system_path(const char *path)
{
        static const char *prefix = PREFIX;
        struct strbuf d = STRBUF_INIT;

        if (is_absolute_path(path))
                return path;

        strbuf_addf(&d, "%s/%s", prefix, path);
        path = strbuf_detach(&d, NULL);
        return path;
}

static void setup_man_path(void)
{
	struct strbuf new_path = STRBUF_INIT;
	const char *old_path = getenv("MANPATH");

	/* We should always put ':' after our path. If there is no
	 * old_path, the ':' at the end will let 'man' to try
	 * system-wide paths after ours to find the manual page. If
	 * there is old_path, we need ':' as delimiter. */
	strbuf_addstr(&new_path, system_path(NDCTL_MAN_PATH));
	strbuf_addch(&new_path, ':');
	if (old_path)
		strbuf_addstr(&new_path, old_path);

	setenv("MANPATH", new_path.buf, 1);

	strbuf_release(&new_path);
}

static void exec_viewer(const char *name, const char *page)
{
	if (!strcasecmp(name, "man"))
		exec_man_man(NULL, page);
	else if (!strcasecmp(name, "konqueror"))
		exec_man_konqueror(NULL, page);
	else
		warning("'%s': unknown man viewer.", name);
}

static int show_man_page(const char *ndctl_cmd)
{
	const char *fallback = getenv("NDCTL_MAN_VIEWER");
	char *page;

	page = cmd_to_page(ndctl_cmd, &page);
	if (!page)
		return -1;
	setup_man_path();
	if (fallback)
		exec_viewer(fallback, page);
	exec_viewer("man", page);

	pr_err("no man viewer handled the request");
	free(page);
	return -1;
}

int cmd_help(int argc, const char **argv, struct ndctl_ctx *ctx)
{
	const char * const builtin_help_subcommands[] = {
		"enable-region", "disable-region", "zero-labels",
		"enable-namespace", "disable-namespace", NULL };
	struct option builtin_help_options[] = {
		OPT_END(),
	};
	const char *builtin_help_usage[] = {
		"ndctl help [command]",
		NULL
	};

	argc = parse_options_subcommand(argc, argv, builtin_help_options,
			builtin_help_subcommands, builtin_help_usage, 0);

	if (!argv[0]) {
		printf("\n usage: %s\n\n", ndctl_usage_string);
		printf("\n %s\n\n", ndctl_more_info_string);
		return 0;
	}

	return show_man_page(argv[0]);
}
