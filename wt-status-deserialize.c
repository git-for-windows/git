#define USE_THE_REPOSITORY_VARIABLE
#include "git-compat-util.h"
#include "environment.h"
#include "hex.h"
#include "hash.h"
#include "wt-status.h"
#include "pkt-line.h"
#include "trace.h"
#include "statinfo.h"
#include "hex.h"

static struct trace_key trace_deserialize = TRACE_KEY_INIT(DESERIALIZE);

enum deserialize_parse_strategy {
	DESERIALIZE_STRATEGY_AS_IS,
	DESERIALIZE_STRATEGY_SKIP,
	DESERIALIZE_STRATEGY_NORMAL,
	DESERIALIZE_STRATEGY_ALL
};

static int check_path_contains(const char *out, int out_len, const char *in, int in_len)
{
	return (out_len > 0 &&
		out_len < in_len &&
		(out[out_len - 1] == '/') &&
		!memcmp(out, in, out_len));
}

static const char *my_packet_read_line(int fd, int *line_len)
{
	static char buf[LARGE_PACKET_MAX];

	*line_len = packet_read(fd, buf, sizeof(buf),
				PACKET_READ_CHOMP_NEWLINE |
				PACKET_READ_GENTLE_ON_EOF);
	return (*line_len > 0) ? buf : NULL;
}

/*
 * mtime_reported contains the mtime of the index when the
 * serialization snapshot was computed.
 *
 * mtime_observed_on_disk contains the mtime of the index now.
 *
 * If these 2 times are different, then the .git/index has
 * changed since the serialization cache was created and we
 * must reject the cache because anything could have changed.
 *
 * If they are the same, we continue trying to use the cache.
 */
static int my_validate_index(const char *path, const struct cache_time *mtime_reported)
{
	struct stat st;
	struct cache_time mtime_observed_on_disk;

	if (lstat(path, &st)) {
		trace_printf_key(&trace_deserialize, "could not stat index");
		return DESERIALIZE_ERR;
	}
	mtime_observed_on_disk.sec = st.st_mtime;
	mtime_observed_on_disk.nsec = ST_MTIME_NSEC(st);
	if ((mtime_observed_on_disk.sec != mtime_reported->sec) ||
	    (mtime_observed_on_disk.nsec != mtime_reported->nsec)) {
		trace_printf_key(&trace_deserialize, "index mtime changed [des %d.%d][obs %d.%d]",
			     mtime_reported->sec, mtime_reported->nsec,
			     mtime_observed_on_disk.sec, mtime_observed_on_disk.nsec);
		return DESERIALIZE_ERR;
	}

	return DESERIALIZE_OK;
}

static int wt_deserialize_v1_header(struct wt_status *s, int fd)
{
	struct cache_time index_mtime;
	int line_len, nr_fields;
	const char *line;
	const char *arg;

	/*
	 * parse header lines up to the first flush packet.
	 */
	while ((line = my_packet_read_line(fd, &line_len))) {

		if (skip_prefix(line, "index_mtime ", &arg)) {
			nr_fields = sscanf(arg, "%u %u",
					   &index_mtime.sec,
					   &index_mtime.nsec);
			if (nr_fields != 2) {
				trace_printf_key(&trace_deserialize, "invalid index_mtime (%d) '%s'",
					     nr_fields, line);
				return DESERIALIZE_ERR;
			}
			continue;
		}

		if (skip_prefix(line, "is_initial ", &arg)) {
			s->is_initial = (int)strtol(arg, NULL, 10);
			continue;
		}
		if (skip_prefix(line, "branch ", &arg)) {
			s->branch = xstrdup(arg);
			continue;
		}
		if (skip_prefix(line, "reference ", &arg)) {
			s->reference = xstrdup(arg);
			continue;
		}
		/* pathspec */
		/* verbose */
		/* amend */
		if (skip_prefix(line, "whence ", &arg)) {
			s->whence = (int)strtol(arg, NULL, 10);
			continue;
		}
		/* nowarn */
		/* use_color */
		/* no_gettext */
		/* display_comment_prefix */
		/* relative_paths */
		/* submodule_summary */
		if (skip_prefix(line, "show_ignored_mode ", &arg)) {
			s->show_ignored_mode = (int)strtol(arg, NULL, 10);
			continue;
		}
		if (skip_prefix(line, "show_untracked_files ", &arg)) {
			s->show_untracked_files = (int)strtol(arg, NULL, 10);
			continue;
		}
		if (skip_prefix(line, "ignore_submodule_arg ", &arg)) {
			s->ignore_submodule_arg = xstrdup(arg);
			continue;
		}
		/* color_palette */
		/* colopts */
		/* null_termination */
		/* commit_template */
		/* show_branch */
		/* show_stash */
		if (skip_prefix(line, "hints ", &arg)) {
			s->hints = (int)strtol(arg, NULL, 10);
			continue;
		}
		if (skip_prefix(line, "detect_rename ", &arg)) {
			s->detect_rename = (int)strtol(arg, NULL, 10);
			continue;
		}
		if (skip_prefix(line, "rename_score ", &arg)) {
			s->rename_score = (int)strtol(arg, NULL, 10);
			continue;
		}
		if (skip_prefix(line, "rename_limit ", &arg)) {
			s->rename_limit = (int)strtol(arg, NULL, 10);
			continue;
		}
		/* status_format */
		if (skip_prefix(line, "sha1_commit ", &arg)) {
			if (get_oid_hex(arg, &s->oid_commit)) {
				trace_printf_key(&trace_deserialize, "invalid sha1_commit");
				return DESERIALIZE_ERR;
			}
			continue;
		}
		if (skip_prefix(line, "committable ", &arg)) {
			s->committable = (int)strtol(arg, NULL, 10);
			continue;
		}
		if (skip_prefix(line, "workdir_dirty ", &arg)) {
			s->workdir_dirty = (int)strtol(arg, NULL, 10);
			continue;
		}
		/* prefix */

		trace_printf_key(&trace_deserialize, "unexpected line '%s'", line);
		return DESERIALIZE_ERR;
	}

	return my_validate_index(s->index_file, &index_mtime);
}

