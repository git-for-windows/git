#define USE_THE_REPOSITORY_VARIABLE
#include "git-compat-util.h"
#include "environment.h"
#include "hex.h"
#include "alloc.h"
#include "setup.h"
#include "protocol.h"
#include "config.h"
#include "pkt-line.h"
#include "run-command.h"
#include "strbuf.h"
#include "string-list.h"
#include "trace2.h"
#include "object.h"
#include "odb.h"
#include "replace-object.h"
#include "repository.h"
#include "version.h"
#include "dir.h"
#include "json-writer.h"
#include "oidset.h"
#include "date.h"
#include "wrapper.h"
#include "git-zlib.h"

#define TR2_CAT "test-gvfs-protocol"

static const char *pid_file;
static int verbose;
static int reuseaddr;
static struct string_list mayhem_list = STRING_LIST_INIT_DUP;
static int mayhem_child = 0;
static struct json_writer jw_config = JSON_WRITER_INIT;

/*
 * We look for one of these "servertypes" in the uri-base
 * so we can behave differently when we need to.
 */
#define MY_SERVER_TYPE__ORIGIN "servertype/origin"
#define MY_SERVER_TYPE__CACHE  "servertype/cache"

static const char test_gvfs_protocol_usage[] =
"gvfs-protocol [--verbose]\n"
"           [--timeout=<n>] [--init-timeout=<n>] [--max-connections=<n>]\n"
"           [--reuseaddr] [--pid-file=<file>]\n"
"           [--listen=<host_or_ipaddr>]* [--port=<n>]\n"
"           [--mayhem=<token>]*\n"
;

/* Timeout, and initial timeout */
static unsigned int timeout;
static unsigned int init_timeout;

static void logreport(const char *label, const char *err, va_list params)
{
	struct strbuf msg = STRBUF_INIT;

	strbuf_addf(&msg, "[%"PRIuMAX"] %s: ", (uintmax_t)getpid(), label);
	strbuf_vaddf(&msg, err, params);
	strbuf_addch(&msg, '\n');

	fwrite(msg.buf, sizeof(char), msg.len, stderr);
	fflush(stderr);

	strbuf_release(&msg);
}

__attribute__((format (printf, 1, 2)))
static void logerror(const char *err, ...)
{
	va_list params;
	va_start(params, err);
	logreport("error", err, params);
	va_end(params);
}

__attribute__((format (printf, 1, 2)))
static void loginfo(const char *err, ...)
{
	va_list params;
	if (!verbose)
		return;
	va_start(params, err);
	logreport("info", err, params);
	va_end(params);
}

__attribute__((format (printf, 1, 2)))
static void logmayhem(const char *err, ...)
{
	va_list params;
	if (!verbose)
		return;
	va_start(params, err);
	logreport("mayhem", err, params);
	va_end(params);
}

static void set_keep_alive(int sockfd)
{
	int ka = 1;

	if (setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, &ka, sizeof(ka)) < 0) {
		if (errno != ENOTSOCK)
			logerror("unable to set SO_KEEPALIVE on socket: %s",
				strerror(errno));
	}
}

//////////////////////////////////////////////////////////////////
// The code in this section is used by "worker" instances to service
// a single connection from a client.  The worker talks to the client
// on 0 and 1.
//////////////////////////////////////////////////////////////////

enum worker_result {
	/*
	 * Operation successful.
	 * Caller *might* keep the socket open and allow keep-alive.
	 */
	WR_OK       = 0,
	/*
	 * Various errors while processing the request and/or the response.
	 * Close the socket and clean up.
	 * Exit child-process with non-zero status.
	 */
	WR_IO_ERROR = 1<<0,
	/*
	 * Close the socket and clean up.  Does not imply an error.
	 */
	WR_HANGUP   = 1<<1,
	/*
	 * The result of a function was influenced by the mayhem settings.
	 * Does not imply that we need to exit or close the socket.
	 * Just advice to callers in the worker stack.
	 */
	WR_MAYHEM   = 1<<2,

	WR_STOP_THE_MUSIC = (WR_IO_ERROR | WR_HANGUP),
};

/*
 * Fields from a parsed HTTP request.
 */
struct req {
	struct strbuf start_line;
	struct string_list start_line_fields;

	struct strbuf uri_base;
	struct strbuf gvfs_api;
	struct strbuf slash_args;
	struct strbuf quest_args;

	struct string_list header_list;
};

#define REQ__INIT { \
	.start_line = STRBUF_INIT, \
	.start_line_fields = STRING_LIST_INIT_DUP, \
	.uri_base = STRBUF_INIT, \
	.gvfs_api = STRBUF_INIT, \
	.slash_args = STRBUF_INIT, \
	.quest_args = STRBUF_INIT, \
	.header_list = STRING_LIST_INIT_DUP, \
	}

static void req__release(struct req *req)
{
	strbuf_release(&req->start_line);
	string_list_clear(&req->start_line_fields, 0);

	strbuf_release(&req->uri_base);
	strbuf_release(&req->gvfs_api);
	strbuf_release(&req->slash_args);
	strbuf_release(&req->quest_args);

	string_list_clear(&req->header_list, 0);
}

/*
 * Generate a somewhat bogus UUID/GUID that is good enough for
 * a test suite, but without requiring platform-specific UUID
 * or GUID libraries.
 */
static void gen_fake_uuid(struct strbuf *uuid)
{
	static unsigned int seq = 0;
	static struct timeval tv;
	static struct tm tm;
	static time_t secs;

	strbuf_setlen(uuid, 0);

	if (!seq) {
		gettimeofday(&tv, NULL);
		secs = tv.tv_sec;
		gmtime_r(&secs, &tm);
	}

	/*
	 * Build a string that looks like:
	 *
	 *     "ffffffff-eeee-dddd-cccc-bbbbbbbbbbbb"
	 *
	 * Note that the first digit in the "dddd" section gives the
	 * UUID type.  We set it to zero so that we won't collide with
	 * any "real" UUIDs.
	 */
	strbuf_addf(uuid, "%04d%02d%02d-%02d%02d-00%02d-%04x-%08x%04x",
		    tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
		    tm.tm_hour, tm.tm_min,
		    tm.tm_sec,
		    (unsigned)(getpid() & 0xffff),
		    (unsigned)(tv.tv_usec & 0xffffffff),
		    (seq++ & 0xffff));
}

/*
 * Send a chunk of data to the client using HTTP chunked
 * transfer coding rules.
 *
 * https://tools.ietf.org/html/rfc7230#section-4.1
 */
static enum worker_result send_chunk(int fd, const unsigned char *buf,
					 size_t len_buf)
{
	char chunk_size[100];
	int chunk_size_len = xsnprintf(chunk_size, sizeof(chunk_size),
				       "%x\r\n", (unsigned int)len_buf);

	if ((write_in_full(fd, chunk_size, chunk_size_len) < 0) ||
	    (write_in_full(fd, buf, len_buf) < 0) ||
	    (write_in_full(fd, "\r\n", 2) < 0)) {
		logerror("unable to send chunk");
		return WR_IO_ERROR;
	}

	return WR_OK;
}

