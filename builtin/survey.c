#define USE_THE_REPOSITORY_VARIABLE

#include "builtin.h"
#include "config.h"
#include "environment.h"
#include "hex.h"
#include "object.h"
#include "odb.h"
#include "object-name.h"
#include "parse-options.h"
#include "path-walk.h"
#include "progress.h"
#include "ref-filter.h"
#include "refs.h"
#include "revision.h"
#include "strbuf.h"
#include "strvec.h"
#include "trace2.h"
#include "tree.h"
#include "tree-walk.h"

static const char * const survey_usage[] = {
	N_("(EXPERIMENTAL!) git survey <options>"),
	NULL,
};

struct survey_refs_wanted {
	int want_all_refs; /* special override */

	int want_branches;
	int want_tags;
	int want_remotes;
	int want_detached;
	int want_other; /* see FILTER_REFS_OTHERS -- refs/notes/, refs/stash/ */
};

static struct survey_refs_wanted default_ref_options = {
	.want_all_refs = 1,
};

struct survey_opts {
	int verbose;
	int show_progress;

	int show_largest_commits_by_nr_parents;
	int show_largest_commits_by_size_bytes;

	int show_largest_trees_by_nr_entries;
	int show_largest_trees_by_size_bytes;

	int show_largest_blobs_by_size_bytes;

	int top_nr;
	struct survey_refs_wanted refs;
};

struct survey_report_ref_summary {
	size_t refs_nr;
	size_t branches_nr;
	size_t remote_refs_nr;
	size_t tags_nr;
	size_t tags_annotated_nr;
	size_t others_nr;
	size_t unknown_nr;

	size_t cnt_symref;

	size_t cnt_packed;
	size_t cnt_loose;

	/*
	 * Measure the length of the refnames.  We can look for
	 * potential platform limits.  The partial sums may help us
	 * estimate the size of a haves/wants conversation, since each
	 * refname and a SHA must be transmitted.
	 */
	size_t len_max_local_refname;
	size_t len_sum_local_refnames;
	size_t len_max_remote_refname;
	size_t len_sum_remote_refnames;
};

/*
 * HBIN -- hex binning (histogram bucketing).
 *
 * We create histograms for various counts and sums.  Since we have a
 * wide range of values (objects range in size from 1 to 4G bytes), a
 * linear bucketing is not interesting.  Instead, lets use a
 * log16()-based bucketing.  This gives us a better spread on the low
 * and middle range and a coarse bucketing on the high end.
 *
 * The idea here is that it doesn't matter if you have n 1GB blobs or
 * n/2 1GB blobs and n/2 1.5GB blobs -- either way you have a scaling
 * problem that we want to report on.
 */
#define HBIN_LEN (sizeof(unsigned long) * 2)
#define HBIN_MASK (0xF)
#define HBIN_SHIFT (4)

static int hbin(unsigned long value)
{
	for (size_t k = 0; k < HBIN_LEN; k++) {
		if ((value & ~(HBIN_MASK)) == 0)
			return k;
		value >>= HBIN_SHIFT;
	}

	return 0; /* should not happen */
}

/*
 * QBIN -- base4 binning (histogram bucketing).
 *
 * This is the same idea as the above, but we want better granularity
 * in the low end and don't expect as many large values.
 */
#define QBIN_LEN (sizeof(unsigned long) * 4)
#define QBIN_MASK (0x3)
#define QBIN_SHIFT (2)

static int qbin(unsigned long value)
{
	for (size_t k = 0; k < QBIN_LEN; k++) {
		if ((value & ~(QBIN_MASK)) == 0)
			return k;
		value >>= (QBIN_SHIFT);
	}

	return 0; /* should not happen */
}

/*
 * histogram bin for objects.
 */
struct obj_hist_bin {
	uint64_t sum_size;      /* sum(object_size) for all objects in this bin */
	uint64_t sum_disk_size; /* sum(on_disk_size) for all objects in this bin */
	uint32_t cnt_seen;      /* number seen in this bin */
};

static void incr_obj_hist_bin(struct obj_hist_bin *pbin,
			       unsigned long object_length,
			       off_t disk_sizep)
{
	pbin->sum_size += object_length;
	pbin->sum_disk_size += disk_sizep;
	pbin->cnt_seen++;
}

/*
 * Remember the largest n objects for some scaling dimension.  This
 * could be the observed object size or number of entries in a tree.
 * We'll use this to generate a sorted vector in the output for that
 * dimension.
 */
struct large_item {
	uint64_t size;
	struct object_id oid;

	/*
	 * For blobs and trees the name field is the pathname of the
	 * file or directory.  Root trees will have a zero-length
	 * name.  The name field is not currenly used for commits.
	 */
	struct strbuf name;

	/*
	 * For blobs and trees remember the transient commit from
	 * the treewalk so that we can say that this large item
	 * first appeared in this commit (relative to the treewalk
	 * order).
	 */
	struct object_id containing_commit_oid;
};

struct large_item_vec {
	char *dimension_label;
	char *item_label;
	uint64_t nr_items;
	struct large_item items[FLEX_ARRAY]; /* nr_items */
};

static struct large_item_vec *alloc_large_item_vec(const char *dimension_label,
						   const char *item_label,
						   uint64_t nr_items)
{
	struct large_item_vec *vec;
	size_t flex_len = nr_items * sizeof(struct large_item);
	size_t k;

	if (!nr_items)
		return NULL;

	vec = xcalloc(1, (sizeof(struct large_item_vec) + flex_len));
	vec->dimension_label = strdup(dimension_label);
	vec->item_label = strdup(item_label);
	vec->nr_items = nr_items;

	for (k = 0; k < nr_items; k++)
		strbuf_init(&vec->items[k].name, 0);

	return vec;
}

static void free_large_item_vec(struct large_item_vec *vec)
{
	if (!vec)
		return;

	for (size_t k = 0; k < vec->nr_items; k++)
		strbuf_release(&vec->items[k].name);

	free(vec->dimension_label);
	free(vec->item_label);
	free(vec);
}

static void maybe_insert_large_item(struct large_item_vec *vec,
				    uint64_t size,
				    struct object_id *oid,
				    const char *name,
				    const struct object_id *containing_commit_oid)
{
	size_t rest_len;
	size_t k;

	if (!vec || !vec->nr_items)
		return;

	/*
	 * Since the odds an object being among the largest n
	 * is small, shortcut and see if it is smaller than
	 * the smallest one in our set and quickly reject it.
	 */
	if (size < vec->items[vec->nr_items - 1].size)
		return;

	for (k = 0; k < vec->nr_items; k++) {
		if (size < vec->items[k].size)
			continue;

		/*
		 * The last large_item in the vector is about to be
		 * overwritten by the previous one during the shift.
		 * Steal its allocated strbuf and reuse it.
		 */
		strbuf_release(&vec->items[vec->nr_items - 1].name);

		/* push items[k..] down one and insert data for this item here */

		rest_len = (vec->nr_items - k - 1) * sizeof(struct large_item);
		if (rest_len)
			memmove(&vec->items[k + 1], &vec->items[k], rest_len);

		memset(&vec->items[k], 0, sizeof(struct large_item));
		vec->items[k].size = size;
		oidcpy(&vec->items[k].oid, oid);
		oidcpy(&vec->items[k].containing_commit_oid, containing_commit_oid ? containing_commit_oid : null_oid(the_hash_algo));
		strbuf_init(&vec->items[k].name, 0);
		if (name && *name)
			strbuf_addstr(&vec->items[k].name, name);

		return;
	}
}

