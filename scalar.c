/*
 * The Scalar command-line interface.
 */

#define USE_THE_REPOSITORY_VARIABLE

#include "git-compat-util.h"
#include "abspath.h"
#include "gettext.h"
#include "hex.h"
#include "parse-options.h"
#include "config.h"
#include "environment.h"
#include "run-command.h"
#include "simple-ipc.h"
#include "fsmonitor-ipc.h"
#include "fsmonitor-settings.h"
#include "refs.h"
#include "dir.h"
#include "object-file.h"
#include "packfile.h"
#include "help.h"
#include "setup.h"
#include "wrapper.h"
#include "trace2.h"
#include "json-parser.h"
#include "remote.h"
#include "path.h"

static int is_unattended(void) {
	return git_env_bool("Scalar_UNATTENDED", 0);
}

static void setup_enlistment_directory(int argc, const char **argv,
				       const char * const *usagestr,
				       const struct option *options,
				       struct strbuf *enlistment_root)
{
	struct strbuf path = STRBUF_INIT;
	int enlistment_is_repo_parent = 0;
	size_t len;

	if (startup_info->have_repository)
		BUG("gitdir already set up?!?");

	if (argc > 1)
		usage_with_options(usagestr, options);

	/* find the worktree, determine its corresponding root */
	if (argc == 1) {
		strbuf_add_absolute_path(&path, argv[0]);
		if (!is_directory(path.buf))
			die(_("'%s' does not exist"), path.buf);
		if (chdir(path.buf) < 0)
			die_errno(_("could not switch to '%s'"), path.buf);
	} else if (strbuf_getcwd(&path) < 0)
		die(_("need a working directory"));

	strbuf_trim_trailing_dir_sep(&path);
#ifdef GIT_WINDOWS_NATIVE
	convert_slashes(path.buf);
#endif

	/* check if currently in enlistment root with src/ workdir */
	len = path.len;
	strbuf_addstr(&path, "/src");
	if (is_nonbare_repository_dir(&path)) {
		enlistment_is_repo_parent = 1;
		if (chdir(path.buf) < 0)
			die_errno(_("could not switch to '%s'"), path.buf);
	}
	strbuf_setlen(&path, len);

	setup_git_directory();

	if (!the_repository->worktree)
		die(_("Scalar enlistments require a worktree"));

	if (enlistment_root) {
		if (enlistment_is_repo_parent)
			strbuf_addbuf(enlistment_root, &path);
		else
			strbuf_addstr(enlistment_root, the_repository->worktree);
	}

	strbuf_release(&path);
}

static int git_retries = 3;

LAST_ARG_MUST_BE_NULL
static int run_git(const char *arg, ...)
{
	va_list args;
	const char *p;
	struct strvec argv = STRVEC_INIT;
	int res = 0, attempts;

	va_start(args, arg);
	strvec_push(&argv, arg);
	while ((p = va_arg(args, const char *)))
		strvec_push(&argv, p);
	va_end(args);

	for (attempts = 0, res = 1;
	     res && attempts < git_retries;
	     attempts++) {
		struct child_process cmd = CHILD_PROCESS_INIT;

		cmd.git_cmd = 1;
		strvec_pushv(&cmd.args, argv.v);
		res = run_command(&cmd);
	}

	strvec_clear(&argv);
	return res;
}

static const char *ensure_absolute_path(const char *path, char **absolute)
{
	struct strbuf buf = STRBUF_INIT;

	if (is_absolute_path(path))
		return path;

	strbuf_realpath_forgiving(&buf, path, 1);
	free(*absolute);
	*absolute = strbuf_detach(&buf, NULL);
	return *absolute;
}

struct scalar_config {
	const char *key;
	const char *value;
	int overwrite_on_reconfigure;
};

static int set_scalar_config(const struct scalar_config *config, int reconfigure)
{
	char *value = NULL;
	int res;

	if ((reconfigure && config->overwrite_on_reconfigure) ||
	    repo_config_get_string(the_repository, config->key, &value)) {
		trace2_data_string("scalar", the_repository, config->key, "created");
		res = repo_config_set_gently(the_repository, config->key, config->value);
	} else {
		trace2_data_string("scalar", the_repository, config->key, "exists");
		res = 0;
	}

	free(value);
	return res;
}

static int have_fsmonitor_support(void)
{
	return fsmonitor_ipc__is_supported() &&
	       fsm_settings__get_reason(the_repository) == FSMONITOR_REASON_OK;
}

