#ifndef __MAIN_H__
#define __MAIN_H__
struct cmd_struct;
int main_handle_options(const char ***argv, int *argc, const char *usage_msg,
		struct cmd_struct *cmds, int num_cmds);
void main_handle_internal_command(int argc, const char **argv, void *ctx,
		struct cmd_struct *cmds, int num_cmds);
int help_show_man_page(const char *cmd, const char *util_name,
		const char *viewer);
#endif /* __MAIN_H__ */