static enum worker_result send_final_chunk(int fd)
{
	if (write_in_full(fd, "0\r\n\r\n", 5) < 0) {
		logerror("unable to send final chunk");
		return WR_IO_ERROR;
	}

	return WR_OK;
}

static enum worker_result send_http_error(
	int fd,
	int http_code, const char *http_code_name,
	int retry_after_seconds, enum worker_result wr_in)
{
	struct strbuf response_header = STRBUF_INIT;
	struct strbuf response_content = STRBUF_INIT;
	struct strbuf uuid = STRBUF_INIT;
	enum worker_result wr;

	strbuf_addf(&response_content, "Error: %d %s\r\n",
		    http_code, http_code_name);
	if (retry_after_seconds > 0)
		strbuf_addf(&response_content, "Retry-After: %d\r\n",
			    retry_after_seconds);

	strbuf_addf  (&response_header, "HTTP/1.1 %d %s\r\n", http_code, http_code_name);
	strbuf_addstr(&response_header, "Cache-Control: private\r\n");
	strbuf_addstr(&response_header,	"Content-Type: text/plain\r\n");
	strbuf_addf  (&response_header,	"Content-Length: %d\r\n", (int)response_content.len);
	if (retry_after_seconds > 0)
		strbuf_addf  (&response_header, "Retry-After: %d\r\n", retry_after_seconds);
	strbuf_addf(  &response_header,	"Server: test-gvfs-protocol/%s\r\n", git_version_string);
	strbuf_addf(  &response_header, "Date: %s\r\n", show_date(time(NULL), 0, DATE_MODE(RFC2822)));
	gen_fake_uuid(&uuid);
	strbuf_addf(  &response_header, "X-VSS-E2EID: %s\r\n", uuid.buf);
	strbuf_addstr(&response_header, "\r\n");

	if (write_in_full(fd, response_header.buf, response_header.len) < 0) {
		logerror("unable to write response header");
		wr = WR_IO_ERROR;
		goto done;
	}

	if (write_in_full(fd, response_content.buf, response_content.len) < 0) {
		logerror("unable to write response content body");
		wr = WR_IO_ERROR;
		goto done;
	}

	wr = wr_in;

done:
	strbuf_release(&uuid);
	strbuf_release(&response_header);
	strbuf_release(&response_content);

	return wr;
}

/*
 * Return 1 if we send an AUTH error to the client.
 */
static int mayhem_try_auth(struct req *req, enum worker_result *wr_out)
{
	*wr_out = WR_OK;

	if (string_list_has_string(&mayhem_list, "http_401")) {
		struct string_list_item *item;
		int has_auth = 0;
		for_each_string_list_item(item, &req->header_list) {
			if (starts_with(item->string, "Authorization: Basic")) {
				has_auth = 1;
				break;
			}
		}
		if (!has_auth) {
			if (strstr(req->uri_base.buf, MY_SERVER_TYPE__ORIGIN)) {
				logmayhem("http_401 (origin)");
				*wr_out = send_http_error(1, 401, "Unauthorized", -1,
							  WR_MAYHEM);
				return 1;
			}

			else if (strstr(req->uri_base.buf, MY_SERVER_TYPE__CACHE)) {
				/*
				 * Cache servers use a non-standard 400 rather than a 401.
				 */
				logmayhem("http_400 (cacheserver)");
				*wr_out = send_http_error(1, 400, "Bad Request", -1,
							  WR_MAYHEM);
				return 1;
			}

			else {
				/*
				 * Non-qualified server type.
				 */
				logmayhem("http_401");
				*wr_out = send_http_error(1, 401, "Unauthorized", -1,
							  WR_MAYHEM);
				return 1;
			}
		}
	}

	return 0;
}

/*
 * Build fake gvfs/config data using our IP address and port.
 *
 * The Min/Max data is just random noise copied from the example
 * in the documentation.
 */
static void build_gvfs_config_json(struct json_writer *jw,
				   struct string_list *listen_addr,
				   int listen_port)
{
	jw_object_begin(jw, 0);
	{
		jw_object_inline_begin_array(jw, "AllowedGvfsClientVersions");
		{
			jw_array_inline_begin_object(jw);
			{
				jw_object_inline_begin_object(jw, "Max");
				{
					jw_object_intmax(jw, "Major", 0);
					jw_object_intmax(jw, "Minor", 4);
					jw_object_intmax(jw, "Build", 0);
					jw_object_intmax(jw, "Revision", 0);
				}
				jw_end(jw);

				jw_object_inline_begin_object(jw, "Min");
				{
					jw_object_intmax(jw, "Major", 0);
					jw_object_intmax(jw, "Minor", 2);
					jw_object_intmax(jw, "Build", 0);
					jw_object_intmax(jw, "Revision", 0);
				}
				jw_end(jw);
			}
			jw_end(jw);

			jw_array_inline_begin_object(jw);
			{
				jw_object_null(jw, "Max");
				jw_object_inline_begin_object(jw, "Min");
				{
					jw_object_intmax(jw, "Major", 0);
					jw_object_intmax(jw, "Minor", 5);
					jw_object_intmax(jw, "Build", 16326);
					jw_object_intmax(jw, "Revision", 1);
				}
				jw_end(jw);
			}
			jw_end(jw);
		}
		jw_end(jw);

		jw_object_inline_begin_array(jw, "CacheServers");
		{
			struct string_list_item *item;
			int k = 0;

			for_each_string_list_item(item, listen_addr) {
				jw_array_inline_begin_object(jw);
				{
					struct strbuf buf = STRBUF_INIT;

					strbuf_addf(&buf, "http://%s:%d/%s",
						    item->string,
						    listen_port,
						    MY_SERVER_TYPE__CACHE);
					jw_object_string(jw, "Url", buf.buf);
					strbuf_release(&buf);

					strbuf_addf(&buf, "cs%02d", k);
					jw_object_string(jw, "Name", buf.buf);
					strbuf_release(&buf);

					jw_object_bool(jw, "GlobalDefault",
						       k++ == 0);
				}
				jw_end(jw);
			}
		}
		jw_end(jw);
	}
	jw_end(jw);
}
/*
 * Per the GVFS Protocol, this should only be recognized on the origin
 * server (not the cache-server).  It returns a JSON payload of config
 * data.
 */
static enum worker_result do__gvfs_config__get(struct req *req)
{
	struct strbuf response_header = STRBUF_INIT;
	struct strbuf uuid = STRBUF_INIT;
	enum worker_result wr;

	if (strstr(req->uri_base.buf, MY_SERVER_TYPE__CACHE))
		return send_http_error(1, 404, "Not Found", -1, WR_OK);

	strbuf_addstr(&response_header, "HTTP/1.1 200 OK\r\n");
	strbuf_addstr(&response_header, "Cache-Control: private\r\n");
	strbuf_addstr(&response_header,	"Content-Type: text/plain\r\n");
	strbuf_addf(  &response_header,	"Content-Length: %d\r\n", (int)jw_config.json.len);
	strbuf_addf(  &response_header,	"Server: test-gvfs-protocol/%s\r\n", git_version_string);
	strbuf_addf(  &response_header, "Date: %s\r\n", show_date(time(NULL), 0, DATE_MODE(RFC2822)));
	gen_fake_uuid(&uuid);
	strbuf_addf(  &response_header, "X-VSS-E2EID: %s\r\n", uuid.buf);
	strbuf_addstr(&response_header, "\r\n");