static int set_recommended_config(int reconfigure)
{
	struct scalar_config config[] = {
		/* Required */
		{ "am.keepCR", "true", 1 },
		{ "core.FSCache", "true", 1 },
		{ "core.multiPackIndex", "true", 1 },
		{ "core.preloadIndex", "true", 1 },
		{ "core.untrackedCache", "true", 1 },
		{ "core.logAllRefUpdates", "true", 1 },
		{ "credential.https://dev.azure.com.useHttpPath", "true", 1 },
		{ "credential.validate", "false", 1 }, /* GCM4W-only */
		{ "gc.auto", "0", 1 },
		{ "gui.GCWarning", "false", 1 },
		{ "index.skipHash", "false", 1 },
		{ "index.threads", "true", 1 },
		{ "index.version", "4", 1 },
		{ "merge.stat", "false", 1 },
		{ "merge.renames", "true", 1 },
		{ "pack.useBitmaps", "false", 1 },
		{ "pack.useSparse", "true", 1 },
		{ "receive.autoGC", "false", 1 },
		{ "feature.manyFiles", "false", 1 },
		{ "feature.experimental", "false", 1 },
		{ "fetch.unpackLimit", "1", 1 },
		{ "fetch.writeCommitGraph", "false", 1 },
#ifdef WIN32
		{ "http.sslBackend", "schannel", 1 },
#endif
		/* Optional */
		{ "status.aheadBehind", "false" },
		{ "commitGraph.changedPaths", "true" },
		{ "commitGraph.generationVersion", "1" },
		{ "core.autoCRLF", "false" },
		{ "core.safeCRLF", "false" },
		{ "fetch.showForcedUpdates", "false" },
		{ "pack.usePathWalk", "true" },
		{ "core.configWriteLockTimeoutMS", "150" },
		{ NULL, NULL },
	};
	int i;
	char *value;

	/*
	 * If a user has "core.usebuiltinfsmonitor" enabled, try to switch to
	 * the new (non-deprecated) setting (core.fsmonitor).
	 */
	if (!repo_config_get_string(the_repository, "core.usebuiltinfsmonitor", &value)) {
		char *dummy = NULL;
		if (repo_config_get_string(the_repository, "core.fsmonitor", &dummy) &&
		    repo_config_set_gently(the_repository, "core.fsmonitor", value) < 0)
			return error(_("could not configure %s=%s"),
				     "core.fsmonitor", value);
		if (repo_config_set_gently(the_repository, "core.usebuiltinfsmonitor", NULL) < 0)
			return error(_("could not configure %s=%s"),
				     "core.useBuiltinFSMonitor", "NULL");
		free(value);
		free(dummy);
	}

	for (i = 0; config[i].key; i++) {
		if (set_scalar_config(config + i, reconfigure))
			return error(_("could not configure %s=%s"),
				     config[i].key, config[i].value);
	}

	if (have_fsmonitor_support()) {
		struct scalar_config fsmonitor = { "core.fsmonitor", "true" };
		if (set_scalar_config(&fsmonitor, reconfigure))
			return error(_("could not configure %s=%s"),
				     fsmonitor.key, fsmonitor.value);
	}

	/*
	 * Set HTTP/1.1 for Azure DevOps URLs
	 * We check for dev.azure.com/ and .visualstudio.com/ patterns
	 * which are sufficient to identify ADO URLs (including formats like
	 * https://orgname@dev.azure.com/...)
	 */
	if (!repo_config_get_string(the_repository, "remote.origin.url", &value)) {
		if (starts_with(value, "https://dev.azure.com/") ||
		    strstr(value, "@dev.azure.com/") ||
		    strstr(value, ".visualstudio.com/")) {
			struct strbuf key = STRBUF_INIT;
			strbuf_addf(&key, "http.%s.version", value);
			FREE_AND_NULL(value);

			if (reconfigure || repo_config_get_string(the_repository, key.buf, &value)) {
				trace2_data_string("scalar", the_repository, key.buf, "created");
				if (repo_config_set_gently(the_repository, key.buf, "HTTP/1.1") < 0) {
					strbuf_release(&key);
					return error(_("could not configure %s=%s"),
						     key.buf, "HTTP/1.1");
				}
			}
			strbuf_release(&key);
		}
		FREE_AND_NULL(value);
	}

	/*
	 * The `log.excludeDecoration` setting is special because it allows
	 * for multiple values.
	 */
	if (repo_config_get_string(the_repository, "log.excludeDecoration", &value)) {
		trace2_data_string("scalar", the_repository,
				   "log.excludeDecoration", "created");
		if (repo_config_set_multivar_gently(the_repository, "log.excludeDecoration",
						    "refs/prefetch/*",
						    CONFIG_REGEX_NONE, 0))
			return error(_("could not configure "
				       "log.excludeDecoration"));
	} else {
		trace2_data_string("scalar", the_repository,
				   "log.excludeDecoration", "exists");
		free(value);
	}

	return 0;
}

/**
 * Enable or disable the maintenance mode for the current repository:
 *
 * * If 'enable' is nonzero, run 'git maintenance start'.
 * * If 'enable' is zero, run 'git maintenance unregister --force'.
 */
static int toggle_maintenance(int enable)
{
	unsigned long ul;

	if (repo_config_get_ulong(the_repository, "core.configWriteLockTimeoutMS", &ul))
		git_config_push_parameter("core.configWriteLockTimeoutMS=150");

	return run_git("maintenance",
		       enable ? "start" : "unregister",
		       enable ? NULL : "--force",
		       NULL);
}

static int add_or_remove_enlistment(int add)
{
	int res;
	unsigned long ul;

	if (!the_repository->worktree)
		die(_("Scalar enlistments require a worktree"));

	if (repo_config_get_ulong(the_repository, "core.configWriteLockTimeoutMS", &ul))
		git_config_push_parameter("core.configWriteLockTimeoutMS=150");

	res = run_git("config", "--global", "--get", "--fixed-value",
		      "scalar.repo", the_repository->worktree, NULL);

	/*
	 * If we want to add and the setting is already there, then do nothing.
	 * If we want to remove and the setting is not there, then do nothing.
	 */
	if ((add && !res) || (!add && res))
		return 0;

	return run_git("config", "--global", add ? "--add" : "--unset",
		       add ? "--no-fixed-value" : "--fixed-value",
		       "scalar.repo", the_repository->worktree, NULL);
}

static int start_fsmonitor_daemon(void)
{
	ASSERT(have_fsmonitor_support());

	if (fsmonitor_ipc__get_state() != IPC_STATE__LISTENING)
		return run_git("fsmonitor--daemon", "start", NULL);

	return 0;
}

static int stop_fsmonitor_daemon(void)
{
	ASSERT(have_fsmonitor_support());

	if (fsmonitor_ipc__get_state() == IPC_STATE__LISTENING)
		return run_git("fsmonitor--daemon", "stop", NULL);

	return 0;
}

/**
 * Register the current directory as a Scalar enlistment, and set the
 * recommended configuration.
 *
 * * If 'maintenance' is non-zero, then enable background maintenance.
 * * If 'maintenance' is zero, then leave background maintenance as it is
 *   currently configured.
 */
static int register_dir(int maintenance)
{
	if (add_or_remove_enlistment(1))
		return error(_("could not add enlistment"));

	if (set_recommended_config(0))
		return error(_("could not set recommended config"));

	if (maintenance &&
	    toggle_maintenance(maintenance))
		warning(_("could not toggle maintenance"));

	if (have_fsmonitor_support() && start_fsmonitor_daemon()) {
		return error(_("could not start the FSMonitor daemon"));
	}

	return 0;
}