/*
 * Build a string-list of (count) <changed-item> lines from the input.
 */
static int wt_deserialize_v1_changed_items(struct wt_status *s, int fd, int count UNUSED)
{
	struct wt_status_serialize_data *sd;
	char *p;
	int line_len;
	const char *line;
	struct string_list_item *item;

	memset(&s->change, 0, sizeof(s->change));
	s->change.strdup_strings = 1;

	/*
	 * <wt_status_change_data_fields>+
	 * <flush>
	 *
	 * <fixed_part><path> NUL [<head_path>] NUL
	 */
	while ((line = my_packet_read_line(fd, &line_len))) {
		struct wt_status_change_data *d = xcalloc(1, sizeof(*d));
		sd = (struct wt_status_serialize_data *)line;

		d->worktree_status = ntohl(sd->fixed.worktree_status);
		d->index_status = ntohl(sd->fixed.index_status);
		d->stagemask = ntohl(sd->fixed.stagemask);
		d->rename_status = ntohl(sd->fixed.rename_status);
		d->rename_score = ntohl(sd->fixed.rename_score);
		d->mode_head = ntohl(sd->fixed.mode_head);
		d->mode_index = ntohl(sd->fixed.mode_index);
		d->mode_worktree = ntohl(sd->fixed.mode_worktree);
		d->dirty_submodule = ntohl(sd->fixed.dirty_submodule);
		d->new_submodule_commits = ntohl(sd->fixed.new_submodule_commits);
		oidcpy(&d->oid_head, &sd->fixed.oid_head);
		oidcpy(&d->oid_index, &sd->fixed.oid_index);

		p = sd->variant;
		item = string_list_append(&s->change, p);
		p += strlen(p) + 1;
		if (*p)
			d->rename_source = xstrdup(p);
		item->util = d;

		trace_printf_key(
			&trace_deserialize,
			"change: %d %d %d %d %d %o %o %o %d %d %s %s '%s' '%s'",
			d->worktree_status,
			d->index_status,
			d->stagemask,
			d->rename_status,
			d->rename_score,
			d->mode_head,
			d->mode_index,
			d->mode_worktree,
			d->dirty_submodule,
			d->new_submodule_commits,
			oid_to_hex(&d->oid_head),
			oid_to_hex(&d->oid_index),
			item->string,
			(d->rename_source ? d->rename_source : ""));
	}

	return DESERIALIZE_OK;
}

static int wt_deserialize_v1_untracked_items(struct wt_status *s,
					     int fd,
					     int count UNUSED,
					     enum deserialize_parse_strategy strategy)
{
	int line_len;
	const char *line;
	char *out = NULL;
	int out_len = 0;