	if (write_in_full(1, response_header.buf, response_header.len) < 0) {
		logerror("unable to write response header");
		wr = WR_IO_ERROR;
		goto done;
	}

	if (write_in_full(1, jw_config.json.buf, jw_config.json.len) < 0) {
		logerror("unable to write response content body");
		wr = WR_IO_ERROR;
		goto done;
	}

	wr = WR_OK;

done:
	strbuf_release(&uuid);
	strbuf_release(&response_header);

	return wr;
}

/*
 * Send the contents of the in-memory inflated object in "compressed
 * loose object" format over the socket.
 *
 * Because we are using keep-alive and are streaming the compressed
 * chunks as we produce them, we set the transport-encoding and not
 * the content-length.
 *
 * Our usage here is different from `git-http-backend` because it will
 * only send a loose object if it exists as a loose object in the ODB
 * (see the "/objects/[0-9a-f]{2}/[0-9a-f]{38}$" regex_t declarations)
 * by doing a file-copy.
 *
 * We want to send an arbitrary object without regard for how it is
 * currently stored in the local ODB.
 *
 * Also, we don't want any of the type-specific branching found in the
 * sha1-file.c functions (such as special casing BLOBs).  Specifically,
 * we DO NOT want any of the content conversion filters.  We just want
 * to send the raw content as is.
 *
 * So, we steal freely from sha1-file.c routines:
 *     write_object_file_prepare()
 *     write_loose_object()
 */
static enum worker_result send_loose_object(const struct object_info *oi,
					    const struct object_id *oid,
					    int fd)
{
#define MAX_HEADER_LEN 32
	struct strbuf response_header = STRBUF_INIT;
	struct strbuf uuid = STRBUF_INIT;
	char object_header[MAX_HEADER_LEN];
	unsigned char compressed[4096];
	git_zstream stream;
	struct object_id oid_check;
	struct git_hash_ctx c;
	int object_header_len;
	int ret;

	/*
	 * We are blending several somewhat independent concepts here:
	 *
	 * [1] reconstructing the object format in parts:
	 *
	 *           <object>          ::= <object_header> <object_content>
	 *
	 *      [1a] <object_header>   ::= <object_type> SP <object_length> NUL
	 *      [1b] <object_conttent> ::= <array_of_bytes>
	 *
	 * [2] verify that we constructed [1] correctly by computing
	 *     the hash of [1] and verify it matches the passed OID.
	 *
	 * [3] compress [1] because that is how loose objects are
	 *     stored on disk.  We compress it as we stream it to
	 *     the client.
	 *
	 * [4] send HTTP response headers to the client.
	 *
	 * [5] stream each chunk from [3] to the client using the HTTP
	 *     chunked transfer coding.
	 *
	 * [6] for extra credit, we repeat the hash construction in [2]
	 *     as we stream it.
	 */

	/* [4] */
	strbuf_addstr(&response_header, "HTTP/1.1 200 OK\r\n");
	strbuf_addstr(&response_header, "Cache-Control: private\r\n");
	strbuf_addstr(&response_header,	"Content-Type: application/x-git-loose-object\r\n");
	strbuf_addf(  &response_header,	"Server: test-gvfs-protocol/%s\r\n", git_version_string);
	strbuf_addstr(&response_header, "Transfer-Encoding: chunked\r\n");
	strbuf_addf(  &response_header, "Date: %s\r\n", show_date(time(NULL), 0, DATE_MODE(RFC2822)));
	gen_fake_uuid(&uuid);
	strbuf_addf(  &response_header, "X-VSS-E2EID: %s\r\n", uuid.buf);
	strbuf_addstr(&response_header, "\r\n");

	if (write_in_full(fd, response_header.buf, response_header.len) < 0) {
		logerror("unable to write response header");
		return WR_IO_ERROR;
	}

	strbuf_release(&uuid);
	strbuf_release(&response_header);

	if (string_list_has_string(&mayhem_list, "close_write")) {
		logmayhem("close_write");
		return WR_MAYHEM | WR_HANGUP;
	}

	/* [1a] */
	object_header_len = 1 + xsnprintf(object_header, MAX_HEADER_LEN,
					  "%s %"PRIuMAX,
					  type_name(*oi->typep),
					  (uintmax_t)*oi->sizep);

	/* [2] */
	memset(&oid_check, 0, sizeof(oid_check));
	the_hash_algo->init_fn(&c);
	the_hash_algo->update_fn(&c, object_header, object_header_len);
	the_hash_algo->update_fn(&c, *oi->contentp, *oi->sizep);
	the_hash_algo->final_fn(oid_check.hash, &c);
	if (!oideq(oid, &oid_check))
		BUG("send_loose_object[2]: invalid construction '%s' '%s'",
		    oid_to_hex(oid), oid_to_hex(&oid_check));

	/* [3, 6] */
	git_deflate_init(&stream, zlib_compression_level);
	stream.next_out = compressed;
	stream.avail_out = sizeof(compressed);
	the_hash_algo->init_fn(&c);

	/* [3, 1a, 6] */
	stream.next_in = (unsigned char *)object_header;
	stream.avail_in = object_header_len;
	while (git_deflate(&stream, 0) == Z_OK)
		; /* nothing */
	the_hash_algo->update_fn(&c, object_header, object_header_len);

	/* [3, 1b, 5, 6] */
	stream.next_in = *oi->contentp;
	stream.avail_in = *oi->sizep;
	do {
		enum worker_result wr;
		unsigned char *in0 = stream.next_in;
		ret = git_deflate(&stream, Z_FINISH);
		the_hash_algo->update_fn(&c, in0, stream.next_in - in0);

		/* [5] */
		wr = send_chunk(fd, compressed, stream.next_out - compressed);
		if (wr & WR_STOP_THE_MUSIC) {
			return wr;
		}

		stream.next_out = compressed;
		stream.avail_out = sizeof(compressed);

	} while (ret == Z_OK);

	/* [3] */
	if (ret != Z_STREAM_END)
		BUG("unable to deflate object '%s' (%d)", oid_to_hex(oid), ret);
	ret = git_deflate_end_gently(&stream);
	if (ret != Z_OK)
		BUG("deflateEnd on object '%s' failed (%d)", oid_to_hex(oid), ret);

	/* [6] */
	the_hash_algo->final_fn(oid_check.hash, &c);
	if (!oideq(oid, &oid_check))
		BUG("send_loose_object[6]: invalid construction '%s' '%s'",
		    oid_to_hex(oid), oid_to_hex(&oid_check));

	/* [5] */
	return send_final_chunk(fd);
}

/*
 * Per the GVFS Protocol, a single OID should be in the slash-arg:
 *
 *     GET /gvfs/objects/fc3fff3a25559d2d30d1719c4f4a6d9fe7e05170 HTTP/1.1
 *
 * Look it up in our repo (loose or packed) and send it to gvfs-helper
 * over the socket as a loose object.
 */
