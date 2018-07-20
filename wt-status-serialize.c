#define USE_THE_REPOSITORY_VARIABLE

#include "git-compat-util.h"
#include "environment.h"
#include "hex.h"
#include "repository.h"
#include "wt-status.h"
#include "pkt-line.h"
#include "trace.h"
#include "read-cache-ll.h"
#include "path.h"

static struct trace_key trace_serialize = TRACE_KEY_INIT(SERIALIZE);

/*
 * Compute header record for exclude file using format:
 *      <key> SP <status_char> SP <variant> LF
 */
void wt_serialize_compute_exclude_header(struct strbuf *sb,
					 const char *key,
					 const char *path)
{
	struct stat st;
	struct stat_data sd;

	memset(&sd, 0, sizeof(sd));

	strbuf_setlen(sb, 0);

	if (!path || !*path) {
		strbuf_addf(sb, "%s U (unset)", key);
	} else if (lstat(path, &st) == -1) {
		if (is_missing_file_error(errno))
			strbuf_addf(sb, "%s E (not-found) %s", key, path);
		else
			strbuf_addf(sb, "%s E (other) %s", key, path);
	} else {
		fill_stat_data(&sd, &st);
		strbuf_addf(sb, "%s F %d %d %s",
			    key, sd.sd_mtime.sec, sd.sd_mtime.nsec, path);
	}
}

static void append_exclude_info(int fd, const char *path, const char *key)
{
	struct strbuf sb = STRBUF_INIT;

	wt_serialize_compute_exclude_header(&sb, key, path);

	packet_write_fmt(fd, "%s\n", sb.buf);

	strbuf_release(&sb);
}

static void append_core_excludes_file_info(int fd)
{
	/*
	 * Write pathname and mtime of the core/global excludes file to
	 * the status cache header.  Since a change in the global excludes
	 * will/may change the results reported by status, the deserialize
	 * code should be able to reject the status cache if the excludes
	 * file changes since when the cache was written.
	 *
	 * The "core.excludefile" setting defaults to $XDG_HOME/git/ignore
	 * and uses a global variable which should have been set during
	 * wt_status_collect_untracked().
	 *
	 * See dir.c:setup_standard_excludes()
	 */
	append_exclude_info(fd, excludes_file, "core_excludes");
}

static void append_repo_excludes_file_info(int fd)
{
	/*
	 * Likewise, there is a per-repo excludes file in .git/info/excludes
	 * that can change the results reported by status.  And the deserialize
	 * code needs to be able to reject the status cache if this file
	 * changes.
	 *
	 * See dir.c:setup_standard_excludes() and git_path_info_excludes().
	 * We replicate the pathname construction here because of the static
	 * variables/functions used in dir.c.
	 */
	char *path = repo_git_path(the_repository, "info/exclude");

	append_exclude_info(fd, path, "repo_excludes");

	free(path);
}

/*
 * WARNING: The status cache attempts to preserve the essential in-memory
 * status data after a status scan into a "serialization" (aka "status cache")
 * file.  It allows later "git status --deserialize=<foo>" instances to
 * just print the cached status results without scanning the workdir (and
 * without reading the index).
 *
 * The status cache file is valid as long as:
 * [1] the set of functional command line options are the same (think "-u").
 * [2] repo-local and user-global configuration settings are compatible.
 * [3] nothing in the workdir has changed.
 *
 * We rely on:
 * [1.a] We remember the relevant (functional, non-display) command line
 *       arguments in the status cache header.
 * [2.a] We use the mtime of the .git/index to detect staging changes.
 * [2.b] We use the mtimes of the excludes files to detect changes that
 *      might affect untracked file reporting.
 *
 * But we need external help to verify [3].
 * [] This includes changes to tracked files.
 * [] This includes changes to tracked .gitignore files that might change
 *    untracked file reporting.
 * [] This includes the creation of new, untracked per-directory .gitignore
 *    files that might change untracked file reporting.
 *
 * [3.a] On GVFS repos, we rely on the GVFS service (mount) daemon to
 *      watch the filesystem and invalidate (delete) the status cache
 *      when anything changes inside the workdir.
 *
 * [3.b] TODO This problem is not solved for non-GVFS repos.
 *       [] It is possible that the untracked-cache index extension
 *          could help with this but that requires status to read the
 *          index to load the extension.
 *       [] It is possible that the new fsmonitor facility could also
 *          provide this information, but that to requires reading the
 *          index.
 */

