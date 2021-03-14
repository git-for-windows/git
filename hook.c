#include "cache.h"

#include "hook.h"
#include "config.h"
#include "run-command.h"
#include "prompt.h"

void free_hook(struct hook *ptr)
{
	if (ptr) {
		strbuf_release(&ptr->command);
		free(ptr->feed_pipe_cb_data);
		free(ptr);
	}
}

static struct hook * find_hook_by_command(struct list_head *head, const char *command)
{
	struct list_head *pos = NULL, *tmp = NULL;
	struct hook *found = NULL;

	list_for_each_safe(pos, tmp, head) {
		struct hook *it = list_entry(pos, struct hook, list);
		if (!strcmp(it->command.buf, command)) {
		    list_del(pos);
		    found = it;
		    break;
		}
	}
	return found;
}

static void append_or_move_hook(struct list_head *head, const char *command)
{
	struct hook *to_add = find_hook_by_command(head, command);

	if (!to_add) {
		/* adding a new hook, not moving an old one */
		to_add = xmalloc(sizeof(*to_add));
		strbuf_init(&to_add->command, 0);
		strbuf_addstr(&to_add->command, command);
		to_add->from_hookdir = 0;
		to_add->feed_pipe_cb_data = NULL;
	}

	/* re-set the scope so we show where an override was specified */
	to_add->origin = current_config_scope();

	list_add_tail(&to_add->list, head);
}

static void remove_hook(struct list_head *to_remove)
{
	struct hook *hook_to_remove = list_entry(to_remove, struct hook, list);
	list_del(to_remove);
	free_hook(hook_to_remove);
}

void clear_hook_list(struct list_head *head)
{
	struct list_head *pos, *tmp;
	list_for_each_safe(pos, tmp, head)
		remove_hook(pos);
}

struct hook_config_cb
{
	struct strbuf *hookname;
	struct list_head *list;
};

static int hook_config_lookup(const char *key, const char *value, void *cb_data)
{
	struct hook_config_cb *data = cb_data;
	const char *hook_key = data->hookname->buf;
	struct list_head *head = data->list;

	if (!strcmp(key, hook_key)) {
		const char *command = value;
		struct strbuf hookcmd_name = STRBUF_INIT;
		int skip = 0;

		/*
		 * Check if we're removing that hook instead. Hookcmds are
		 * removed by name, and inlined hooks are removed by command
		 * content.
		 */
		strbuf_addf(&hookcmd_name, "hookcmd.%s.skip", command);
		git_config_get_bool(hookcmd_name.buf, &skip);

		/*
		 * Check if a hookcmd with that name exists. If it doesn't,
		 * 'git_config_get_value()' is documented not to touch &command,
		 * so we don't need to do anything.
		 */
		strbuf_reset(&hookcmd_name);
		strbuf_addf(&hookcmd_name, "hookcmd.%s.command", command);
		git_config_get_value(hookcmd_name.buf, &command);

		if (!command) {
			strbuf_release(&hookcmd_name);
			BUG("git_config_get_value overwrote a string it shouldn't have");
		}

		/*
		 * TODO: implement an option-getting callback, e.g.
		 *   get configs by pattern hookcmd.$value.*
		 *   for each key+value, do_callback(key, value, cb_data)
		 */

		if (skip) {
			struct hook *to_remove = find_hook_by_command(head, command);
			if (to_remove)
				remove_hook(&(to_remove->list));
		} else {
			append_or_move_hook(head, command);
		}

		strbuf_release(&hookcmd_name);
	}

	return 0;
}

enum hookdir_opt configured_hookdir_opt(void)
{
	const char *key;
	if (git_config_get_value("hook.runhookdir", &key))
		return HOOKDIR_YES; /* by default, just run it. */

	if (!strcmp(key, "no"))
		return HOOKDIR_NO;

	if (!strcmp(key, "error"))
		return HOOKDIR_ERROR;

	if (!strcmp(key, "yes"))
		return HOOKDIR_YES;

	if (!strcmp(key, "warn"))
		return HOOKDIR_WARN;

	if (!strcmp(key, "interactive"))
		return HOOKDIR_INTERACTIVE;

	return HOOKDIR_UNKNOWN;
}

int configured_hook_jobs(void)
{
	int n = online_cpus();
	git_config_get_int("hook.jobs", &n);

	return n;
}

