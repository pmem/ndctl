// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2015-2020 Intel Corporation. All rights reserved.
// Copyright (C) 2008 Miklos Vajna. All rights reserved.
// Copyright (C) 2006 Linus Torvalds. All rights reserved.

/* originally copied from perf and git */

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
				char *dest;
				char *new = strdup(path);
				if (!new) {
					pr_err("strdup(path) failed.\n");
					exit(1);
				}

				dest = strrchr(new, '/');

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

static char *cmd_to_page(const char *cmd, char **page, const char *util_name)
{
	int rc;

	if (!cmd)
		rc = asprintf(page, "%s", util_name);
	else if (!prefixcmp(cmd, util_name))
		rc = asprintf(page, "%s", cmd);
	else
		rc = asprintf(page, "%s-%s", util_name, cmd);

	if (rc < 0)
		return NULL;
	return *page;
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

int help_show_man_page(const char *cmd, const char *util_name,
		const char *viewer)
{
	const char *fallback = getenv(viewer);
	char *page;

	page = cmd_to_page(cmd, &page, util_name);
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
