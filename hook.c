#define USE_THE_REPOSITORY_VARIABLE

#include "git-compat-util.h"
#include "trace2/tr2_sid.h"
#include "abspath.h"
#include "environment.h"
#include "advice.h"
#include "gettext.h"
#include "hook.h"
#include "path.h"
#include "run-command.h"
#include "config.h"
#include "strbuf.h"
#include "environment.h"
#include "setup.h"

static int early_hooks_path_config(const char *var, const char *value,
				   const struct config_context *ctx UNUSED, void *cb)
{
	if (!strcmp(var, "core.hookspath"))
		return git_config_pathname((char **)cb, var, value);

	return 0;
}

/* Discover the hook before setup_git_directory() was called */
static const char *hook_path_early(const char *name, struct strbuf *result)
{
	static struct strbuf hooks_dir = STRBUF_INIT;
	static int initialized;

	if (initialized < 0)
		return NULL;

	if (!initialized) {
		struct strbuf gitdir = STRBUF_INIT, commondir = STRBUF_INIT;
		char *early_hooks_dir = NULL;

		if (discover_git_directory(&commondir, &gitdir) < 0) {
			strbuf_release(&gitdir);
			strbuf_release(&commondir);
			initialized = -1;
			return NULL;
		}

		read_early_config(the_repository, early_hooks_path_config, &early_hooks_dir);
		if (!early_hooks_dir)
			strbuf_addf(&hooks_dir, "%s/hooks/", commondir.buf);
		else {
			strbuf_add_absolute_path(&hooks_dir, early_hooks_dir);
			free(early_hooks_dir);
			strbuf_addch(&hooks_dir, '/');
		}

		strbuf_release(&gitdir);
		strbuf_release(&commondir);

		initialized = 1;
	}

	strbuf_addf(result, "%s%s", hooks_dir.buf, name);
	return result->buf;
}

const char *find_hook(struct repository *r, const char *name)
{
	static struct strbuf path = STRBUF_INIT;

	int found_hook;

	strbuf_reset(&path);
	if (have_git_dir())
		repo_git_path_replace(r, &path, "hooks/%s", name);
	else if (!hook_path_early(name, &path))
		return NULL;

	found_hook = access(path.buf, X_OK) >= 0;
#ifdef STRIP_EXTENSION
	if (!found_hook) {
		int err = errno;

		strbuf_addstr(&path, STRIP_EXTENSION);
		found_hook = access(path.buf, X_OK) >= 0;
		if (!found_hook)
			errno = err;
	}
#endif

	if (!found_hook) {
		if (errno == EACCES && advice_enabled(ADVICE_IGNORED_HOOK)) {
			static struct string_list advise_given = STRING_LIST_INIT_DUP;

			if (!string_list_lookup(&advise_given, name)) {
				string_list_insert(&advise_given, name);
				advise(_("The '%s' hook was ignored because "
					 "it's not set as executable.\n"
					 "You can disable this warning with "
					 "`git config set advice.ignoredHook false`."),
				       path.buf);
			}
		}
		return NULL;
	}
	return path.buf;
}

int hook_exists(struct repository *r, const char *name)
{
	return !!find_hook(r, name);
}

static int pick_next_hook(struct child_process *cp,
			  struct strbuf *out UNUSED,
			  void *pp_cb,
			  void **pp_task_cb UNUSED)
{
	struct hook_cb_data *hook_cb = pp_cb;
	const char *hook_path = hook_cb->hook_path;

	if (!hook_path)
		return 0;

	cp->no_stdin = 1;
	strvec_pushv(&cp->env, hook_cb->options->env.v);
	/* reopen the file for stdin; run_command closes it. */
	if (hook_cb->options->path_to_stdin) {
		cp->no_stdin = 0;
		cp->in = xopen(hook_cb->options->path_to_stdin, O_RDONLY);
	}
	cp->stdout_to_stderr = 1;
	cp->trace2_hook_name = hook_cb->hook_name;
	cp->dir = hook_cb->options->dir;

	strvec_push(&cp->args, hook_path);
	strvec_pushv(&cp->args, hook_cb->options->args.v);

	/*
	 * This pick_next_hook() will be called again, we're only
	 * running one hook, so indicate that no more work will be
	 * done.
	 */
	hook_cb->hook_path = NULL;

	return 1;
}

static int notify_start_failure(struct strbuf *out UNUSED,
				void *pp_cb,
				void *pp_task_cp UNUSED)
{
	struct hook_cb_data *hook_cb = pp_cb;

	hook_cb->rc |= 1;

	return 1;
}

static int notify_hook_finished(int result,
				struct strbuf *out UNUSED,
				void *pp_cb,
				void *pp_task_cb UNUSED)
{
	struct hook_cb_data *hook_cb = pp_cb;
	struct run_hooks_opt *opt = hook_cb->options;

	hook_cb->rc |= result;

	if (opt->invoked_hook)
		*opt->invoked_hook = 1;

	return 0;
}

static void run_hooks_opt_clear(struct run_hooks_opt *options)
{
	strvec_clear(&options->env);
	strvec_clear(&options->args);
}

