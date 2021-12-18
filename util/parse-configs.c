// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2021, FUJITSU LIMITED. ALL rights reserved.

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <iniparser.h>
#include <sys/stat.h>
#include <util/parse-configs.h>
#include <util/strbuf.h>

int filter_conf_files(const struct dirent *dir)
{
	if (!dir)
		return 0;

	if (dir->d_type == DT_REG) {
		const char *ext = strrchr(dir->d_name, '.');

		if ((!ext) || (ext == dir->d_name))
			return 0;
		if (strcmp(ext, ".conf") == 0)
			return 1;
	}

	return 0;
}

static void set_str_val(const char **value, const char *val)
{
	struct strbuf buf = STRBUF_INIT;
	size_t len = *value ? strlen(*value) : 0;

	if (!val)
		return;

	if (len) {
		strbuf_add(&buf, *value, len);
		strbuf_addstr(&buf, " ");
	}
	strbuf_addstr(&buf, val);
	*value = strbuf_detach(&buf, NULL);
}

static const char *search_section_kv(dictionary *d, const struct config *c)
{
	int i;

	for (i = 0; i < iniparser_getnsec(d); i++) {
		const char *cur_sec_full = iniparser_getsecname(d, i);
		char *cur_sec = strdup(cur_sec_full);
		const char *search_val, *ret_val;
		const char *delim = " \t\n\r";
		char *save, *cur, *query;

		if (!cur_sec)
			return NULL;
		if (!c->section || !c->search_key || !c->search_val || !c->get_key) {
			fprintf(stderr, "warning: malformed config query, skipping\n");
			goto out_sec;
		}

		cur = strtok_r(cur_sec, delim, &save);
		if ((cur == NULL) || (strcmp(cur, c->section) != 0))
			goto out_sec;

		if (asprintf(&query, "%s:%s", cur_sec_full, c->search_key) < 0)
			goto out_sec;
		search_val = iniparser_getstring(d, query, NULL);
		if (!search_val)
			goto out_query;
		if (strcmp(search_val, c->search_val) != 0)
			goto out_query;

		/* we're now in a matching section */
		free(query);
		if (asprintf(&query, "%s:%s", cur_sec_full, c->get_key) < 0)
			goto out_sec;
		ret_val = iniparser_getstring(d, query, NULL);
		free(query);
		free(cur_sec);
		return ret_val;

out_query:
		free(query);
out_sec:
		free(cur_sec);
	}

	return NULL;
}

static int parse_config_file(const char *config_file,
			const struct config *configs)
{
	dictionary *dic;

	if ((configs->type == MONITOR_CALLBACK) &&
			(strcmp(config_file, configs->key) == 0))
		return configs->callback(configs, configs->key);

	dic = iniparser_load(config_file);
	if (!dic)
		return -errno;

	for (; configs->type != CONFIG_END; configs++) {
		switch (configs->type) {
		case CONFIG_STRING:
			set_str_val((const char **)configs->value,
					iniparser_getstring(dic,
					configs->key, configs->defval));
			break;
		case CONFIG_SEARCH_SECTION:
			set_str_val((const char **)configs->value,
					search_section_kv(dic, configs));
		case MONITOR_CALLBACK:
		case CONFIG_END:
			break;
		}
	}

	iniparser_freedict(dic);
	return 0;
}

int parse_configs_prefix(const char *config_path, const char *prefix,
			 const struct config *configs)
{
	const char *config_file = NULL;
	struct dirent **namelist;
	int rc, count;

	if (configs->type == MONITOR_CALLBACK)
		return parse_config_file(config_path, configs);

	count = scandir(config_path, &namelist, filter_conf_files, alphasort);
	if (count == -1)
		return -errno;

	while (count--) {
		char *conf_abspath;

		config_file = namelist[count]->d_name;
		rc = asprintf(&conf_abspath, "%s/%s", config_path, config_file);
		if (rc < 0)
			return -ENOMEM;

		rc = parse_config_file(conf_abspath, configs);

		free(conf_abspath);
		if (rc)
			return rc;
	}

	return 0;
}
