#include "cache.h"
#include "blob.h"
#include "repository.h"
#include "alloc.h"
#include "object-store.h"

const char *blob_type = "blob";

struct blob *lookup_blob_type(struct repository *r,
			      const struct object_id *oid,
			      enum object_type type)
{
	struct object *obj = lookup_object(r, oid);
	if (!obj)
		return create_object(r, oid, alloc_blob_node(r));
	if (type != OBJ_NONE &&
	    obj->type != OBJ_NONE) {
		enum object_type want = OBJ_BLOB;
		if (oid_is_type_or_error(oid, obj->type, &want))
			return NULL;
	}
	return object_as_type(obj, OBJ_BLOB, 0);
}

struct blob *lookup_blob(struct repository *r, const struct object_id *oid)
{
	return lookup_blob_type(r, oid, OBJ_NONE);
}

int parse_blob_buffer(struct blob *item, void *buffer, unsigned long size)
{
	item->object.parsed = 1;
	return 0;
}
