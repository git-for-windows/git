#include "unit-test.h"
#include "path-trie.h"

struct test_entry {
	struct path_trie_entry ent;
	const char *tag;
};

static struct test_entry *entry(const char *tag)
{
	struct test_entry *e = xcalloc(1, sizeof(*e));

	e->tag = tag;
	return e;
}

static size_t drain_count(struct path_trie *trie, const char *path)
{
	struct path_trie_entry *list = path_trie_drain(trie, path);
	size_t n = 0;

	while (list) {
		struct path_trie_entry *next = list->next;

		path_trie_entry_clear(list);
		free(container_of(list, struct test_entry, ent));
		list = next;
		n++;
	}
	return n;
}

static int drained_tags_contain(struct path_trie_entry *list, const char *tag)
{
	for (; list; list = list->next) {
		struct test_entry *e =
			container_of(list, struct test_entry, ent);

		if (!strcmp(e->tag, tag))
			return 1;
	}
	return 0;
}

static void free_drained(struct path_trie_entry *list)
{
	while (list) {
		struct path_trie_entry *next = list->next;

		path_trie_entry_clear(list);
		free(container_of(list, struct test_entry, ent));
		list = next;
	}
}

void test_path_trie__drain_empty(void)
{
	struct path_trie trie;

	path_trie_init(&trie, 0);
	cl_assert_equal_p(path_trie_drain(&trie, "a/b"), NULL);
	path_trie_clear(&trie);
}

void test_path_trie__drain_exact_path(void)
{
	struct path_trie trie;

	path_trie_init(&trie, 0);
	path_trie_add(&trie, "a/b/c", &entry("x")->ent);
	cl_assert_equal_i(drain_count(&trie, "a/b/c"), 1);
	cl_assert_equal_i(drain_count(&trie, "a/b/c"), 0);
	path_trie_clear(&trie);
}

void test_path_trie__drain_covers_subtree(void)
{
	struct path_trie trie;

	path_trie_init(&trie, 0);
	path_trie_add(&trie, "a/b", &entry("shallow")->ent);
	path_trie_add(&trie, "a/b/c/d", &entry("deep")->ent);
	path_trie_add(&trie, "a/other", &entry("sibling")->ent);
	cl_assert_equal_i(drain_count(&trie, "a/b"), 2);
	cl_assert_equal_i(drain_count(&trie, "a"), 1);
	path_trie_clear(&trie);
}

void test_path_trie__drain_does_not_cover_ancestors(void)
{
	struct path_trie trie;

	path_trie_init(&trie, 0);
	path_trie_add(&trie, "a", &entry("above")->ent);
	cl_assert_equal_i(drain_count(&trie, "a/b"), 0);
	cl_assert_equal_i(drain_count(&trie, "a"), 1);
	path_trie_clear(&trie);
}

void test_path_trie__multiple_entries_per_path(void)
{
	struct path_trie trie;

	path_trie_init(&trie, 0);
	path_trie_add(&trie, "a/b", &entry("one")->ent);
	path_trie_add(&trie, "a/b", &entry("two")->ent);
	path_trie_add(&trie, "a/b", &entry("three")->ent);
	cl_assert_equal_i(drain_count(&trie, "a/b"), 3);
	path_trie_clear(&trie);
}

void test_path_trie__separators_are_normalized(void)
{
	struct path_trie trie;

	path_trie_init(&trie, 0);
	path_trie_add(&trie, "a/b/c", &entry("slash")->ent);
	path_trie_add(&trie, "a//b///c", &entry("doubled")->ent);
	cl_assert_equal_i(drain_count(&trie, "/a/b/c/"), 2);
	path_trie_clear(&trie);
}

void test_path_trie__case_sensitivity(void)
{
	struct path_trie trie;

	path_trie_init(&trie, 0);
	path_trie_add(&trie, "a/b", &entry("lower")->ent);
	cl_assert_equal_i(drain_count(&trie, "A/B"), 0);
	cl_assert_equal_i(drain_count(&trie, "a/b"), 1);
	path_trie_clear(&trie);

	path_trie_init(&trie, 1);
	path_trie_add(&trie, "a/b", &entry("lower")->ent);
	cl_assert_equal_i(drain_count(&trie, "A/B"), 1);
	path_trie_clear(&trie);
}

void test_path_trie__move_relocates_subtree(void)
{
	struct path_trie trie;
	struct path_trie_entry *drained;

	path_trie_init(&trie, 0);
	path_trie_add(&trie, "link/sub", &entry("via-link")->ent);
	path_trie_add(&trie, "link/sub/deeper", &entry("nested")->ent);

	path_trie_move(&trie, "link", "real");

	cl_assert_equal_i(drain_count(&trie, "link"), 0);
	drained = path_trie_drain(&trie, "real/sub");
	cl_assert(drained != NULL);
	cl_assert(drained_tags_contain(drained, "via-link"));
	cl_assert(drained_tags_contain(drained, "nested"));
	free_drained(drained);
	path_trie_clear(&trie);
}

void test_path_trie__move_merges_with_existing(void)
{
	struct path_trie trie;

	path_trie_init(&trie, 0);
	path_trie_add(&trie, "from/x", &entry("moved")->ent);
	path_trie_add(&trie, "to/x", &entry("already-there")->ent);

	path_trie_move(&trie, "from", "to");

	cl_assert_equal_i(drain_count(&trie, "to/x"), 2);
	path_trie_clear(&trie);
}

void test_path_trie__move_missing_source_is_noop(void)
{
	struct path_trie trie;

	path_trie_init(&trie, 0);
	path_trie_add(&trie, "to/x", &entry("existing")->ent);
	path_trie_move(&trie, "does/not/exist", "to");
	cl_assert_equal_i(drain_count(&trie, "to"), 1);
	path_trie_clear(&trie);
}

void test_path_trie__move_into_itself_is_noop(void)
{
	struct path_trie trie;

	path_trie_init(&trie, 0);
	path_trie_add(&trie, "a/b", &entry("kept")->ent);
	path_trie_move(&trie, "a", "a/b/c");
	path_trie_move(&trie, "a", "a");
	cl_assert_equal_i(drain_count(&trie, "a/b"), 1);
	path_trie_clear(&trie);
}

void test_path_trie__move_to_ancestor(void)
{
	struct path_trie trie;

	path_trie_init(&trie, 0);
	path_trie_add(&trie, "a/b/c", &entry("moves-up")->ent);
	path_trie_move(&trie, "a/b", "a");
	cl_assert_equal_i(drain_count(&trie, "a/c"), 1);
	path_trie_clear(&trie);
}

void test_path_trie__move_rewrites_keys(void)
{
	struct path_trie trie;
	struct path_trie_entry *drained;

	path_trie_init(&trie, 0);
	path_trie_add(&trie, "link/sub", &entry("aliased")->ent);
	path_trie_move(&trie, "link", "real");

	drained = path_trie_drain(&trie, "real");
	cl_assert(drained != NULL);
	cl_assert_equal_s(drained->key, "real/sub");

	/* re-registering by the entry's own key must be safe */
	path_trie_add(&trie, drained->key, drained);
	cl_assert_equal_i(drain_count(&trie, "real/sub"), 1);
	path_trie_clear(&trie);
}