static enum worker_result do__gvfs_objects__get(struct req *req)
{
	struct object_id oid;
	void *content;
	unsigned long size;
	enum object_type type;
	struct object_info oi = OBJECT_INFO_INIT;
	unsigned flags = 0;

	/*
	 * Since `test-gvfs-protocol` is mocking a real GVFS server (cache or
	 * main), we don't want a request for a missing object to cause the
	 * implicit dynamic fetch mechanism to try to fault-it-in (and cause
	 * our call to oid_object_info_extended() to launch another instance
	 * of `gvfs-helper` to magically fetch it (which would connect to a
	 * new instance of `test-gvfs-protocol`)).
	 *
	 * Rather, we want a missing object to fail, so we can respond with
	 * a 404, for example.
	 */
	flags |= OBJECT_INFO_FOR_PREFETCH;
	flags |= OBJECT_INFO_LOOKUP_REPLACE;

	if (!req->slash_args.len ||
	    get_oid_hex(req->slash_args.buf, &oid)) {
		logerror("invalid OID in GET gvfs/objects: '%s'",
			 req->slash_args.buf);
		return WR_IO_ERROR;
	}

	trace2_printf("%s: GET %s", TR2_CAT, oid_to_hex(&oid));

	oi.typep = &type;
	oi.sizep = &size;
	oi.contentp = &content;

	if (oid_object_info_extended(the_repository, &oid, &oi, flags)) {
		logerror("Could not find OID: '%s'", oid_to_hex(&oid));
		return send_http_error(1, 404, "Not Found", -1, WR_OK);
	}

	if (string_list_has_string(&mayhem_list, "http_404")) {
		logmayhem("http_404");
		return send_http_error(1, 404, "Not Found", -1, WR_MAYHEM);
	}

	trace2_printf("%s: OBJECT type=%d len=%ld '%.40s'", TR2_CAT,
		      type, size, (const char *)content);

	return send_loose_object(&oi, &oid, 1);
}

static enum worker_result read_json_post_body(
	struct req *req,
	struct oidset *oids)
{
	struct object_id oid;
	struct string_list_item *item;
	char *post_body = NULL;
	const char *v;
	ssize_t len_expected = 0;
	ssize_t len_received;
	const char *pkey;
	const char *plbracket;
	const char *pstart;
	const char *pend;

	for_each_string_list_item(item, &req->header_list) {
		if (skip_prefix(item->string, "Content-Length: ", &v)) {
			char *p;
			len_expected = strtol(v, &p, 10);
			break;
		}
	}
	if (!len_expected) {
		logerror("no content length in POST");
		return WR_IO_ERROR;
	}
	post_body = xcalloc(1, len_expected + 1);
	if (!post_body) {
		logerror("could not malloc buffer for POST body");
		return WR_IO_ERROR;
	}
	len_received = read_in_full(0, post_body, len_expected);
	if (len_received != len_expected) {
		logerror("short read in POST (expected %d, received %d)",
			 (int)len_expected, (int)len_received);
		return WR_IO_ERROR;
	}

	/*
	 * A very primitive JSON parser for a very fixed and well-known
	 * message format.  Please don't judge me.
	 *
	 * We expect:
	 *
	 *     ..."objectIds":["<oid_1>","<oid_2>",..."<oid_n>"]...
	 *
	 * We expect compact (non-pretty) JSON, but do allow it.
	 */
	pkey = strstr(post_body, "\"objectIds\"");
	if (!pkey)
		goto could_not_parse_json;
	plbracket = strchr(pkey, '[');
	if (!plbracket)
		goto could_not_parse_json;
	pstart = plbracket + 1;

	while (1) {
		/* Eat leading whitespace before opening DQUOTE */
		while (*pstart && isspace(*pstart))
			pstart++;
		if (!*pstart)
			goto could_not_parse_json;
		pstart++;

		/* find trailing DQUOTE */
		pend = strchr(pstart, '"');
		if (!pend)
			goto could_not_parse_json;

		if (get_oid_hex(pstart, &oid))
			goto could_not_parse_json;
		oidset_insert(oids, &oid);
		trace2_printf("%s: POST %s", TR2_CAT, oid_to_hex(&oid));

		/* Eat trailing whitespace after trailing DQUOTE */
		pend++;
		while (*pend && isspace(*pend))
			pend++;
		if (!*pend)
			goto could_not_parse_json;

		/* End of list or is there another OID */
		if (*pend == ']')
			break;
		if (*pend != ',')
			goto could_not_parse_json;

		pstart = pend + 1;
	}

	/*
	 * We do not care about the "commitDepth" parameter.
	 */

	free(post_body);
	return WR_OK;

could_not_parse_json:
	logerror("could not parse JSON in POST body");
	free(post_body);
	return WR_IO_ERROR;
}

/*
 * Since this is a test helper, I'm going to be lazy and
 * run pack-objects as a background child using pipe_command
 * and get the resulting packfile into a buffer.  And then
 * the caller can pump it to the client over the socket.
 *
 * This avoids the need to set up a custom loop (like in
 * upload-pack) to drive it and/or the use of a bunch of
 * tempfiles.
 *
 * My assumption here is that we're not testing with GBs
 * of data....
 *
 * Note: The GVFS Protocol POST verb behaves like GET for
 * Note: non-commit objects (in that it just returns the
 * Note: requested object), but for commit objects POST
 * Note: *also* returns all trees referenced by the commit.
 * Note:
 * Note: Since the goal of this test is to confirm that
 * Note: gvfs-helper can request and receive a packfile
 * Note: *at all*, I'm not going to blur the issue and
 * Note: support the extra semantics for commit objects.
 */
static enum worker_result get_packfile_from_oids(
	struct oidset *oids,
	struct strbuf *buf_packfile)
{
	struct child_process pack_objects = CHILD_PROCESS_INIT;
	struct strbuf buf_child_stdin = STRBUF_INIT;
	struct strbuf buf_child_stderr = STRBUF_INIT;
	struct oidset_iter iter;
	struct object_id *oid;
	enum worker_result wr;
	int result;

	strvec_push(&pack_objects.args, "git");
	strvec_push(&pack_objects.args, "pack-objects");
	strvec_push(&pack_objects.args, "-q");
	strvec_push(&pack_objects.args, "--revs");
	strvec_push(&pack_objects.args, "--delta-base-offset");
	strvec_push(&pack_objects.args, "--window=0");
	strvec_push(&pack_objects.args, "--depth=4095");
	strvec_push(&pack_objects.args, "--compression=1");
	strvec_push(&pack_objects.args, "--stdout");

	pack_objects.in = -1;
	pack_objects.out = -1;
	pack_objects.err = -1;

	oidset_iter_init(oids, &iter);
	while ((oid = oidset_iter_next(&iter)))
		strbuf_addf(&buf_child_stdin, "%s\n", oid_to_hex(oid));
	strbuf_addstr(&buf_child_stdin, "\n");

	result = pipe_command(&pack_objects,
			      buf_child_stdin.buf, buf_child_stdin.len,
			      buf_packfile, 0,
			      &buf_child_stderr, 0);
	if (result) {
		logerror("pack-objects failed: %s", buf_child_stderr.buf);
		wr = WR_IO_ERROR;
		goto done;
	}