/*
 * Common fields for any type of object.
 */
struct survey_stats_base_object {
	uint32_t cnt_seen;

	uint32_t cnt_missing; /* we may have a partial clone. */

	/*
	 * Number of objects grouped by where they are stored on disk.
	 * This is a function of how the ODB is packed.
	 */
	uint32_t cnt_cached;   /* see oi.whence */
	uint32_t cnt_loose;    /* see oi.whence */
	uint32_t cnt_packed;   /* see oi.whence */
	uint32_t cnt_dbcached; /* see oi.whence */

	uint64_t sum_size; /* sum(object_size) */
	uint64_t sum_disk_size; /* sum(disk_size) */

	/*
	 * A histogram of the count of objects, the observed size, and
	 * the on-disk size grouped by the observed size.
	 */
	struct obj_hist_bin size_hbin[HBIN_LEN];
};

/*
 * PBIN -- parent vector binning (histogram bucketing).
 *
 * We create a histogram based upon the number of parents
 * in a commit.  This is a simple linear vector.  It starts
 * at zero for "initial" commits.
 *
 * If a commit has more parents, just put it in the last bin.
 */
#define PBIN_VEC_LEN (32)

struct survey_stats_commits {
	struct survey_stats_base_object base;

	/*
	 * Count of commits with k parents.
	 */
	uint32_t parent_cnt_pbin[PBIN_VEC_LEN];

	struct large_item_vec *vec_largest_by_nr_parents;
	struct large_item_vec *vec_largest_by_size_bytes;
};

/*
 * Stats for reachable trees.
 */
struct survey_stats_trees {
	struct survey_stats_base_object base;

	/*
	 * Keep a vector of the trees with the most number of entries.
	 * This gives us a feel for the width of a tree when there are
	 * gigantic directories.
	 */
	struct large_item_vec *vec_largest_by_nr_entries;

	/*
	 * Keep a vector of the trees with the largest size in bytes.
	 * The contents of this may or may not match items in the other
	 * vector, since entryname length can alter the results.
	 */
	struct large_item_vec *vec_largest_by_size_bytes;

	/*
	 * Computing the sum of the number of entries across all trees
	 * is probably not that interesting.
	 */
	uint64_t sum_entries; /* sum(nr_entries) -- sum across all trees */

	/*
	 * A histogram of the count of trees, the observed size, and
	 * the on-disk size grouped by the number of entries in the tree.
	 */
	struct obj_hist_bin entry_qbin[QBIN_LEN];
};

/*
 * Stats for reachable blobs.
 */
struct survey_stats_blobs {
	struct survey_stats_base_object base;

	/*
	 * Remember the OIDs of the largest n blobs.
	 */
	struct large_item_vec *vec_largest_by_size_bytes;
};

struct survey_report_object_summary {
	size_t commits_nr;
	size_t tags_nr;
	size_t trees_nr;
	size_t blobs_nr;

	struct survey_stats_commits commits;
	struct survey_stats_trees   trees;
	struct survey_stats_blobs   blobs;
};

/**
 * For some category given by 'label', count the number of objects
 * that match that label along with the on-disk size and the size
 * after decompressing (both with delta bases and zlib).
 */
struct survey_report_object_size_summary {
	char *label;
	size_t nr;
	size_t disk_size;
	size_t inflated_size;
	size_t num_missing;
};

typedef int (*survey_top_cmp)(void *v1, void *v2);

static int cmp_by_nr(void *v1, void *v2)
{
	struct survey_report_object_size_summary *s1 = v1;
	struct survey_report_object_size_summary *s2 = v2;

	if (s1->nr < s2->nr)
		return -1;
	if (s1->nr > s2->nr)
		return 1;
	return 0;
}

static int cmp_by_disk_size(void *v1, void *v2)
{
	struct survey_report_object_size_summary *s1 = v1;
	struct survey_report_object_size_summary *s2 = v2;

	if (s1->disk_size < s2->disk_size)
		return -1;
	if (s1->disk_size > s2->disk_size)
		return 1;
	return 0;
}

static int cmp_by_inflated_size(void *v1, void *v2)
{
	struct survey_report_object_size_summary *s1 = v1;
	struct survey_report_object_size_summary *s2 = v2;

	if (s1->inflated_size < s2->inflated_size)
		return -1;
	if (s1->inflated_size > s2->inflated_size)
		return 1;
	return 0;
}

/**
 * Store a list of "top" categories by some sorting function. When
 * inserting a new category, reorder the list and free the one that
 * got ejected (if any).
 */
struct survey_report_top_table {
	const char *name;
	survey_top_cmp cmp_fn;
	size_t nr;
	size_t alloc;

	/**
	 * 'data' stores an array of structs and must be cast into
	 * the proper array type before evaluating an index.
	 */
	void *data;
};

static void init_top_sizes(struct survey_report_top_table *top,
			   size_t limit, const char *name,
			   survey_top_cmp cmp)
{
	struct survey_report_object_size_summary *sz_array;

	top->name = name;
	top->cmp_fn = cmp;
	top->alloc = limit;
	top->nr = 0;

	CALLOC_ARRAY(sz_array, limit);
	top->data = sz_array;
}

MAYBE_UNUSED
static void clear_top_sizes(struct survey_report_top_table *top)
{
	struct survey_report_object_size_summary *sz_array = top->data;

	for (size_t i = 0; i < top->nr; i++)
		free(sz_array[i].label);
	free(sz_array);
}

static void maybe_insert_into_top_size(struct survey_report_top_table *top,
				       struct survey_report_object_size_summary *summary)
{
	struct survey_report_object_size_summary *sz_array = top->data;
	size_t pos = top->nr;

	/* Compare against list from the bottom. */
	while (pos > 0 && top->cmp_fn(&sz_array[pos - 1], summary) < 0)
		pos--;

	/* Not big enough! */
	if (pos >= top->alloc)
		return;

	/* We need to shift the data. */
	if (top->nr == top->alloc)
		free(sz_array[top->nr - 1].label);
	else
		top->nr++;

	for (size_t i = top->nr - 1; i > pos; i--)
		memcpy(&sz_array[i], &sz_array[i - 1], sizeof(*sz_array));

	memcpy(&sz_array[pos], summary, sizeof(*summary));
	sz_array[pos].label = xstrdup(summary->label);
}

