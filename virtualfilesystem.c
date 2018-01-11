#define USE_THE_REPOSITORY_VARIABLE

#include "git-compat-util.h"
#include "environment.h"
#include "gettext.h"
#include "config.h"
#include "dir.h"
#include "hashmap.h"
#include "run-command.h"
#include "name-hash.h"
#include "read-cache-ll.h"
#include "virtualfilesystem.h"

#define HOOK_INTERFACE_VERSION	(1)

static struct strbuf virtual_filesystem_data = STRBUF_INIT;
static struct hashmap virtual_filesystem_hashmap;
static struct hashmap parent_directory_hashmap;

struct virtualfilesystem {
	struct hashmap_entry ent; /* must be the first member! */
	const char *pattern;
	int patternlen;
};

static unsigned int(*vfshash)(const void *buf, size_t len);
static int(*vfscmp)(const char *a, const char *b, size_t len);

static int vfs_hashmap_cmp(const void *cmp_data UNUSED,
			   const struct hashmap_entry *he1,
			   const struct hashmap_entry *he2,
			   const void *key UNUSED)
{
	const struct virtualfilesystem *vfs1 =
		container_of(he1, const struct virtualfilesystem, ent);
	const struct virtualfilesystem *vfs2 =
		container_of(he2, const struct virtualfilesystem, ent);

	return vfscmp(vfs1->pattern, vfs2->pattern, vfs1->patternlen);
}

static void get_virtual_filesystem_data(struct repository *r, struct strbuf *vfs_data)
{
	struct child_process cp = CHILD_PROCESS_INIT;
	int err;

	strbuf_init(vfs_data, 0);

	strvec_push(&cp.args, core_virtualfilesystem);
	strvec_pushf(&cp.args, "%d", HOOK_INTERFACE_VERSION);
	cp.use_shell = 1;
	cp.dir = repo_get_work_tree(r);

	err = capture_command(&cp, vfs_data, 1024);
	if (err)
		die("unable to load virtual file system");
}

static int check_includes_hashmap(struct hashmap *map, const char *pattern, int patternlen)
{
	struct strbuf sb = STRBUF_INIT;
	struct virtualfilesystem vfs;
	char *slash;

	/* Check straight mapping */
	strbuf_reset(&sb);
	strbuf_add(&sb, pattern, patternlen);
	vfs.pattern = sb.buf;
	vfs.patternlen = sb.len;
	hashmap_entry_init(&vfs.ent, vfshash(vfs.pattern, vfs.patternlen));
	if (hashmap_get_entry(map, &vfs, ent, NULL)) {
		strbuf_release(&sb);
		return 1;
	}

	/*
	 * Check to see if it matches a directory or any path
	 * underneath it.  In other words, 'a/b/foo.txt' will match
	 * '/', 'a/', and 'a/b/'.
	 */
	slash = strchr(sb.buf, '/');
	while (slash) {
		vfs.pattern = sb.buf;
		vfs.patternlen = slash - sb.buf + 1;
		hashmap_entry_init(&vfs.ent, vfshash(vfs.pattern, vfs.patternlen));
		if (hashmap_get_entry(map, &vfs, ent, NULL)) {
			strbuf_release(&sb);
			return 1;
		}
		slash = strchr(slash + 1, '/');
	}

	strbuf_release(&sb);
	return 0;
}

static void includes_hashmap_add(struct hashmap *map, const char *pattern, const int patternlen)
{
	struct virtualfilesystem *vfs;

	vfs = xmalloc(sizeof(struct virtualfilesystem));
	vfs->pattern = pattern;
	vfs->patternlen = patternlen;
	hashmap_entry_init(&vfs->ent, vfshash(vfs->pattern, vfs->patternlen));
	hashmap_add(map, &vfs->ent);
}

static void initialize_includes_hashmap(struct hashmap *map, struct strbuf *vfs_data)
{
	char *buf, *entry;
	size_t len, i;

	/*
	 * Build a hashmap of the virtual file system data we can use to look
	 * for cache entry matches quickly
	 */
	vfshash = ignore_case ? memihash : memhash;
	vfscmp = ignore_case ? strncasecmp : strncmp;
	hashmap_init(map, vfs_hashmap_cmp, NULL, 0);

	entry = buf = vfs_data->buf;
	len = vfs_data->len;
	for (i = 0; i < len; i++) {
		if (buf[i] == '\0') {
			includes_hashmap_add(map, entry, buf + i - entry);
			entry = buf + i + 1;
		}
	}
}

/*
 * Return 1 if the requested item is found in the virtual file system,
 * 0 for not found and -1 for undecided.
 */
int is_included_in_virtualfilesystem(const char *pathname, int pathlen)
{
	if (!core_virtualfilesystem)
		return -1;

	if (!virtual_filesystem_hashmap.tablesize && virtual_filesystem_data.len)
		initialize_includes_hashmap(&virtual_filesystem_hashmap, &virtual_filesystem_data);
	if (!virtual_filesystem_hashmap.tablesize)
		return -1;

	return check_includes_hashmap(&virtual_filesystem_hashmap, pathname, pathlen);
}

