#define USE_THE_REPOSITORY_VARIABLE

#include "test-tool.h"
#include "git-compat-util.h"
#include "delta.h"
#include "git-zlib.h"
#include "hash.h"
#include "hex.h"
#include "pack.h"
#include "packfile.h"
#include "pack-objects.h"
#include "parse-options.h"
#include "setup.h"
#include "strbuf.h"
#include "string-list.h"

static const char *usage_str[] = {
	"test-tool pack-deltas --num-objects <num-objects>",
	"test-tool pack-deltas --list-deltas <pack>.idx",
	NULL
};

static size_t do_compress(void **pptr, size_t size)
{
	git_zstream stream;
	void *in, *out;
	size_t maxsize;

	git_deflate_init(&stream, 1);
	maxsize = git_deflate_bound(&stream, size);

	in = *pptr;
	out = xmalloc(maxsize);
	*pptr = out;

	stream.next_in = in;
	stream.avail_in = size;
	stream.next_out = out;
	stream.avail_out = maxsize;
	while (git_deflate(&stream, Z_FINISH) == Z_OK)
		; /* nothing */
	git_deflate_end(&stream);

	free(in);
	return stream.total_out;
}

static void write_ref_delta(struct hashfile *f,
			    struct object_id *oid,
			    struct object_id *base)
{
	unsigned char header[MAX_PACK_OBJECT_HEADER];
	unsigned long compressed_size, hdrlen;
	size_t size, base_size, delta_size;
	enum object_type type;
	void *base_buf, *delta_buf;
	void *buf = odb_read_object(the_repository->objects,
				    oid, &type, &size);

	if (!buf)
		die("unable to read %s", oid_to_hex(oid));

	base_buf = odb_read_object(the_repository->objects,
				   base, &type, &base_size);

	if (!base_buf)
		die("unable to read %s", oid_to_hex(base));

	delta_buf = diff_delta(base_buf, base_size,
			       buf, size, &delta_size, 0);

	compressed_size = do_compress(&delta_buf, delta_size);

	hdrlen = encode_in_pack_object_header(header, sizeof(header),
					      OBJ_REF_DELTA, delta_size);
	hashwrite(f, header, hdrlen);
	hashwrite(f, base->hash, the_repository->hash_algo->rawsz);
	hashwrite(f, delta_buf, compressed_size);

	free(buf);
	free(base_buf);
	free(delta_buf);
}

static int list_delta(const struct object_id *oid,
		      struct packed_git *p,
		      uint32_t pos,
		      void *_w_curs)
{
	struct pack_window **w_curs = _w_curs;
	off_t obj_offset = nth_packed_object_offset(p, pos);
	off_t cur = obj_offset;
	size_t size;
	enum object_type type = unpack_object_header(p, w_curs, &cur,
						      &size);

	if (type < 0)
		die("unable to parse object at position %"PRIu32, pos);
	if (type != OBJ_REF_DELTA && type != OBJ_OFS_DELTA)
		return 0;

	if (type == OBJ_REF_DELTA) {
		struct object_id base_oid;
		const unsigned char *base = use_pack(p, w_curs, cur,
						     NULL);

		oidread(&base_oid, base, p->repo->hash_algo);
		printf("%s REF_DELTA %s\n", oid_to_hex(oid),
		       oid_to_hex(&base_oid));
	} else {
		off_t base_offset = get_delta_base(p, w_curs, &cur,
						   type, obj_offset);

		if (!base_offset)
			die("unable to read base of object %s", oid_to_hex(oid));
		printf("%s OFS_DELTA %"PRIuMAX"\n", oid_to_hex(oid),
		       (uintmax_t)base_offset);
	}

	return 0;
}

static void list_deltas(const char *idx_name)
{
	struct packed_git *p;
	struct pack_window *w_curs = NULL;

	p = add_packed_git(the_repository, idx_name, strlen(idx_name), 1);
	if (!p || open_pack_index(p))
		die("unable to open pack index %s", idx_name);

	if (for_each_object_in_pack(p, list_delta, &w_curs,
				    ODB_FOR_EACH_OBJECT_PACK_ORDER))
		die("unable to iterate over objects in %s", idx_name);

	unuse_pack(&w_curs);
	close_pack(p);
	free(p);
}

int cmd__pack_deltas(int argc, const char **argv)
{
	int num_objects = -1;
	int list_deltas_mode = 0;
	struct hashfile *f;
	struct strbuf line = STRBUF_INIT;
	struct option options[] = {
		OPT_INTEGER('n', "num-objects", &num_objects, N_("the number of objects to write")),
		OPT_BOOL(0, "list-deltas", &list_deltas_mode,
			 N_("list REF_DELTA and OFS_DELTA entries")),
		OPT_END()
	};

	argc = parse_options(argc, argv, NULL,
			     options, usage_str, 0);

	if (list_deltas_mode) {
		if (argc != 1 || num_objects >= 0)
			usage_with_options(usage_str, options);
		setup_git_directory(the_repository);
		list_deltas(argv[0]);
		return 0;
	}

	if (argc || num_objects < 0)
		usage_with_options(usage_str, options);

	setup_git_directory(the_repository);

	f = hashfd(the_repository->hash_algo, 1, "<stdout>");
	write_pack_header(f, num_objects);

	/* Read each line from stdin into 'line' */
	while (strbuf_getline_lf(&line, stdin) != EOF) {
		const char *type_str, *content_oid_str, *base_oid_str = NULL;
		struct object_id content_oid, base_oid;
		struct string_list items = STRING_LIST_INIT_NODUP;
		/*
		 * Tokenize into two or three parts:
		 * 1. REF_DELTA, OFS_DELTA, or FULL.
		 * 2. The object ID for the content object.
		 * 3. The object ID for the base object (optional).
		 */
		if (string_list_split_in_place(&items, line.buf, " ", 3) < 0)
			die("invalid input format: %s", line.buf);

		if (items.nr < 2)
			die("invalid input format: %s", line.buf);

		type_str = items.items[0].string;
		content_oid_str = items.items[1].string;

		if (get_oid_hex(content_oid_str, &content_oid))
			die("invalid object: %s", content_oid_str);
		if (items.nr >= 3) {
			base_oid_str = items.items[2].string;
			if (get_oid_hex(base_oid_str, &base_oid))
				die("invalid object: %s", base_oid_str);
		}
		string_list_clear(&items, 0);

		if (!strcmp(type_str, "REF_DELTA"))
			write_ref_delta(f, &content_oid, &base_oid);
		else if (!strcmp(type_str, "OFS_DELTA"))
			die("OFS_DELTA not implemented");
		else if (!strcmp(type_str, "FULL"))
			die("FULL not implemented");
		else
			die("unknown pack type: %s", type_str);
	}

	finalize_hashfile(f, NULL, FSYNC_COMPONENT_PACK,
			  CSUM_HASH_IN_STREAM | CSUM_FSYNC | CSUM_CLOSE);
	strbuf_release(&line);
	return 0;
}
