#define USE_THE_INDEX_COMPATIBILITY_MACROS
#include "builtin.h"
#include "lockfile.h"
#include "merge-strategies.h"

int cmd_merge_index(int argc, const char **argv, const char *prefix)
{
	int i, force_file = 0, err = 0, one_shot = 0, quiet = 0;
	const char *pgm;
	void *data = NULL;
	merge_fn merge_action;
	struct lock_file lock = LOCK_INIT;

	/* Without this we cannot rely on waitpid() to tell
	 * what happened to our children.
	 */
	signal(SIGCHLD, SIG_DFL);

	if (argc < 3)
		usage("git merge-index [-o] [-q] <merge-program> (-a | [--] [<filename>...])");

	read_cache();

	i = 1;
	if (!strcmp(argv[i], "-o")) {
		one_shot = 1;
		i++;
	}
	if (!strcmp(argv[i], "-q")) {
		quiet = 1;
		i++;
	}

	pgm = argv[i++];
	setup_work_tree();

	if (!strcmp(pgm, "git-merge-one-file")) {
		merge_action = merge_one_file_func;
		hold_locked_index(&lock, LOCK_DIE_ON_ERROR);
	} else {
		merge_action = merge_one_file_spawn;
		data = (void *)pgm;
	}

	for (; i < argc; i++) {
		const char *arg = argv[i];
		if (!force_file && *arg == '-') {
			if (!strcmp(arg, "--")) {
				force_file = 1;
				continue;
			}
			if (!strcmp(arg, "-a")) {
				err |= merge_all_index(the_repository, one_shot, quiet,
						       merge_action, data);
				continue;
			}
			die("git merge-index: unknown option %s", arg);
		}
		err |= merge_index_path(the_repository, one_shot, quiet, arg,
					merge_action, data);
	}

	if (merge_action == merge_one_file_func) {
		if (err) {
			rollback_lock_file(&lock);
			return err;
		}

		return write_locked_index(&the_index, &lock, COMMIT_LOCK);
	}
	return err;
}