static int unregister_dir(void)
{
	int res = 0;

	if (toggle_maintenance(0))
		res = error(_("could not turn off maintenance"));

	if (add_or_remove_enlistment(0))
		res = error(_("could not remove enlistment"));

	return res;
}

/* printf-style interface, expects `<key>=<value>` argument */
__attribute__((format (printf, 1, 2)))
static int set_config(const char *fmt, ...)
{
	struct strbuf buf = STRBUF_INIT;
	char *value;
	int res;
	va_list args;

	va_start(args, fmt);
	strbuf_vaddf(&buf, fmt, args);
	va_end(args);

	value = strchr(buf.buf, '=');
	if (value)
		*(value++) = '\0';
	res = repo_config_set_gently(the_repository, buf.buf, value);
	strbuf_release(&buf);

	return res;
}

static int list_cache_server_urls(struct json_iterator *it)
{
	const char *p;
	char *q;
	long l;

	if (it->type == JSON_STRING &&
	    skip_iprefix(it->key.buf, ".CacheServers[", &p) &&
	    (l = strtol(p, &q, 10)) >= 0 && p != q &&
	    !strcasecmp(q, "].Url"))
		printf("#%ld: %s\n", l, it->string_value.buf);

	return 0;
}

/* Find N for which .CacheServers[N].GlobalDefault == true */
static int get_cache_server_index(struct json_iterator *it)
{
	const char *p;
	char *q;
	long l;

	if (it->type == JSON_TRUE &&
	    skip_iprefix(it->key.buf, ".CacheServers[", &p) &&
	    (l = strtol(p, &q, 10)) >= 0 && p != q &&
	    !strcasecmp(q, "].GlobalDefault")) {
		*(long *)it->fn_data = l;
		return 1;
	}

	return 0;
}

struct cache_server_url_data {
	char *key, *url;
};

/* Get .CacheServers[N].Url */
static int get_cache_server_url(struct json_iterator *it)
{
	struct cache_server_url_data *data = it->fn_data;

	if (it->type == JSON_STRING &&
	    !strcasecmp(data->key, it->key.buf)) {
		data->url = strbuf_detach(&it->string_value, NULL);
		return 1;
	}

	return 0;
}

static int can_url_support_gvfs(const char *url)
{
	return starts_with(url, "https://") ||
		(git_env_bool("GIT_TEST_ALLOW_GVFS_VIA_HTTP", 0) &&
		 starts_with(url, "http://"));
}

/*
 * If `cache_server_url` is `NULL`, print the list to `stdout`.
 *
 * Since `gvfs-helper` requires a Git directory, this _must_ be run in
 * a worktree.
 */
static int supports_gvfs_protocol(const char *url, char **cache_server_url)
{
	struct child_process cp = CHILD_PROCESS_INIT;
	struct strbuf out = STRBUF_INIT;

	/*
	 * The GVFS protocol is only supported via https://; For testing, we
	 * also allow http://.
	 */
	if (!can_url_support_gvfs(url))
		return 0;

	cp.git_cmd = 1;
	strvec_pushl(&cp.args, "-c", "http.version=HTTP/1.1",
		     "gvfs-helper", "--remote", url, "config", NULL);
	if (!pipe_command(&cp, NULL, 0, &out, 512, NULL, 0)) {
		long l = 0;
		struct json_iterator it =
			JSON_ITERATOR_INIT(out.buf, get_cache_server_index, &l);
		struct cache_server_url_data data = { .url = NULL };

		if (!cache_server_url) {
			it.fn = list_cache_server_urls;
			if (iterate_json(&it) < 0) {
				reset_iterator(&it);
				strbuf_release(&out);
				return error("JSON parse error");
			}
			reset_iterator(&it);
			strbuf_release(&out);
			return 0;
		}

		if (iterate_json(&it) < 0) {
			reset_iterator(&it);
			strbuf_release(&out);
			return error("JSON parse error");
		}
		data.key = xstrfmt(".CacheServers[%ld].Url", l);
		it.fn = get_cache_server_url;
		it.fn_data = &data;
		if (iterate_json(&it) < 0) {
			reset_iterator(&it);
			strbuf_release(&out);
			return error("JSON parse error");
		}
		*cache_server_url = data.url;
		free(data.key);
		reset_iterator(&it);
		strbuf_release(&out);
		return 1;
	}
	strbuf_release(&out);
	/* error out quietly, unless we wanted to list URLs */
	return cache_server_url ?
		0 : error(_("Could not access gvfs/config endpoint"));
}

static char *default_cache_root(const char *root)
{
	const char *env;

	if (is_unattended()) {
		struct strbuf path = STRBUF_INIT;
		strbuf_addstr(&path, root);
		strip_last_path_component(&path);
		strbuf_addstr(&path, "/.scalarCache");
		return strbuf_detach(&path, NULL);
	}

#ifdef WIN32
	(void)env;
	return xstrfmt("%.*s.scalarCache", offset_1st_component(root), root);
#elif defined(__APPLE__)
	if ((env = getenv("HOME")) && *env)
		return xstrfmt("%s/.scalarCache", env);
	return NULL;
#else
	if ((env = getenv("XDG_CACHE_HOME")) && *env)
		return xstrfmt("%s/scalar", env);
	if ((env = getenv("HOME")) && *env)
		return xstrfmt("%s/.cache/scalar", env);
	return NULL;
#endif
}

static int get_repository_id(struct json_iterator *it)
{
	if (it->type == JSON_STRING &&
	    !strcasecmp(".repository.id", it->key.buf)) {
		*(char **)it->fn_data = strbuf_detach(&it->string_value, NULL);
		return 1;
	}

	return 0;
}