	memset(&s->untracked, 0, sizeof(s->untracked));
	s->untracked.strdup_strings = 1;

	/*
	 * <pathname>+
	 * <flush>
	 */
	while ((line = my_packet_read_line(fd, &line_len))) {
		if (strategy == DESERIALIZE_STRATEGY_AS_IS)
			string_list_append(&s->untracked, line);
		if (strategy == DESERIALIZE_STRATEGY_SKIP)
			continue;
		if (strategy == DESERIALIZE_STRATEGY_NORMAL) {

			/* Only add "normal" entries to list */
			if (out &&
				check_path_contains(out, out_len, line, line_len)) {
				continue;
			}
			else {
				out = string_list_append(&s->untracked, line)->string;
				out_len = line_len;
			}
		}
		if (strategy == DESERIALIZE_STRATEGY_ALL) {
			/* Only add "all" entries to list */
			if (line[line_len - 1] != '/')
				string_list_append(&s->untracked, line);
		}
	}

	return DESERIALIZE_OK;
}

static int wt_deserialize_v1_ignored_items(struct wt_status *s,
					   int fd,
					   int count UNUSED,
					   enum deserialize_parse_strategy strategy)
{
	int line_len;
	const char *line;

	memset(&s->ignored, 0, sizeof(s->ignored));
	s->ignored.strdup_strings = 1;

	/*
	 * <pathname>+
	 * <flush>
	 */
	while ((line = my_packet_read_line(fd, &line_len))) {
		if (strategy == DESERIALIZE_STRATEGY_AS_IS)
			string_list_append(&s->ignored, line);
		else
			continue;
	}

	return DESERIALIZE_OK;
}

static int validate_untracked_files_arg(enum untracked_status_type cmd,
					enum untracked_status_type des,
					enum deserialize_parse_strategy *strategy)
{
	*strategy = DESERIALIZE_STRATEGY_AS_IS;

	if (cmd == des) {
		*strategy = DESERIALIZE_STRATEGY_AS_IS;
	} else if (cmd == SHOW_NO_UNTRACKED_FILES) {
		*strategy = DESERIALIZE_STRATEGY_SKIP;
	} else if (des == SHOW_COMPLETE_UNTRACKED_FILES) {
		if (cmd == SHOW_ALL_UNTRACKED_FILES)
			*strategy = DESERIALIZE_STRATEGY_ALL;
		else if (cmd == SHOW_NORMAL_UNTRACKED_FILES)
			*strategy = DESERIALIZE_STRATEGY_NORMAL;
	} else {
		return DESERIALIZE_ERR;
	}

	return DESERIALIZE_OK;
}

static int validate_ignored_files_arg(enum show_ignored_type cmd,
				      enum show_ignored_type des,
				      enum deserialize_parse_strategy *strategy)
{
	*strategy = DESERIALIZE_STRATEGY_AS_IS;

	if (cmd == SHOW_NO_IGNORED) {
		*strategy = DESERIALIZE_STRATEGY_SKIP;
	}
	else if (cmd != des) {
		return DESERIALIZE_ERR;
	}

	return DESERIALIZE_OK;
}