static char *get_post_index_change_sentinel_name(struct repository *r)
{
	struct strbuf path = STRBUF_INIT;
	const char *sid = tr2_sid_get();
	char *slash = strchr(sid, '/');

	/*
	 * Name is based on top-level SID, so children can indicate that
	 * the top-level process should run the post-command hook.
	 */
	if (slash)
		*slash = 0;

	/*
	 * Do not write to hooks directory, as it could be redirected
	 * somewhere like the source tree.
	 */
	repo_git_path_replace(r, &path, "info/index-change-%s.snt", sid);

	return strbuf_detach(&path, NULL);
}

static int write_post_index_change_sentinel(struct repository *r)
{
	char *path = get_post_index_change_sentinel_name(r);
	FILE *fp = xfopen(path, "w");

	if (fp) {
		fprintf(fp, "run post-command hook");
		fclose(fp);
	}

	free(path);
	return fp ? 0 : -1;
}

/**
 * Try to delete the sentinel file for this repository. If that succeeds, then
 * return 1.
 */
static int post_index_change_sentinel_exists(struct repository *r)
{
	char *path;
	int res = 1;

	/* It can't exist if we don't have a gitdir. */
	if (!r->gitdir)
		return 0;

	path = get_post_index_change_sentinel_name(r);

	if (unlink(path)) {
		if (is_missing_file_error(errno))
			res = 0;
		else
			warning_errno("failed to remove index-change sentinel file '%s'", path);
	}

	free(path);
	return res;
}

static int check_worktree_change(const char *key, const char *value,
				       UNUSED const struct config_context *ctx,
				       void *data)
{
	int *enabled = data;

	if (!strcmp(key, "postcommand.strategy") &&
	    !strcasecmp(value, "worktree-change")) {
		*enabled = 1;
		return 1;
	}

	return 0;
}

/**
 * See if we can replace the requested hook with an internal behavior.
 * Returns 0 if the real hook should run. Returns nonzero if we instead
 * executed custom internal behavior and the real hook should not run.
 */
static int handle_hook_replacement(struct repository *r,
				   const char *hook_name,
				   struct strvec *args)
{
	int enabled = 0;

	read_early_config(r, check_worktree_change, &enabled);

	if (!enabled)
		return 0;

	if (!strcmp(hook_name, "post-index-change")) {
		/* Create a sentinel file only if the worktree changed. */
		if (!strcmp(args->v[0], "1"))
			write_post_index_change_sentinel(r);

		/* We don't skip post-index-change hooks that exist. */
		return 0;
	}
	if (!strcmp(hook_name, "post-command") &&
	    !post_index_change_sentinel_exists(r)) {
		/* We skip the post-command hook in this case. */
		return 1;
	}

	return 0;
}

int run_hooks_opt(struct repository *r, const char *hook_name,
		  struct run_hooks_opt *options)
{
	struct strbuf abs_path = STRBUF_INIT;
	struct hook_cb_data cb_data = {
		.rc = 0,
		.hook_name = hook_name,
		.options = options,
	};
	const char *hook_path;
	int ret = 0;
	const struct run_process_parallel_opts opts = {
		.tr2_category = "hook",
		.tr2_label = hook_name,

		.processes = 1,
		.ungroup = 1,

		.get_next_task = pick_next_hook,
		.start_failure = notify_start_failure,
		.task_finished = notify_hook_finished,

		.data = &cb_data,
	};

	/* Interject hook behavior depending on strategy. */
	if (r && handle_hook_replacement(r, hook_name, &options->args))
		return 0;

	hook_path = find_hook(r, hook_name);

	/*
	 * Backwards compatibility hack in VFS for Git: when originally
	 * introduced (and used!), it was called `post-indexchanged`, but this
	 * name was changed during the review on the Git mailing list.
	 *
	 * Therefore, when the `post-index-change` hook is not found, let's
	 * look for a hook with the old name (which would be found in case of
	 * already-existing checkouts).
	 */
	if (!hook_path && !strcmp(hook_name, "post-index-change"))
		hook_path = find_hook(r, "post-indexchanged");

	if (!options)
		BUG("a struct run_hooks_opt must be provided to run_hooks");

	if (options->invoked_hook)
		*options->invoked_hook = 0;

	if (!hook_path && !options->error_if_missing)
		goto cleanup;

	if (!hook_path) {
		ret = error("cannot find a hook named %s", hook_name);
		goto cleanup;
	}

	cb_data.hook_path = hook_path;
	if (options->dir) {
		strbuf_add_absolute_path(&abs_path, hook_path);
		cb_data.hook_path = abs_path.buf;
	}

	run_processes_parallel(&opts);
	ret = cb_data.rc;
cleanup:
	strbuf_release(&abs_path);
	run_hooks_opt_clear(options);
	return ret;
}

int run_hooks(struct repository *r, const char *hook_name)
{
	struct run_hooks_opt opt = RUN_HOOKS_OPT_INIT;

	return run_hooks_opt(r, hook_name, &opt);
}

int run_hooks_l(struct repository *r, const char *hook_name, ...)
{
	struct run_hooks_opt opt = RUN_HOOKS_OPT_INIT;
	va_list ap;
	int result;
	const char *arg;

	va_start(ap, hook_name);
	while ((arg = va_arg(ap, const char *)))
		strvec_push(&opt.args, arg);
	va_end(ap);

	result = run_hooks_opt(r, hook_name, &opt);
	strvec_clear(&opt.args);
	return result;
}