/**
 * This struct contains all of the information that needs to be printed
 * at the end of the exploration of the repository and its references.
 */
struct survey_report {
	struct survey_report_ref_summary refs;
	struct survey_report_object_summary reachable_objects;

	struct survey_report_object_size_summary *by_type;

	struct survey_report_top_table *top_paths_by_count;
	struct survey_report_top_table *top_paths_by_disk;
	struct survey_report_top_table *top_paths_by_inflate;
};

#define REPORT_TYPE_COMMIT 0
#define REPORT_TYPE_TREE 1
#define REPORT_TYPE_BLOB 2
#define REPORT_TYPE_TAG 3
#define REPORT_TYPE_COUNT 4

struct survey_context {
	struct repository *repo;

	/* Options that control what is done. */
	struct survey_opts opts;

	/* Info for output only. */
	struct survey_report report;

	/*
	 * The rest of the members are about enabling the activity
	 * of the 'git survey' command, including ref listings, object
	 * pointers, and progress.
	 */

	struct progress *progress;
	size_t progress_nr;
	size_t progress_total;

	struct strvec refs;
	struct ref_array ref_array;
};

static void clear_survey_context(struct survey_context *ctx)
{
	free_large_item_vec(ctx->report.reachable_objects.commits.vec_largest_by_nr_parents);
	free_large_item_vec(ctx->report.reachable_objects.commits.vec_largest_by_size_bytes);
	free_large_item_vec(ctx->report.reachable_objects.trees.vec_largest_by_nr_entries);
	free_large_item_vec(ctx->report.reachable_objects.trees.vec_largest_by_size_bytes);
	free_large_item_vec(ctx->report.reachable_objects.blobs.vec_largest_by_size_bytes);

	ref_array_clear(&ctx->ref_array);
	strvec_clear(&ctx->refs);
}

struct survey_table {
	const char *table_name;
	struct strvec header;
	struct strvec *rows;
	size_t rows_nr;
	size_t rows_alloc;
};

#define SURVEY_TABLE_INIT {	\
	.header = STRVEC_INIT,	\
}

static void clear_table(struct survey_table *table)
{
	strvec_clear(&table->header);
	for (size_t i = 0; i < table->rows_nr; i++)
		strvec_clear(&table->rows[i]);
	free(table->rows);
}

static void insert_table_rowv(struct survey_table *table, ...)
{
	va_list ap;
	char *arg;
	ALLOC_GROW(table->rows, table->rows_nr + 1, table->rows_alloc);

	memset(&table->rows[table->rows_nr], 0, sizeof(struct strvec));

	va_start(ap, table);
	while ((arg = va_arg(ap, char *)))
		strvec_push(&table->rows[table->rows_nr], arg);
	va_end(ap);

	table->rows_nr++;
}

#define SECTION_SEGMENT "========================================"
#define SECTION_SEGMENT_LEN 40
static const char *section_line = SECTION_SEGMENT
				  SECTION_SEGMENT
				  SECTION_SEGMENT
				  SECTION_SEGMENT;
static const size_t section_len = 4 * SECTION_SEGMENT_LEN;

static void print_table_title(const char *name, size_t *widths, size_t nr)
{
	size_t width = 3 * (nr - 1);
	size_t min_width = strlen(name);

	for (size_t i = 0; i < nr; i++)
		width += widths[i];

	if (width < min_width)
		width = min_width;

	if (width > section_len)
		width = section_len;

	printf("\n%s\n%.*s\n", name, (int)width, section_line);
}

static void print_row_plaintext(struct strvec *row, size_t *widths)
{
	static struct strbuf line = STRBUF_INIT;
	strbuf_setlen(&line, 0);

	for (size_t i = 0; i < row->nr; i++) {
		const char *str = row->v[i];
		size_t len = strlen(str);
		if (i)
			strbuf_add(&line, " | ", 3);
		strbuf_addchars(&line, ' ', widths[i] - len);
		strbuf_add(&line, str, len);
	}
	printf("%s\n", line.buf);
}

static void print_divider_plaintext(size_t *widths, size_t nr)
{
	static struct strbuf line = STRBUF_INIT;
	strbuf_setlen(&line, 0);

	for (size_t i = 0; i < nr; i++) {
		if (i)
			strbuf_add(&line, "-+-", 3);
		strbuf_addchars(&line, '-', widths[i]);
	}
	printf("%s\n", line.buf);
}

static void print_table_plaintext(struct survey_table *table)
{
	size_t *column_widths;
	size_t columns_nr = table->header.nr;
	CALLOC_ARRAY(column_widths, columns_nr);

	for (size_t i = 0; i < columns_nr; i++) {
		column_widths[i] = strlen(table->header.v[i]);

		for (size_t j = 0; j < table->rows_nr; j++) {
			size_t rowlen = strlen(table->rows[j].v[i]);
			if (column_widths[i] < rowlen)
				column_widths[i] = rowlen;
		}
	}

	print_table_title(table->table_name, column_widths, columns_nr);
	print_row_plaintext(&table->header, column_widths);
	print_divider_plaintext(column_widths, columns_nr);

	for (size_t j = 0; j < table->rows_nr; j++)
		print_row_plaintext(&table->rows[j], column_widths);

	free(column_widths);
}

static void pretty_print_bin_table(const char *title_caption,
				   const char *bucket_header,
				   struct obj_hist_bin *bin,
				   uint64_t bin_len, int bin_shift, uint64_t bin_mask)
{
	struct survey_table table = SURVEY_TABLE_INIT;
	struct strbuf bucket = STRBUF_INIT, cnt_seen = STRBUF_INIT;
	struct strbuf sum_size = STRBUF_INIT, sum_disk_size = STRBUF_INIT;
	uint64_t lower = 0;
	uint64_t upper = bin_mask;

	table.table_name = title_caption;
	strvec_pushl(&table.header, bucket_header, "Count", "Size", "Disk Size", NULL);

	for (size_t k = 0; k < bin_len; k++) {
		struct obj_hist_bin *p = bin + k;
		uintmax_t lower_k = lower;
		uintmax_t upper_k = upper;

		lower = upper+1;
		upper = (upper << bin_shift) + bin_mask;

		if (!p->cnt_seen)
			continue;

		strbuf_reset(&bucket);
		strbuf_addf(&bucket, "%"PRIuMAX"..%"PRIuMAX, lower_k, upper_k);

		strbuf_reset(&cnt_seen);
		strbuf_addf(&cnt_seen, "%"PRIuMAX, (uintmax_t)p->cnt_seen);

		strbuf_reset(&sum_size);
		strbuf_addf(&sum_size, "%"PRIuMAX, (uintmax_t)p->sum_size);

		strbuf_reset(&sum_disk_size);
		strbuf_addf(&sum_disk_size, "%"PRIuMAX, (uintmax_t)p->sum_disk_size);

		insert_table_rowv(&table, bucket.buf,
			     cnt_seen.buf, sum_size.buf, sum_disk_size.buf, NULL);
	}
	strbuf_release(&bucket);
	strbuf_release(&cnt_seen);
	strbuf_release(&sum_size);
	strbuf_release(&sum_disk_size);

	print_table_plaintext(&table);
	clear_table(&table);
}