/* Needs to run this in a worktree; gvfs-helper requires a Git repository */
static char *get_cache_key(const char *url)
{
	struct child_process cp = CHILD_PROCESS_INIT;
	struct strbuf out = STRBUF_INIT;
	char *cache_key = NULL;

	/*
	 * The GVFS protocol is only supported via https://; For testing, we
	 * also allow http://.
	 */
	if (!git_env_bool("SCALAR_TEST_SKIP_VSTS_INFO", 0) &&
	    can_url_support_gvfs(url)) {
		cp.git_cmd = 1;
		strvec_pushl(&cp.args, "gvfs-helper", "--remote", url,
			     "endpoint", "vsts/info", NULL);
		if (!pipe_command(&cp, NULL, 0, &out, 512, NULL, 0)) {
			char *id = NULL;
			struct json_iterator it =
				JSON_ITERATOR_INIT(out.buf, get_repository_id,
						   &id);

			if (iterate_json(&it) < 0)
				warning("JSON parse error (%s)", out.buf);
			else if (id)
				cache_key = xstrfmt("id_%s", id);
			free(id);
		}
	}

	if (!cache_key) {
		struct strbuf downcased = STRBUF_INIT;
		int hash_algo_index = hash_algo_by_name("sha1");
		const struct git_hash_algo *hash_algo = hash_algo_index < 0 ?
			the_hash_algo : &hash_algos[hash_algo_index];
		struct git_hash_ctx ctx;
		unsigned char hash[GIT_MAX_RAWSZ];

		strbuf_addstr(&downcased, url);
		strbuf_tolower(&downcased);

		hash_algo->init_fn(&ctx);
		hash_algo->update_fn(&ctx, downcased.buf, downcased.len);
		hash_algo->final_fn(hash, &ctx);

		strbuf_release(&downcased);

		cache_key = xstrfmt("url_%s",
				    hash_to_hex_algop(hash, hash_algo));
	}

	strbuf_release(&out);
	return cache_key;
}

static char *remote_default_branch(const char *url)
{
	struct child_process cp = CHILD_PROCESS_INIT;
	struct strbuf out = STRBUF_INIT;

	cp.git_cmd = 1;
	strvec_pushl(&cp.args, "ls-remote", "--symref", url, "HEAD", NULL);
	if (!pipe_command(&cp, NULL, 0, &out, 0, NULL, 0)) {
		const char *line = out.buf;

		while (*line) {
			const char *eol = strchrnul(line, '\n'), *p;
			size_t len = eol - line;
			char *branch;

			if (!skip_prefix(line, "ref: ", &p) ||
			    !strip_suffix_mem(line, &len, "\tHEAD")) {
				line = eol + (*eol == '\n');
				continue;
			}

			eol = line + len;
			if (skip_prefix(p, "refs/heads/", &p)) {
				branch = xstrndup(p, eol - p);
				strbuf_release(&out);
				return branch;
			}

			error(_("remote HEAD is not a branch: '%.*s'"),
			      (int)(eol - p), p);
			strbuf_release(&out);
			return NULL;
		}
	}
	warning(_("failed to get default branch name from remote; "
		  "using local default"));
	strbuf_reset(&out);

	child_process_init(&cp);
	cp.git_cmd = 1;
	strvec_pushl(&cp.args, "symbolic-ref", "--short", "HEAD", NULL);
	if (!pipe_command(&cp, NULL, 0, &out, 0, NULL, 0)) {
		strbuf_trim(&out);
		return strbuf_detach(&out, NULL);
	}

	strbuf_release(&out);
	error(_("failed to get default branch name"));
	return NULL;
}

static int delete_enlistment(struct strbuf *enlistment)
{
	struct strbuf parent = STRBUF_INIT;
	size_t offset;
	char *path_sep;

	if (unregister_dir())
		return error(_("failed to unregister repository"));

	/*
	 * Change the current directory to one outside of the enlistment so
	 * that we may delete everything underneath it.
	 */
	offset = offset_1st_component(enlistment->buf);
	path_sep = find_last_dir_sep(enlistment->buf + offset);
	strbuf_add(&parent, enlistment->buf,
		   path_sep ? (size_t) (path_sep - enlistment->buf) : offset);
	if (chdir(parent.buf) < 0) {
		int res = error_errno(_("could not switch to '%s'"), parent.buf);
		strbuf_release(&parent);
		return res;
	}
	strbuf_release(&parent);

	if (have_fsmonitor_support() && stop_fsmonitor_daemon())
		return error(_("failed to stop the FSMonitor daemon"));

	if (remove_dir_recursively(enlistment, 0))
		return error(_("failed to delete enlistment directory"));

	return 0;
}

/*
 * Dummy implementation; Using `get_version_info()` would cause a link error
 * without this.
 */
void load_builtin_commands(const char *prefix UNUSED,
			   struct cmdnames *cmds UNUSED)
{
	die("not implemented");
}

static int init_shared_object_cache(const char *url,
				    const char *local_cache_root)
{
	struct strbuf buf = STRBUF_INIT;
	int res = 0;
	char *cache_key = NULL, *shared_cache_path = NULL, *alternates = NULL;

	if (!(cache_key = get_cache_key(url))) {
		res = error(_("could not determine cache key for '%s'"), url);
		goto cleanup;
	}

	shared_cache_path = xstrfmt("%s/%s", local_cache_root, cache_key);
	if (set_config("gvfs.sharedCache=%s", shared_cache_path)) {
		res = error(_("could not configure shared cache"));
		goto cleanup;
	}

	strbuf_addf(&buf, "%s/pack", shared_cache_path);
	switch (safe_create_leading_directories(the_repository, buf.buf)) {
	case SCLD_OK: case SCLD_EXISTS:
		break; /* okay */
	default:
		res = error_errno(_("could not initialize '%s'"), buf.buf);
		goto cleanup;
	}

	alternates = repo_git_path(the_repository, "objects/info/alternates");
	write_file(alternates, "%s\n", shared_cache_path);

	cleanup:
	strbuf_release(&buf);
	free(shared_cache_path);
	free(cache_key);
	free(alternates);
	return res;
}