static int wt_deserialize_v1(const struct wt_status *cmd_s, struct wt_status *s, int fd)
{
	int line_len;
	const char *line;
	const char *arg;
	int nr_changed = 0;
	int nr_untracked = 0;
	int nr_ignored = 0;

	enum deserialize_parse_strategy ignored_strategy = DESERIALIZE_STRATEGY_AS_IS, untracked_strategy = DESERIALIZE_STRATEGY_AS_IS;

	if (wt_deserialize_v1_header(s, fd) == DESERIALIZE_ERR)
		return DESERIALIZE_ERR;

	/*
	 * We now have the header parsed. Look at the command args (as passed in), and see how to parse
	 * the serialized data
	*/
	if (validate_untracked_files_arg(cmd_s->show_untracked_files, s->show_untracked_files, &untracked_strategy)) {
		trace_printf_key(&trace_deserialize, "reject: show_untracked_file: command: %d, serialized : %d",
				cmd_s->show_untracked_files,
				s->show_untracked_files);
		return DESERIALIZE_ERR;
	}

	if (validate_ignored_files_arg(cmd_s->show_ignored_mode, s->show_ignored_mode, &ignored_strategy)) {
		trace_printf_key(&trace_deserialize, "reject: show_ignored_mode: command: %d, serialized: %d",
				cmd_s->show_ignored_mode,
				s->show_ignored_mode);
		return DESERIALIZE_ERR;
	}

	/*
	 * [<changed-header> [<changed-item>+] <flush>]
	 * [<untracked-header> [<untracked-item>+] <flush>]
	 * [<ignored-header> [<ignored-item>+] <flush>]
	 */
	while ((line = my_packet_read_line(fd, &line_len))) {
		if (skip_prefix(line, "changed ", &arg)) {
			nr_changed = (int)strtol(arg, NULL, 10);
			if (wt_deserialize_v1_changed_items(s, fd, nr_changed)
			    == DESERIALIZE_ERR)
				return DESERIALIZE_ERR;
			continue;
		}
		if (skip_prefix(line, "untracked ", &arg)) {
			nr_untracked = (int)strtol(arg, NULL, 10);
			if (wt_deserialize_v1_untracked_items(s, fd, nr_untracked, untracked_strategy)
			    == DESERIALIZE_ERR)
				return DESERIALIZE_ERR;
			continue;
		}
		if (skip_prefix(line, "ignored ", &arg)) {
			nr_ignored = (int)strtol(arg, NULL, 10);
			if (wt_deserialize_v1_ignored_items(s, fd, nr_ignored, ignored_strategy)
			    == DESERIALIZE_ERR)
				return DESERIALIZE_ERR;
			continue;
		}
		trace_printf_key(&trace_deserialize, "unexpected line '%s'", line);
		return DESERIALIZE_ERR;
	}

	return DESERIALIZE_OK;
}

static int wt_deserialize_parse(const struct wt_status *cmd_s, struct wt_status *s, int fd)
{
	int line_len;
	const char *line;
	const char *arg;

	if ((line = my_packet_read_line(fd, &line_len)) &&
	    (skip_prefix(line, "version ", &arg))) {
		int version = (int)strtol(arg, NULL, 10);
		if (version == 1)
			return wt_deserialize_v1(cmd_s, s, fd);
	}
	trace_printf_key(&trace_deserialize, "missing/unsupported version");
	return DESERIALIZE_ERR;
}

static inline int my_strcmp_null(const char *a, const char *b)
{
	const char *alt_a = (a) ? a : "";
	const char *alt_b = (b) ? b : "";

	return strcmp(alt_a, alt_b);
}

