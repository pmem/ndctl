// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2021, FUJITSU LIMITED. ALL rights reserved.

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <iniparser/iniparser.h>
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