static int cmd_clone(int argc, const char **argv)
{
	int dummy = 0;
	const char *branch = NULL;
	char *branch_to_free = NULL;
	int full_clone = 0, single_branch = 0, show_progress = isatty(2);
	int src = 1, tags = 1, maintenance = 1;
	const char *cache_server_url = NULL, *local_cache_root = NULL;
	char *default_cache_server_url = NULL, *local_cache_root_abs = NULL;
	int gvfs_protocol = -1;

	struct option clone_options[] = {
		OPT_STRING('b', "branch", &branch, N_("<branch>"),
			   N_("branch to checkout after clone")),
		OPT_BOOL(0, "full-clone", &full_clone,
			 N_("when cloning, create full working directory")),
		OPT_BOOL(0, "single-branch", &single_branch,
			 N_("only download metadata for the branch that will "
			    "be checked out")),
		OPT_BOOL(0, "src", &src,
			 N_("create repository within 'src' directory")),
		OPT_BOOL(0, "tags", &tags,
			 N_("specify if tags should be fetched during clone")),
		OPT_BOOL(0, "maintenance", &maintenance,
			 N_("specify if background maintenance should be enabled")),
		OPT_BOOL(0, "gvfs-protocol", &gvfs_protocol,
			 N_("force enable (or disable) the GVFS Protocol")),
		OPT_STRING(0, "cache-server-url", &cache_server_url,
			   N_("<url>"),
			   N_("the url or friendly name of the cache server")),
		OPT_STRING(0, "local-cache-path", &local_cache_root,
			   N_("<path>"),
			   N_("override the path for the local Scalar cache")),
		OPT_HIDDEN_BOOL(0, "no-fetch-commits-and-trees",
				&dummy, N_("no longer used")),
		OPT_END(),
	};
	const char * const clone_usage[] = {
		N_("scalar clone [--single-branch] [--branch <main-branch>] [--full-clone]\n"
		   "\t[--[no-]src] [--[no-]tags] [--[no-]maintenance] <url> [<enlistment>]"),
		NULL
	};
	const char *url;
	char *enlistment = NULL, *dir = NULL;
	struct strbuf buf = STRBUF_INIT;
	int res;

	argc = parse_options(argc, argv, NULL, clone_options, clone_usage, 0);

	if (argc == 2) {
		url = argv[0];
		enlistment = xstrdup(argv[1]);
	} else if (argc == 1) {
		url = argv[0];

		strbuf_addstr(&buf, url);
		/* Strip trailing slashes, if any */
		while (buf.len > 0 && is_dir_sep(buf.buf[buf.len - 1]))
			strbuf_setlen(&buf, buf.len - 1);
		/* Strip suffix `.git`, if any */
		strbuf_strip_suffix(&buf, ".git");

		enlistment = find_last_dir_sep(buf.buf);
		if (!enlistment) {
			die(_("cannot deduce worktree name from '%s'"), url);
		}
		enlistment = xstrdup(enlistment + 1);
	} else {
		usage_msg_opt(_("You must specify a repository to clone."),
			      clone_usage, clone_options);
	}

	if (is_directory(enlistment))
		die(_("directory '%s' exists already"), enlistment);

	ensure_absolute_path(enlistment, &enlistment);

	if (src)
		dir = xstrfmt("%s/src", enlistment);
	else
		dir = xstrdup(enlistment);

	if (!local_cache_root)
		local_cache_root = local_cache_root_abs =
			default_cache_root(enlistment);
	else
		local_cache_root = ensure_absolute_path(local_cache_root,
							&local_cache_root_abs);

	if (!local_cache_root)
		die(_("could not determine local cache root"));

	strbuf_reset(&buf);
	if (branch)
		strbuf_addf(&buf, "init.defaultBranch=%s", branch);
	else {
		char *b = repo_default_branch_name(the_repository, 1);
		strbuf_addf(&buf, "init.defaultBranch=%s", b);
		free(b);
	}

	if ((res = run_git("-c", buf.buf, "init", "--", dir, NULL)))
		goto cleanup;

	if (chdir(dir) < 0) {
		res = error_errno(_("could not switch to '%s'"), dir);
		goto cleanup;
	}

	setup_git_directory();

	repo_config(the_repository, git_default_config, NULL);

	/*
	 * This `dir_inside_of()` call relies on git_config() having parsed the
	 * newly-initialized repository config's `core.ignoreCase` value.
	 */
	if (dir_inside_of(local_cache_root, dir) >= 0) {
		struct strbuf path = STRBUF_INIT;

		strbuf_addstr(&path, enlistment);
		if (chdir("../..") < 0 ||
		    remove_dir_recursively(&path, 0) < 0)
			die(_("'--local-cache-path' cannot be inside the src "
			      "folder;\nCould not remove '%s'"), enlistment);

		die(_("'--local-cache-path' cannot be inside the src folder"));
	}

	/* common-main already logs `argv` */
	trace2_def_repo(the_repository);
	trace2_data_intmax("scalar", the_repository, "unattended",
			   is_unattended());

	if (!branch && !(branch = branch_to_free = remote_default_branch(url))) {
		res = error(_("failed to get default branch for '%s'"), url);
		goto cleanup;
	}

	if (set_config("remote.origin.url=%s", url) ||
	    set_config("remote.origin.fetch="
		       "+refs/heads/%s:refs/remotes/origin/%s",
		       single_branch ? branch : "*",
		       single_branch ? branch : "*")) {
		res = error(_("could not configure remote in '%s'"), dir);
		goto cleanup;
	}

	if (!tags && set_config("remote.origin.tagOpt=--no-tags")) {
		res = error(_("could not disable tags in '%s'"), dir);
		goto cleanup;
	}

	if (set_config("credential.https://dev.azure.com.useHttpPath=true")) {
		res = error(_("could not configure credential.useHttpPath"));
		goto cleanup;
	}

	/* Is --[no-]gvfs-protocol unspecified? Infer from url. */
	if (gvfs_protocol < 0) {
		if (cache_server_url ||
		    strstr(url, "dev.azure.com/") ||
		    strstr(url, "visualstudio.com"))
			gvfs_protocol = 1;
		else
			gvfs_protocol = 0;
	}

	if (gvfs_protocol && !supports_gvfs_protocol(url, &default_cache_server_url))
		die(_("failed to contact server via GVFS Protocol"));

	if (gvfs_protocol) {
		if ((res = init_shared_object_cache(url, local_cache_root)))
			goto cleanup;
		if (!cache_server_url)
			cache_server_url = default_cache_server_url;
		if (set_config("core.useGVFSHelper=true") ||
		    set_config("core.gvfs=150") ||
		    set_config("http.%s.version=HTTP/1.1", url)) {
			res = error(_("could not turn on GVFS helper"));
			goto cleanup;
		}
		if (cache_server_url &&
		    set_config("gvfs.cache-server=%s", cache_server_url)) {
			res = error(_("could not configure cache server"));
			goto cleanup;
		}
		if (cache_server_url)
			fprintf(stderr, "Cache server URL: %s\n",
				cache_server_url);
	} else {
		if (set_config("core.useGVFSHelper=false") ||
		    set_config("remote.origin.promisor=true") ||
		    set_config("remote.origin.partialCloneFilter=blob:none")) {
			res = error(_("could not configure partial clone in "
				      "'%s'"), dir);
			goto cleanup;
		}
	}

	if (!full_clone &&
	    (res = run_git("sparse-checkout", "init", "--cone", NULL)))
		goto cleanup;

	if (set_recommended_config(0))
		return error(_("could not configure '%s'"), dir);

	if ((res = run_git("fetch", "--quiet",
				show_progress ? "--progress" : "--no-progress",
				"origin",
				(tags ? NULL : "--no-tags"),
				NULL))) {
		if (gvfs_protocol) {
			res = error(_("failed to prefetch commits and trees"));
			goto cleanup;
		}

		warning(_("partial clone failed; attempting full clone"));

		if (set_config("remote.origin.promisor") ||
		    set_config("remote.origin.partialCloneFilter")) {
			res = error(_("could not configure for full clone"));
			goto cleanup;
		}

		if ((res = run_git("fetch", "--quiet",
					show_progress ? "--progress" : "--no-progress",
					"origin", NULL)))
			goto cleanup;
	}

	if ((res = set_config("branch.%s.remote=origin", branch)))
		goto cleanup;
	if ((res = set_config("branch.%s.merge=refs/heads/%s",
			      branch, branch)))
		goto cleanup;

	strbuf_reset(&buf);
	strbuf_addf(&buf, "origin/%s", branch);
	res = run_git("checkout", "-f", "-t", buf.buf, NULL);
	if (res)
		goto cleanup;

	/* If --no-maintenance, then skip maintenance command entirely. */
	res = register_dir(maintenance);

cleanup:
	free(branch_to_free);
	free(enlistment);
	free(dir);
	strbuf_release(&buf);
	free(default_cache_server_url);
	free(local_cache_root_abs);
	return res;
}

