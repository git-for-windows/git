#ifndef GVFS_HELPER_CLIENT_H
#define GVFS_HELPER_CLIENT_H

struct repository;
struct commit;
struct object_id;

enum gh_client__created {
	/*
	 * The _get_ operation did not create anything.  If doesn't
	 * matter if `gvfs-helper` had errors or not -- just that
	 * nothing was created.
	 */
	GHC__CREATED__NOTHING  = 0,

	/*
	 * The _get_ operation created one or more packfiles.
	 */
	GHC__CREATED__PACKFILE = 1<<1,

	/*
	 * The _get_ operation created one or more loose objects.
	 * (Not necessarily the for the individual OID you requested.)
	 */
	GHC__CREATED__LOOSE    = 1<<2,

	/*
	 * The _get_ operation created one or more packfilea *and*
	 * one or more loose objects.
	 */
	GHC__CREATED__PACKFILE_AND_LOOSE = (GHC__CREATED__PACKFILE |
					    GHC__CREATED__LOOSE),
};

/*
 * Ask `gvfs-helper server` to immediately fetch a single object
 * using "/gvfs/objects" GET semantics.
 *
 * A long-running background process is used to make subsequent
 * requests more efficient.
 *
 * A loose object will be created in the shared-cache ODB and
 * in-memory cache updated.
 */
int gh_client__get_immediate(const struct object_id *oid,
			     enum gh_client__created *p_ghc);

/*
 * Queue this OID for a future fetch using `gvfs-helper service`.
 * It does not wait.
 *
 * Callers should not rely on the queued object being on disk until
 * the queue has been drained.
 */
void gh_client__queue_oid(const struct object_id *oid);
void gh_client__queue_oid_array(const struct object_id *oids, int oid_nr);

/*
 * Ask `gvfs-helper server` to fetch the set of queued OIDs using
 * "/gvfs/objects" POST semantics.
 *
 * A long-running background process is used to subsequent requests
 * more efficient.
 *
 * One or more packfiles will be created in the shared-cache ODB.
 */
int gh_client__drain_queue(enum gh_client__created *p_ghc);

#endif /* GVFS_HELPER_CLIENT_H */
