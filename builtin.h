#ifndef _NDCTL_BUILTIN_H_
#define _NDCTL_BUILTIN_H_
extern const char ndctl_usage_string[];
extern const char ndctl_more_info_string[];

struct cmd_struct {
	const char *cmd;
	int (*fn)(int, const char **);
};

int cmd_create_nfit(int argc, const char **argv);
int cmd_enable_namespace(int argc, const char **argv);
int cmd_disable_namespace(int argc, const char **argv);
int cmd_enable_region(int argc, const char **argv);
int cmd_disable_region(int argc, const char **argv);
int cmd_zero_labels(int argc, const char **argv);
int cmd_help(int argc, const char **argv);
#ifdef ENABLE_TEST
int cmd_test(int argc, const char **argv);
#endif
#ifdef ENABLE_DESTRUCTIVE
int cmd_bat(int argc, const char **argv);
#endif

#endif /* _NDCTL_BUILTIN_H_ */