static int cmd_diagnose(int argc, const char **argv)
{
	struct option options[] = {
		OPT_END(),
	};
	const char * const usage[] = {
		N_("scalar diagnose [<enlistment>]"),
		NULL
	};
	struct strbuf diagnostics_root = STRBUF_INIT;
	int res = 0;

	argc = parse_options(argc, argv, NULL, options,
			     usage, 0);

	setup_enlistment_directory(argc, argv, usage, options, &diagnostics_root);
	strbuf_addstr(&diagnostics_root, "/.scalarDiagnostics");

	/* Here, a failure should not repeat itself. */
	git_retries = 1;
	res = run_git("diagnose", "--mode=all", "-s", "%Y%m%d_%H%M%S",
		      "-o", diagnostics_root.buf, NULL);

	strbuf_release(&diagnostics_root);
	return res;
}

static int cmd_list(int argc, const char **argv UNUSED)
{
	if (argc != 1)
		die(_("`scalar list` does not take arguments"));

	if (run_git("config", "--global", "--get-all", "scalar.repo", NULL) < 0)
		return -1;
	return 0;
}

static int cmd_register(int argc, const char **argv)
{
	int maintenance = 1;
	struct option options[] = {
		OPT_BOOL(0, "maintenance", &maintenance,
			 N_("specify if background maintenance should be enabled")),
		OPT_END(),
	};
	const char * const usage[] = {
		N_("scalar register [--[no-]maintenance] [<enlistment>]"),
		NULL
	};

	argc = parse_options(argc, argv, NULL, options,
			     usage, 0);

	setup_enlistment_directory(argc, argv, usage, options, NULL);

	/* If --no-maintenance, then leave maintenance as-is. */
	return register_dir(maintenance);
}

static int get_scalar_repos(const char *key, const char *value,
			    const struct config_context *ctx UNUSED,
			    void *data)
{
	struct string_list *list = data;

	if (!strcmp(key, "scalar.repo"))
		string_list_append(list, value);

	return 0;
}

static int remove_deleted_enlistment(struct strbuf *path)
{
	int res = 0;
	strbuf_realpath_forgiving(path, path->buf, 1);

	if (run_git("config", "--global",
		    "--unset", "--fixed-value",
		    "scalar.repo", path->buf, NULL) < 0)
		res = -1;

	if (run_git("config", "--global",
		    "--unset", "--fixed-value",
		    "maintenance.repo", path->buf, NULL) < 0)
		res = -1;

	return res;
}