static void parent_directory_hashmap_add(struct hashmap *map, const char *pattern, const int patternlen)
{
	char *slash;
	struct virtualfilesystem *vfs;

	/*
	 * Add any directories leading up to the file as the excludes logic
	 * needs to match directories leading up to the files as well. Detect
	 * and prevent unnecessary duplicate entries which will be common.
	 */
	if (patternlen > 1) {
		slash = strchr(pattern + 1, '/');
		while (slash) {
			vfs = xmalloc(sizeof(struct virtualfilesystem));
			vfs->pattern = pattern;
			vfs->patternlen = slash - pattern + 1;
			hashmap_entry_init(&vfs->ent, vfshash(vfs->pattern, vfs->patternlen));
			if (hashmap_get_entry(map, vfs, ent, NULL))
				free(vfs);
			else
				hashmap_add(map, &vfs->ent);
			slash = strchr(slash + 1, '/');
		}
	}
}

static void initialize_parent_directory_hashmap(struct hashmap *map, struct strbuf *vfs_data)
{
	char *buf, *entry;
	size_t len, i;

	/*
	 * Build a hashmap of the parent directories contained in the virtual
	 * file system data we can use to look for matches quickly
	 */
	vfshash = ignore_case ? memihash : memhash;
	vfscmp = ignore_case ? strncasecmp : strncmp;
	hashmap_init(map, vfs_hashmap_cmp, NULL, 0);

	entry = buf = vfs_data->buf;
	len = vfs_data->len;
	for (i = 0; i < len; i++) {
		if (buf[i] == '\0') {
			parent_directory_hashmap_add(map, entry, buf + i - entry);
			entry = buf + i + 1;
		}
	}
}

static int check_directory_hashmap(struct hashmap *map, const char *pathname, int pathlen)
{
	struct strbuf sb = STRBUF_INIT;
	struct virtualfilesystem vfs;

	/* Check for directory */
	strbuf_reset(&sb);
	strbuf_add(&sb, pathname, pathlen);
	strbuf_addch(&sb, '/');
	vfs.pattern = sb.buf;
	vfs.patternlen = sb.len;
	hashmap_entry_init(&vfs.ent, vfshash(vfs.pattern, vfs.patternlen));
	if (hashmap_get_entry(map, &vfs, ent, NULL)) {
		strbuf_release(&sb);
		return 0;
	}

	strbuf_release(&sb);
	return 1;
}

/*
 * Return 1 for exclude, 0 for include and -1 for undecided.
 */
int is_excluded_from_virtualfilesystem(const char *pathname, int pathlen, int dtype)
{
	if (!core_virtualfilesystem)
		return -1;

	if (dtype != DT_REG && dtype != DT_DIR && dtype != DT_LNK)
		die(_("is_excluded_from_virtualfilesystem passed unhandled dtype"));

	if (dtype == DT_REG || dtype == DT_LNK) {
		int ret = is_included_in_virtualfilesystem(pathname, pathlen);
		if (ret > 0)
			return 0;
		if (ret == 0)
			return 1;
		return ret;
	}

	if (dtype == DT_DIR) {
		if (!parent_directory_hashmap.tablesize && virtual_filesystem_data.len)
			initialize_parent_directory_hashmap(&parent_directory_hashmap, &virtual_filesystem_data);
		if (!parent_directory_hashmap.tablesize)
			return -1;

		return check_directory_hashmap(&parent_directory_hashmap, pathname, pathlen);
	}

	return -1;
}

/*
 * Update the CE_SKIP_WORKTREE bits based on the virtual file system.
 */
void apply_virtualfilesystem(struct index_state *istate)
{
	char *buf, *entry;
	size_t i;

	if (!repo_config_get_virtualfilesystem(istate->repo))
		return;

	if (!virtual_filesystem_data.len)
		get_virtual_filesystem_data(istate->repo, &virtual_filesystem_data);

	/* set CE_SKIP_WORKTREE bit on all entries */
	for (i = 0; i < istate->cache_nr; i++)
		istate->cache[i]->ce_flags |= CE_SKIP_WORKTREE;

	/* clear CE_SKIP_WORKTREE bit for everything in the virtual file system */
	entry = buf = virtual_filesystem_data.buf;
	for (i = 0; i < virtual_filesystem_data.len; i++) {
		if (buf[i] == '\0') {
			ssize_t pos, len;

			len = buf + i - entry;

			/* look for a directory wild card (ie "dir1/") */
			if (buf[i - 1] == '/') {
				if (ignore_case)
					adjust_dirname_case(istate, entry);
				pos = index_name_pos(istate, entry, len);
				if (pos < 0) {
					pos = -pos - 1;
					while ((size_t)pos < istate->cache_nr && !fspathncmp(istate->cache[pos]->name, entry, len)) {
						istate->cache[pos]->ce_flags &= ~CE_SKIP_WORKTREE;
						pos++;
					}
				}
			} else {
				if (ignore_case) {
					struct cache_entry *ce = index_file_exists(istate, entry, len, ignore_case);
					if (ce)
						ce->ce_flags &= ~CE_SKIP_WORKTREE;
				} else {
					int pos = index_name_pos(istate, entry, len);
					if (pos >= 0)
						istate->cache[pos]->ce_flags &= ~CE_SKIP_WORKTREE;
				}
			}

			entry += len + 1;
		}
	}
}

/*
 * Free the virtual file system data structures.
 */
void free_virtualfilesystem(void) {
	hashmap_clear_and_free(&virtual_filesystem_hashmap, struct virtualfilesystem, ent);
	hashmap_clear_and_free(&parent_directory_hashmap, struct virtualfilesystem, ent);
	strbuf_release(&virtual_filesystem_data);
}
