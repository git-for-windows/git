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
#elif defined(__APPLE__)
/*
 * On macOS, we expect the user to have the microsoft-git
 * cask installed via Homebrew. We check using these
 * commands:
 *
 * 1. 'brew update' to get latest versions.
 * 2. 'brew upgrade --cask microsoft-git' to get the
 *    latest version.
 */
static int platform_specific_upgrade(void)
{
	int res;
	struct child_process update = CHILD_PROCESS_INIT;
	struct child_process upgrade = CHILD_PROCESS_INIT;

	printf("Updating Homebrew with 'brew update'\n");

	strvec_pushl(&update.args, "brew", "update", NULL);
	res = run_command(&update);

	if (res) {
		error(_("'brew update' failed; is brew installed?"));
		return 1;
	}

	printf("Upgrading microsoft-git with 'brew upgrade --cask microsoft-git'\n");
	strvec_pushl(&upgrade.args, "brew", "upgrade", "--cask", "microsoft-git", NULL);
	res = run_command(&upgrade);

	return res;
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