static int cmd_reconfigure(int argc, const char **argv)
{
	int all = 0;
	const char *maintenance_str = NULL;
	int maintenance = 1; /* Enable maintenance by default. */

	struct option options[] = {
		OPT_BOOL('a', "all", &all,
			 N_("reconfigure all registered enlistments")),
		OPT_STRING(0, "maintenance", &maintenance_str,
			 N_("(enable|disable|keep)"),
			 N_("signal how to adjust background maintenance")),
		OPT_END(),
	};
	const char * const usage[] = {
		N_("scalar reconfigure [--maintenance=(enable|disable|keep)] [--all | <enlistment>]"),
		NULL
	};
	struct string_list scalar_repos = STRING_LIST_INIT_DUP;
	int res = 0;
	struct strbuf commondir = STRBUF_INIT, gitdir = STRBUF_INIT;

	argc = parse_options(argc, argv, NULL, options,
			     usage, 0);

	if (!all) {
		setup_enlistment_directory(argc, argv, usage, options, NULL);

		return set_recommended_config(1);
	}

	if (argc > 0)
		usage_msg_opt(_("--all or <enlistment>, but not both"),
			      usage, options);

	if (maintenance_str) {
		if (!strcmp(maintenance_str, "enable"))
			maintenance = 1;
		else if (!strcmp(maintenance_str, "disable"))
			maintenance = 0;
		else if (!strcmp(maintenance_str, "keep"))
			maintenance = -1;
		else
			die(_("unknown mode for --maintenance option: %s"),
			    maintenance_str);
	}

	repo_config(the_repository, get_scalar_repos, &scalar_repos);

	for (size_t i = 0; i < scalar_repos.nr; i++) {
		int succeeded = 0;
		struct repository *old_repo, r = { NULL };
		const char *dir = scalar_repos.items[i].string;

		strbuf_reset(&commondir);
		strbuf_reset(&gitdir);

		if (chdir(dir) < 0) {
			struct strbuf buf = STRBUF_INIT;

			if (errno != ENOENT) {
				warning_errno(_("could not switch to '%s'"), dir);
				goto loop_end;
			}

			strbuf_addstr(&buf, dir);
			if (remove_deleted_enlistment(&buf))
				error(_("could not remove stale "
					"scalar.repo '%s'"), dir);
			else {
				warning(_("removed stale scalar.repo '%s'"),
					dir);
				succeeded = 1;
			}
			strbuf_release(&buf);
			goto loop_end;
		}

		switch (discover_git_directory_reason(&commondir, &gitdir)) {
		case GIT_DIR_INVALID_OWNERSHIP:
			warning(_("repository at '%s' has different owner"), dir);
			goto loop_end;

		case GIT_DIR_INVALID_GITFILE:
		case GIT_DIR_INVALID_FORMAT:
			warning(_("repository at '%s' has a format issue"), dir);
			goto loop_end;

		case GIT_DIR_DISCOVERED:
			succeeded = 1;
			break;

		default:
			warning(_("repository not found in '%s'"), dir);
			break;
		}

		repo_config_clear(the_repository);

		if (repo_init(&r, gitdir.buf, commondir.buf))
			goto loop_end;

		old_repo = the_repository;
		the_repository = &r;

		if (set_recommended_config(1) >= 0)
			succeeded = 1;

		the_repository = old_repo;
		repo_clear(&r);

		if (maintenance >= 0 &&
		    toggle_maintenance(maintenance) >= 0)
			succeeded = 1;

loop_end:
		if (!succeeded) {
			res = -1;
			warning(_("to unregister this repository from Scalar, run\n"
				  "\tgit config --global --unset --fixed-value scalar.repo \"%s\""),
				dir);
		}
	}

	string_list_clear(&scalar_repos, 1);
	strbuf_release(&commondir);
	strbuf_release(&gitdir);

	return res;
}

static int cmd_run(int argc, const char **argv)
{
	struct option options[] = {
		OPT_END(),
	};
	struct {
		const char *arg, *task;
	} tasks[] = {
		{ "config", NULL },
		{ "commit-graph", "commit-graph" },
		{ "fetch", "prefetch" },
		{ "loose-objects", "loose-objects" },
		{ "pack-files", "incremental-repack" },
		{ NULL, NULL }
	};
	struct strbuf buf = STRBUF_INIT;
	const char *usagestr[] = { NULL, NULL };
	int i;

	strbuf_addstr(&buf, N_("scalar run <task> [<enlistment>]\nTasks:\n"));
	for (i = 0; tasks[i].arg; i++)
		strbuf_addf(&buf, "\t%s\n", tasks[i].arg);
	usagestr[0] = buf.buf;

	argc = parse_options(argc, argv, NULL, options,
			     usagestr, 0);

	if (!argc)
		usage_with_options(usagestr, options);

	if (!strcmp("all", argv[0])) {
		i = -1;
	} else {
		for (i = 0; tasks[i].arg && strcmp(tasks[i].arg, argv[0]); i++)
			; /* keep looking for the task */

		if (i > 0 && !tasks[i].arg) {
			error(_("no such task: '%s'"), argv[0]);
			usage_with_options(usagestr, options);
		}
	}

	argc--;
	argv++;
	setup_enlistment_directory(argc, argv, usagestr, options, NULL);
	strbuf_release(&buf);

	if (i == 0)
		return register_dir(1);

	if (i > 0)
		return run_git("maintenance", "run",
			       "--task", tasks[i].task, NULL);

	if (register_dir(1))
		return -1;
	for (i = 1; tasks[i].arg; i++)
		if (run_git("maintenance", "run",
			    "--task", tasks[i].task, NULL))
			return -1;
	return 0;
}

static int cmd_unregister(int argc, const char **argv)
{
	struct option options[] = {
		OPT_END(),
	};
	const char * const usage[] = {
		N_("scalar unregister [<enlistment>]"),
		NULL
	};

	argc = parse_options(argc, argv, NULL, options,
			     usage, 0);

	/*
	 * Be forgiving when the enlistment or worktree does not even exist any
	 * longer; This can be the case if a user deleted the worktree by
	 * mistake and _still_ wants to unregister the thing.
	 */
	if (argc == 1) {
		struct strbuf src_path = STRBUF_INIT, workdir_path = STRBUF_INIT;

		strbuf_addf(&src_path, "%s/src/.git", argv[0]);
		strbuf_addf(&workdir_path, "%s/.git", argv[0]);
		if (!is_directory(src_path.buf) && !is_directory(workdir_path.buf)) {
			/* remove possible matching registrations */
			int res = -1;

			strbuf_strip_suffix(&src_path, "/.git");
			res = remove_deleted_enlistment(&src_path) && res;

			strbuf_strip_suffix(&workdir_path, "/.git");
			res = remove_deleted_enlistment(&workdir_path) && res;

			strbuf_release(&src_path);
			strbuf_release(&workdir_path);
			return res;
		}
		strbuf_release(&src_path);
		strbuf_release(&workdir_path);
	}

	setup_enlistment_directory(argc, argv, usage, options, NULL);

	return unregister_dir();
}