static int should_include_hookdir(const char *path, enum hookdir_opt cfg)
{
	struct strbuf prompt = STRBUF_INIT;
	/*
	 * If the path doesn't exist, don't bother adding the empty hook and
	 * don't bother checking the config or prompting the user.
	 */
	if (!path)
		return 0;

	switch (cfg)
	{
		case HOOKDIR_ERROR:
			fprintf(stderr, _("Skipping legacy hook at '%s'\n"),
				path);
			/* FALLTHROUGH */
		case HOOKDIR_NO:
			return 0;
		case HOOKDIR_WARN:
			fprintf(stderr, _("Running legacy hook at '%s'\n"),
				path);
			return 1;
		case HOOKDIR_INTERACTIVE:
			do {
				/*
				 * TRANSLATORS: Make sure to include [Y] and [n]
				 * in your translation. Only English input is
				 * accepted. Default option is "yes".
				 */
				fprintf(stderr, _("Run '%s'? [Yn] "), path);
				git_read_line_interactively(&prompt);
				strbuf_tolower(&prompt);
				if (starts_with(prompt.buf, "n")) {
					strbuf_release(&prompt);
					return 0;
				} else if (starts_with(prompt.buf, "y")) {
					strbuf_release(&prompt);
					return 1;
				}
				/* otherwise, we didn't understand the input */
			} while (prompt.len); /* an empty reply means "Yes" */
			strbuf_release(&prompt);
			return 1;
		/*
		 * HOOKDIR_UNKNOWN should match the default behavior, but let's
		 * give a heads up to the user.
		 */
		case HOOKDIR_UNKNOWN:
			fprintf(stderr,
				_("Unrecognized value for 'hook.runHookDir'. "
				  "Is there a typo? "));
			/* FALLTHROUGH */
		case HOOKDIR_YES:
		default:
			return 1;
	}
}

static const char *find_legacy_hook(const char *name)
{
	static struct strbuf path = STRBUF_INIT;

	strbuf_reset(&path);
	strbuf_git_path(&path, "hooks/%s", name);
	if (access(path.buf, X_OK) < 0) {
		int err = errno;

#ifdef STRIP_EXTENSION
		strbuf_addstr(&path, STRIP_EXTENSION);
		if (access(path.buf, X_OK) >= 0)
			return path.buf;
		if (errno == EACCES)
			err = errno;
#endif

		if (err == EACCES && advice_ignored_hook) {
			static struct string_list advise_given = STRING_LIST_INIT_DUP;

			if (!string_list_lookup(&advise_given, name)) {
				string_list_insert(&advise_given, name);
				advise(_("The '%s' hook was ignored because "
					 "it's not set as executable.\n"
					 "You can disable this warning with "
					 "`git config advice.ignoredHook false`."),
				       path.buf);
			}
		}
		return NULL;
	}
	return path.buf;
}


struct list_head* hook_list(const struct strbuf* hookname)
{
	struct strbuf hook_key = STRBUF_INIT;
	struct list_head *hook_head = xmalloc(sizeof(struct list_head));
	struct hook_config_cb cb_data = { &hook_key, hook_head };

	INIT_LIST_HEAD(hook_head);

	if (!hookname)
		return NULL;

	strbuf_addf(&hook_key, "hook.%s.command", hookname->buf);

	git_config(hook_config_lookup, &cb_data);

	if (have_git_dir()) {
		const char *legacy_hook_path = find_legacy_hook(hookname->buf);

		/* Unconditionally add legacy hook, but annotate it. */
		if (legacy_hook_path) {
			struct hook *legacy_hook;

			append_or_move_hook(hook_head,
					    absolute_path(legacy_hook_path));
			legacy_hook = list_entry(hook_head->prev, struct hook,
						 list);
			legacy_hook->from_hookdir = 1;
		}
	}

	strbuf_release(&hook_key);
	return hook_head;
}

void run_hooks_opt_init_sync(struct run_hooks_opt *o)
{
	strvec_init(&o->env);
	strvec_init(&o->args);
	o->path_to_stdin = NULL;
	o->run_hookdir = configured_hookdir_opt();
	o->jobs = 1;
	o->dir = NULL;
	o->feed_pipe = NULL;
	o->feed_pipe_ctx = NULL;
	o->consume_sideband = NULL;
}

void run_hooks_opt_init_async(struct run_hooks_opt *o)
{
	run_hooks_opt_init_sync(o);
	o->jobs = configured_hook_jobs();
}

int hook_exists(const char *hookname, enum hookdir_opt should_run_hookdir)
{
	const char *value = NULL; /* throwaway */
	struct strbuf hook_key = STRBUF_INIT;
	int could_run_hookdir;

	if (should_run_hookdir == HOOKDIR_USE_CONFIG)
		should_run_hookdir = configured_hookdir_opt();

	could_run_hookdir = (should_run_hookdir == HOOKDIR_INTERACTIVE ||
				should_run_hookdir == HOOKDIR_WARN ||
				should_run_hookdir == HOOKDIR_YES)
				&& !!find_legacy_hook(hookname);

	strbuf_addf(&hook_key, "hook.%s.command", hookname);

	return (!git_config_get_value(hook_key.buf, &value)) || could_run_hookdir;
}

void run_hooks_opt_clear(struct run_hooks_opt *o)
{
	strvec_clear(&o->env);
	strvec_clear(&o->args);
}