static void survey_report_hbin(const char *title_caption,
			       struct obj_hist_bin *bin)
{
	pretty_print_bin_table(title_caption,
			       "Byte Range",
			       bin,
			       HBIN_LEN, HBIN_SHIFT, HBIN_MASK);
}

static void survey_report_tree_lengths(struct survey_context *ctx)
{
	pretty_print_bin_table(_("TREE HISTOGRAM BY NUMBER OF ENTRIES"),
			       "Entry Range",
			       ctx->report.reachable_objects.trees.entry_qbin,
			       QBIN_LEN, QBIN_SHIFT, QBIN_MASK);
}

static void survey_report_commit_parents(struct survey_context *ctx)
{
	struct survey_stats_commits *psc = &ctx->report.reachable_objects.commits;
	struct survey_table table = SURVEY_TABLE_INIT;
	struct strbuf parents = STRBUF_INIT, counts = STRBUF_INIT;

	table.table_name = _("HISTOGRAM BY NUMBER OF COMMIT PARENTS");
	strvec_pushl(&table.header, "Parents", "Counts", NULL);

	for (int k = 0; k < PBIN_VEC_LEN; k++)
		if (psc->parent_cnt_pbin[k]) {
			strbuf_reset(&parents);
			strbuf_addf(&parents, "%02d", k);

			strbuf_reset(&counts);
			strbuf_addf(&counts, "%14"PRIuMAX, (uintmax_t)psc->parent_cnt_pbin[k]);

			insert_table_rowv(&table, parents.buf, counts.buf, NULL);
		}
	strbuf_release(&parents);
	strbuf_release(&counts);

	print_table_plaintext(&table);
	clear_table(&table);
}

static void survey_report_largest_vec(struct large_item_vec *vec)
{
	struct survey_table table = SURVEY_TABLE_INIT;
	struct strbuf size = STRBUF_INIT;

	if (!vec || !vec->nr_items)
		return;

	table.table_name = vec->dimension_label;
	strvec_pushl(&table.header, "Size", "OID", "Name", "Commit", NULL);

	for (size_t k = 0; k < vec->nr_items; k++) {
		struct large_item *pk = &vec->items[k];
		if (!is_null_oid(&pk->oid)) {
			strbuf_reset(&size);
			strbuf_addf(&size, "%"PRIuMAX, (uintmax_t)pk->size);

			insert_table_rowv(&table, size.buf, oid_to_hex(&pk->oid), pk->name.buf,
					  is_null_oid(&pk->containing_commit_oid) ?
					  "" : oid_to_hex(&pk->containing_commit_oid),
					  NULL);
		}
	}
	strbuf_release(&size);

	print_table_plaintext(&table);
	clear_table(&table);
}

static void survey_report_plaintext_refs(struct survey_context *ctx)
{
	struct survey_report_ref_summary *refs = &ctx->report.refs;
	struct survey_table table = SURVEY_TABLE_INIT;

	table.table_name = _("REFERENCES SUMMARY");

	strvec_push(&table.header, _("Ref Type"));
	strvec_push(&table.header, _("Count"));

	if (ctx->opts.refs.want_all_refs || ctx->opts.refs.want_branches) {
		char *fmt = xstrfmt("%"PRIuMAX"", (uintmax_t)refs->branches_nr);
		insert_table_rowv(&table, _("Branches"), fmt, NULL);
		free(fmt);
	}

	if (ctx->opts.refs.want_all_refs || ctx->opts.refs.want_remotes) {
		char *fmt = xstrfmt("%"PRIuMAX"", (uintmax_t)refs->remote_refs_nr);
		insert_table_rowv(&table, _("Remote refs"), fmt, NULL);
		free(fmt);
	}

	if (ctx->opts.refs.want_all_refs || ctx->opts.refs.want_tags) {
		char *fmt = xstrfmt("%"PRIuMAX"", (uintmax_t)refs->tags_nr);
		insert_table_rowv(&table, _("Tags (all)"), fmt, NULL);
		free(fmt);
		fmt = xstrfmt("%"PRIuMAX"", (uintmax_t)refs->tags_annotated_nr);
		insert_table_rowv(&table, _("Tags (annotated)"), fmt, NULL);
		free(fmt);
	}

	/*
	 * SymRefs are somewhat orthogonal to the above classification (e.g.
	 * "HEAD" --> detached and "refs/remotes/origin/HEAD" --> remote) so the
	 * above classified counts will already include them, but it is less
	 * confusing to display them here than to create a whole new section.
	 */
	if (ctx->report.refs.cnt_symref) {
		char *fmt = xstrfmt("%"PRIuMAX"", (uintmax_t)refs->cnt_symref);
		insert_table_rowv(&table, _("Symbolic refs"), fmt, NULL);
		free(fmt);
	}

	if (ctx->report.refs.cnt_loose || ctx->report.refs.cnt_packed) {
		char *fmt = xstrfmt("%"PRIuMAX"", (uintmax_t)refs->cnt_loose);
		insert_table_rowv(&table, _("Loose refs"), fmt, NULL);
		free(fmt);
		fmt = xstrfmt("%"PRIuMAX"", (uintmax_t)refs->cnt_packed);
		insert_table_rowv(&table, _("Packed refs"), fmt, NULL);
		free(fmt);
	}

	if (ctx->report.refs.len_max_local_refname || ctx->report.refs.len_max_remote_refname) {
		char *fmt = xstrfmt("%"PRIuMAX"", (uintmax_t)refs->len_max_local_refname);
		insert_table_rowv(&table, _("Max local refname length"), fmt, NULL);
		free(fmt);
		fmt = xstrfmt("%"PRIuMAX"", (uintmax_t)refs->len_sum_local_refnames);
		insert_table_rowv(&table, _("Sum local refnames length"), fmt, NULL);
		free(fmt);
		fmt = xstrfmt("%"PRIuMAX"", (uintmax_t)refs->len_max_remote_refname);
		insert_table_rowv(&table, _("Max remote refname length"), fmt, NULL);
		free(fmt);
		fmt = xstrfmt("%"PRIuMAX"", (uintmax_t)refs->len_sum_remote_refnames);
		insert_table_rowv(&table, _("Sum remote refnames length"), fmt, NULL);
		free(fmt);
	}

	print_table_plaintext(&table);
	clear_table(&table);
}

