/*
Copyright 2020 Google LLC

Use of this source code is governed by a BSD-style
license that can be found in the LICENSE file or at
https://developers.google.com/open-source/licenses/bsd
*/

#ifndef REFTABLE_GENERIC_H
#define REFTABLE_GENERIC_H

#include "reftable-iterator.h"
#include "reftable-reader.h"
#include "reftable-merged.h"

/*
 * Provides a unified API for reading tables, either merged tables, or single
 * readers. */
struct reftable_table {
	struct reftable_table_vtable *ops;
	void *table_arg;
};

int reftable_table_seek_ref(struct reftable_table *tab,
			    struct reftable_iterator *it, const char *name);

void reftable_table_from_reader(struct reftable_table *tab,
				struct reftable_reader *reader);

/* returns the hash ID from a generic reftable_table */
uint32_t reftable_table_hash_id(struct reftable_table *tab);

/* create a generic table from reftable_merged_table */
void reftable_table_from_merged_table(struct reftable_table *tab,
				      struct reftable_merged_table *table);

/* returns the max update_index covered by this table. */
uint64_t reftable_table_max_update_index(struct reftable_table *tab);

/* returns the min update_index covered by this table. */
uint64_t reftable_table_min_update_index(struct reftable_table *tab);

/* convenience function to read a single ref. Returns < 0 for error, 0
   for success, and 1 if ref not found. */
int reftable_table_read_ref(struct reftable_table *tab, const char *name,
			    struct reftable_ref_record *ref);

#endif
