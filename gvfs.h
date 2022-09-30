#ifndef GVFS_H
#define GVFS_H

struct repository;

/*
 * This file is for the specific settings and methods
 * used for GVFS functionality
 */

/*
 * The list of bits in the core_gvfs setting
 */
#define GVFS_SKIP_SHA_ON_INDEX                      (1 << 0)
#define GVFS_MISSING_OK                             (1 << 2)

/*
 * This behavior of not deleting outside of the sparse-checkout
 * is specific to the virtual filesystem support. It is only
 * enabled by VFS for Git, and so can be used as an indicator
 * that we are in a virtualized filesystem environment and not
 * in a Scalar environment. This bit has two names to reflect
 * that.
 */
#define GVFS_NO_DELETE_OUTSIDE_SPARSECHECKOUT       (1 << 3)
#define GVFS_USE_VIRTUAL_FILESYSTEM                 (1 << 3)

#define GVFS_FETCH_SKIP_REACHABILITY_AND_UPLOADPACK (1 << 4)
#define GVFS_BLOCK_FILTERS_AND_EOL_CONVERSIONS      (1 << 6)

int gvfs_config_is_set(struct repository *r, int mask);
int gvfs_virtualize_objects(struct repository *r);

#endif /* GVFS_H */
