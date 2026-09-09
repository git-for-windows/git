#ifndef PATH_TRIE_H
#define PATH_TRIE_H

#include "hashmap.h"

/*
 * A trie over the components of file system paths, mapping each path
 * to a list of caller-provided entries.
 *
 * Entries are intrusive, like `struct hashmap_entry`: embed a
 * `struct path_trie_entry` in your own struct and use `container_of`
 * to get back to it. An entry belongs to at most one trie at a time.
 *
 * Paths are split on directory separators (both '/' and '\\');
 * repeated separators are ignored. Components are compared
 * case-insensitively if `icase` is set at init time. Callers are
 * expected to pass paths in a consistent (e.g. canonicalized,
 * absolute) form; the trie does not resolve '.' or '..'.
 *
 * Unlike a flat hashmap keyed by whole paths, a trie can answer
 * prefix questions: all entries registered at or below a path can be
 * removed in one operation (path_trie_drain()) or moved onto another
 * path (path_trie_move()). Both are useful when a path turns out to
 * be an alias for another (e.g. a symbolic link), and everything
 * registered through the alias must be found via the real path from
 * then on.
 */

struct path_trie_entry {
	struct path_trie_entry *next;
	/*
	 * The path this entry is currently registered under, owned by
	 * the trie: set by path_trie_add(), rewritten by
	 * path_trie_move(). After draining, the caller may pass the
	 * entry back to path_trie_add() (e.g. with `entry->key`
	 * itself) to re-register it, or free the key -- e.g. via
	 * path_trie_entry_clear() -- once done with the entry.
	 */
	char *key;
};

static inline void path_trie_entry_clear(struct path_trie_entry *entry)
{
	FREE_AND_NULL(entry->key);
}

struct path_trie_node;

struct path_trie {
	struct path_trie_node *root;
	unsigned int icase;
};

void path_trie_init(struct path_trie *trie, int icase);

/*
 * Removes all nodes from the trie. Entries themselves are not freed
 * (the trie does not own them); drain first if they need releasing.
 */
void path_trie_clear(struct path_trie *trie);

/*
 * Adds an entry at `path`, recording the path in `entry->key`
 * (replacing -- and releasing -- any previous key, so passing
 * `entry->key` itself as `path` re-registers a drained entry).
 */
void path_trie_add(struct path_trie *trie, const char *path,
		   struct path_trie_entry *entry);

/*
 * Removes and returns all entries registered at `path` or nested
 * anywhere below it, linked through their `next` fields (in no
 * particular order). Returns NULL if there are none.
 */
struct path_trie_entry *path_trie_drain(struct path_trie *trie,
					const char *path);

/*
 * Moves every entry registered at or below `from` to the
 * corresponding path with the `from` prefix replaced by `to`, e.g.
 * moving "a/b" to "x" moves entries at "a/b/c" to "x/c", updating
 * each moved entry's `key` accordingly. Entries already present
 * under `to` are kept. Moving a path into itself or below itself is
 * a no-op.
 */
void path_trie_move(struct path_trie *trie, const char *from,
		    const char *to);

#endif /* PATH_TRIE_H */