static void survey_report_plaintext_reachable_object_summary(struct survey_context *ctx)
{
	struct survey_report_object_summary *objs = &ctx->report.reachable_objects;
	struct survey_table table = SURVEY_TABLE_INIT;
	char *fmt;

	table.table_name = _("REACHABLE OBJECT SUMMARY");

	strvec_push(&table.header, _("Object Type"));
	strvec_push(&table.header, _("Count"));

	fmt = xstrfmt("%"PRIuMAX"", (uintmax_t)objs->tags_nr);
	insert_table_rowv(&table, _("Tags"), fmt, NULL);
	free(fmt);

	fmt = xstrfmt("%"PRIuMAX"", (uintmax_t)objs->commits_nr);
	insert_table_rowv(&table, _("Commits"), fmt, NULL);
	free(fmt);

	fmt = xstrfmt("%"PRIuMAX"", (uintmax_t)objs->trees_nr);
	insert_table_rowv(&table, _("Trees"), fmt, NULL);
	free(fmt);

	fmt = xstrfmt("%"PRIuMAX"", (uintmax_t)objs->blobs_nr);
	insert_table_rowv(&table, _("Blobs"), fmt, NULL);
	free(fmt);

	print_table_plaintext(&table);
	clear_table(&table);
}

static void survey_report_object_sizes(const char *title,
				       const char *categories,
				       struct survey_report_object_size_summary *summary,
				       size_t summary_nr)
{
	struct survey_table table = SURVEY_TABLE_INIT;
	table.table_name = title;

	strvec_push(&table.header, categories);
	strvec_push(&table.header, _("Count"));
	strvec_push(&table.header, _("Disk Size"));
	strvec_push(&table.header, _("Inflated Size"));

	for (size_t i = 0; i < summary_nr; i++) {
		char *label_str =  xstrdup(summary[i].label);
		char *nr_str = xstrfmt("%"PRIuMAX, (uintmax_t)summary[i].nr);
		char *disk_str = xstrfmt("%"PRIuMAX, (uintmax_t)summary[i].disk_size);
		char *inflate_str = xstrfmt("%"PRIuMAX, (uintmax_t)summary[i].inflated_size);

		insert_table_rowv(&table, label_str, nr_str,
				  disk_str, inflate_str, NULL);

		free(label_str);
		free(nr_str);
		free(disk_str);
		free(inflate_str);
	}

	print_table_plaintext(&table);
	clear_table(&table);
}

static void survey_report_plaintext_sorted_size(
		struct survey_report_top_table *top)
{
	survey_report_object_sizes(top->name,  _("Path"),
				   top->data, top->nr);
}

static void survey_report_plaintext(struct survey_context *ctx)
{
	printf("GIT SURVEY for \"%s\"\n", ctx->repo->worktree);
	printf("-----------------------------------------------------\n");
	survey_report_plaintext_refs(ctx);
	survey_report_plaintext_reachable_object_summary(ctx);
	survey_report_object_sizes(_("TOTAL OBJECT SIZES BY TYPE"),
				   _("Object Type"),
				   ctx->report.by_type,
				   REPORT_TYPE_COUNT);

	survey_report_commit_parents(ctx);

	survey_report_hbin(_("COMMITS HISTOGRAM BY SIZE IN BYTES"),
			   ctx->report.reachable_objects.commits.base.size_hbin);

	survey_report_tree_lengths(ctx);

	survey_report_hbin(_("TREES HISTOGRAM BY SIZE IN BYTES"),
			   ctx->report.reachable_objects.trees.base.size_hbin);

	survey_report_hbin(_("BLOBS HISTOGRAM BY SIZE IN BYTES"),
			   ctx->report.reachable_objects.blobs.base.size_hbin);

	survey_report_plaintext_sorted_size(
		&ctx->report.top_paths_by_count[REPORT_TYPE_TREE]);
	survey_report_plaintext_sorted_size(
		&ctx->report.top_paths_by_count[REPORT_TYPE_BLOB]);

	survey_report_plaintext_sorted_size(
		&ctx->report.top_paths_by_disk[REPORT_TYPE_TREE]);
	survey_report_plaintext_sorted_size(
		&ctx->report.top_paths_by_disk[REPORT_TYPE_BLOB]);

	survey_report_plaintext_sorted_size(
		&ctx->report.top_paths_by_inflate[REPORT_TYPE_TREE]);
	survey_report_plaintext_sorted_size(
		&ctx->report.top_paths_by_inflate[REPORT_TYPE_BLOB]);

	survey_report_largest_vec(ctx->report.reachable_objects.commits.vec_largest_by_nr_parents);
	survey_report_largest_vec(ctx->report.reachable_objects.commits.vec_largest_by_size_bytes);
	survey_report_largest_vec(ctx->report.reachable_objects.trees.vec_largest_by_nr_entries);
	survey_report_largest_vec(ctx->report.reachable_objects.trees.vec_largest_by_size_bytes);
	survey_report_largest_vec(ctx->report.reachable_objects.blobs.vec_largest_by_size_bytes);
}

/*
 * After parsing the command line arguments, figure out which refs we
 * should scan.
 *
 * If ANY were given in positive sense, then we ONLY include them and
 * do not use the builtin values.
 */
static void fixup_refs_wanted(struct survey_context *ctx)
{
	struct survey_refs_wanted *rw = &ctx->opts.refs;

	/*
	 * `--all-refs` overrides and enables everything.
	 */
	if (rw->want_all_refs == 1) {
		rw->want_branches = 1;
		rw->want_tags = 1;
		rw->want_remotes = 1;
		rw->want_detached = 1;
		rw->want_other = 1;
		return;
	}

	/*
	 * If none of the `--<ref-type>` were given, we assume all
	 * of the builtin unspecified values.
	 */
	if (rw->want_branches == -1 &&
	    rw->want_tags == -1 &&
	    rw->want_remotes == -1 &&
	    rw->want_detached == -1 &&
	    rw->want_other == -1) {
		*rw = default_ref_options;
		return;
	}

	/*
	 * Since we only allow positive boolean values on the command
	 * line, we will only have true values where they specified
	 * a `--<ref-type>`.
	 *
	 * So anything that still has an unspecified value should be
	 * set to false.
	 */
	if (rw->want_branches == -1)
		rw->want_branches = 0;
	if (rw->want_tags == -1)
		rw->want_tags = 0;
	if (rw->want_remotes == -1)
		rw->want_remotes = 0;
	if (rw->want_detached == -1)
		rw->want_detached = 0;
	if (rw->want_other == -1)
		rw->want_other = 0;
}

