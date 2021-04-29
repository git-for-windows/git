#include "builtin.h"
#include "repository.h"
#include "parse-options.h"
#include "run-command.h"

static int platform_specific_upgrade(void)
{
	return 1;
}

static const char * const update_microsoft_git_usage[] = {
	N_("git update-microsoft-git"),
	NULL,
};


int cmd_update_microsoft_git(int argc, const char **argv, const char *prefix UNUSED, struct repository *repo UNUSED)
{
	static struct option microsoft_git_options[] = {
		OPT_END(),
	};
	show_usage_with_options_if_asked(argc, argv,
					 update_microsoft_git_usage,
					 microsoft_git_options);

	return platform_specific_upgrade();
}