int pipe_from_string_list(struct strbuf *pipe, void *pp_cb, void *pp_task_cb)
{
	int *item_idx;
	struct hook *ctx = pp_task_cb;
	struct string_list *to_pipe = ((struct hook_cb_data*)pp_cb)->options->feed_pipe_ctx;

	/* Bootstrap the state manager if necessary. */
	if (!ctx->feed_pipe_cb_data) {
		ctx->feed_pipe_cb_data = xmalloc(sizeof(unsigned int));
		*(int*)ctx->feed_pipe_cb_data = 0;
	}

	item_idx = ctx->feed_pipe_cb_data;

	if (*item_idx < to_pipe->nr) {
		strbuf_addf(pipe, "%s\n", to_pipe->items[*item_idx].string);
		(*item_idx)++;
		return 0;
	}
	return 1;
}

static int pick_next_hook(struct child_process *cp,
			  struct strbuf *out,
			  void *pp_cb,
			  void **pp_task_cb)
{
	struct hook_cb_data *hook_cb = pp_cb;
	struct hook *hook = hook_cb->run_me;

	if (!hook)
		return 0;

	/* reopen the file for stdin; run_command closes it. */
	if (hook_cb->options->path_to_stdin) {
		cp->no_stdin = 0;
		cp->in = xopen(hook_cb->options->path_to_stdin, O_RDONLY);
	} else if (hook_cb->options->feed_pipe) {
		/* ask for start_command() to make a pipe for us */
		cp->in = -1;
		cp->no_stdin = 0;
	} else {
		cp->no_stdin = 1;
	}

	cp->env = hook_cb->options->env.v;
	cp->stdout_to_stderr = 1;
	cp->trace2_hook_name = hook->command.buf;
	cp->dir = hook_cb->options->dir;

	/*
	 * Commands from the config could be oneliners, but we know
	 * for certain that hookdir commands are not.
	 */
	cp->use_shell = !hook->from_hookdir;

	/* add command */
	strvec_push(&cp->args, hook->command.buf);

	/*
	 * add passed-in argv, without expanding - let the user get back
	 * exactly what they put in
	 */
	strvec_pushv(&cp->args, hook_cb->options->args.v);

	/* Provide context for errors if necessary */
	*pp_task_cb = hook;

	/* Get the next entry ready */
	if (hook_cb->run_me->list.next == hook_cb->head)
		hook_cb->run_me = NULL;
	else
		hook_cb->run_me = list_entry(hook_cb->run_me->list.next,
					     struct hook, list);

	return 1;
}

static int notify_start_failure(struct strbuf *out,
				void *pp_cb,
				void *pp_task_cp)
{
	struct hook_cb_data *hook_cb = pp_cb;
	struct hook *attempted = pp_task_cp;

	/* |= rc in cb */
	hook_cb->rc |= 1;

	strbuf_addf(out, _("Couldn't start '%s', configured in '%s'\n"),
		    attempted->command.buf,
		    attempted->from_hookdir ? "hookdir"
			: config_scope_name(attempted->origin));

	/* NEEDSWORK: if halt_on_error is desired, do it here. */
	return 0;
}

static int notify_hook_finished(int result,
				struct strbuf *out,
				void *pp_cb,
				void *pp_task_cb)
{
	struct hook_cb_data *hook_cb = pp_cb;

	/* |= rc in cb */
	hook_cb->rc |= result;

	/* NEEDSWORK: if halt_on_error is desired, do it here. */
	return 0;
}

int run_hooks(const char *hookname, struct run_hooks_opt *options)
{
	struct strbuf hookname_str = STRBUF_INIT;
	struct list_head *to_run, *pos = NULL, *tmp = NULL;
	struct hook_cb_data cb_data = { 0, NULL, NULL, options };

	if (!options)
		BUG("a struct run_hooks_opt must be provided to run_hooks");

	if (options->path_to_stdin && options->feed_pipe)
		BUG("choose only one method to populate stdin");

	strbuf_addstr(&hookname_str, hookname);

	to_run = hook_list(&hookname_str);

	list_for_each_safe(pos, tmp, to_run) {
		struct hook *hook = list_entry(pos, struct hook, list);

		if (hook->from_hookdir &&
		    !should_include_hookdir(hook->command.buf, options->run_hookdir))
			    list_del(pos);
	}

	if (list_empty(to_run))
		return 0;

	cb_data.head = to_run;
	cb_data.run_me = list_entry(to_run->next, struct hook, list);

	run_processes_parallel_tr2(options->jobs,
				   pick_next_hook,
				   notify_start_failure,
				   options->feed_pipe,
				   options->consume_sideband,
				   notify_hook_finished,
				   &cb_data,
				   "hook",
				   hookname);

	return cb_data.rc;
}
