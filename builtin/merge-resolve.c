/*
 * Builtin "git merge-resolve"
 *
 * Copyright (c) 2020 Alban Gruin
 *
 * Based on git-merge-resolve.sh, written by Linus Torvalds and Junio C
 * Hamano.
 *
 * Resolve two trees, using enhanced multi-base read-tree.
 */

#define USE_THE_INDEX_COMPATIBILITY_MACROS
#include "cache.h"
#include "builtin.h"
#include "merge-strategies.h"

static const char builtin_merge_resolve_usage[] =
	"git merge-resolve <bases>... -- <head> <remote>";

int cmd_merge_resolve(int argc, const char **argv, const char *prefix)
{
	int i, sep_seen = 0;
	const char *head = NULL;
	struct commit_list *bases = NULL, *remote = NULL;
	struct commit_list **next_base = &bases;

	if (argc < 5)
		usage(builtin_merge_resolve_usage);

	setup_work_tree();
	if (read_cache() < 0)
		die("invalid index");

	/*
	 * The first parameters up to -- are merge bases; the rest are
	 * heads.
	 */
	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--"))
			sep_seen = 1;
		else if (!strcmp(argv[i], "-h"))
			usage(builtin_merge_resolve_usage);
		else if (sep_seen && !head)
			head = argv[i];
		else {
			struct object_id oid;
			struct commit *commit;

			if (get_oid(argv[i], &oid))
				die("object %s not found.", argv[i]);

			commit = lookup_commit_or_die(&oid, argv[i]);

			if (sep_seen)
				commit_list_insert(commit, &remote);
			else
				next_base = commit_list_append(commit, next_base);
		}
	}

	/*
	 * Give up if we are given two or more remotes.  Not handling
	 * octopus.
	 */
	if (remote && remote->next)
		return 2;

	/* Give up if this is a baseless merge. */
	if (!bases)
		return 2;

	return merge_strategies_resolve(the_repository, bases, head, remote);
}
