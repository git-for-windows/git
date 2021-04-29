#include "builtin.h"
#include "repository.h"
#include "parse-options.h"
#include "run-command.h"
#include "strvec.h"

#if defined(GIT_WINDOWS_NATIVE)
/*
 * On Windows, run 'git update-git-for-windows' which
 * is installed by the installer, based on the script
 * in git-for-windows/build-extra.
 */
static int platform_specific_upgrade(void)
{
	struct child_process cp = CHILD_PROCESS_INIT;

	strvec_push(&cp.args, "git-update-git-for-windows");
	return run_command(&cp);
}
#else
static int platform_specific_upgrade(void)
{
	error(_("update-microsoft-git is not supported on this platform"));
	return 1;
}
#endif

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
