#include "strmap.h"
static void free_emitted_diff_symbols(struct emitted_diff_symbols *e)
{
	if (!e)
		return;
	free(e->buf);
	free(e);
}

		free_emitted_diff_symbols(ecbdata->diff_words->opt->emitted_symbols);
static struct strbuf *additional_headers(struct diff_options *o,
					 const char *path)
{
	if (!o->additional_path_headers)
		return NULL;
	return strmap_get(o->additional_path_headers, path);
}

static void add_formatted_headers(struct strbuf *msg,
				  struct strbuf *more_headers,
				  const char *line_prefix,
				  const char *meta,
				  const char *reset)
{
	char *next, *newline;

	for (next = more_headers->buf; *next; next = newline) {
		newline = strchrnul(next, '\n');
		strbuf_addf(msg, "%s%s%.*s%s\n", line_prefix, meta,
			    (int)(newline - next), next, reset);
		if (*newline)
			newline++;
	}
}

	if (!DIFF_FILE_VALID(one) && !DIFF_FILE_VALID(two)) {
		/*
		 * We should only reach this point for pairs from
		 * create_filepairs_for_header_only_notifications().  For
		 * these, we should avoid the "/dev/null" special casing
		 * above, meaning we avoid showing such pairs as either
		 * "new file" or "deleted file" below.
		 */
		lbl[0] = a_one;
		lbl[1] = b_two;
	}
	struct strbuf *more_headers = NULL;
	if ((more_headers = additional_headers(o, name))) {
		add_formatted_headers(msg, more_headers,
				      line_prefix, set, reset);
		*must_show_header = 1;
	}
static const char diff_status_letters[] = {
	DIFF_STATUS_ADDED,
	DIFF_STATUS_COPIED,
	DIFF_STATUS_DELETED,
	DIFF_STATUS_MODIFIED,
	DIFF_STATUS_RENAMED,
	DIFF_STATUS_TYPE_CHANGED,
	DIFF_STATUS_UNKNOWN,
	DIFF_STATUS_UNMERGED,
	DIFF_STATUS_FILTER_AON,
	DIFF_STATUS_FILTER_BROKEN,
	'\0',
};

static unsigned int filter_bit['Z' + 1];

static void prepare_filter_bits(void)
{
	int i;

	if (!filter_bit[DIFF_STATUS_ADDED]) {
		for (i = 0; diff_status_letters[i]; i++)
			filter_bit[(int) diff_status_letters[i]] = (1 << i);
	}
}

static unsigned filter_bit_tst(char status, const struct diff_options *opt)
{
	return opt->filter & filter_bit[(int) status];
}

unsigned diff_filter_bit(char status)
{
	prepare_filter_bits();
	return filter_bit[(int) status];
}

	if (options->filter_not) {
		if (!options->filter)
			options->filter = ~filter_bit[DIFF_STATUS_FILTER_AON];
		options->filter &= ~options->filter_not;
	}

			opt->filter_not |= bit;
		  N_("output to a specific file"),
	int include_conflict_headers =
	    (additional_headers(o, p->one->path) &&
	     (!o->filter || filter_bit_tst(DIFF_STATUS_UNMERGED, o)));

	/*
	 * Check if we can return early without showing a diff.  Note that
	 * diff_filepair only stores {oid, path, mode, is_valid}
	 * information for each path, and thus diff_unmodified_pair() only
	 * considers those bits of info.  However, we do not want pairs
	 * created by create_filepairs_for_header_only_notifications()
	 * (which always look like unmodified pairs) to be ignored, so
	 * return early if both p is unmodified AND we don't want to
	 * include_conflict_headers.
	 */
	if (diff_unmodified_pair(p) && !include_conflict_headers)
	/* Actually, we can also return early to avoid showing tree diffs */
		return;
int diff_queue_is_empty(struct diff_options *o)
	int include_conflict_headers =
	    (o->additional_path_headers &&
	     (!o->filter || filter_bit_tst(DIFF_STATUS_UNMERGED, o)));

	if (include_conflict_headers)
		return 0;

static void create_filepairs_for_header_only_notifications(struct diff_options *o)
{
	struct strset present;
	struct diff_queue_struct *q = &diff_queued_diff;
	struct hashmap_iter iter;
	struct strmap_entry *e;
	int i;

	strset_init_with_options(&present, /*pool*/ NULL, /*strdup*/ 0);

	/*
	 * Find out which paths exist in diff_queued_diff, preferring
	 * one->path for any pair that has multiple paths.
	 */
	for (i = 0; i < q->nr; i++) {
		struct diff_filepair *p = q->queue[i];
		char *path = p->one->path ? p->one->path : p->two->path;

		if (strmap_contains(o->additional_path_headers, path))
			strset_add(&present, path);
	}

	/*
	 * Loop over paths in additional_path_headers; for each NOT already
	 * in diff_queued_diff, create a synthetic filepair and insert that
	 * into diff_queued_diff.
	 */
	strmap_for_each_entry(o->additional_path_headers, &iter, e) {
		if (!strset_contains(&present, e->key)) {
			struct diff_filespec *one, *two;
			struct diff_filepair *p;

			one = alloc_filespec(e->key);
			two = alloc_filespec(e->key);
			fill_filespec(one, null_oid(), 0, 0);
			fill_filespec(two, null_oid(), 0, 0);
			p = diff_queue(q, one, two);
			p->status = DIFF_STATUS_MODIFIED;
		}
	}

	/* Re-sort the filepairs */
	diffcore_fix_diff_index();

	/* Cleanup */
	strset_clear(&present);
}

	if (o->additional_path_headers)
		create_filepairs_for_header_only_notifications(o);

	clear_pathspec(&options->pathspec);
	FREE_AND_NULL(options->parseopts);
	if (!q->nr && !options->additional_path_headers)