	trace2_printf("%s: pack-objects returned %d bytes", TR2_CAT, buf_packfile->len);
	wr = WR_OK;

done:
	strbuf_release(&buf_child_stdin);
	strbuf_release(&buf_child_stderr);

	return wr;
}

static enum worker_result send_packfile_from_buffer(const struct strbuf *packfile)
{
	struct strbuf response_header = STRBUF_INIT;
	struct strbuf uuid = STRBUF_INIT;
	enum worker_result wr;

	strbuf_addstr(&response_header, "HTTP/1.1 200 OK\r\n");
	strbuf_addstr(&response_header, "Cache-Control: private\r\n");
	strbuf_addstr(&response_header,	"Content-Type: application/x-git-packfile\r\n");
	strbuf_addf(  &response_header,	"Content-Length: %d\r\n", (int)packfile->len);
	strbuf_addf(  &response_header,	"Server: test-gvfs-protocol/%s\r\n", git_version_string);
	strbuf_addf(  &response_header, "Date: %s\r\n", show_date(time(NULL), 0, DATE_MODE(RFC2822)));
	gen_fake_uuid(&uuid);
	strbuf_addf(  &response_header, "X-VSS-E2EID: %s\r\n", uuid.buf);
	strbuf_addstr(&response_header, "\r\n");

	if (write_in_full(1, response_header.buf, response_header.len) < 0) {
		logerror("unable to write response header");
		wr = WR_IO_ERROR;
		goto done;
	}

	if (write_in_full(1, packfile->buf, packfile->len) < 0) {
		logerror("unable to write response content body");
		wr = WR_IO_ERROR;
		goto done;
	}

	wr = WR_OK;

done:
	strbuf_release(&uuid);
	strbuf_release(&response_header);

	return wr;
}

static enum worker_result do__gvfs_objects__post(struct req *req)
{
	struct oidset oids = OIDSET_INIT;
	struct strbuf packfile = STRBUF_INIT;
	enum worker_result wr;

	wr = read_json_post_body(req, &oids);
	if (wr & WR_STOP_THE_MUSIC)
		goto done;

	wr = get_packfile_from_oids(&oids, &packfile);
	if (wr & WR_STOP_THE_MUSIC)
		goto done;

	wr = send_packfile_from_buffer(&packfile);

done:
	oidset_clear(&oids);
	strbuf_release(&packfile);

	return wr;
}

/*
 * Read the HTTP request up to the start of the optional message-body.
 * We do this byte-by-byte because we have keep-alive turned on and
 * cannot rely on an EOF.
 *
 * https://tools.ietf.org/html/rfc7230
 * https://github.com/microsoft/VFSForGit/blob/master/Protocol.md
 *
 * We cannot call die() here because our caller needs to properly
 * respond to the client and/or close the socket before this
 * child exits so that the client doesn't get a connection reset
 * by peer error.
 */
static enum worker_result req__read(struct req *req, int fd)
{
	struct strbuf h = STRBUF_INIT;
	int nr_start_line_fields;
	const char *uri_target;
	const char *http_version;
	const char *gvfs;

	/*
	 * Read line 0 of the request and split it into component parts:
	 *
	 *    <method> SP <uri-target> SP <HTTP-version> CRLF
	 *
	 */
	if (strbuf_getwholeline_fd(&req->start_line, fd, '\n') == EOF)
		return WR_OK | WR_HANGUP;

	if (string_list_has_string(&mayhem_list, "close_read")) {
		logmayhem("close_read");
		return WR_MAYHEM | WR_HANGUP;
	}

	if (string_list_has_string(&mayhem_list, "close_read_1") &&
	    mayhem_child == 0) {
		/*
		 * Mayhem: fail the first request, but let retries succeed.
		 */
		logmayhem("close_read_1");
		return WR_MAYHEM | WR_HANGUP;
	}

	strbuf_trim_trailing_newline(&req->start_line);

	nr_start_line_fields = string_list_split(&req->start_line_fields,
						 req->start_line.buf,
						 " ", -1);
	if (nr_start_line_fields != 3) {
		logerror("could not parse request start-line '%s'",
			 req->start_line.buf);
		return WR_IO_ERROR;
	}
	uri_target = req->start_line_fields.items[1].string;
	http_version = req->start_line_fields.items[2].string;

	if (strcmp(http_version, "HTTP/1.1")) {
		logerror("unsupported version '%s' (expecting HTTP/1.1)",
			 http_version);
		return WR_IO_ERROR;
	}

	/*
	 * Next, extract the GVFS terms from the <uri-target>.  The
	 * GVFS Protocol defines a REST API containing several GVFS
	 * commands of the form:
	 *
	 *     [<uri-base>]/gvfs/<token>[/<args>]
	 *     [<uri-base>]/gvfs/<token>[?<args>]
	 *
	 * For example:
	 *     "GET /gvfs/config HTTP/1.1"
	 *     "GET /gvfs/objects/aaaaaaaaaabbbbbbbbbbccccccccccdddddddddd HTTP/1.1"
	 *     "GET /gvfs/prefetch?lastPackTimestamp=123456789 HTTP/1.1"
	 *
	 *     "GET /<uri-base>/gvfs/config HTTP/1.1"
	 *     "GET /<uri-base>/gvfs/objects/aaaaaaaaaabbbbbbbbbbccccccccccdddddddddd HTTP/1.1"
	 *     "GET /<uri-base>/gvfs/prefetch?lastPackTimestamp=123456789 HTTP/1.1"
	 *
	 *     "POST /<uri-base>/gvfs/objects HTTP/1.1"
	 *
	 * For other testing later, we also allow non-gvfs URLs of the form:
	 *     "GET /<uri>[?<args>] HTTP/1.1"
	 *
	 * We do not attempt to split the query-params within the args.
	 * The caller can do that if they need to.
	 */
	gvfs = strstr(uri_target, "/gvfs/");
	if (gvfs) {
		strbuf_add(&req->uri_base, uri_target, (gvfs - uri_target));
		strbuf_trim_trailing_dir_sep(&req->uri_base);

		gvfs += 6; /* skip "/gvfs/" */
		strbuf_add(&req->gvfs_api, "gvfs/", 5);
		while (*gvfs && *gvfs != '/' && *gvfs != '?')
			strbuf_addch(&req->gvfs_api, *gvfs++);

		/*
		 */
		if (*gvfs == '/')
			strbuf_addstr(&req->slash_args, gvfs + 1);
		else if (*gvfs == '?')
			strbuf_addstr(&req->quest_args, gvfs + 1);
	} else {

		const char *quest = strchr(uri_target, '?');

		if (quest) {
			strbuf_add(&req->uri_base, uri_target, (quest - uri_target));
			strbuf_trim_trailing_dir_sep(&req->uri_base);
			strbuf_addstr(&req->quest_args, quest + 1);
		} else {
			strbuf_addstr(&req->uri_base, uri_target);
			strbuf_trim_trailing_dir_sep(&req->uri_base);
		}
	}

