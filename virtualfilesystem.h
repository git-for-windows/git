#ifndef VIRTUALFILESYSTEM_H
#define VIRTUALFILESYSTEM_H

/*
 * Update the CE_SKIP_WORKTREE bits based on the virtual file system.
 */
void apply_virtualfilesystem(struct index_state *istate);

/*
 * Return 1 if the requested item is found in the virtual file system,
 * 0 for not found and -1 for undecided.
 */
int is_included_in_virtualfilesystem(const char *pathname, int pathlen);

/*
 * Return 1 for exclude, 0 for include and -1 for undecided.
 */
int is_excluded_from_virtualfilesystem(const char *pathname, int pathlen, int dtype);

/*
 * Free the virtual file system data structures.
 */
void free_virtualfilesystem(void);

#endif