/*
 * Write V1 header fields.
 */
static void wt_serialize_v1_header(struct wt_status *s, int fd)
{
	/*
	 * Write select fields from the current index to help
	 * the deserializer recognize a stale data set.
	 */
	packet_write_fmt(fd, "index_mtime %d %d\n",
			 s->repo->index->timestamp.sec,
			 s->repo->index->timestamp.nsec);
	append_core_excludes_file_info(fd);
	append_repo_excludes_file_info(fd);

	/*
	 * Write data from wt_status to qualify this status report.
	 * That is, if this run specified "-uno", the consumer of
	 * our serialization should know that.
	 */
	packet_write_fmt(fd, "is_initial %d\n", s->is_initial);
	if (s->branch)
		packet_write_fmt(fd, "branch %s\n", s->branch);
	if (s->reference)
		packet_write_fmt(fd, "reference %s\n", s->reference);
	/* pathspec */
	/* verbose */
	/* amend */
	packet_write_fmt(fd, "whence %d\n", s->whence);
	/* nowarn */
	/* use_color */
	/* no_gettext */
	/* display_comment_prefix */
	/* relative_paths */
	/* submodule_summary */
	packet_write_fmt(fd, "show_ignored_mode %d\n", s->show_ignored_mode);
	packet_write_fmt(fd, "show_untracked_files %d\n", s->show_untracked_files);
	if (s->ignore_submodule_arg)
		packet_write_fmt(fd, "ignore_submodule_arg %s\n", s->ignore_submodule_arg);
	/* color_palette */
	/* colopts */
	/* null_termination */
	/* commit_template */
	/* show_branch */
	/* show_stash */
	packet_write_fmt(fd, "hints %d\n", s->hints);
	/* ahead_behind_flags */
	packet_write_fmt(fd, "detect_rename %d\n", s->detect_rename);
	packet_write_fmt(fd, "rename_score %d\n", s->rename_score);
	packet_write_fmt(fd, "rename_limit %d\n", s->rename_limit);
	/* status_format */
	packet_write_fmt(fd, "sha1_commit %s\n", oid_to_hex(&s->oid_commit));
	packet_write_fmt(fd, "committable %d\n", s->committable);
	packet_write_fmt(fd, "workdir_dirty %d\n", s->workdir_dirty);
	/* prefix */
	packet_flush(fd);
}

/*
 * Print changed/unmerged items.
 * We write raw (not c-quoted) pathname(s).  The rename_source is only
 * set when status computed a rename/copy.
 *
 * We ALWAYS write a final LF to the packet-line (for debugging)
 * even though Linux pathnames allow LFs.
 */
static inline void wt_serialize_v1_changed(struct wt_status *s UNUSED, int fd,
					   struct string_list_item *item)
{
	struct wt_status_change_data *d = item->util;
	struct wt_status_serialize_data sd;
	char *begin;
	char *end;
	char *p;
	int len_path, len_rename_source;