	/*
	 * Read the set of HTTP headers into a string-list.
	 */
	while (1) {
		if (strbuf_getwholeline_fd(&h, fd, '\n') == EOF)
			goto done;
		strbuf_trim_trailing_newline(&h);

		if (!h.len)
			goto done; /* a blank line ends the header */

		string_list_append(&req->header_list, h.buf);
	}

	/*
	 * TODO If the set of HTTP headers includes things like:
	 * TODO
	 * TODO     Connection: Upgrade, HTTP2-Settings
	 * TODO     Upgrade: h2c
	 * TODO     HTTP2-Settings: AAMAAABkAARAAAAAAAIAAAAA
	 * TODO
	 * TODO then the client is asking to optionally switch to HTTP/2.
	 * TODO
	 * TODO We currently DO NOT support that (and I don't currently
	 * TODO see a need to do so (because we don't need the multiplexed
	 * TODO streams feature (because the client never asks for n packfiles
	 * TODO at the same time))).
	 * TODO
	 * TODO https://en.wikipedia.org/wiki/HTTP/1.1_Upgrade_header
	 */

	/*
	 * We do not attempt to read the <message-body>, if it exists.
	 * We let our caller read/chunk it in as appropriate.
	 */
done:
	if (trace2_is_enabled()) {
		struct string_list_item *item;
		trace2_printf("%s: %s", TR2_CAT, req->start_line.buf);
		for_each_string_list_item(item, &req->start_line_fields)
			trace2_printf("%s: Field: %s", TR2_CAT, item->string);
		trace2_printf("%s: [uri-base '%s'][gvfs '%s'][args '%s' '%s']",
			      TR2_CAT,
			      req->uri_base.buf,
			      req->gvfs_api.buf,
			      req->slash_args.buf,
			      req->quest_args.buf);
		for_each_string_list_item(item, &req->header_list)
			trace2_printf("%s: Hdrs: %s", TR2_CAT, item->string);
	}

	strbuf_release(&h);

	return WR_OK;
}

static enum worker_result dispatch(struct req *req)
{
	const char *method;
	enum worker_result wr;

	if (string_list_has_string(&mayhem_list, "close_no_write")) {
		logmayhem("close_no_write");
		return WR_MAYHEM | WR_HANGUP;
	}
	if (string_list_has_string(&mayhem_list, "http_503")) {
		logmayhem("http_503");
		return send_http_error(1, 503, "Service Unavailable", 2,
				       WR_MAYHEM | WR_HANGUP);
	}
	if (string_list_has_string(&mayhem_list, "http_429")) {
		logmayhem("http_429");
		return send_http_error(1, 429, "Too Many Requests", 2,
				       WR_MAYHEM | WR_HANGUP);
	}
	if (string_list_has_string(&mayhem_list, "http_429_1") &&
	    mayhem_child == 0) {
		logmayhem("http_429_1");
		return send_http_error(1, 429, "Too Many Requests", 2,
				       WR_MAYHEM | WR_HANGUP);
	}
	if (mayhem_try_auth(req, &wr))
		return wr;

	method = req->start_line_fields.items[0].string;

	if (!strcmp(req->gvfs_api.buf, "gvfs/objects")) {

		if (!strcmp(method, "GET"))
			return do__gvfs_objects__get(req);
		if (!strcmp(method, "POST"))
			return do__gvfs_objects__post(req);
	}

	if (!strcmp(req->gvfs_api.buf, "gvfs/config")) {

		if (!strcmp(method, "GET"))
			return do__gvfs_config__get(req);
	}

	return send_http_error(1, 501, "Not Implemented", -1,
			       WR_OK | WR_HANGUP);
}

static enum worker_result worker(void)
{
	struct req req = REQ__INIT;
	char *client_addr = getenv("REMOTE_ADDR");
	char *client_port = getenv("REMOTE_PORT");
	enum worker_result wr = WR_OK;

	if (client_addr)
		loginfo("Connection from %s:%s", client_addr, client_port);

	set_keep_alive(0);

	while (1) {
		req__release(&req);

		alarm(init_timeout ? init_timeout : timeout);
		wr = req__read(&req, 0);
		alarm(0);

		if (wr & WR_STOP_THE_MUSIC)
			break;

		wr = dispatch(&req);
		if (wr & WR_STOP_THE_MUSIC)
			break;
	}

	close(0);
	close(1);

	req__release(&req);
	return !!(wr & WR_IO_ERROR);
}

//////////////////////////////////////////////////////////////////
// This section contains the listener and child-process management
// code used by the primary instance to accept incoming connections
// and dispatch them to async child process "worker" instances.
//////////////////////////////////////////////////////////////////

static int addrcmp(const struct sockaddr_storage *s1,
		   const struct sockaddr_storage *s2)
{
	const struct sockaddr *sa1 = (const struct sockaddr*) s1;
	const struct sockaddr *sa2 = (const struct sockaddr*) s2;

	if (sa1->sa_family != sa2->sa_family)
		return sa1->sa_family - sa2->sa_family;
	if (sa1->sa_family == AF_INET)
		return memcmp(&((struct sockaddr_in *)s1)->sin_addr,
		    &((struct sockaddr_in *)s2)->sin_addr,
		    sizeof(struct in_addr));
#ifndef NO_IPV6
	if (sa1->sa_family == AF_INET6)
		return memcmp(&((struct sockaddr_in6 *)s1)->sin6_addr,
		    &((struct sockaddr_in6 *)s2)->sin6_addr,
		    sizeof(struct in6_addr));
#endif
	return 0;
}

static int max_connections = 32;

static unsigned int live_children;

static struct child {
	struct child *next;
	struct child_process cld;
	struct sockaddr_storage address;
} *firstborn;

static void add_child(struct child_process *cld, struct sockaddr *addr, socklen_t addrlen)
{
	struct child *newborn, **cradle;

	newborn = xcalloc(1, sizeof(*newborn));
	live_children++;
	memcpy(&newborn->cld, cld, sizeof(*cld));
	memcpy(&newborn->address, addr, addrlen);
	for (cradle = &firstborn; *cradle; cradle = &(*cradle)->next)
		if (!addrcmp(&(*cradle)->address, &newborn->address))
			break;
	newborn->next = *cradle;
	*cradle = newborn;
}

/*
 * This gets called if the number of connections grows
 * past "max_connections".
 *
 * We kill the newest connection from a duplicate IP.
 */
static void kill_some_child(void)
{
	const struct child *blanket, *next;

	if (!(blanket = firstborn))
		return;

	for (; (next = blanket->next); blanket = next)
		if (!addrcmp(&blanket->address, &next->address)) {
			kill(blanket->cld.pid, SIGTERM);
			break;
		}
}

static void check_dead_children(void)
{
	int status;
	pid_t pid;

	struct child **cradle, *blanket;
	for (cradle = &firstborn; (blanket = *cradle);)
		if ((pid = waitpid(blanket->cld.pid, &status, WNOHANG)) > 1) {
			const char *dead = "";
			if (status)
				dead = " (with error)";
			loginfo("[%"PRIuMAX"] Disconnected%s", (uintmax_t)pid, dead);

			/* remove the child */
			*cradle = blanket->next;
			live_children--;
			child_process_clear(&blanket->cld);
			free(blanket);
		} else
			cradle = &blanket->next;
}