static int survey_load_config_cb(const char *var, const char *value,
				 const struct config_context *cctx, void *pvoid)
{
	struct survey_context *ctx = pvoid;

	if (!strcmp(var, "survey.verbose")) {
		ctx->opts.verbose = git_config_bool(var, value);
		return 0;
	}
	if (!strcmp(var, "survey.progress")) {
		ctx->opts.show_progress = git_config_bool(var, value);
		return 0;
	}
	if (!strcmp(var, "survey.showcommitparents")) {
		ctx->opts.show_largest_commits_by_nr_parents = git_config_ulong(var, value, cctx->kvi);
		return 0;
	}
	if (!strcmp(var, "survey.showcommitsizes")) {
		ctx->opts.show_largest_commits_by_size_bytes = git_config_ulong(var, value, cctx->kvi);
		return 0;
	}

	if (!strcmp(var, "survey.showtreeentries")) {
		ctx->opts.show_largest_trees_by_nr_entries = git_config_ulong(var, value, cctx->kvi);
		return 0;
	}
	if (!strcmp(var, "survey.showtreesizes")) {
		ctx->opts.show_largest_trees_by_size_bytes = git_config_ulong(var, value, cctx->kvi);
		return 0;
	}
	if (!strcmp(var, "survey.showblobsizes")) {
		ctx->opts.show_largest_blobs_by_size_bytes = git_config_ulong(var, value, cctx->kvi);
		return 0;
	}
	if (!strcmp(var, "survey.top")) {
		ctx->opts.top_nr = git_config_bool(var, value);
		return 0;
	}

	return git_default_config(var, value, cctx, pvoid);
}

static void survey_load_config(struct survey_context *ctx)
{
	repo_config(the_repository, survey_load_config_cb, ctx);
}

static void do_load_refs(struct survey_context *ctx,
			 struct ref_array *ref_array)
{
	struct ref_filter filter = REF_FILTER_INIT;
	struct ref_sorting *sorting;
	struct string_list sorting_options = STRING_LIST_INIT_DUP;

	string_list_append(&sorting_options, "objectname");
	sorting = ref_sorting_options(&sorting_options);

	if (ctx->opts.refs.want_detached)
		strvec_push(&ctx->refs, "HEAD");

	if (ctx->opts.refs.want_all_refs) {
		strvec_push(&ctx->refs, "refs/");
	} else {
		if (ctx->opts.refs.want_branches)
			strvec_push(&ctx->refs, "refs/heads/");
		if (ctx->opts.refs.want_tags)
			strvec_push(&ctx->refs, "refs/tags/");
		if (ctx->opts.refs.want_remotes)
			strvec_push(&ctx->refs, "refs/remotes/");
		if (ctx->opts.refs.want_other) {
			strvec_push(&ctx->refs, "refs/notes/");
			strvec_push(&ctx->refs, "refs/stash/");
		}
	}

	filter.name_patterns = ctx->refs.v;
	filter.ignore_case = 0;
	filter.match_as_path = 1;

	if (ctx->opts.show_progress) {
		ctx->progress_total = 0;
		ctx->progress = start_progress(ctx->repo,
					       _("Scanning refs..."), 0);
	}

	filter_refs(ref_array, &filter, FILTER_REFS_KIND_MASK);

	if (ctx->opts.show_progress) {
		ctx->progress_total = ref_array->nr;
		display_progress(ctx->progress, ctx->progress_total);
	}

	ref_array_sort(sorting, ref_array);

	stop_progress(&ctx->progress);
	ref_filter_clear(&filter);
	ref_sorting_release(sorting);
}

/*
 * The REFS phase:
 *
 * Load the set of requested refs and assess them for scalablity problems.
 * Use that set to start a treewalk to all reachable objects and assess
 * them.
 *
 * This data will give us insights into the repository itself (the number
 * of refs, the size and shape of the DAG, the number and size of the
 * objects).
 *
 * Theoretically, this data is independent of the on-disk representation
 * (e.g. independent of packing concerns).
 */
static void survey_phase_refs(struct survey_context *ctx)
{
	trace2_region_enter("survey", "phase/refs", ctx->repo);
	do_load_refs(ctx, &ctx->ref_array);

	ctx->report.refs.refs_nr = ctx->ref_array.nr;
	for (int i = 0; i < ctx->ref_array.nr; i++) {
		unsigned long size;
		struct ref_array_item *item = ctx->ref_array.items[i];
		size_t len = strlen(item->refname);

		switch (item->kind) {
		case FILTER_REFS_TAGS:
			ctx->report.refs.tags_nr++;
			if (odb_read_object_info(ctx->repo->objects,
						 &item->objectname,
						 &size) == OBJ_TAG)
				ctx->report.refs.tags_annotated_nr++;
			break;

		case FILTER_REFS_BRANCHES:
			ctx->report.refs.branches_nr++;
			break;

		case FILTER_REFS_REMOTES:
			ctx->report.refs.remote_refs_nr++;
			break;

		case FILTER_REFS_OTHERS:
			ctx->report.refs.others_nr++;
			break;

		default:
			ctx->report.refs.unknown_nr++;
			break;
		}

		/*
		 * SymRefs are somewhat orthogonal to the above
		 * classification (e.g. "HEAD" --> detached
		 * and "refs/remotes/origin/HEAD" --> remote) so
		 * our totals will already include them.
		 */
		if (item->flag & REF_ISSYMREF)
			ctx->report.refs.cnt_symref++;

		/*
		 * Where/how is the ref stored in GITDIR.
		 */
		if (item->flag & REF_ISPACKED)
			ctx->report.refs.cnt_packed++;
		else
			ctx->report.refs.cnt_loose++;

		if (item->kind == FILTER_REFS_REMOTES) {
			ctx->report.refs.len_sum_remote_refnames += len;
			if (len > ctx->report.refs.len_max_remote_refname)
				ctx->report.refs.len_max_remote_refname = len;
		} else {
			ctx->report.refs.len_sum_local_refnames += len;
			if (len > ctx->report.refs.len_max_local_refname)
				ctx->report.refs.len_max_local_refname = len;
		}
	}

	trace2_region_leave("survey", "phase/refs", ctx->repo);
}

static void increment_object_counts(
		struct survey_report_object_summary *summary,
		enum object_type type,
		size_t nr)
{
	switch (type) {
	case OBJ_COMMIT:
		summary->commits_nr += nr;
		break;

	case OBJ_TREE:
		summary->trees_nr += nr;
		break;

	case OBJ_BLOB:
		summary->blobs_nr += nr;
		break;

	case OBJ_TAG:
		summary->tags_nr += nr;
		break;

	default:
		break;
	}
}