	trace_printf_key(&trace_serialize,
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

	sd.fixed.worktree_status       = htonl(d->worktree_status);
	sd.fixed.index_status          = htonl(d->index_status);
	sd.fixed.stagemask             = htonl(d->stagemask);
	sd.fixed.rename_status         = htonl(d->rename_status);
	sd.fixed.rename_score          = htonl(d->rename_score);
	sd.fixed.mode_head             = htonl(d->mode_head);
	sd.fixed.mode_index            = htonl(d->mode_index);
	sd.fixed.mode_worktree         = htonl(d->mode_worktree);
	sd.fixed.dirty_submodule       = htonl(d->dirty_submodule);
	sd.fixed.new_submodule_commits = htonl(d->new_submodule_commits);
	oidcpy(&sd.fixed.oid_head,  &d->oid_head);
	oidcpy(&sd.fixed.oid_index, &d->oid_index);

	begin = (char *)&sd;
	end = begin + sizeof(sd);

	p = sd.variant;

	/*
	 * Write <path> NUL [<rename_source>] NUL LF at the end of the buffer.
	 */
	len_path = strlen(item->string);
	len_rename_source = d->rename_source ? strlen(d->rename_source) : 0;

	/*
	 * This is a bit of a hack, but I don't want to split the
	 * status detail record across multiple pkt-lines.
	 */
	if (p + len_path + 1 + len_rename_source + 1 + 1 >= end)
		BUG("path to long to serialize '%s'", item->string);

	memcpy(p, item->string, len_path);
	p += len_path;
	*p++ = '\0';

	if (len_rename_source) {
		memcpy(p, d->rename_source, len_rename_source);
		p += len_rename_source;
	}
	*p++ = '\0';
	*p++ = '\n';

	if (packet_write_gently(fd, begin, (p - begin)))
		BUG("cannot serialize '%s'", item->string);
}

/*
 * Write raw (not c-quoted) pathname for an untracked item.
 * We ALWAYS write a final LF to the packet-line (for debugging)
 * even though Linux pathnames allows LFs.  That is, deserialization
 * should use the packet-line length and omit the final LF.
 */
static inline void wt_serialize_v1_untracked(struct wt_status *s UNUSED, int fd,
					     struct string_list_item *item)
{
	packet_write_fmt(fd, "%s\n", item->string);
}

/*
 * Write raw (not c-quoted) pathname for an ignored item.
 * We ALWAYS write a final LF to the packet-line (for debugging)
 * even though Linux pathnames allows LFs.
 */
static inline void wt_serialize_v1_ignored(struct wt_status *s UNUSED, int fd,
					   struct string_list_item *item)
{
	packet_write_fmt(fd, "%s\n", item->string);
}

/*
 * Serialize the list of changes to the given file.  The goal of this
 * is to just serialize the key fields in wt_status so that a
 * later command can rebuilt it and do the printing.
 *
 * We DO NOT include the contents of wt_status_state NOR
 * current branch info.  This info easily gets stale and
 * is relatively quick for the status consumer to compute
 * as necessary.
 */
void wt_status_serialize_v1(int fd, struct wt_status *s)
{
	struct string_list_item *iter;
	size_t k;

	/*
	 * version header must be first line.
	 */
	packet_write_fmt(fd, "version 1\n");

	wt_serialize_v1_header(s, fd);

	if (s->change.nr > 0) {
		packet_write_fmt(fd, "changed %"PRIuMAX"\n", (uintmax_t)s->change.nr);
		for (k = 0; k < s->change.nr; k++) {
			iter = &(s->change.items[k]);
			wt_serialize_v1_changed(s, fd, iter);
		}
		packet_flush(fd);
	}

	if (s->untracked.nr > 0) {
		packet_write_fmt(fd, "untracked %"PRIuMAX"\n", (uintmax_t)s->untracked.nr);
		for (k = 0; k < s->untracked.nr; k++) {
			iter = &(s->untracked.items[k]);
			wt_serialize_v1_untracked(s, fd, iter);
		}
		packet_flush(fd);
	}

	if (s->ignored.nr > 0) {
		packet_write_fmt(fd, "ignored %"PRIuMAX"\n", (uintmax_t)s->ignored.nr);
		for (k = 0; k < s->ignored.nr; k++) {
			iter = &(s->ignored.items[k]);
			wt_serialize_v1_ignored(s, fd, iter);
		}
		packet_flush(fd);
	}
}