static struct strvec cld_argv = STRVEC_INIT;
static void handle(int incoming, struct sockaddr *addr, socklen_t addrlen)
{
	struct child_process cld = CHILD_PROCESS_INIT;

	if (max_connections >= 0 && live_children >= (unsigned int)max_connections) {
		kill_some_child();
		sleep(1);  /* give it some time to die */
		check_dead_children();
		if (live_children >= (unsigned int)max_connections) {
			close(incoming);
			logerror("Too many children, dropping connection");
			return;
		}
	}

	if (addr->sa_family == AF_INET) {
		char buf[128] = "";
		struct sockaddr_in *sin_addr = (void *) addr;
		inet_ntop(addr->sa_family, &sin_addr->sin_addr, buf, sizeof(buf));
		strvec_pushf(&cld.env, "REMOTE_ADDR=%s", buf);
		strvec_pushf(&cld.env, "REMOTE_PORT=%d",
				 ntohs(sin_addr->sin_port));
#ifndef NO_IPV6
	} else if (addr->sa_family == AF_INET6) {
		char buf[128] = "";
		struct sockaddr_in6 *sin6_addr = (void *) addr;
		inet_ntop(AF_INET6, &sin6_addr->sin6_addr, buf, sizeof(buf));
		strvec_pushf(&cld.env, "REMOTE_ADDR=[%s]", buf);
		strvec_pushf(&cld.env, "REMOTE_PORT=%d",
				 ntohs(sin6_addr->sin6_port));
#endif
	}

	if (mayhem_list.nr) {
		strvec_pushf(&cld.env, "MAYHEM_CHILD=%d",
				 mayhem_child++);
	}

	strvec_pushv(&cld.args, cld_argv.v);
	cld.in = incoming;
	cld.out = dup(incoming);

	if (cld.out < 0)
		logerror("could not dup() `incoming`");
	else if (start_command(&cld))
		logerror("unable to fork");
	else
		add_child(&cld, addr, addrlen);
}

static void child_handler(int signo UNUSED)
{
	/*
	 * Otherwise empty handler because systemcalls will get interrupted
	 * upon signal receipt
	 * SysV needs the handler to be rearmed
	 */
	signal(SIGCHLD, child_handler);
}

static int set_reuse_addr(int sockfd)
{
	int on = 1;

	if (!reuseaddr)
		return 0;
	return setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,
			  &on, sizeof(on));
}

struct socketlist {
	int *list;
	size_t nr;
	size_t alloc;
};

static const char *ip2str(int family, struct sockaddr *sin, socklen_t len)
{
#ifdef NO_IPV6
	static char ip[INET_ADDRSTRLEN];
#else
	static char ip[INET6_ADDRSTRLEN];
#endif

	switch (family) {
#ifndef NO_IPV6
	case AF_INET6:
		inet_ntop(family, &((struct sockaddr_in6*)sin)->sin6_addr, ip, len);
		break;
#endif
	case AF_INET:
		inet_ntop(family, &((struct sockaddr_in*)sin)->sin_addr, ip, len);
		break;
	default:
		xsnprintf(ip, sizeof(ip), "<unknown>");
	}
	return ip;
}

#ifndef NO_IPV6

static int setup_named_sock(const char *listen_addr, int listen_port, struct socketlist *socklist)
{
	int socknum = 0;
	char pbuf[NI_MAXSERV];
	struct addrinfo hints, *ai0, *ai;
	int gai;
	long flags;

	xsnprintf(pbuf, sizeof(pbuf), "%d", listen_port);
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;

	gai = getaddrinfo(listen_addr, pbuf, &hints, &ai0);
	if (gai) {
		logerror("getaddrinfo() for %s failed: %s", listen_addr, gai_strerror(gai));
		return 0;
	}

	for (ai = ai0; ai; ai = ai->ai_next) {
		int sockfd;

		sockfd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
		if (sockfd < 0)
			continue;
		if (sockfd >= FD_SETSIZE) {
			logerror("Socket descriptor too large");
			close(sockfd);
			continue;
		}

#ifdef IPV6_V6ONLY
		if (ai->ai_family == AF_INET6) {
			int on = 1;
			setsockopt(sockfd, IPPROTO_IPV6, IPV6_V6ONLY,
				   &on, sizeof(on));
			/* Note: error is not fatal */
		}
#endif

		if (set_reuse_addr(sockfd)) {
			logerror("Could not set SO_REUSEADDR: %s", strerror(errno));
			close(sockfd);
			continue;
		}

		set_keep_alive(sockfd);

		if (bind(sockfd, ai->ai_addr, ai->ai_addrlen) < 0) {
			logerror("Could not bind to %s: %s",
				 ip2str(ai->ai_family, ai->ai_addr, ai->ai_addrlen),
				 strerror(errno));
			close(sockfd);
			continue;	/* not fatal */
		}
		if (listen(sockfd, 5) < 0) {
			logerror("Could not listen to %s: %s",
				 ip2str(ai->ai_family, ai->ai_addr, ai->ai_addrlen),
				 strerror(errno));
			close(sockfd);
			continue;	/* not fatal */
		}

		flags = fcntl(sockfd, F_GETFD, 0);
		if (flags >= 0)
			fcntl(sockfd, F_SETFD, flags | FD_CLOEXEC);

		ALLOC_GROW(socklist->list, socklist->nr + 1, socklist->alloc);
		socklist->list[socklist->nr++] = sockfd;
		socknum++;
	}

	freeaddrinfo(ai0);

	return socknum;
}

#else /* NO_IPV6 */

static int setup_named_sock(char *listen_addr, int listen_port, struct socketlist *socklist)
{
	struct sockaddr_in sin;
	int sockfd;
	long flags;

	memset(&sin, 0, sizeof sin);
	sin.sin_family = AF_INET;
	sin.sin_port = htons(listen_port);

	if (listen_addr) {
		/* Well, host better be an IP address here. */
		if (inet_pton(AF_INET, listen_addr, &sin.sin_addr.s_addr) <= 0)
			return 0;
	} else {
		sin.sin_addr.s_addr = htonl(INADDR_ANY);
	}

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0)
		return 0;

	if (set_reuse_addr(sockfd)) {
		logerror("Could not set SO_REUSEADDR: %s", strerror(errno));
		close(sockfd);
		return 0;
	}

	set_keep_alive(sockfd);

	if ( bind(sockfd, (struct sockaddr *)&sin, sizeof sin) < 0 ) {
		logerror("Could not bind to %s: %s",
			 ip2str(AF_INET, (struct sockaddr *)&sin, sizeof(sin)),
			 strerror(errno));
		close(sockfd);
		return 0;
	}

	if (listen(sockfd, 5) < 0) {
		logerror("Could not listen to %s: %s",
			 ip2str(AF_INET, (struct sockaddr *)&sin, sizeof(sin)),
			 strerror(errno));
		close(sockfd);
		return 0;
	}

	flags = fcntl(sockfd, F_GETFD, 0);
	if (flags >= 0)
		fcntl(sockfd, F_SETFD, flags | FD_CLOEXEC);

	ALLOC_GROW(socklist->list, socklist->nr + 1, socklist->alloc);
	socklist->list[socklist->nr++] = sockfd;
	return 1;
}

