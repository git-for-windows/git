#include "git-compat-util.h"
#include "hex.h"
#include "repository.h"
#include "wt-status.h"
#include "pkt-line.h"
#include "trace.h"
#include "read-cache-ll.h"

static struct trace_key trace_serialize = TRACE_KEY_INIT(SERIALIZE);

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
 * Serialize the list of changes to stdout.  The goal of this
 * is to just serialize the key fields in wt_status so that a
 * later command can rebuilt it and do the printing.
 *
 * We DO NOT include the contents of wt_status_state NOR
 * current branch info.  This info easily gets stale and
 * is relatively quick for the status consumer to compute
 * as necessary.
 */
void wt_status_serialize_v1(struct wt_status *s)
{
	int fd = 1; /* we always write to stdout */
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
