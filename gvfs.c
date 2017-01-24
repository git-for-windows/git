#define USE_THE_REPOSITORY_VARIABLE
#include "git-compat-util.h"
#include "environment.h"
#include "gvfs.h"
#include "setup.h"
#include "config.h"

static int gvfs_config_loaded;
static struct repository *gvfs_repo;
static int core_gvfs;
static int core_gvfs_is_bool;

static int early_core_gvfs_config(const char *var, const char *value,
				  const struct config_context *ctx, void *cb UNUSED)
{
	if (!strcmp(var, "core.gvfs"))
		core_gvfs = git_config_bool_or_int("core.gvfs", value, ctx->kvi,
						   &core_gvfs_is_bool);
	return 0;
}

static void gvfs_load_config_value(struct repository *r)
{
	if (gvfs_config_loaded && gvfs_repo == r)
		return;

	if (r) {
		repo_config_get_bool_or_int(r, "core.gvfs",
					    &core_gvfs_is_bool, &core_gvfs);
	} else if (startup_info->have_repository == 0)
		read_early_config(the_repository, early_core_gvfs_config, NULL);
	else
		repo_config_get_bool_or_int(the_repository, "core.gvfs",
					    &core_gvfs_is_bool, &core_gvfs);

	/* Turn on all bits if a bool was set in the settings */
	if (core_gvfs_is_bool && core_gvfs)
		core_gvfs = -1;

	gvfs_config_loaded = 1;
	gvfs_repo = r;
}

int gvfs_config_is_set(struct repository *r, int mask)
{
	gvfs_load_config_value(r);
	return (core_gvfs & mask) == mask;
}