#endif

static void socksetup(struct string_list *listen_addr, int listen_port, struct socketlist *socklist)
{
	if (!listen_addr->nr)
		setup_named_sock("127.0.0.1", listen_port, socklist);
	else {
		size_t i;
		int socknum;
		for (i = 0; i < listen_addr->nr; i++) {
			socknum = setup_named_sock(listen_addr->items[i].string,
						   listen_port, socklist);

			if (socknum == 0)
				logerror("unable to allocate any listen sockets for host %s on port %u",
					 listen_addr->items[i].string, listen_port);
		}
	}
}

static int service_loop(struct socketlist *socklist)
{
	struct pollfd *pfd;
	size_t i;

	CALLOC_ARRAY(pfd, socklist->nr);

	for (i = 0; i < socklist->nr; i++) {
		pfd[i].fd = socklist->list[i];
		pfd[i].events = POLLIN;
	}

	signal(SIGCHLD, child_handler);

	for (;;) {
		size_t i;
		int nr_ready;
		int timeout = (pid_file ? 100 : -1);

		check_dead_children();

		nr_ready = poll(pfd, socklist->nr, timeout);
		if (nr_ready < 0) {
			if (errno != EINTR) {
				logerror("Poll failed, resuming: %s",
				      strerror(errno));
				sleep(1);
			}
			continue;
		}
		else if (nr_ready == 0) {
			/*
			 * If we have a pid_file, then we watch it.
			 * If someone deletes it, we shutdown the service.
			 * The shell scripts in the test suite will use this.
			 */
			if (!pid_file || file_exists(pid_file))
				continue;
			goto shutdown;
		}

		for (i = 0; i < socklist->nr; i++) {
			if (pfd[i].revents & POLLIN) {
				union {
					struct sockaddr sa;
					struct sockaddr_in sai;
#ifndef NO_IPV6
					struct sockaddr_in6 sai6;
#endif
				} ss;
				socklen_t sslen = sizeof(ss);
				int incoming = accept(pfd[i].fd, &ss.sa, &sslen);
				if (incoming < 0) {
					switch (errno) {
					case EAGAIN:
					case EINTR:
					case ECONNABORTED:
						continue;
					default:
						die_errno("accept returned");
					}
				}
				handle(incoming, &ss.sa, sslen);
			}
		}
	}

shutdown:
	loginfo("Starting graceful shutdown (pid-file gone)");
	for (i = 0; i < socklist->nr; i++)
		close(socklist->list[i]);
	free(socklist->list);
	free(pfd);

	return 0;
}

static int serve(struct string_list *listen_addr, int listen_port)
{
	struct socketlist socklist = { NULL, 0, 0 };

	socksetup(listen_addr, listen_port, &socklist);
	if (socklist.nr == 0)
		die("unable to allocate any listen sockets on port %u",
		    listen_port);

	loginfo("Ready to rumble");

	/*
	 * Wait to create the pid-file until we've setup the sockets
	 * and are open for business.
	 */
	if (pid_file)
		write_file(pid_file, "%"PRIuMAX, (uintmax_t) getpid());

	return service_loop(&socklist);
}

//////////////////////////////////////////////////////////////////
// This section is executed by both the primary instance and all
// worker instances.  So, yes, each child-process re-parses the
// command line argument and re-discovers how it should behave.
//////////////////////////////////////////////////////////////////

int cmd_main(int argc, const char **argv)
{
	int listen_port = 0;
	static struct string_list listen_addr = STRING_LIST_INIT_NODUP;
	int worker_mode = 0;
	int i;

	trace2_cmd_name("test-gvfs-protocol");
	setup_git_directory_gently(NULL);

	for (i = 1; i < argc; i++) {
		const char *arg = argv[i];
		const char *v;

		if (skip_prefix(arg, "--listen=", &v)) {
			string_list_append_nodup(&listen_addr, xstrdup_tolower(v));
			continue;
		}
		if (skip_prefix(arg, "--port=", &v)) {
			char *end;
			unsigned long n;
			n = strtoul(v, &end, 0);
			if (*v && !*end) {
				listen_port = n;
				continue;
			}
		}
		if (!strcmp(arg, "--worker")) {
			worker_mode = 1;
			trace2_cmd_mode("worker");
			continue;
		}
		if (!strcmp(arg, "--verbose")) {
			verbose = 1;
			continue;
		}
		if (skip_prefix(arg, "--timeout=", &v)) {
			timeout = atoi(v);
			continue;
		}
		if (skip_prefix(arg, "--init-timeout=", &v)) {
			init_timeout = atoi(v);
			continue;
		}
		if (skip_prefix(arg, "--max-connections=", &v)) {
			max_connections = atoi(v);
			if (max_connections < 0)
				max_connections = 0; /* unlimited */
			continue;
		}
		if (!strcmp(arg, "--reuseaddr")) {
			reuseaddr = 1;
			continue;
		}
		if (skip_prefix(arg, "--pid-file=", &v)) {
			pid_file = v;
			continue;
		}
		if (skip_prefix(arg, "--mayhem=", &v)) {
			string_list_append(&mayhem_list, v);
			continue;
		}

		usage(test_gvfs_protocol_usage);
	}

	/* avoid splitting a message in the middle */
	setvbuf(stderr, NULL, _IOFBF, 4096);

	if (listen_port == 0)
		listen_port = DEFAULT_GIT_PORT;

	/*
	 * If no --listen=<addr> args are given, the setup_named_sock()
	 * code will use receive a NULL address and set INADDR_ANY.
	 * This exposes both internal and external interfaces on the
	 * port.
	 *
	 * Disallow that and default to the internal-use-only loopback
	 * address.
	 */
	if (!listen_addr.nr)
		string_list_append(&listen_addr, "127.0.0.1");

	/*
	 * worker_mode is set in our own child process instances
	 * (that are bound to a connected socket from a client).
	 */
	if (worker_mode) {
		if (mayhem_list.nr) {
			const char *string = getenv("MAYHEM_CHILD");
			if (string && *string)
				mayhem_child = atoi(string);
		}

		build_gvfs_config_json(&jw_config, &listen_addr, listen_port);

		return worker();
	}

	/*
	 * `cld_argv` is a bit of a clever hack.  The top-level instance
	 * of test-gvfs-protocol.exe does the normal bind/listen/accept
	 * stuff.  For each incoming socket, the top-level process spawns
	 * a child instance of test-gvfs-protocol.exe *WITH* the additional
	 * `--worker` argument.  This causes the child to set `worker_mode`
	 * and immediately call `worker()` using the connected socket (and
	 * without the usual need for fork() or threads).
	 *
	 * The magic here is made possible because `cld_argv` is static
	 * and handle() (called by service_loop()) knows about it.
	 */
	strvec_push(&cld_argv, argv[0]);
	strvec_push(&cld_argv, "--worker");
	for (i = 1; i < argc; ++i)
		strvec_push(&cld_argv, argv[i]);

	/*
	 * Setup primary instance to listen for connections.
	 */
	return serve(&listen_addr, listen_port);
}