static void increment_totals(struct survey_context *ctx,
			     struct oid_array *oids,
			     struct survey_report_object_size_summary *summary,
			     const char *path)
{
	for (size_t i = 0; i < oids->nr; i++) {
		struct object_info oi = OBJECT_INFO_INIT;
		unsigned oi_flags = OBJECT_INFO_FOR_PREFETCH;
		unsigned long object_length = 0;
		off_t disk_sizep = 0;
		enum object_type type;
		struct survey_stats_base_object *base;
		int hb;

		oi.typep = &type;
		oi.sizep = &object_length;
		oi.disk_sizep = &disk_sizep;

		if (odb_read_object_info_extended(ctx->repo->objects,
						  &oids->oid[i],
						  &oi, oi_flags) < 0) {
			summary->num_missing++;
			continue;
		}

		summary->nr++;
		summary->disk_size += disk_sizep;
		summary->inflated_size += object_length;

		switch (type) {
		case OBJ_COMMIT: {
			struct commit *commit = lookup_commit(ctx->repo, &oids->oid[i]);
			unsigned k = commit_list_count(commit->parents);

			if (k >= PBIN_VEC_LEN)
				k = PBIN_VEC_LEN - 1;

			ctx->report.reachable_objects.commits.parent_cnt_pbin[k]++;
			base = &ctx->report.reachable_objects.commits.base;

			maybe_insert_large_item(ctx->report.reachable_objects.commits.vec_largest_by_nr_parents, k, &commit->object.oid, NULL, &commit->object.oid);
			maybe_insert_large_item(ctx->report.reachable_objects.commits.vec_largest_by_size_bytes, object_length, &commit->object.oid, NULL, &commit->object.oid);
			break;
		}
		case OBJ_TREE: {
			struct tree *tree = lookup_tree(ctx->repo, &oids->oid[i]);
			if (tree) {
				struct survey_stats_trees *pst = &ctx->report.reachable_objects.trees;
				struct tree_desc desc;
				struct name_entry entry;
				uint64_t nr_entries;
				int qb;

				parse_tree(tree);
				init_tree_desc(&desc, &oids->oid[i], tree->buffer, tree->size);
				nr_entries = 0;
				while (tree_entry(&desc, &entry))
					nr_entries++;

				pst->sum_entries += nr_entries;

				maybe_insert_large_item(pst->vec_largest_by_nr_entries, nr_entries, &tree->object.oid, path, NULL);
				maybe_insert_large_item(pst->vec_largest_by_size_bytes, object_length, &tree->object.oid, path, NULL);

				qb = qbin(nr_entries);
				incr_obj_hist_bin(&pst->entry_qbin[qb], object_length, disk_sizep);
			}
			base = &ctx->report.reachable_objects.trees.base;
			break;
		}
		case OBJ_BLOB:
			base = &ctx->report.reachable_objects.blobs.base;

			maybe_insert_large_item(ctx->report.reachable_objects.blobs.vec_largest_by_size_bytes, object_length, &oids->oid[i], path, NULL);
			break;
		default:
			continue;
		}

		switch (oi.whence) {
		case OI_CACHED:
			base->cnt_cached++;
			break;
		case OI_LOOSE:
			base->cnt_loose++;
			break;
		case OI_PACKED:
			base->cnt_packed++;
			break;
		case OI_DBCACHED:
			base->cnt_dbcached++;
			break;
		default:
			break;
		}

		base->sum_size += object_length;
		base->sum_disk_size += disk_sizep;

		hb = hbin(object_length);
		incr_obj_hist_bin(&base->size_hbin[hb], object_length, disk_sizep);

	}
}

static void increment_object_totals(struct survey_context *ctx,
				    struct oid_array *oids,
				    enum object_type type,
				    const char *path)
{
	struct survey_report_object_size_summary *total;
	struct survey_report_object_size_summary summary = { 0 };

	increment_totals(ctx, oids, &summary, path);

	switch (type) {
	case OBJ_COMMIT:
		total = &ctx->report.by_type[REPORT_TYPE_COMMIT];
		break;

	case OBJ_TREE:
		total = &ctx->report.by_type[REPORT_TYPE_TREE];
		break;

	case OBJ_BLOB:
		total = &ctx->report.by_type[REPORT_TYPE_BLOB];
		break;

	case OBJ_TAG:
		total = &ctx->report.by_type[REPORT_TYPE_TAG];
		break;

	default:
		BUG("No other type allowed");
	}

	total->nr += summary.nr;
	total->disk_size += summary.disk_size;
	total->inflated_size += summary.inflated_size;
	total->num_missing += summary.num_missing;

	if (type == OBJ_TREE || type == OBJ_BLOB) {
		int index = type == OBJ_TREE ?
			    REPORT_TYPE_TREE : REPORT_TYPE_BLOB;
		struct survey_report_top_table *top;

		/*
		 * Temporarily store (const char *) here, but it will
		 * be duped if inserted and will not be freed.
		 */
		summary.label = (char *)path;

		top = ctx->report.top_paths_by_count;
		maybe_insert_into_top_size(&top[index], &summary);

		top = ctx->report.top_paths_by_disk;
		maybe_insert_into_top_size(&top[index], &summary);

		top = ctx->report.top_paths_by_inflate;
		maybe_insert_into_top_size(&top[index], &summary);
	}
}

static int survey_objects_path_walk_fn(const char *path,
				       struct oid_array *oids,
				       enum object_type type,
				       void *data)
{
	struct survey_context *ctx = data;

	increment_object_counts(&ctx->report.reachable_objects,
				type, oids->nr);
	increment_object_totals(ctx, oids, type, path);

	ctx->progress_nr += oids->nr;
	display_progress(ctx->progress, ctx->progress_nr);

	return 0;
}

static void initialize_report(struct survey_context *ctx)
{
	CALLOC_ARRAY(ctx->report.by_type, REPORT_TYPE_COUNT);
	ctx->report.by_type[REPORT_TYPE_COMMIT].label = xstrdup(_("Commits"));
	ctx->report.by_type[REPORT_TYPE_TREE].label = xstrdup(_("Trees"));
	ctx->report.by_type[REPORT_TYPE_BLOB].label = xstrdup(_("Blobs"));
	ctx->report.by_type[REPORT_TYPE_TAG].label = xstrdup(_("Tags"));

	CALLOC_ARRAY(ctx->report.top_paths_by_count, REPORT_TYPE_COUNT);
	init_top_sizes(&ctx->report.top_paths_by_count[REPORT_TYPE_TREE],
		       ctx->opts.top_nr, _("TOP DIRECTORIES BY COUNT"), cmp_by_nr);
	init_top_sizes(&ctx->report.top_paths_by_count[REPORT_TYPE_BLOB],
		       ctx->opts.top_nr, _("TOP FILES BY COUNT"), cmp_by_nr);

	CALLOC_ARRAY(ctx->report.top_paths_by_disk, REPORT_TYPE_COUNT);
	init_top_sizes(&ctx->report.top_paths_by_disk[REPORT_TYPE_TREE],
		       ctx->opts.top_nr, _("TOP DIRECTORIES BY DISK SIZE"), cmp_by_disk_size);
	init_top_sizes(&ctx->report.top_paths_by_disk[REPORT_TYPE_BLOB],
		       ctx->opts.top_nr, _("TOP FILES BY DISK SIZE"), cmp_by_disk_size);

	CALLOC_ARRAY(ctx->report.top_paths_by_inflate, REPORT_TYPE_COUNT);
	init_top_sizes(&ctx->report.top_paths_by_inflate[REPORT_TYPE_TREE],
		       ctx->opts.top_nr, _("TOP DIRECTORIES BY INFLATED SIZE"), cmp_by_inflated_size);
	init_top_sizes(&ctx->report.top_paths_by_inflate[REPORT_TYPE_BLOB],
		       ctx->opts.top_nr, _("TOP FILES BY INFLATED SIZE"), cmp_by_inflated_size);
}

