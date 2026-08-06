#include "git-compat-util.h"
#include "path-trie.h"
#include "strbuf.h"

struct path_trie_node {
	struct hashmap_entry ent; /* in the parent node's `children` */
	struct hashmap children;
	struct path_trie_entry *entries;
	char component[FLEX_ARRAY];
};

static int node_cmp(const void *cmp_data,
		    const struct hashmap_entry *eptr,
		    const struct hashmap_entry *entry_or_key,
		    const void *keydata)
{
	const unsigned int icase = *(const unsigned int *)cmp_data;
	const struct path_trie_node *e =
		container_of(eptr, const struct path_trie_node, ent);
	const char *key = keydata ? keydata :
		container_of(entry_or_key, const struct path_trie_node,
			     ent)->component;

	return icase ? strcasecmp(e->component, key) :
		strcmp(e->component, key);
}

static unsigned int component_hash(const struct path_trie *trie,
				   const char *component)
{
	return trie->icase ? strihash(component) : strhash(component);
}

static struct path_trie_node *make_node(struct path_trie *trie,
					const char *component)
{
	struct path_trie_node *node;

	FLEX_ALLOC_STR(node, component, component);
	hashmap_init(&node->children, node_cmp, &trie->icase, 0);
	hashmap_entry_init(&node->ent, component_hash(trie, component));
	return node;
}

static struct path_trie_node *get_child(struct path_trie *trie,
					struct path_trie_node *node,
					const char *component, int create)
{
	unsigned int hash = component_hash(trie, component);
	struct path_trie_node *child = hashmap_get_entry_from_hash(
		&node->children, hash, component,
		struct path_trie_node, ent);

	if (!child && create) {
		child = make_node(trie, component);
		hashmap_add(&node->children, &child->ent);
	}
	return child;
}

/*
 * Walks the trie to the node for `path`, optionally creating missing
 * nodes on the way. Returns NULL if the node does not exist (and
 * `create` is not set), or if `path` contains no components at all.
 */
static struct path_trie_node *walk(struct path_trie *trie, const char *path,
				   int create)
{
	struct path_trie_node *node = trie->root;
	struct strbuf component = STRBUF_INIT;
	const char *p = path;

	while (*p && node) {
		const char *start;

		while (is_dir_sep(*p))
			p++;
		if (!*p)
			break;
		start = p;
		while (*p && !is_dir_sep(*p))
			p++;

		strbuf_reset(&component);
		strbuf_add(&component, start, p - start);
		node = get_child(trie, node, component.buf, create);
	}

	strbuf_release(&component);
	return node == trie->root ? NULL : node;
}

void path_trie_init(struct path_trie *trie, int icase)
{
	trie->icase = !!icase;
	trie->root = make_node(trie, "");
}

static void free_node(struct path_trie_node *node)
{
	struct hashmap_iter iter;
	struct path_trie_node *child;

	hashmap_for_each_entry(&node->children, &iter, child, ent)
		free_node(child);
	hashmap_clear(&node->children);
	free(node);
}

void path_trie_clear(struct path_trie *trie)
{
	if (trie->root) {
		free_node(trie->root);
		trie->root = NULL;
	}
}

void path_trie_add(struct path_trie *trie, const char *path,
		   struct path_trie_entry *entry)
{
	struct path_trie_node *node = walk(trie, path, 1);
	char *key = xstrdup(path); /* `path` may be `entry->key` itself */

	if (!node)
		BUG("cannot add to path trie under '%s'", path);
	free(entry->key);
	entry->key = key;
	entry->next = node->entries;
	node->entries = entry;
}

/* Unlinks and returns all entries at `node` and below, prepended to `list`. */
static struct path_trie_entry *drain_node(struct path_trie_node *node,
					  struct path_trie_entry *list)
{
	struct hashmap_iter iter;
	struct path_trie_node *child;

	while (node->entries) {
		struct path_trie_entry *e = node->entries;

		node->entries = e->next;
		e->next = list;
		list = e;
	}

	hashmap_for_each_entry(&node->children, &iter, child, ent)
		list = drain_node(child, list);
	return list;
}

struct path_trie_entry *path_trie_drain(struct path_trie *trie,
					const char *path)
{
	struct path_trie_node *node = walk(trie, path, 0);

	return node ? drain_node(node, NULL) : NULL;
}

/*
 * Moves entries at `from` and below to the same position under `to`;
 * `to_path` holds `to`'s path and is extended and restored around
 * each recursion step, so moved entries' keys can be rewritten.
 */
static void move_node(struct path_trie *trie, struct path_trie_node *to,
		      struct path_trie_node *from, struct strbuf *to_path)
{
	struct hashmap_iter iter;
	struct path_trie_node *from_child;

	while (from->entries) {
		struct path_trie_entry *e = from->entries;

		from->entries = e->next;
		e->next = to->entries;
		to->entries = e;
		free(e->key);
		e->key = xstrdup(to_path->buf);
	}

	hashmap_for_each_entry(&from->children, &iter, from_child, ent) {
		struct path_trie_node *to_child =
			get_child(trie, to, from_child->component, 1);
		size_t len = to_path->len;

		strbuf_addch(to_path, '/');
		strbuf_addstr(to_path, from_child->component);
		move_node(trie, to_child, from_child, to_path);
		strbuf_setlen(to_path, len);
	}
}

static size_t component_len(const char *path)
{
	size_t len = 0;

	while (path[len] && !is_dir_sep(path[len]))
		len++;
	return len;
}

/* Is `path` equal to, or nested somewhere below, `prefix`? */
static int path_is_at_or_below(const struct path_trie *trie,
			       const char *path, const char *prefix)
{
	while (1) {
		size_t p_len, x_len;

		while (is_dir_sep(*prefix))
			prefix++;
		while (is_dir_sep(*path))
			path++;
		if (!*prefix)
			return 1;
		if (!*path)
			return 0;

		p_len = component_len(prefix);
		x_len = component_len(path);
		if (p_len != x_len)
			return 0;
		if (trie->icase ? strncasecmp(path, prefix, p_len) :
		    strncmp(path, prefix, p_len))
			return 0;
		prefix += p_len;
		path += x_len;
	}
}

void path_trie_move(struct path_trie *trie, const char *from,
		    const char *to)
{
	struct path_trie_node *from_node;
	struct path_trie_node *to_node;
	struct strbuf to_path = STRBUF_INIT;

	/*
	 * Moving a subtree into itself (or below itself) cannot
	 * terminate meaningfully; treat it as a no-op.
	 */
	if (path_is_at_or_below(trie, to, from))
		return;

	from_node = walk(trie, from, 0);
	if (!from_node)
		return;
	to_node = walk(trie, to, 1);
	if (!to_node)
		return;
	strbuf_addstr(&to_path, to);
	move_node(trie, to_node, from_node, &to_path);
	strbuf_release(&to_path);
}
