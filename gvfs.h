#ifndef GVFS_H
#define GVFS_H

struct repository;

/*
 * This file is for the specific settings and methods
 * used for GVFS functionality
 */

int gvfs_config_is_set(struct repository *r, int mask);

#endif /* GVFS_H */
