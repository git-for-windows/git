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

int gvfs_config_is_set(struct repository *r, int mask);

#endif /* GVFS_H */