static int wt_deserialize_fd(const struct wt_status *cmd_s, struct wt_status *des_s, int fd)
{
	/*
	 * Check the path spec on the current command
	 */
	if (cmd_s->pathspec.nr > 1) {
		trace_printf_key(&trace_deserialize, "reject: multiple pathspecs");
		return DESERIALIZE_ERR;
	}

	/*
	 * If we have a pathspec, but it maches the root (e.g. no filtering)
	 * then this is OK.
	 */
	if (cmd_s->pathspec.nr == 1 &&
		my_strcmp_null(cmd_s->pathspec.items[0].match, "")) {
		trace_printf_key(&trace_deserialize, "reject: pathspec");
		return DESERIALIZE_ERR;
	}

	/*
	 * Copy over some required fields from the current command.
	 */
	des_s->repo = cmd_s->repo;
	des_s->index_file = cmd_s->index_file;

	/*
	 * Deserialize cached status
	 */
	if (wt_deserialize_parse(cmd_s, des_s, fd) == DESERIALIZE_ERR)
		return DESERIALIZE_ERR;

	/*
	 * Compare fields in cmd_s with those observed in des_s and
	 * complain if they are incompatible (such as different "-u"
	 * or "--ignored" settings).
	 */
	if (cmd_s->is_initial != des_s->is_initial) {
		trace_printf_key(&trace_deserialize, "reject: is_initial");
		return DESERIALIZE_ERR;
	}
	if (my_strcmp_null(cmd_s->branch, des_s->branch)) {
		trace_printf_key(&trace_deserialize, "reject: branch");
		return DESERIALIZE_ERR;
	}
	if (my_strcmp_null(cmd_s->reference, des_s->reference)) {
		trace_printf_key(&trace_deserialize, "reject: reference");
		return DESERIALIZE_ERR;
	}
	/* verbose */
	/* amend */
	if (cmd_s->whence != des_s->whence) {
		trace_printf_key(&trace_deserialize, "reject: whence");
		return DESERIALIZE_ERR;
	}
	/* nowarn */
	/* use_color */
	/* no_gettext */
	/* display_comment_prefix */
	/* relative_paths */
	/* submodule_summary */

	/* show_ignored_files - already validated */
	/* show_untrackes_files - already validated */

	/*
	 * Submodules are not supported by status serialization.
	 * The status will not be serialized if it contains submodules,
	 * and so this check is not needed.
	 *
	 * if (my_strcmp_null(cmd_s->ignore_submodule_arg, des_s->ignore_submodule_arg)) {
	 *	trace_printf_key(&trace_deserialize, "reject: ignore_submodule_arg");
	 * 	return DESERIALIZE_ERR;
	 * }
	 */

	/* color_palette */
	/* colopts */
	/* null_termination */
	/* commit_template */
	/* show_branch */
	/* show_stash */
	/* hints */
	/* ahead_behind_flags */
	if (cmd_s->detect_rename != des_s->detect_rename) {
		trace_printf_key(&trace_deserialize, "reject: detect_rename");
		return DESERIALIZE_ERR;
	}
	if (cmd_s->rename_score != des_s->rename_score) {
		trace_printf_key(&trace_deserialize, "reject: rename_score");
		return DESERIALIZE_ERR;
	}
	if (cmd_s->rename_limit != des_s->rename_limit) {
		trace_printf_key(&trace_deserialize, "reject: rename_limit");
		return DESERIALIZE_ERR;
	}
	/* status_format */
	if (!oideq(&cmd_s->oid_commit, &des_s->oid_commit)) {
		trace_printf_key(&trace_deserialize, "reject: sha1_commit");
		return DESERIALIZE_ERR;
	}

	/*
	 * Copy over display-related fields from the current command.
	 */
	des_s->verbose = cmd_s->verbose;
	/* amend */
	/* whence */
	des_s->nowarn = cmd_s->nowarn;
	des_s->use_color = cmd_s->use_color;
	des_s->no_gettext = cmd_s->no_gettext;
	des_s->display_comment_prefix = cmd_s->display_comment_prefix;
	des_s->relative_paths = cmd_s->relative_paths;
	des_s->submodule_summary = cmd_s->submodule_summary;
	memcpy(des_s->color_palette, cmd_s->color_palette,
	       sizeof(char)*WT_STATUS_MAXSLOT*COLOR_MAXLEN);
	des_s->colopts = cmd_s->colopts;
	des_s->null_termination = cmd_s->null_termination;
	/* commit_template */
	des_s->show_branch = cmd_s->show_branch;
	des_s->show_stash = cmd_s->show_stash;
	/* hints */
	des_s->ahead_behind_flags = cmd_s->ahead_behind_flags;
	des_s->status_format = cmd_s->status_format;
	des_s->fp = cmd_s->fp;
	if (cmd_s->prefix && *cmd_s->prefix)
		des_s->prefix = xstrdup(cmd_s->prefix);

	return DESERIALIZE_OK;
}

/*
 * Read raw serialized status data from the given file
 *
 * Verify that the args specified in the current command
 * are compatible with the deserialized data (such as "-uno").
 *
 * Copy display-related fields from the current command
 * into the deserialized data (so that the user can request
 * long or short as they please).
 */
int wt_status_deserialize(const struct wt_status *cmd_s,
			  const char *path)
{
	struct wt_status des_s;
	int result;
	struct string_list_item *change;

	if (path && *path && strcmp(path, "0")) {
		int fd = xopen(path, O_RDONLY);
		if (fd == -1) {
			trace_printf_key(&trace_deserialize, "could not read '%s'", path);
			return DESERIALIZE_ERR;
		}
		trace_printf_key(&trace_deserialize, "reading serialization file '%s'", path);
		result = wt_deserialize_fd(cmd_s, &des_s, fd);
		close(fd);
	} else {
		trace_printf_key(&trace_deserialize, "reading stdin");
		result = wt_deserialize_fd(cmd_s, &des_s, 0);
	}

	if (result == DESERIALIZE_OK) {
		wt_status_get_state(cmd_s->repo, &des_s.state, des_s.branch &&
				    !strcmp(des_s.branch, "HEAD"));
		wt_status_print(&des_s);
	}

	free(des_s.branch);
	free((char *)des_s.reference);
	for_each_string_list_item(change, &des_s.change) {
		struct wt_status_change_data *d = change->util;
		if (d)
			free(d->rename_source);
	}
	string_list_clear(&des_s.change, 1);
	string_list_clear(&des_s.ignored, 1);
	string_list_clear(&des_s.untracked, 1);
	return result;
}