static int cmd_delete(int argc, const char **argv)
{
	char *cwd = xgetcwd();
	struct option options[] = {
		OPT_END(),
	};
	const char * const usage[] = {
		N_("scalar delete <enlistment>"),
		NULL
	};
	struct strbuf enlistment = STRBUF_INIT;
	int res = 0;

	argc = parse_options(argc, argv, NULL, options,
			     usage, 0);

	if (argc != 1)
		usage_with_options(usage, options);

	setup_enlistment_directory(argc, argv, usage, options, &enlistment);

	if (dir_inside_of(cwd, enlistment.buf) >= 0)
		res = error(_("refusing to delete current working directory"));
	else {
		close_object_store(the_repository->objects);
		res = delete_enlistment(&enlistment);
	}
	strbuf_release(&enlistment);
	free(cwd);

	return res;
}

static int cmd_help(int argc, const char **argv)
{
	struct option options[] = {
		OPT_END(),
	};
	const char * const usage[] = {
		"scalar help",
		NULL
	};

	argc = parse_options(argc, argv, NULL, options,
			     usage, 0);

	if (argc != 0)
		usage_with_options(usage, options);

	return run_git("help", "scalar", NULL);
}

static int cmd_version(int argc, const char **argv)
{
	int verbose = 0, build_options = 0;
	struct option options[] = {
		OPT__VERBOSE(&verbose, N_("include Git version")),
		OPT_BOOL(0, "build-options", &build_options,
			 N_("include Git's build options")),
		OPT_END(),
	};
	const char * const usage[] = {
		N_("scalar verbose [-v | --verbose] [--build-options]"),
		NULL
	};
	struct strbuf buf = STRBUF_INIT;

	argc = parse_options(argc, argv, NULL, options,
			     usage, 0);

	if (argc != 0)
		usage_with_options(usage, options);

	get_version_info(&buf, build_options);
	fprintf(stderr, "%s\n", buf.buf);
	strbuf_release(&buf);

	return 0;
}

static int cmd_cache_server(int argc, const char **argv)
{
	int get = 0;
	const char *set = NULL, *list = NULL;
	struct option options[] = {
		OPT_CMDMODE(0, "get", &get,
			    N_("get the configured cache-server URL"), 1),
		OPT_STRING(0, "set", &set, N_("URL"),
			   N_("configure the cache-server to use")),
		OPT_STRING(0, "list", &list, N_("remote"),
			   N_("list the possible cache-server URLs")),
		OPT_END(),
	};
	const char * const usage[] = {
		N_("scalar cache-server "
		   "[--get | --set <url> | --list <remote>] [<enlistment>]"),
		NULL
	};
	int res = 0;

	argc = parse_options(argc, argv, NULL, options,
			     usage, 0);

	if (get + !!set + !!list > 1)
		usage_msg_opt(_("--get/--set/--list are mutually exclusive"),
			      usage, options);

	setup_enlistment_directory(argc, argv, usage, options, NULL);

	if (list) {
		const char *name = list, *url = list;

		if (!strchr(list, '/')) {
			struct remote *remote;

			/* Look up remote */
			remote = remote_get(list);
			if (!remote) {
				error("no such remote: '%s'", name);
				return 1;
			}
			if (!remote->url.nr) {
				return error(_("remote '%s' has no URLs"),
					     name);
			}
			url = remote->url.v[0];
		}
		res = supports_gvfs_protocol(url, NULL);
	} else if (set) {
		res = set_config("gvfs.cache-server=%s", set);
	} else {
		char *url = NULL;

		printf("Using cache server: %s\n",
		       repo_config_get_string(the_repository, "gvfs.cache-server", &url) ?
		       "(undefined)" : url);
		free(url);
	}

	return !!res;
}

static struct {
	const char *name;
	int (*fn)(int, const char **);
} builtins[] = {
	{ "clone", cmd_clone },
	{ "list", cmd_list },
	{ "register", cmd_register },
	{ "unregister", cmd_unregister },
	{ "run", cmd_run },
	{ "reconfigure", cmd_reconfigure },
	{ "delete", cmd_delete },
	{ "help", cmd_help },
	{ "version", cmd_version },
	{ "diagnose", cmd_diagnose },
	{ "cache-server", cmd_cache_server },
	{ NULL, NULL},
};

int cmd_main(int argc, const char **argv)
{
	struct strbuf scalar_usage = STRBUF_INIT;
	int i;

	if (is_unattended()) {
		setenv("GIT_ASKPASS", "", 0);
		setenv("GIT_TERMINAL_PROMPT", "false", 0);
		git_config_push_parameter("credential.interactive=false");
	}

	while (argc > 1 && *argv[1] == '-') {
		if (!strcmp(argv[1], "-C")) {
			if (argc < 3)
				die(_("-C requires a <directory>"));
			if (chdir(argv[2]) < 0)
				die_errno(_("could not change to '%s'"),
					  argv[2]);
			argc -= 2;
			argv += 2;
		} else if (!strcmp(argv[1], "-c")) {
			if (argc < 3)
				die(_("-c requires a <key>=<value> argument"));
			git_config_push_parameter(argv[2]);
			argc -= 2;
			argv += 2;
		} else
			break;
	}

	if (argc > 1) {
		argv++;
		argc--;

		if (!strcmp(argv[0], "config"))
			argv[0] = "reconfigure";

		for (i = 0; builtins[i].name; i++)
			if (!strcmp(builtins[i].name, argv[0]))
				return !!builtins[i].fn(argc, argv);
	}

	strbuf_addstr(&scalar_usage,
		      N_("scalar [-C <directory>] [-c <key>=<value>] "
			 "<command> [<options>]\n\nCommands:\n"));
	for (i = 0; builtins[i].name; i++)
		strbuf_addf(&scalar_usage, "\t%s\n", builtins[i].name);

	usage(scalar_usage.buf);
}