static void survey_phase_objects(struct survey_context *ctx)
{
	struct rev_info revs = REV_INFO_INIT;
	struct path_walk_info info = PATH_WALK_INFO_INIT;
	unsigned int add_flags = 0;

	trace2_region_enter("survey", "phase/objects", ctx->repo);

	info.revs = &revs;
	info.path_fn = survey_objects_path_walk_fn;
	info.path_fn_data = ctx;

	initialize_report(ctx);

	repo_init_revisions(ctx->repo, &revs, "");
	revs.tag_objects = 1;

	ctx->progress_nr = 0;
	ctx->progress_total = ctx->ref_array.nr;
	if (ctx->opts.show_progress)
		ctx->progress = start_progress(ctx->repo,
					       _("Preparing object walk"),
					       ctx->progress_total);
	for (int i = 0; i < ctx->ref_array.nr; i++) {
		struct ref_array_item *item = ctx->ref_array.items[i];
		add_pending_oid(&revs, NULL, &item->objectname, add_flags);
		display_progress(ctx->progress, ++(ctx->progress_nr));
	}
	stop_progress(&ctx->progress);

	ctx->progress_nr = 0;
	ctx->progress_total = 0;
	if (ctx->opts.show_progress)
		ctx->progress = start_progress(ctx->repo,
					       _("Walking objects"), 0);
	walk_objects_by_path(&info);
	stop_progress(&ctx->progress);

	release_revisions(&revs);
	trace2_region_leave("survey", "phase/objects", ctx->repo);
}

int cmd_survey(int argc, const char **argv, const char *prefix, struct repository *repo)
{
	static struct survey_context ctx = {
		.opts = {
			.verbose = 0,
			.show_progress = -1, /* defaults to isatty(2) */
			.top_nr = 10,

			.refs.want_all_refs = -1,

			.refs.want_branches = -1, /* default these to undefined */
			.refs.want_tags = -1,
			.refs.want_remotes = -1,
			.refs.want_detached = -1,
			.refs.want_other = -1,
		},
		.refs = STRVEC_INIT,
	};

	static struct option survey_options[] = {
		OPT__VERBOSE(&ctx.opts.verbose, N_("verbose output")),
		OPT_BOOL(0, "progress", &ctx.opts.show_progress, N_("show progress")),
		OPT_INTEGER('n', "top", &ctx.opts.top_nr,
			    N_("number of entries to include in detail tables")),

		OPT_BOOL_F(0, "all-refs", &ctx.opts.refs.want_all_refs, N_("include all refs"),          PARSE_OPT_NONEG),

		OPT_BOOL_F(0, "branches", &ctx.opts.refs.want_branches, N_("include branches"),          PARSE_OPT_NONEG),
		OPT_BOOL_F(0, "tags",     &ctx.opts.refs.want_tags,     N_("include tags"),              PARSE_OPT_NONEG),
		OPT_BOOL_F(0, "remotes",  &ctx.opts.refs.want_remotes,  N_("include all remotes refs"),  PARSE_OPT_NONEG),
		OPT_BOOL_F(0, "detached", &ctx.opts.refs.want_detached, N_("include detached HEAD"),     PARSE_OPT_NONEG),
		OPT_BOOL_F(0, "other",    &ctx.opts.refs.want_other,    N_("include notes and stashes"), PARSE_OPT_NONEG),

		OPT_INTEGER_F(0, "commit-parents", &ctx.opts.show_largest_commits_by_nr_parents, N_("show N largest commits by parent count"),  PARSE_OPT_NONEG),
		OPT_INTEGER_F(0, "commit-sizes",   &ctx.opts.show_largest_commits_by_size_bytes, N_("show N largest commits by size in bytes"), PARSE_OPT_NONEG),

		OPT_INTEGER_F(0, "tree-entries",   &ctx.opts.show_largest_trees_by_nr_entries,   N_("show N largest trees by entry count"),     PARSE_OPT_NONEG),
		OPT_INTEGER_F(0, "tree-sizes",     &ctx.opts.show_largest_trees_by_size_bytes,   N_("show N largest trees by size in bytes"),   PARSE_OPT_NONEG),

		OPT_INTEGER_F(0, "blob-sizes",     &ctx.opts.show_largest_blobs_by_size_bytes,   N_("show N largest blobs by size in bytes"),   PARSE_OPT_NONEG),

		OPT_END(),
	};

	show_usage_with_options_if_asked(argc, argv,
					 survey_usage, survey_options);

	if (isatty(2))
		color_fprintf_ln(stderr,
				 want_color_fd(2, GIT_COLOR_AUTO) ? GIT_COLOR_YELLOW : "",
				 "(THIS IS EXPERIMENTAL, EXPECT THE OUTPUT FORMAT TO CHANGE!)");

	ctx.repo = repo;

	prepare_repo_settings(ctx.repo);
	survey_load_config(&ctx);

	argc = parse_options(argc, argv, prefix, survey_options, survey_usage, 0);

	if (ctx.opts.show_progress < 0)
		ctx.opts.show_progress = isatty(2);

	fixup_refs_wanted(&ctx);

	if (ctx.opts.show_largest_commits_by_nr_parents)
		ctx.report.reachable_objects.commits.vec_largest_by_nr_parents =
			alloc_large_item_vec(
				"largest_commits_by_nr_parents",
				"nr_parents",
				ctx.opts.show_largest_commits_by_nr_parents);
	if (ctx.opts.show_largest_commits_by_size_bytes)
		ctx.report.reachable_objects.commits.vec_largest_by_size_bytes =
			alloc_large_item_vec(
				"largest_commits_by_size_bytes",
				"size",
				ctx.opts.show_largest_commits_by_size_bytes);

	if (ctx.opts.show_largest_trees_by_nr_entries)
		ctx.report.reachable_objects.trees.vec_largest_by_nr_entries =
			alloc_large_item_vec(
				"largest_trees_by_nr_entries",
				"nr_entries",
				ctx.opts.show_largest_trees_by_nr_entries);
	if (ctx.opts.show_largest_trees_by_size_bytes)
		ctx.report.reachable_objects.trees.vec_largest_by_size_bytes =
			alloc_large_item_vec(
				"largest_trees_by_size_bytes",
				"size",
				ctx.opts.show_largest_trees_by_size_bytes);

	if (ctx.opts.show_largest_blobs_by_size_bytes)
		ctx.report.reachable_objects.blobs.vec_largest_by_size_bytes =
			alloc_large_item_vec(
				"largest_blobs_by_size_bytes",
				"size",
				ctx.opts.show_largest_blobs_by_size_bytes);

	survey_phase_refs(&ctx);

	survey_phase_objects(&ctx);

	survey_report_plaintext(&ctx);

	clear_survey_context(&ctx);
	return 0;
}
