#include "mem-pool.h"
#include "dir.h"
#include "strmap.h"
static int diff_rename_limit_default = 1000;
	unsigned id;
#define EMITTED_DIFF_SYMBOL_INIT { 0 }
#define EMITTED_DIFF_SYMBOLS_INIT { 0 }
static void free_emitted_diff_symbols(struct emitted_diff_symbols *e)
{
	if (!e)
		return;
	free(e->buf);
	free(e);
}

	struct moved_entry *next_match;
			    const struct emitted_diff_symbol *b)
{
	int a_width = a->indent_width,
	if (a_width == INDENT_BLANKLINE && b_width == INDENT_BLANKLINE)
		return INDENT_BLANKLINE;
	return a_width - b_width;
static int cmp_in_block_with_wsd(const struct moved_entry *cur,
				 const struct emitted_diff_symbol *l,
				 struct moved_block *pmb)
{
	int a_width = cur->es->indent_width, b_width = l->indent_width;
	/* The text of each line must match */
	if (cur->es->id != l->id)
	/*
	 * If 'l' and 'cur' are both blank then we don't need to check the
	 * indent. We only need to check cur as we know the strings match.
	 * */
	if (a_width == INDENT_BLANKLINE)
	 * match those of the current block.
	delta = b_width - a_width;
	return delta != pmb->wsd;
struct interned_diff_symbol {
	struct hashmap_entry ent;
	struct emitted_diff_symbol *es;
};

static int interned_diff_symbol_cmp(const void *hashmap_cmp_fn_data,
				    const struct hashmap_entry *eptr,
				    const struct hashmap_entry *entry_or_key,
				    const void *keydata)
	const struct emitted_diff_symbol *a, *b;
	a = container_of(eptr, const struct interned_diff_symbol, ent)->es;
	b = container_of(entry_or_key, const struct interned_diff_symbol, ent)->es;
	return !xdiff_compare_lines(a->line + a->indent_off,
				    a->len - a->indent_off,
				    b->line + b->indent_off,
				    b->len - b->indent_off, flags);
static void prepare_entry(struct diff_options *o, struct emitted_diff_symbol *l,
			  struct interned_diff_symbol *s)
	unsigned int hash = xdiff_hash_string(l->line + l->indent_off,
					      l->len - l->indent_off, flags);
	hashmap_entry_init(&s->ent, hash);
	s->es = l;
struct moved_entry_list {
	struct moved_entry *add, *del;
};

static struct moved_entry_list *add_lines_to_move_detection(struct diff_options *o,
							    struct mem_pool *entry_mem_pool)
	struct mem_pool interned_pool;
	struct hashmap interned_map;
	struct moved_entry_list *entry_list = NULL;
	size_t entry_list_alloc = 0;
	unsigned id = 0;

	hashmap_init(&interned_map, interned_diff_symbol_cmp, o, 8096);
	mem_pool_init(&interned_pool, 1024 * 1024);

		struct interned_diff_symbol key;
		struct emitted_diff_symbol *l = &o->emitted_symbols->buf[n];
		struct interned_diff_symbol *s;
		struct moved_entry *entry;
		if (l->s != DIFF_SYMBOL_PLUS && l->s != DIFF_SYMBOL_MINUS) {
			fill_es_indent_data(l);
		prepare_entry(o, l, &key);
		s = hashmap_get_entry(&interned_map, &key, ent, &key.ent);
		if (s) {
			l->id = s->es->id;
		} else {
			l->id = id;
			ALLOC_GROW_BY(entry_list, id, 1, entry_list_alloc);
			hashmap_add(&interned_map,
				    memcpy(mem_pool_alloc(&interned_pool,
							  sizeof(key)),
					   &key, sizeof(key)));
		}
		entry = mem_pool_alloc(entry_mem_pool, sizeof(*entry));
		entry->es = l;
		entry->next_line = NULL;
		if (prev_line && prev_line->es->s == l->s)
			prev_line->next_line = entry;
		prev_line = entry;
		if (l->s == DIFF_SYMBOL_PLUS) {
			entry->next_match = entry_list[l->id].add;
			entry_list[l->id].add = entry;
		} else {
			entry->next_match = entry_list[l->id].del;
			entry_list[l->id].del = entry;
		}

	hashmap_clear(&interned_map);
	mem_pool_discard(&interned_pool, 0);

	return entry_list;
				struct emitted_diff_symbol *l,
				int *pmb_nr)
	int i, j;

	for (i = 0, j = 0; i < *pmb_nr; i++) {
		int match;
		if (o->color_moved_ws_handling &
		    COLOR_MOVED_WS_ALLOW_INDENTATION_CHANGE)
			match = cur &&
				!cmp_in_block_with_wsd(cur, l, &pmb[i]);
		else
			match = cur && cur->es->id == l->id;
		if (match) {
			pmb[j] = pmb[i];
			pmb[j++].match = cur;
	*pmb_nr = j;
static void fill_potential_moved_blocks(struct diff_options *o,
					struct moved_entry *match,
					struct emitted_diff_symbol *l,
					struct moved_block **pmb_p,
					int *pmb_alloc_p, int *pmb_nr_p)
{
	struct moved_block *pmb = *pmb_p;
	int pmb_alloc = *pmb_alloc_p, pmb_nr = *pmb_nr_p;
	/*
	 * The current line is the start of a new block.
	 * Setup the set of potential blocks.
	 */
	for (; match; match = match->next_match) {
		ALLOC_GROW(pmb, pmb_nr + 1, pmb_alloc);
		if (o->color_moved_ws_handling &
		    COLOR_MOVED_WS_ALLOW_INDENTATION_CHANGE)
			pmb[pmb_nr].wsd = compute_ws_delta(l, match->es);
		else
			pmb[pmb_nr].wsd = 0;
		pmb[pmb_nr++].match = match;
	*pmb_p = pmb;
	*pmb_alloc_p = pmb_alloc;
	*pmb_nr_p = pmb_nr;
#define DIFF_SYMBOL_MOVED_LINE_ZEBRA_MASK \
  (DIFF_SYMBOL_MOVED_LINE | DIFF_SYMBOL_MOVED_LINE_ALT)
		o->emitted_symbols->buf[n - i].flags &= ~DIFF_SYMBOL_MOVED_LINE_ZEBRA_MASK;
				struct moved_entry_list *entry_list)
	enum diff_symbol moved_symbol = DIFF_SYMBOL_BINARY_DIFF_HEADER;
			match = entry_list[l->id].del;
			match = entry_list[l->id].add;
		if (pmb_nr && (!match || l->s != moved_symbol)) {
			if (!adjust_last_block(o, n, block_length) &&
			    block_length > 1) {
				/*
				 * Rewind in case there is another match
				 * starting at the second line of the block
				 */
				match = NULL;
				n -= block_length;
			}
		}
		if (!match) {
			moved_symbol = DIFF_SYMBOL_BINARY_DIFF_HEADER;
		pmb_advance_or_null(o, l, pmb, &pmb_nr);
			int contiguous = adjust_last_block(o, n, block_length);

			if (!contiguous && block_length > 1)
				/*
				 * Rewind in case there is another match
				 * starting at the second line of the block
				 */
				n -= block_length;
			else
				fill_potential_moved_blocks(o, match, l,
							    &pmb, &pmb_alloc,
							    &pmb_nr);
			if (contiguous && pmb_nr && moved_symbol == l->s)
			if (pmb_nr)
				moved_symbol = l->s;
			else
				moved_symbol = DIFF_SYMBOL_BINARY_DIFF_HEADER;

	struct emitted_diff_symbol e = {
		.line = line, .len = len, .flags = flags, .s = s
	};
		free_emitted_diff_symbols(ecbdata->diff_words->opt->emitted_symbols);
static int fn_out_consume(void *priv, char *line, unsigned long len)
		return 0;
			return 0;
			return 0;
			return 0;
		return 0;
	return 0;
static int diffstat_consume(void *priv, char *line, unsigned long len)
	return 0;
static int checkdiff_consume(void *priv, char *line, unsigned long len)
			return 0;
	return 0;
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
		xecfg.flags = XDL_EMIT_NO_HUNK_HDR;
		if (xdi_diff_outf(&mf1, &mf2, NULL,
	/*
	 * If this path does not match our sparse-checkout definition,
	 * then the file will not be in the working directory.
	 */
	if (!path_in_sparse_checkout(name, istate))
		return 0;

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

		die(_("options '%s', '%s', '%s', and '%s' cannot be used together"),
			"--name-only", "--name-status", "--check", "-s");
		die(_("options '%s', '%s', and '%s' cannot be used together"),
			"-G", "-S", "--find-object");

	if (HAS_MULTI_BITS(options->pickaxe_opts & DIFF_PICKAXE_KINDS_G_REGEX_MASK))
		die(_("options '%s' and '%s' cannot be used together, use '%s' with '%s'"),
			"-G", "--pickaxe-regex", "--pickaxe-regex", "-S");

	if (HAS_MULTI_BITS(options->pickaxe_opts & DIFF_PICKAXE_KINDS_ALL_OBJFIND_MASK))
		die(_("options '%s' and '%s' cannot be used together, use '%s' with '%s' and '%s'"),
			"--pickaxe-all", "--find-object", "--pickaxe-all", "-G", "-S");
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

static int patch_id_consume(void *priv, char *line, unsigned long len)
		return 0;
	return 0;
		xecfg.flags = XDL_EMIT_NO_HUNK_HDR;
		if (xdi_diff_outf(&mf1, &mf2, NULL,
N_("exhaustive rename detection was skipped due to too many files.");
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

			struct mem_pool entry_pool;
			struct moved_entry_list *entry_list;
			mem_pool_init(&entry_pool, 1024 * 1024);
			entry_list = add_lines_to_move_detection(o,
								 &entry_pool);
			mark_color_as_moved(o, entry_list);
			mem_pool_discard(&entry_pool, 0);
			free(entry_list);
	clear_pathspec(&options->pathspec);
	FREE_AND_NULL(options->parseopts);
	if (!q->nr && !options->additional_path_headers)
	strvec_push(&child.args, pgm);
	strvec_push(&child.args, temp->name);