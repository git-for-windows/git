// TODO Write a man page.  Here are some notes for dogfooding.
// TODO
//
// Usage: git gvfs-helper [<main_options>] <sub-command> [<sub-command-options>]
//
// <main_options>:
//
//     --remote=<remote-name>         // defaults to "origin"
//
//     --fallback                     // boolean. defaults to off
//
//            When a fetch from the cache-server fails, automatically
//            fallback to the main Git server.  This option has no effect
//            if no cache-server is defined.
//
//     --cache-server=<use>  // defaults to "verify"
//
//            verify   := lookup the set of defined cache-servers using
//                        "gvfs/config" and confirm that the selected
//                        cache-server is well-known.  Silently disable the
//                        cache-server if not.  (See security notes later.)
//
//            error    := verify cache-server and abort if not well-known.
//
//            trust    := do not verify cache-server.  just use it.
//
//            disable  := disable the cache-server and always use the main
//                        Git server.
//
//     --shared-cache=<odb-directory-pathname>
//
//            A relative or absolute pathname to the ODB directory to store
//            fetched objects.
//
//            If this option is not specified, we default to the value
//            in the "gvfs.sharedcache" config setting and then to the
//            local ".git/objects" directory.
//
// <sub-command>:
//
//     config
//
//            Fetch the "gvfs/config" string from the main Git server.
//            (The cache-server setting is ignored because cache-servers
//            do not support this REST API.)
//
//     get
//
//            Fetch 1 or more objects.  If a cache-server is configured,
//            try it first.  Optionally fallback to the main Git server.
//
//            The set of objects is given on stdin and is assumed to be
//            a list of <oid>, one per line.
//
//            <get-options>:
//
//                 --block-size=<n>      // defaults to "4000"
//
//                       Request objects from server in batches of at
//                       most n objects (not bytes).
//
//                 --depth=<depth>       // defaults to "1"
//
//     server
//
//            Interactive/sub-process mode.  Listen for a series of commands
//            and data on stdin and return results on stdout.  This command
//            uses pkt-line format [1] and implements the long-running process
//            protocol [2] to communicate with the foreground/parent process.
//
//            <server-options>:
//
//                 --block-size=<n>      // defaults to "4000"
//
//                       Request objects from server in batches of at
//                       most n objects (not bytes).
//
//                 --depth=<depth>       // defaults to "1"
//
//            Interactive verb: get
//
//                 Fetch 1 or more objects.  If a cache-server is configured,
//                 try it first.  Optionally fallback to the main Git server.
//
//                 Create 1 or more loose objects and/or packfiles in the
//                 shared-cache ODB.  (The pathname of the selected ODB is
//                 reported at the beginning of the response; this should
//                 match the pathname given on the command line).
//
//                 git> get
//                 git> <oid>
//                 git> <oid>
//                 git> ...
//                 git> <oid>
//                 git> 0000
//
//                 git< odb <directory>
//                 git< loose <oid> | packfile <filename.pack>
//                 git< loose <oid> | packfile <filename.pack>
//                 gid< ...
//                 git< loose <oid> | packfile <filename.pack>
//                 git< ok | partial | error <message>
//                 git< 0000
//
//            [1] Documentation/technical/protocol-common.txt
//            [2] Documentation/technical/long-running-process-protocol.txt
//            [3] See GIT_TRACE_PACKET
//
// Example:
//
// $ git -c core.virtualizeobjects=false -c core.usegvfshelper=false
//           rev-list --objects --no-walk --missing=print HEAD
//     | grep "^?"
//     | sed 's/^?//'
//     | git gvfs-helper get-missing
//
// Note: In this example, we need to turn off "core.virtualizeobjects" and
//       "core.usegvfshelper" when building the list of objects.  This prevents
//       rev-list (in oid_object_info_extended() from automatically fetching
//       them with read-object-hook or "gvfs-helper server" sub-process (and
//       defeating the whole purpose of this example).
//
//////////////////////////////////////////////////////////////////

#define USE_THE_REPOSITORY_VARIABLE
#include "git-compat-util.h"
#include "git-curl-compat.h"
#include "environment.h"
#include "hex.h"
#include "setup.h"
#include "config.h"
#include "remote.h"
#include "connect.h"
#include "strbuf.h"
#include "walker.h"
#include "http.h"
#include "exec-cmd.h"
#include "run-command.h"
#include "pkt-line.h"
#include "string-list.h"
#include "sideband.h"
#include "strvec.h"
#include "credential.h"
#include "oid-array.h"
#include "send-pack.h"
#include "protocol.h"
#include "quote.h"
#include "transport.h"
#include "parse-options.h"
#include "odb.h"
#include "object-file.h"
#include "json-writer.h"
#include "tempfile.h"
#include "oidset.h"
#include "dir.h"
#include "url.h"
#include "abspath.h"
#include "progress.h"
#include "trace2.h"

static const char * const main_usage[] = {
	N_("git gvfs-helper [<main_options>] config      [<options>]"),
	N_("git gvfs-helper [<main_options>] get         [<options>]"),
	N_("git gvfs-helper [<main_options>] server      [<options>]"),
	NULL
};

static const char *const get_usage[] = {
	N_("git gvfs-helper [<main_options>] get [<options>]"),
	NULL
};

static const char *const server_usage[] = {
	N_("git gvfs-helper [<main_options>] server [<options>]"),
	NULL
};

#define GH__DEFAULT_BLOCK_SIZE 4000

/*
 * Our exit-codes.
 */
enum gh__error_code {
	GH__ERROR_CODE__USAGE = -1, /* will be mapped to usage() */
	GH__ERROR_CODE__OK = 0,
	GH__ERROR_CODE__ERROR = 1, /* unspecified */
//	GH__ERROR_CODE__CACHE_SERVER_NOT_FOUND = 2,
	GH__ERROR_CODE__CURL_ERROR = 3,
	GH__ERROR_CODE__HTTP_401 = 4,
	GH__ERROR_CODE__HTTP_404 = 5,
	GH__ERROR_CODE__HTTP_UNEXPECTED_CODE = 6,
	GH__ERROR_CODE__UNEXPECTED_CONTENT_TYPE = 7,
	GH__ERROR_CODE__COULD_NOT_CREATE_TEMPFILE = 8,
	GH__ERROR_CODE__COULD_NOT_INSTALL_LOOSE = 9,
	GH__ERROR_CODE__COULD_NOT_INSTALL_PACKFILE = 10,
	GH__ERROR_CODE__SUBPROCESS_SYNTAX = 11,
};

enum gh__cache_server_mode {
	/* verify URL. disable if unknown. */
	GH__CACHE_SERVER_MODE__VERIFY_DISABLE = 0,
	/* verify URL. error if unknown. */
	GH__CACHE_SERVER_MODE__VERIFY_ERROR,
	/* disable the cache-server, if defined */
	GH__CACHE_SERVER_MODE__DISABLE,
	/* trust any cache-server */
	GH__CACHE_SERVER_MODE__TRUST_WITHOUT_VERIFY,
};

/*
 * The set of command line, config, and environment variables
 * that we use as input to decide how we should operate.
 */
static struct gh__cmd_opts {
	const char *remote_name;

	int try_fallback; /* to git server if cache-server fails */
	int show_progress;

	int depth;
	unsigned int block_size;

	enum gh__cache_server_mode cache_server_mode;
} gh__cmd_opts;

/*
 * The chosen global state derrived from the inputs in gh__cmd_opts.
 */
static struct gh__global {
	struct remote *remote;

	struct credential main_creds;
	struct credential cache_creds;

	const char *main_url;
	const char *cache_server_url;

	struct strbuf buf_odb_path;

	int http_is_initialized;
	int cache_server_is_initialized; /* did sub-command look for one */
	int main_creds_need_approval; /* try to only approve them once */

} gh__global;

/*
 * Stolen from http.c
 */
static CURLcode gh__curlinfo_strbuf(CURL *curl, CURLINFO info, struct strbuf *buf)
{
	char *ptr;
	CURLcode ret;

	strbuf_reset(buf);
	ret = curl_easy_getinfo(curl, info, &ptr);
	if (!ret && ptr)
		strbuf_addstr(buf, ptr);
	return ret;
}

enum gh__progress_state {
	GH__PROGRESS_STATE__START = 0,
	GH__PROGRESS_STATE__PHASE1,
	GH__PROGRESS_STATE__PHASE2,
	GH__PROGRESS_STATE__PHASE3,
};

/*
 * Parameters to drive an HTTP request (with any necessary retries).
 */
struct gh__request_params {
	int b_is_post;            /* POST=1 or GET=0 */
	int b_write_to_file;      /* write to file=1 or strbuf=0 */
	int b_no_cache_server;    /* force main server only */

	unsigned long object_count; /* number of objects being fetched */

	const struct strbuf *post_payload; /* POST body to send */

	struct curl_slist *headers; /* additional http headers to send */
	struct tempfile *tempfile; /* for response content when file */
	struct strbuf *buffer;     /* for response content when strbuf */
	struct strbuf label;       /* for trace2 regions */

	struct strbuf loose_path;

	/*
	 * Note that I am putting all of the progress-related instance data
	 * inside the request-params in the hope that we can eventually
	 * do multi-threaded/concurrent HTTP requests when chunking
	 * large requests.  However, the underlying "struct progress" API
	 * is not thread safe (that is, it doesn't allow concurrent progress
	 * reports (since that might require multiple lines on the screen
	 * or something)).
	 */
	enum gh__progress_state progress_state;
	struct strbuf progress_base_phase2_msg;
	struct strbuf progress_base_phase3_msg;

	/*
	 * The buffer for the formatted progress message is shared by the
	 * "struct progress" API and must remain valid for the duration of
	 * the start_progress..stop_progress lifespan.
	 */
	struct strbuf progress_msg;
	struct progress *progress;
};

#define GH__REQUEST_PARAMS_INIT { \
	.b_is_post = 0, \
	.b_write_to_file = 0, \
	.b_no_cache_server = 0, \
	.object_count = 0, \
	.post_payload = NULL, \
	.headers = NULL, \
	.tempfile = NULL, \
	.buffer = NULL, \
	.label = STRBUF_INIT, \
	.loose_path = STRBUF_INIT, \
	.progress_state = GH__PROGRESS_STATE__START, \
	.progress_base_phase2_msg = STRBUF_INIT, \
	.progress_base_phase3_msg = STRBUF_INIT, \
	.progress_msg = STRBUF_INIT, \
	.progress = NULL, \
	}

static void gh__request_params__release(struct gh__request_params *params)
{
	if (!params)
		return;

	params->post_payload = NULL; /* we do not own this */

	curl_slist_free_all(params->headers);
	params->headers = NULL;

	delete_tempfile(&params->tempfile);

	params->buffer = NULL; /* we do not own this */

	strbuf_release(&params->label);
	strbuf_release(&params->loose_path);

	strbuf_release(&params->progress_base_phase2_msg);
	strbuf_release(&params->progress_base_phase3_msg);
	strbuf_release(&params->progress_msg);

	stop_progress(&params->progress);
	params->progress = NULL;
}

/*
 * Bucket to describe the results of an HTTP requests (may be
 * overwritten during retries so that it describes the final attempt).
 */
struct gh__response_status {
	struct strbuf error_message;
	struct strbuf content_type;
	long response_code; /* http response code */
	CURLcode curl_code;
	enum gh__error_code ec;
	intmax_t bytes_received;
};

#define GH__RESPONSE_STATUS_INIT { \
	.error_message = STRBUF_INIT, \
	.content_type = STRBUF_INIT, \
	.response_code = 0, \
	.curl_code = CURLE_OK, \
	.ec = GH__ERROR_CODE__OK, \
	.bytes_received = 0, \
	}

static void gh__response_status__zero(struct gh__response_status *s)
{
	strbuf_setlen(&s->error_message, 0);
	strbuf_setlen(&s->content_type, 0);
	s->response_code = 0;
	s->curl_code = CURLE_OK;
	s->ec = GH__ERROR_CODE__OK;
	s->bytes_received = 0;
}

/*
 * Create a single normalized 'ec' error-code from the status we
 * received from the HTTP request.  Map a few of the expected HTTP
 * status code to 'ec', but don't get too crazy here.
 */
static void gh__response_status__set_from_slot(
	struct gh__request_params *params,
	struct gh__response_status *status,
	const struct active_request_slot *slot)
{
	status->curl_code = slot->results->curl_result;
	gh__curlinfo_strbuf(slot->curl, CURLINFO_CONTENT_TYPE,
			    &status->content_type);
	curl_easy_getinfo(slot->curl, CURLINFO_RESPONSE_CODE,
			  &status->response_code);

	strbuf_setlen(&status->error_message, 0);

	if (status->response_code == 200)
		status->ec = GH__ERROR_CODE__OK;

	else if (status->response_code == 401) {
		strbuf_addstr(&status->error_message, "401 Not Authorized");
		status->ec = GH__ERROR_CODE__HTTP_401;

	} else if (status->response_code == 404) {
		strbuf_addstr(&status->error_message, "404 Not Found");
		status->ec = GH__ERROR_CODE__HTTP_404;

	} else if (status->curl_code != CURLE_OK) {
		strbuf_addf(&status->error_message, "%s (curl)",
			    curl_easy_strerror(status->curl_code));
		status->ec = GH__ERROR_CODE__CURL_ERROR;
	} else {
		strbuf_addf(&status->error_message, "HTTP %ld Unexpected",
			    status->response_code);
		status->ec = GH__ERROR_CODE__HTTP_UNEXPECTED_CODE;
	}

	if (status->ec != GH__ERROR_CODE__OK)
		status->bytes_received = 0;
	else if (params->b_write_to_file)
		status->bytes_received = (intmax_t)ftell(params->tempfile->fp);
	else
		status->bytes_received = (intmax_t)params->buffer->len;
}

static void gh__response_status__release(struct gh__response_status *status)
{
	if (!status)
		return;
	strbuf_release(&status->error_message);
	strbuf_release(&status->content_type);
}

/*
 * The cache-server sends a somewhat bogus 400 instead of
 * the normal 401 when AUTH is required.  Fixup the status
 * to hide that.
 */
static void fixup_cache_server_400_to_401(struct gh__response_status *status)
{
	if (status->response_code != 400)
		return;

	/*
	 * TODO Technically, the cache-server could send a 400
	 * TODO for many reasons, not just for their bogus
	 * TODO pseudo-401, but we're going to assume it is a
	 * TODO 401 for now.  We should confirm the expected
	 * TODO error message in the response-body.
	 */
	status->response_code = 401;
}

static int gh__curl_progress_cb(void *clientp,
				curl_off_t dltotal, curl_off_t dlnow,
				curl_off_t ultotal, curl_off_t ulnow)
{
	struct gh__request_params *params = clientp;

	/*
	 * From what I can tell, CURL progress arrives in 3 phases.
	 *
	 * [1] An initial connection setup phase where we get [0,0] [0,0].
	 * [2] An upload phase where we start sending the request headers
	 *     and body. ulnow will be > 0.  ultotal may or may not be 0.
	 * [3] A download phase where we start receiving the response
	 *     headers and payload body.  dlnow will be > 0. dltotal may
	 *     or may not be 0.
	 *
	 * If we pass zero for the total to the "struct progress" API, we
	 * get simple numbers rather than percentages.  So our progress
	 * output format may vary depending.
	 *
	 * It is unclear if CURL will give us a final callback after
	 * everything is finished, so we leave the progress handle open
	 * and let the caller issue the final stop_progress().
	 *
	 * There is a bit of a mismatch between the CURL API and the
	 * "struct progress" API.  The latter requires us to set the
	 * progress message when we call one of the start_progress
	 * methods.  We cannot change the progress message while we are
	 * showing progress state.  And we cannot change the denominator
	 * (total) after we start.  CURL may or may not give us the total
	 * sizes for each phase.
	 *
	 * Also be advised that the "struct progress" API eats messages
	 * so that the screen is only updated every second or so.  And
	 * may not print anything if the start..stop happen in less then
	 * 2 seconds.  Whereas CURL calls this callback very frequently.
	 * The net-net is that we may not actually see this progress
	 * message for small/fast HTTP requests.
	 */

	switch (params->progress_state) {
	case GH__PROGRESS_STATE__START: /* first callback */
		if (dlnow == 0 && ulnow == 0)
			goto enter_phase_1;

		if (ulnow)
			goto enter_phase_2;
		else
			goto enter_phase_3;

	case GH__PROGRESS_STATE__PHASE1:
		if (dlnow == 0 && ulnow == 0)
			return 0;

		if (ulnow)
			goto enter_phase_2;
		else
			goto enter_phase_3;

	case GH__PROGRESS_STATE__PHASE2:
		display_progress(params->progress, ulnow);
		if (dlnow == 0)
			return 0;

		stop_progress(&params->progress);
		goto enter_phase_3;

	case GH__PROGRESS_STATE__PHASE3:
		display_progress(params->progress, dlnow);
		return 0;

	default:
		return 0;
	}

enter_phase_1:
	/*
	 * Don't bother to create a progress handle during phase [1].
	 * Because we get [0,0,0,0], we don't have any data to report
	 * and would just have to synthesize some type of progress.
	 * From my testing, phase [1] is fairly quick (probably just
	 * the SSL handshake), so the "struct progress" API will most
	 * likely completely eat any messages that we did produce.
	 */
	params->progress_state = GH__PROGRESS_STATE__PHASE1;
	return 0;

enter_phase_2:
	strbuf_setlen(&params->progress_msg, 0);
	if (params->progress_base_phase2_msg.len) {
		strbuf_addf(&params->progress_msg, "%s (bytes sent)",
			    params->progress_base_phase2_msg.buf);
		params->progress = start_progress(the_repository, params->progress_msg.buf, ultotal);
		display_progress(params->progress, ulnow);
	}
	params->progress_state = GH__PROGRESS_STATE__PHASE2;
	return 0;

enter_phase_3:
	strbuf_setlen(&params->progress_msg, 0);
	if (params->progress_base_phase3_msg.len) {
		strbuf_addf(&params->progress_msg, "%s (bytes received)",
			    params->progress_base_phase3_msg.buf);
		params->progress = start_progress(the_repository, params->progress_msg.buf, dltotal);
		display_progress(params->progress, dlnow);
	}
	params->progress_state = GH__PROGRESS_STATE__PHASE3;
	return 0;
}

/*
 * Run the request without using "run_one_slot()" because we
 * don't want the post-request normalization, error handling,
 * and auto-reauth handling in http.c.
 */
static void gh__run_one_slot(struct active_request_slot *slot,
			     struct gh__request_params *params,
			     struct gh__response_status *status)
{
	trace2_region_enter("gvfs-helper", params->label.buf, NULL);

	if (!start_active_slot(slot)) {
		status->curl_code = CURLE_FAILED_INIT; /* a bit of a lie */
		strbuf_addstr(&status->error_message,
			      "failed to start HTTP request");
	} else {
		run_active_slot(slot);
		if (params->b_write_to_file)
			fflush(params->tempfile->fp);

		gh__response_status__set_from_slot(params, status, slot);

		if (status->ec == GH__ERROR_CODE__OK) {
			int old_len = params->label.len;

			strbuf_addstr(&params->label, "/nr_objects");
			trace2_data_intmax("gvfs-helper", NULL,
					   params->label.buf,
					   params->object_count);
			strbuf_setlen(&params->label, old_len);

			strbuf_addstr(&params->label, "/nr_bytes");
			trace2_data_intmax("gvfs-helper", NULL,
					   params->label.buf,
					   status->bytes_received);
			strbuf_setlen(&params->label, old_len);
		}
	}

	if (params->progress)
		stop_progress(&params->progress);

	trace2_region_leave("gvfs-helper", params->label.buf, NULL);
}

static int option_parse_cache_server_mode(const struct option *opt,
					  const char *arg, int unset)
{
	if (unset) /* should not happen */
		return error(_("missing value for switch '%s'"),
			     opt->long_name);

	else if (!strcmp(arg, "verify"))
		gh__cmd_opts.cache_server_mode =
			GH__CACHE_SERVER_MODE__VERIFY_DISABLE;

	else if (!strcmp(arg, "error"))
		gh__cmd_opts.cache_server_mode =
			GH__CACHE_SERVER_MODE__VERIFY_ERROR;

	else if (!strcmp(arg, "disable"))
		gh__cmd_opts.cache_server_mode =
			GH__CACHE_SERVER_MODE__DISABLE;

	else if (!strcmp(arg, "trust"))
		gh__cmd_opts.cache_server_mode =
			GH__CACHE_SERVER_MODE__TRUST_WITHOUT_VERIFY;

	else
		return error(_("invalid value for switch '%s'"),
			     opt->long_name);

	return 0;
}

/*
 * Let command line args override "gvfs.sharedcache" config setting
 * and override the value set by git_default_config().
 *
 * The command line is parsed *AFTER* the config is loaded, so
 * prepared_alt_odb() has already been called any default or inherited
 * shared-cache has already been set.
 *
 * We have a chance to override it here.
 */
static int option_parse_shared_cache_directory(const struct option *opt,
					       const char *arg, int unset)
{
	struct strbuf buf_arg = STRBUF_INIT;

	if (unset) /* should not happen */
		return error(_("missing value for switch '%s'"),
			     opt->long_name);

	strbuf_addstr(&buf_arg, arg);
	if (strbuf_normalize_path(&buf_arg) < 0) {
		/*
		 * Pretend command line wasn't given.  Use whatever
		 * settings we already have from the config.
		 */
		strbuf_release(&buf_arg);
		return 0;
	}
	strbuf_trim_trailing_dir_sep(&buf_arg);

	if (!strbuf_cmp(&buf_arg, &gvfs_shared_cache_pathname)) {
		/*
		 * The command line argument matches what we got from
		 * the config, so we're already setup correctly. (And
		 * we have already verified that the directory exists
		 * on disk.)
		 */
		strbuf_release(&buf_arg);
		return 0;
	}

	else if (!gvfs_shared_cache_pathname.len) {
		/*
		 * A shared-cache was requested and we did not inherit one.
		 * Try it, but let alt_odb_usable() secretly disable it if
		 * it cannot create the directory on disk.
		 */
		strbuf_addbuf(&gvfs_shared_cache_pathname, &buf_arg);

		odb_add_to_alternates_memory(the_repository->objects, buf_arg.buf);

		strbuf_release(&buf_arg);
		return 0;
	}

	else {
		/*
		 * The requested shared-cache is different from the one
		 * we inherited.  Replace the inherited value with this
		 * one, but smartly fallback if necessary.
		 */
		struct strbuf buf_prev = STRBUF_INIT;

		strbuf_addbuf(&buf_prev, &gvfs_shared_cache_pathname);

		strbuf_setlen(&gvfs_shared_cache_pathname, 0);
		strbuf_addbuf(&gvfs_shared_cache_pathname, &buf_arg);

		odb_add_to_alternates_memory(the_repository->objects, buf_arg.buf);

		/*
		 * alt_odb_usable() releases gvfs_shared_cache_pathname
		 * if it cannot create the directory on disk, so fallback
		 * to the previous choice when it fails.
		 */
		if (!gvfs_shared_cache_pathname.len)
			strbuf_addbuf(&gvfs_shared_cache_pathname,
				      &buf_prev);

		strbuf_release(&buf_arg);
		strbuf_release(&buf_prev);
		return 0;
	}
}

/*
 * Lookup the URL for this remote (defaults to 'origin').
 */
static void lookup_main_url(void)
{
	/*
	 * Both VFS and Scalar only work with 'origin', so we expect this.
	 * The command line arg is mainly for debugging.
	 */
	if (!gh__cmd_opts.remote_name || !*gh__cmd_opts.remote_name)
		gh__cmd_opts.remote_name = "origin";

	gh__global.remote = remote_get(gh__cmd_opts.remote_name);
	if (!gh__global.remote->url.v[0] || !*gh__global.remote->url.v[0])
		die("unknown remote '%s'", gh__cmd_opts.remote_name);

	/*
	 * Strip out any in-line auth in the origin server URL so that
	 * we can control which creds we fetch.
	 *
	 * Azure DevOps has been known to suggest https URLS of the
	 * form "https://<account>@dev.azure.com/<account>/<path>".
	 *
	 * Break that so that we can force the use of a PAT.
	 */
	gh__global.main_url = transport_anonymize_url(gh__global.remote->url.v[0]);

	trace2_data_string("gvfs-helper", NULL, "remote/url", gh__global.main_url);
}

static void do__gvfs_config(struct gh__response_status *status,
			    struct strbuf *config_data);

/*
 * Find the URL of the cache-server, if we have one.
 *
 * This routine is called by the initialization code and is allowed
 * to call die() rather than returning an 'ec'.
 */
static void select_cache_server(void)
{
	struct gh__response_status status = GH__RESPONSE_STATUS_INIT;
	struct strbuf config_data = STRBUF_INIT;
	const char *match = NULL;

	/*
	 * This only indicates that the sub-command actually called
	 * this routine.  We rely on gh__global.cache_server_url to tell
	 * us if we actually have a cache-server configured.
	 */
	gh__global.cache_server_is_initialized = 1;
	gh__global.cache_server_url = NULL;

	if (gh__cmd_opts.cache_server_mode == GH__CACHE_SERVER_MODE__DISABLE) {
		trace2_data_string("gvfs-helper", NULL, "cache/url", "disabled");
		return;
	}

	/*
	 * If the cache-server and main Git server have the same URL, we
	 * can silently disable the cache-server (by NOT setting the field
	 * in gh__global and explicitly disable the fallback logic.)
	 */
	if (!strcmp(gvfs_cache_server_url, gh__global.main_url)) {
		gh__cmd_opts.try_fallback = 0;
		trace2_data_string("gvfs-helper", NULL, "cache/url", "same");
		return;
	}

	if (gh__cmd_opts.cache_server_mode ==
	    GH__CACHE_SERVER_MODE__TRUST_WITHOUT_VERIFY) {
		gh__global.cache_server_url = gvfs_cache_server_url;
		trace2_data_string("gvfs-helper", NULL, "cache/url",
				   gvfs_cache_server_url);
		return;
	}

	/*
	 * GVFS cache-servers use the main Git server's creds rather
	 * than having their own creds.  This feels like a security
	 * hole.  For example, if the cache-server URL is pointed to a
	 * bad site, we'll happily send them our creds to the main Git
	 * server with each request to the cache-server.  This would
	 * allow an attacker to later use our creds to impersonate us
	 * on the main Git server.
	 *
	 * So we optionally verify that the URL to the cache-server is
	 * well-known by the main Git server.
	 */

	do__gvfs_config(&status, &config_data);

	if (status.ec == GH__ERROR_CODE__OK) {
		/*
		 * The gvfs/config response is in JSON, but I don't think
		 * we need to parse it and all that.  Lets just do a simple
		 * strstr() and assume it is sufficient.
		 *
		 * We do add some context to the pattern to guard against
		 * some attacks.
		 */
		struct strbuf pattern = STRBUF_INIT;

		strbuf_addf(&pattern, "\"Url\":\"%s\"", gvfs_cache_server_url);
		match = strstr(config_data.buf, pattern.buf);

		strbuf_release(&pattern);
	}

	strbuf_release(&config_data);

	if (match) {
		gh__global.cache_server_url = gvfs_cache_server_url;
		trace2_data_string("gvfs-helper", NULL, "cache/url",
				   gvfs_cache_server_url);
	}

	else if (gh__cmd_opts.cache_server_mode ==
		 GH__CACHE_SERVER_MODE__VERIFY_ERROR) {
		if (status.ec != GH__ERROR_CODE__OK)
			die("could not verify cache-server '%s': %s",
			    gvfs_cache_server_url,
			    status.error_message.buf);
		else
			die("could not verify cache-server '%s'",
			    gvfs_cache_server_url);
	}

	else if (gh__cmd_opts.cache_server_mode ==
		 GH__CACHE_SERVER_MODE__VERIFY_DISABLE) {
		if (status.ec != GH__ERROR_CODE__OK)
			warning("could not verify cache-server '%s': %s",
				gvfs_cache_server_url,
				status.error_message.buf);
		else
			warning("could not verify cache-server '%s'",
				gvfs_cache_server_url);
		trace2_data_string("gvfs-helper", NULL, "cache/url",
				   "disabled");
	}

	gh__response_status__release(&status);
}

/*
 * Read stdin until EOF (or a blank line) and add the desired OIDs
 * to the oidset.
 *
 * Stdin should contain a list of OIDs.  It may have additional
 * decoration that we need to strip out.
 *
 * We expect:
 * <hex_oid> [<path>]   // present OIDs
 */
static unsigned long read_stdin_from_rev_list(struct oidset *oids)
{
	struct object_id oid;
	struct strbuf buf_stdin = STRBUF_INIT;
	unsigned long count = 0;

	do {
		if (strbuf_getline(&buf_stdin, stdin) == EOF || !buf_stdin.len)
			break;

		if (get_oid_hex(buf_stdin.buf, &oid))
			continue; /* just silently eat it */

		if (!oidset_insert(oids, &oid))
			count++;
	} while (1);

	strbuf_release(&buf_stdin);
	return count;
}

/*
 * Build a complete JSON payload for a gvfs/objects POST request
 * containing the first n OIDs in an OIDSET index by the iterator.
 *
 * https://github.com/microsoft/VFSForGit/blob/master/Protocol.md
 */
static unsigned long build_json_payload__gvfs_objects(
	struct json_writer *jw_req,
	struct oidset_iter *iter,
	unsigned long nr_in_block)
{
	unsigned long k;
	const struct object_id *oid;

	k = 0;

	jw_init(jw_req);
	jw_object_begin(jw_req, 0);
	jw_object_intmax(jw_req, "commitDepth", gh__cmd_opts.depth);
	jw_object_inline_begin_array(jw_req, "objectIds");
	while (k < nr_in_block && (oid = oidset_iter_next(iter))) {
		jw_array_string(jw_req, oid_to_hex(oid));
		k++;
	}
	jw_end(jw_req);
	jw_end(jw_req);

	return k;
}

/*
 * Lookup the creds for the main/origin Git server.
 */
static void lookup_main_creds(void)
{
	if (gh__global.main_creds.username && *gh__global.main_creds.username)
		return;

	credential_from_url(&gh__global.main_creds, gh__global.main_url);
	credential_fill(the_repository, &gh__global.main_creds, 0);
	gh__global.main_creds_need_approval = 1;
}

/*
 * If we have a set of creds for the main Git server, tell the credential
 * manager to throw them away and ask it to reacquire them.
 */
static void refresh_main_creds(void)
{
	if (gh__global.main_creds.username && *gh__global.main_creds.username)
		credential_reject(the_repository, &gh__global.main_creds);

	lookup_main_creds();

	// TODO should we compare before and after values of u/p and
	// TODO shortcut reauth if we already know it will fail?
	// TODO if so, return a bool if same/different.
}

static void approve_main_creds(void)
{
	if (!gh__global.main_creds_need_approval)
		return;

	credential_approve(the_repository, &gh__global.main_creds);
	gh__global.main_creds_need_approval = 0;
}

/*
 * Build a set of creds for the cache-server based upon the main Git
 * server (assuming we have a cache-server configured).
 *
 * That is, we NEVER fill them directly for the cache-server -- we
 * only synthesize them from the filled main creds.
 */
static void synthesize_cache_server_creds(void)
{
	if (!gh__global.cache_server_is_initialized)
		BUG("sub-command did not initialize cache-server vars");

	if (!gh__global.cache_server_url)
		return;

	if (gh__global.cache_creds.username && *gh__global.cache_creds.username)
		return;

	/*
	 * Get the main Git server creds so we can borrow the username
	 * and password when we talk to the cache-server.
	 */
	lookup_main_creds();
	free(gh__global.cache_creds.username);
	gh__global.cache_creds.username = xstrdup(gh__global.main_creds.username);
	free(gh__global.cache_creds.password);
	gh__global.cache_creds.password = xstrdup(gh__global.main_creds.password);
}

/*
 * Flush and refresh the cache-server creds.  Because the cache-server
 * does not do 401s (or manage creds), we have to reload the main Git
 * server creds first.
 *
 * That is, we NEVER reject them directly because we never filled them.
 */
static void refresh_cache_server_creds(void)
{
	credential_clear(&gh__global.cache_creds);

	refresh_main_creds();
	synthesize_cache_server_creds();
}

/*
 * We NEVER approve cache-server creds directly because we never directly
 * filled them.  However, we should be able to infer that the main ones
 * are valid and can approve them if necessary.
 */
static void approve_cache_server_creds(void)
{
	approve_main_creds();
}

/*
 * Get the pathname to the ODB where we write objects that we download.
 */
static void select_odb(void)
{
	odb_prepare_alternates(the_repository->objects);

	strbuf_init(&gh__global.buf_odb_path, 0);

	if (gvfs_shared_cache_pathname.len)
		strbuf_addbuf(&gh__global.buf_odb_path,
			      &gvfs_shared_cache_pathname);
	else
		strbuf_addstr(&gh__global.buf_odb_path,
			      the_repository->objects->sources->path);
}

/*
 * Create a tempfile to stream the packfile into.
 *
 * We create a tempfile in the chosen ODB directory and let CURL
 * automatically stream data to the file.  If successful, we can
 * later rename it to a proper .pack and run "git index-pack" on
 * it to create the corresponding .idx file.
 *
 * TODO I would rather to just stream the packfile directly into
 * TODO "git index-pack --stdin" (and save some I/O) because it
 * TODO will automatically take care of the rename of both files
 * TODO and any other cleanup.  BUT INDEX-PACK WILL ONLY WRITE
 * TODO TO THE PRIMARY ODB -- it will not write into the alternates
 * TODO (this is considered bad form).  So we would need to add
 * TODO an option to index-pack to handle this.  I don't want to
 * TODO deal with this issue right now.
 *
 * TODO Consider using lockfile for this rather than naked tempfile.
 */
static struct tempfile *create_tempfile_for_packfile(void)
{
	static unsigned int nth = 0;
	static struct timeval tv = {0};
	static struct tm tm = {0};
	static time_t secs = 0;
	static char tbuf[32] = {0};

	struct tempfile *tempfile = NULL;
	struct strbuf buf_path = STRBUF_INIT;

	if (!nth) {
		gettimeofday(&tv, NULL);
		secs = tv.tv_sec;
		gmtime_r(&secs, &tm);

		xsnprintf(tbuf, sizeof(tbuf), "%4d%02d%02d-%02d%02d%02d-%06ld",
			  tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
			  tm.tm_hour, tm.tm_min, tm.tm_sec,
			  (long)tv.tv_usec);
	}

	// TODO should this be in the "<ODB>/pack/tempPacks/"
	// TODO directory instead? YES

	strbuf_addbuf(&buf_path, &gh__global.buf_odb_path);
	strbuf_complete(&buf_path, '/');
	strbuf_addf(&buf_path, "pack/vfs-%s-%04d.temp", tbuf, nth++);

	tempfile = create_tempfile(buf_path.buf);
	fdopen_tempfile(tempfile, "w");

	strbuf_release(&buf_path);

	return tempfile;
}

/*
 * Create a tempfile to stream a loose object into.
 *
 * We create a tempfile in the chosen ODB directory and let CURL
 * automatically stream data to the file.
 *
 * We put it directly in the "<odb>/xx/" directory.
 */
static void create_tempfile_for_loose(
	struct gh__request_params *params,
	struct gh__response_status *status,
	const struct object_id *oid)
{
	struct strbuf buf_path = STRBUF_INIT;
	const char *hex;

	gh__response_status__zero(status);

	hex = oid_to_hex(oid);

	strbuf_addbuf(&buf_path, &gh__global.buf_odb_path);
	strbuf_complete(&buf_path, '/');
	strbuf_add(&buf_path, hex, 2);

	if (!file_exists(buf_path.buf) &&
	    mkdir(buf_path.buf, 0777) == -1 &&
		!file_exists(buf_path.buf)) {
		strbuf_addf(&status->error_message,
			    "cannot create directory for loose object '%s'",
			    buf_path.buf);
		status->ec = GH__ERROR_CODE__COULD_NOT_CREATE_TEMPFILE;
		goto cleanup;
	}

	strbuf_addch(&buf_path, '/');
	strbuf_addstr(&buf_path, hex+2);

	/* Remember the full path of the final destination. */
	strbuf_setlen(&params->loose_path, 0);
	strbuf_addbuf(&params->loose_path, &buf_path);

	/*
	 * Build a unique tempfile pathname based upon it.  We avoid
	 * using lockfiles to avoid issues with stale locks after
	 * crashes.
	 */
	strbuf_addf(&buf_path, ".%08u.temp", getpid());

	params->tempfile = create_tempfile(buf_path.buf);
	if (!params->tempfile) {
		strbuf_addstr(&status->error_message,
			      "could not create tempfile for loose object");
		status->ec = GH__ERROR_CODE__COULD_NOT_CREATE_TEMPFILE;
		goto cleanup;
	}

	fdopen_tempfile(params->tempfile, "w");

cleanup:
	strbuf_release(&buf_path);
}

/*
 * Extract the filename portion of the given pathname.
 *
 * TODO Wish I could find a strbuf_filename() function for this.
 */
static void extract_filename(struct strbuf *filename,
			     const struct strbuf *pathname)
{
	size_t len = pathname->len;

	strbuf_setlen(filename, 0);

	while (len > 0 && !is_dir_sep(pathname->buf[len - 1]))
		len--;

	strbuf_addstr(filename, &pathname->buf[len]);
}

/*
 * Convert the tempfile into a permanent .pack packfile in the ODB.
 * Create the corresponding .idx file.
 *
 * Return the filename (not pathname) of the resulting packfile.
 */
static void install_packfile(struct gh__response_status *status,
			     struct tempfile **pp_tempfile,
			     struct strbuf *packfile_filename)
{
	struct child_process ip = CHILD_PROCESS_INIT;
	struct strbuf pack_name_tmp = STRBUF_INIT;
	struct strbuf pack_name_dst = STRBUF_INIT;
	struct strbuf idx_name_tmp = STRBUF_INIT;
	struct strbuf idx_name_dst = STRBUF_INIT;
	size_t len_base;

	gh__response_status__zero(status);

	strbuf_setlen(packfile_filename, 0);

	/*
	 * start with "<base>.temp" (that is owned by tempfile class).
	 * rename to "<base>.pack.temp" to break ownership.
	 *
	 * create "<base>.idx.temp" on provisional packfile.
	 *
	 * officially install both "<base>.{pack,idx}.temp" as
	 * "<base>.{pack,idx}".
	 */

	strbuf_addstr(&pack_name_tmp, get_tempfile_path(*pp_tempfile));
	if (!strip_suffix(pack_name_tmp.buf, ".temp", &len_base)) {
		/*
		 * This is more of a BUG(), but I want the error
		 * code propagated.
		 */
		strbuf_addf(&status->error_message,
			    "packfile tempfile does not end in '.temp': '%s'",
			    pack_name_tmp.buf);
		status->ec = GH__ERROR_CODE__COULD_NOT_INSTALL_PACKFILE;
		goto cleanup;
	}

	strbuf_setlen(&pack_name_tmp, (int)len_base);
	strbuf_addbuf(&pack_name_dst, &pack_name_tmp);
	strbuf_addbuf(&idx_name_tmp, &pack_name_tmp);
	strbuf_addbuf(&idx_name_dst, &pack_name_tmp);

	strbuf_addstr(&pack_name_tmp, ".pack.temp");
	strbuf_addstr(&pack_name_dst, ".pack");
	strbuf_addstr(&idx_name_tmp, ".idx.temp");
	strbuf_addstr(&idx_name_dst, ".idx");

	// TODO if either pack_name_dst or idx_name_dst already
	// TODO exists in the ODB, create alternate names so that
	// TODO we don't step on them.

	if (rename_tempfile(pp_tempfile, pack_name_tmp.buf) == -1) {
		strbuf_addf(&status->error_message,
			    "could not rename packfile to '%s'",
			    pack_name_tmp.buf);
		status->ec = GH__ERROR_CODE__COULD_NOT_INSTALL_PACKFILE;
		goto cleanup;
	}

	strvec_push(&ip.args, "index-pack");
	if (gh__cmd_opts.show_progress)
		strvec_push(&ip.args, "-v");
	strvec_pushl(&ip.args, "-o", idx_name_tmp.buf, NULL);
	strvec_push(&ip.args, pack_name_tmp.buf);
	ip.git_cmd = 1;
	ip.no_stdin = 1;
	ip.no_stdout = 1;

	// TODO consider capturing stdout from index-pack because
	// TODO it will contain the SHA of the packfile and we can
	// TODO (should?) add it to the .pack and .idx pathnames
	// TODO when we install them.
	// TODO
	// TODO See pipe_command() rather than run_command().
	// TODO
	// TODO Or should be SHA-it ourselves (or read the last 20 bytes)?

	/*
	 * Note that I DO NOT have a region around the index-pack process.
	 * The region in gh__run_one_slot() currently only covers the
	 * download time.  This index-pack is a separate step not covered
	 * in the above region.  Later, if/when we have CURL directly stream
	 * to index-pack, that region will be the combined download+index
	 * time.  So, I'm not going to introduce it here.
	 */
	if (run_command(&ip)) {
		unlink(pack_name_tmp.buf);
		unlink(idx_name_tmp.buf);
		strbuf_addf(&status->error_message,
			    "index-pack failed on '%s'", pack_name_tmp.buf);
		status->ec = GH__ERROR_CODE__COULD_NOT_INSTALL_PACKFILE;
		goto cleanup;
	}

	if (finalize_object_file(the_repository, pack_name_tmp.buf, pack_name_dst.buf) ||
	    finalize_object_file(the_repository, idx_name_tmp.buf, idx_name_dst.buf)) {
		unlink(pack_name_tmp.buf);
		unlink(pack_name_dst.buf);
		unlink(idx_name_tmp.buf);
		unlink(idx_name_dst.buf);
		strbuf_addf(&status->error_message,
			    "could not install packfile '%s'",
			    pack_name_dst.buf);
		status->ec = GH__ERROR_CODE__COULD_NOT_INSTALL_PACKFILE;
		goto cleanup;
	}

	extract_filename(packfile_filename, &pack_name_dst);

cleanup:
	child_process_clear(&ip);
	strbuf_release(&pack_name_tmp);
	strbuf_release(&pack_name_dst);
	strbuf_release(&idx_name_tmp);
	strbuf_release(&idx_name_dst);
}

/*
 * Convert the tempfile into a permanent loose object in the ODB.
 */
static void install_loose(struct gh__request_params *params,
			  struct gh__response_status *status)
{
	struct strbuf tmp_path = STRBUF_INIT;

	gh__response_status__zero(status);

	/*
	 * close tempfile to steal ownership away from tempfile class.
	 */
	strbuf_addstr(&tmp_path, get_tempfile_path(params->tempfile));
	close_tempfile_gently(params->tempfile);

	/*
	 * Try to install the tempfile as the actual loose object.
	 *
	 * If the loose object already exists, finalize_object_file()
	 * will NOT overwrite/replace it.  It will silently eat the
	 * EEXIST error and unlink the tempfile as it if was
	 * successful.  We just let it lie to us.
	 *
	 * Since our job is to back-fill missing objects needed by a
	 * foreground git process -- git should have called
	 * oid_object_info_extended() and loose_object_info() BEFORE
	 * asking us to download the missing object.  So if we get a
	 * collision we have to assume something else is happening in
	 * parallel and we lost the race.  And that's OK.
	 */
	if (finalize_object_file(the_repository, tmp_path.buf, params->loose_path.buf)) {
		unlink(tmp_path.buf);
		strbuf_addf(&status->error_message,
			    "could not install loose object '%s'",
			    params->loose_path.buf);
		status->ec = GH__ERROR_CODE__COULD_NOT_INSTALL_LOOSE;
	}

	strbuf_release(&tmp_path);
}

/*
 * Our wrapper to initialize the HTTP layer.
 *
 * We always use the real origin server, not the cache-server, when
 * initializing the http/curl layer.
 */
static void gh_http_init(void)
{
	if (gh__global.http_is_initialized)
		return;

	http_init(gh__global.remote, gh__global.main_url, 0);
	gh__global.http_is_initialized = 1;
}

static void gh_http_cleanup(void)
{
	if (!gh__global.http_is_initialized)
		return;

	http_cleanup();
	gh__global.http_is_initialized = 0;
}

static void set_main_creds_on_slot(struct active_request_slot *slot,
				   const struct credential *creds)
{
	assert(creds == &gh__global.main_creds);

	/*
	 * When talking to the main/origin server, we have 3 modes
	 * of operation:
	 *
	 * [1] The initial request is sent without loading creds
	 *     and with ANY-AUTH set.  (And the `":"` is a magic
	 *     value.)
	 *
	 *     This allows libcurl to negotiate for us if it can.
	 *     For example, this allows NTLM to work by magic and
	 *     we get 200s without ever seeing a 401.  If libcurl
	 *     cannot negotiate for us, it gives us a 401 (and all
	 *     of the 401 code in this file responds to that).
	 *
	 * [2] A 401 retry will load the main creds and try again.
	 *     This causes `creds->username`to be non-NULL (even
	 *     if refers to a zero-length string).  And we assume
	 *     BASIC Authentication.  (And a zero-length username
	 *     is a convention for PATs, but then sometimes users
	 *     put the PAT in their `username` field and leave the
	 *     `password` field blank.  And that works too.)
	 *
	 * [3] Subsequent requests on the same connection use
	 *     whatever worked before.
	 */
	if (creds && creds->username) {
		curl_easy_setopt(slot->curl, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
		curl_easy_setopt(slot->curl, CURLOPT_USERNAME, creds->username);
		curl_easy_setopt(slot->curl, CURLOPT_PASSWORD, creds->password);
	} else {
		curl_easy_setopt(slot->curl, CURLOPT_HTTPAUTH, CURLAUTH_ANY);
		curl_easy_setopt(slot->curl, CURLOPT_USERPWD, ":");
	}
}

static void set_cache_server_creds_on_slot(struct active_request_slot *slot,
					   const struct credential *creds)
{
	assert(creds == &gh__global.cache_creds);
	assert(creds->username);

	/*
	 * Things are weird when talking to a cache-server:
	 *
	 * [1] They don't send 401s on an auth error, rather they send
	 *     a 400 (with a nice human-readable string in the html body).
	 *     This prevents libcurl from doing any negotiation for us.
	 *
	 * [2] Cache-servers don't manage their own passwords, but
	 *     rather require us to send the Basic Authentication
	 *     username & password that we would send to the main
	 *     server.  (So yes, we have to get creds validated
	 *     against the main server creds and substitute them when
	 *     talking to the cache-server.)
	 *
	 * This means that:
	 *
	 * [a] We cannot support cache-servers that want to use NTLM.
	 *
	 * [b] If we want to talk to a cache-server, we have get the
	 *     Basic Auth creds for the main server.  And this may be
	 *     problematic if the libcurl and/or the credential manager
	 *     insists on using NTLM and prevents us from getting them.
	 *
	 * So we never try AUTH-ANY and force Basic Auth (if possible).
	 */
	if (creds && creds->username) {
		curl_easy_setopt(slot->curl, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
		curl_easy_setopt(slot->curl, CURLOPT_USERNAME, creds->username);
		curl_easy_setopt(slot->curl, CURLOPT_PASSWORD, creds->password);
	}
}

/*
 * Do a single HTTP request without auth-retry or fallback.
 */
static void do_req(const char *url_base,
		   const char *url_component,
		   const struct credential *creds,
		   struct gh__request_params *params,
		   struct gh__response_status *status)
{
	struct active_request_slot *slot;
	struct slot_results results;
	struct strbuf rest_url = STRBUF_INIT;

	gh__response_status__zero(status);

	if (params->b_write_to_file) {
		// TODO ftruncate tempfile ??
	} else {
		strbuf_setlen(params->buffer, 0);
	}

	end_url_with_slash(&rest_url, url_base);
	strbuf_addstr(&rest_url, url_component);

	slot = get_active_slot();
	slot->results = &results;

	curl_easy_setopt(slot->curl, CURLOPT_NOBODY, 0L); /* not a HEAD request */
	curl_easy_setopt(slot->curl, CURLOPT_URL, rest_url.buf);
	curl_easy_setopt(slot->curl, CURLOPT_HTTPHEADER, params->headers);
	if (curl_version_info(CURLVERSION_NOW)->version_num < 0x074b00)
		/*
		 * cURL 7.75.0 allows headers to be parsed even when
		 * `CURLOPT_FAILONERROR` is enabled and the HTTP result code
		 * indicates an error. This is the behavior expected by
		 * `gvfs-helper`.
		 *
		 * On older cURL versions, `gvfs-helper` still needs to parse
		 * the HTTP headers and therefore needs to _not_ fail upon
		 * HTTP result codes indicating errors; For newer cURL
		 * versions, we still prefer to enable `FAILONERROR`.
		 */
		curl_easy_setopt(slot->curl, CURLOPT_FAILONERROR, (long)0);

	if (params->b_is_post) {
		curl_easy_setopt(slot->curl, CURLOPT_POST, 1L);
		curl_easy_setopt(slot->curl, CURLOPT_ENCODING, NULL);
		curl_easy_setopt(slot->curl, CURLOPT_POSTFIELDS,
				 params->post_payload->buf);
		curl_easy_setopt(slot->curl, CURLOPT_POSTFIELDSIZE,
				 (long)params->post_payload->len);
	} else {
		curl_easy_setopt(slot->curl, CURLOPT_POST, 0L);
	}

	if (params->b_write_to_file) {
		curl_easy_setopt(slot->curl, CURLOPT_WRITEFUNCTION, fwrite);
		curl_easy_setopt(slot->curl, CURLOPT_WRITEDATA,
				 (void*)params->tempfile->fp);
	} else {
		curl_easy_setopt(slot->curl, CURLOPT_WRITEFUNCTION,
				 fwrite_buffer);
		curl_easy_setopt(slot->curl, CURLOPT_FILE, params->buffer);
	}

	if (url_base == gh__global.main_url)
		set_main_creds_on_slot(slot, creds);
	else
		set_cache_server_creds_on_slot(slot, creds);

	if (params->progress_base_phase2_msg.len ||
	    params->progress_base_phase3_msg.len) {
		curl_easy_setopt(slot->curl, CURLOPT_XFERINFOFUNCTION,
				 gh__curl_progress_cb);
		curl_easy_setopt(slot->curl, CURLOPT_XFERINFODATA, params);
		curl_easy_setopt(slot->curl, CURLOPT_NOPROGRESS, 0L);
	} else {
		curl_easy_setopt(slot->curl, CURLOPT_NOPROGRESS, 1L);
	}

	gh__run_one_slot(slot, params, status);
	strbuf_release(&rest_url);
}

static void do_req__to_main(const char *url_component,
			    struct gh__request_params *params,
			    struct gh__response_status *status)
{
//	lookup_main_creds();

	/*
	 * When talking to the main Git server, we DO NOT preload the
	 * creds before the first request.
	 */

	do_req(gh__global.main_url, url_component, &gh__global.main_creds,
	       params, status);

	if (status->response_code == 401) {
		refresh_main_creds();

		do_req(gh__global.main_url, url_component, &gh__global.main_creds,
		       params, status);
	}

	if (status->response_code == 200)
		approve_main_creds();
}

static void do_req__to_cache_server(const char *url_component,
				    struct gh__request_params *params,
				    struct gh__response_status *status)
{
	/*
	 * When talking to a cache-server, DO force load the creds.
	 * This implicitly preloads the creds to the main server.
	 */
	synthesize_cache_server_creds();

	do_req(gh__global.cache_server_url, url_component, &gh__global.cache_creds,
	       params, status);
	fixup_cache_server_400_to_401(status);

	if (status->response_code == 401) {
		refresh_cache_server_creds();

		do_req(gh__global.cache_server_url, url_component,
		       &gh__global.cache_creds, params, status);
		fixup_cache_server_400_to_401(status);
	}

	if (status->response_code == 200)
		approve_cache_server_creds();
}

static void do_req__with_fallback(const char *url_component,
				  struct gh__request_params *params,
				  struct gh__response_status *status)
{
	if (gh__global.cache_server_url && !params->b_no_cache_server) {
		do_req__to_cache_server(url_component, params, status);

		if (status->response_code == 200)
			return;

		if (!gh__cmd_opts.try_fallback)
			return;

		/*
		 * The cache-server shares creds with the main Git server,
		 * so if our creds failed against the cache-server, they
		 * will also fail against the main Git server.  We just let
		 * this fail.
		 *
		 * Falling-back would likely just cause the 3rd (or maybe
		 * 4th) cred prompt.
		 */
		if (status->response_code == 401)
			return;
	}

	do_req__to_main(url_component, params, status);
}

/*
 * Call "gvfs/config" REST API.
 *
 * Return server's response buffer.  This is probably a raw JSON string.
 */
static void do__gvfs_config(struct gh__response_status *status,
			    struct strbuf *config_data)
{
	struct gh__request_params params = GH__REQUEST_PARAMS_INIT;

	strbuf_addstr(&params.label, "GET/config");

	params.b_is_post = 0;
	params.b_write_to_file = 0;
	params.b_no_cache_server = 1; /* they don't handle gvfs/config API */
	params.buffer = config_data;

	params.object_count = 1; /* a bit of a lie */

	/*
	 * "X-TFS-FedAuthRedirect: Suppress" disables the 302 + 203 redirect
	 * sequence to a login page and forces the main Git server to send a
	 * normal 401.
	 */
	params.headers = http_copy_default_headers();
	params.headers = curl_slist_append(params.headers,
					   "X-TFS-FedAuthRedirect: Suppress");
	params.headers = curl_slist_append(params.headers,
					   "Pragma: no-cache");

	if (gh__cmd_opts.show_progress) {
		/*
		 * gvfs/config has a very small reqest payload, so I don't
		 * see any need to report progress on the upload side of
		 * the GET.  So just report progress on the download side.
		 */
		strbuf_addstr(&params.progress_base_phase3_msg,
			      "Receiving gvfs/config");
	}

	do_req__with_fallback("gvfs/config", &params, status);

	gh__request_params__release(&params);
}

/*
 * Call "gvfs/objects/<oid>" REST API to fetch a loose object
 * and write it to the ODB.
 */
static void do__loose__gvfs_object(struct gh__response_status *status,
				   const struct object_id *oid)
{
	struct gh__request_params params = GH__REQUEST_PARAMS_INIT;
	struct strbuf component_url = STRBUF_INIT;

	gh__response_status__zero(status);

	strbuf_addf(&component_url, "gvfs/objects/%s", oid_to_hex(oid));

	strbuf_addstr(&params.label, "GET/objects");

	params.b_is_post = 0;
	params.b_write_to_file = 1;
	params.b_no_cache_server = 0;

	params.object_count = 1;

	params.headers = http_copy_default_headers();
	params.headers = curl_slist_append(params.headers,
					   "X-TFS-FedAuthRedirect: Suppress");
	params.headers = curl_slist_append(params.headers,
					   "Pragma: no-cache");

	create_tempfile_for_loose(&params, status, oid);
	if (!params.tempfile)
		goto cleanup;

	if (gh__cmd_opts.show_progress) {
		/*
		 * Likewise, a gvfs/objects/{oid} has a very small reqest
		 * payload, so I don't see any need to report progress on
		 * the upload side of the GET.  So just report progress
		 * on the download side.
		 */
		strbuf_addstr(&params.progress_base_phase3_msg,
			      "Receiving 1 loose object");
	}

	do_req__with_fallback(component_url.buf, &params, status);

	if (status->ec == GH__ERROR_CODE__OK)
		install_loose(&params, status);

cleanup:
	gh__request_params__release(&params);
	strbuf_release(&component_url);
}

/*
 * Call "gvfs/objects" POST REST API to fetch a packfile containing
 * the objects in the requested OIDSET.  Returns the filename (not
 * pathname) to the new packfile.
 */
static void do__packfile__gvfs_objects(struct gh__response_status *status,
				       struct oidset_iter *iter,
				       unsigned long nr_wanted_in_block,
				       struct strbuf *output_filename,
				       unsigned long *nr_taken)
{
	struct json_writer jw_req = JSON_WRITER_INIT;
	struct gh__request_params params = GH__REQUEST_PARAMS_INIT;

	gh__response_status__zero(status);

	params.object_count = build_json_payload__gvfs_objects(
		&jw_req, iter, nr_wanted_in_block);
	*nr_taken = params.object_count;

	strbuf_addstr(&params.label, "POST/objects");

	params.b_is_post = 1;
	params.b_write_to_file = 1;
	params.b_no_cache_server = 0;

	params.post_payload = &jw_req.json;

	params.headers = http_copy_default_headers();
	params.headers = curl_slist_append(params.headers,
					   "X-TFS-FedAuthRedirect: Suppress");
	params.headers = curl_slist_append(params.headers,
					   "Pragma: no-cache");
	params.headers = curl_slist_append(params.headers,
					   "Content-Type: application/json");
	/*
	 * We really always want a packfile.  But if the payload only
	 * requests 1 OID, the server will/may send us a single loose
	 * objects instead.  (Apparently the server ignores us when we
	 * only send application/x-git-packfile and does it anyway.)
	 *
	 * So to make it clear to my future self, go ahead and add
	 * an accept header for loose objects and own it.
	 */
	params.headers = curl_slist_append(params.headers,
					   "Accept: application/x-git-packfile");
	params.headers = curl_slist_append(params.headers,
					   "Accept: application/x-git-loose-object");

	params.tempfile = create_tempfile_for_packfile();
	if (!params.tempfile) {
		strbuf_addstr(&status->error_message,
			      "could not create tempfile for packfile");
		status->ec = GH__ERROR_CODE__COULD_NOT_CREATE_TEMPFILE;
		goto cleanup;
	}

	if (gh__cmd_opts.show_progress) {
		strbuf_addf(&params.progress_base_phase2_msg,
			    "Requesting packfile with %ld objects",
			    params.object_count);
		strbuf_addf(&params.progress_base_phase3_msg,
			    "Receiving packfile with %ld objects",
			    params.object_count);
	}

	do_req__with_fallback("gvfs/objects", &params, status);

	if (status->ec == GH__ERROR_CODE__OK) {
		if (!strcmp(status->content_type.buf,
			    "application/x-git-packfile")) {

			// TODO Consider having a worker thread to manage
			// TODO running index-pack and then install the
			// TODO resulting .idx and .pack files.  This would
			// TODO let us interleave those steps with our thread
			// TODO fetching the next block of objects from the
			// TODO server.  (Need to think about how progress
			// TODO messages from our thread and index-pack
			// TODO would mesh.)
			// TODO
			// TODO But then again, if we hack index-pack to write
			// TODO to our alternate and stream the data thru it,
			// TODO it won't matter.

			install_packfile(status, &params.tempfile,
					 output_filename);
			goto cleanup;
		}

		if (!strcmp(status->content_type.buf,
			    "application/x-git-loose-object"))
		{
			/*
			 * This should not happen (when we request
			 * more than one object).  The server can send
			 * us a loose object (even when we use the
			 * POST form) if there is only one object in
			 * the payload (and despite the set of accept
			 * headers we send), so I'm going to leave
			 * this here.
			 */
			strbuf_addstr(&status->error_message,
				      "received loose object when packfile expected");
			status->ec = GH__ERROR_CODE__UNEXPECTED_CONTENT_TYPE;
			goto cleanup;
		}

		strbuf_addf(&status->error_message,
			    "received unknown content-type '%s'",
			    status->content_type.buf);
		status->ec = GH__ERROR_CODE__UNEXPECTED_CONTENT_TYPE;
		goto cleanup;
	}

cleanup:
	gh__request_params__release(&params);
	jw_release(&jw_req);
}

/*
 * Bulk or individually fetch a list of objects in one or more http requests.
 * Create one or more packfiles and/or loose objects.
 *
 * We accumulate results for each request in `result_list` until we get a
 * hard error and have to stop.
 */
static void do_fetch_oidset(struct gh__response_status *status,
			    struct oidset *oids,
			    unsigned long nr_total,
			    struct string_list *result_list)
{
	struct oidset_iter iter;
	struct strbuf output_filename = STRBUF_INIT;
	struct strbuf msg = STRBUF_INIT;
	struct strbuf err404 = STRBUF_INIT;
	const struct object_id *oid;
	unsigned long k;
	unsigned long nr_taken;
	int had_404 = 0;

	gh__response_status__zero(status);
	if (!nr_total)
		return;

	oidset_iter_init(oids, &iter);

	for (k = 0; k < nr_total; k += nr_taken) {
		if (nr_total - k == 1 || gh__cmd_opts.block_size == 1) {
			oid = oidset_iter_next(&iter);
			nr_taken = 1;

			do__loose__gvfs_object(status, oid);

			/*
			 * If we get a 404 for an individual object, ignore
			 * it and get the rest.  We'll fixup the 'ec' later.
			 */
			if (status->ec == GH__ERROR_CODE__HTTP_404) {
				if (!err404.len)
					strbuf_addf(&err404, "%s: loose object %s",
						    status->error_message.buf,
						    oid_to_hex(oid));
				/*
				 * Mark the fetch as "incomplete", but don't
				 * stop trying to get other chunks.
				 */
				had_404 = 1;
				continue;
			}

			if (status->ec != GH__ERROR_CODE__OK) {
				/* Stop at the first hard error. */
				strbuf_addf(&status->error_message, ": loose %s",
					    oid_to_hex(oid));
				goto cleanup;
			}

			strbuf_setlen(&msg, 0);
			strbuf_addf(&msg, "loose %s", oid_to_hex(oid));
			string_list_append(result_list, msg.buf);

		} else {
			strbuf_setlen(&output_filename, 0);

			do__packfile__gvfs_objects(status, &iter,
						   gh__cmd_opts.block_size,
						   &output_filename,
						   &nr_taken);

			/*
			 * Because the oidset iterator has random
			 * order, it does no good to say the k-th or
			 * n-th chunk was incomplete; the client
			 * cannot use that index for anything.
			 *
			 * We get a 404 when at least one object in
			 * the chunk was not found.
			 *
			 * TODO Consider various retry strategies (such as
			 * TODO loose or bisect) on the members within this
			 * TODO chunk to reduce the impact of the miss.
			 *
			 * For now, ignore the 404 and go on to the
			 * next chunk and then fixup the 'ec' later.
			 */
			if (status->ec == GH__ERROR_CODE__HTTP_404) {
				if (!err404.len)
					strbuf_addf(&err404,
						    "%s: packfile object",
						    status->error_message.buf);
				/*
				 * Mark the fetch as "incomplete", but don't
				 * stop trying to get other chunks.
				 */
				had_404 = 1;
				continue;
			}

			if (status->ec != GH__ERROR_CODE__OK) {
				/* Stop at the first hard error. */
				strbuf_addstr(&status->error_message,
					      ": in packfile");
				goto cleanup;
			}

			strbuf_setlen(&msg, 0);
			strbuf_addf(&msg, "packfile %s", output_filename.buf);
			string_list_append(result_list, msg.buf);
		}
	}

cleanup:
	strbuf_release(&msg);
	strbuf_release(&err404);
	strbuf_release(&output_filename);

	if (had_404 && status->ec == GH__ERROR_CODE__OK) {
		strbuf_setlen(&status->error_message, 0);
		strbuf_addstr(&status->error_message, "404 Not Found");
		status->ec = GH__ERROR_CODE__HTTP_404;
	}
}

/*
 * Finish with initialization.  This happens after the main option
 * parsing, dispatch to sub-command, and sub-command option parsing
 * and before actually doing anything.
 *
 * Optionally configure the cache-server if the sub-command will
 * use it.
 */
static void finish_init(int setup_cache_server)
{
	select_odb();

	lookup_main_url();
	gh_http_init();

	if (setup_cache_server)
		select_cache_server();
}

/*
 * Request gvfs/config from main Git server.  (Config data is not
 * available from a GVFS cache-server.)
 *
 * Print the received server configuration (as the raw JSON string).
 */
static enum gh__error_code do_sub_cmd__config(int argc UNUSED, const char **argv UNUSED)
{
	struct gh__response_status status = GH__RESPONSE_STATUS_INIT;
	struct strbuf config_data = STRBUF_INIT;
	enum gh__error_code ec = GH__ERROR_CODE__OK;

	trace2_cmd_mode("config");

	finish_init(0);

	do__gvfs_config(&status, &config_data);
	ec = status.ec;

	if (ec == GH__ERROR_CODE__OK)
		printf("%s\n", config_data.buf);
	else
		error("config: %s", status.error_message.buf);

	gh__response_status__release(&status);
	strbuf_release(&config_data);

	return ec;
}

/*
 * Read a list of objects from stdin and fetch them in a single request (or
 * multiple block-size requests).
 */
static enum gh__error_code do_sub_cmd__get(int argc, const char **argv)
{
	static struct option get_options[] = {
		OPT_UNSIGNED('b', "block-size", &gh__cmd_opts.block_size,
			     N_("number of objects to request at a time")),
		OPT_INTEGER('d', "depth", &gh__cmd_opts.depth,
			    N_("Commit depth")),
		OPT_END(),
	};

	struct gh__response_status status = GH__RESPONSE_STATUS_INIT;
	struct oidset oids = OIDSET_INIT;
	struct string_list result_list = STRING_LIST_INIT_DUP;
	enum gh__error_code ec = GH__ERROR_CODE__OK;
	unsigned long nr_total;
	size_t k;

	trace2_cmd_mode("get");

	if (argc > 1 && !strcmp(argv[1], "-h"))
		usage_with_options(get_usage, get_options);

	argc = parse_options(argc, argv, NULL, get_options, get_usage, 0);
	if (gh__cmd_opts.depth < 1)
		gh__cmd_opts.depth = 1;

	finish_init(1);

	nr_total = read_stdin_from_rev_list(&oids);

	trace2_region_enter("gvfs-helper", "get", NULL);
	trace2_data_intmax("gvfs-helper", NULL, "get/nr_objects", nr_total);
	do_fetch_oidset(&status, &oids, nr_total, &result_list);
	trace2_region_leave("gvfs-helper", "get", NULL);

	ec = status.ec;

	for (k = 0; k < result_list.nr; k++)
		printf("%s\n", result_list.items[k].string);

	if (ec != GH__ERROR_CODE__OK)
		error("get: %s", status.error_message.buf);

	gh__response_status__release(&status);
	oidset_clear(&oids);
	string_list_clear(&result_list, 0);

	return ec;
}

/*
 * Handle the 'get' command when in "server mode".  Only call error()
 * for hard errors where we cannot communicate correctly with the foreground
 * client process.  Pass any actual data errors (such as 404's or 401's from
 * the fetch back to the client process.
 */
static enum gh__error_code do_server_subprocess_get(void)
{
	struct gh__response_status status = GH__RESPONSE_STATUS_INIT;
	struct oidset oids = OIDSET_INIT;
	struct object_id oid;
	struct string_list result_list = STRING_LIST_INIT_DUP;
	enum gh__error_code ec = GH__ERROR_CODE__OK;
	char *line;
	int len;
	int err;
	size_t k;
	unsigned long nr_total = 0;

	/*
	 * Inside the "get" command, we expect a list of OIDs
	 * and a flush.
	 */
	while (1) {
		len = packet_read_line_gently(0, NULL, &line);
		if (len < 0 || !line)
			break;

		if (get_oid_hex(line, &oid)) {
			error("server: invalid oid syntax '%s'", line);
			ec = GH__ERROR_CODE__SUBPROCESS_SYNTAX;
			goto cleanup;
		}

		if (!oidset_insert(&oids, &oid))
			nr_total++;
	}

	if (!nr_total) {
		if (packet_write_fmt_gently(1, "ok\n")) {
			error("server: cannot write 'get' result to client");
			ec = GH__ERROR_CODE__SUBPROCESS_SYNTAX;
		} else
			ec = GH__ERROR_CODE__OK;
		goto cleanup;
	}

	trace2_region_enter("gvfs-helper", "server/get", NULL);
	trace2_data_intmax("gvfs-helper", NULL, "server/get/nr_objects", nr_total);
	do_fetch_oidset(&status, &oids, nr_total, &result_list);
	trace2_region_leave("gvfs-helper", "server/get", NULL);

	/*
	 * Write pathname of the ODB where we wrote all of the objects
	 * we fetched.
	 */
	if (packet_write_fmt_gently(1, "odb %s\n",
				    gh__global.buf_odb_path.buf)) {
		error("server: cannot write 'odb' to client");
		ec = GH__ERROR_CODE__SUBPROCESS_SYNTAX;
		goto cleanup;
	}

	for (k = 0; k < result_list.nr; k++)
		if (packet_write_fmt_gently(1, "%s\n",
					    result_list.items[k].string))
		{
			error("server: cannot write result to client: '%s'",
			      result_list.items[k].string);
			ec = GH__ERROR_CODE__SUBPROCESS_SYNTAX;
			goto cleanup;
		}

	err = 0;
	if (ec == GH__ERROR_CODE__OK)
		err = packet_write_fmt_gently(1, "ok\n");
	else if (ec == GH__ERROR_CODE__HTTP_404)
		err = packet_write_fmt_gently(1, "partial\n");
	else
		err = packet_write_fmt_gently(1, "error %s\n",
					      status.error_message.buf);
	if (err) {
		error("server: cannot write result to client");
		ec = GH__ERROR_CODE__SUBPROCESS_SYNTAX;
		goto cleanup;
	}

	if (packet_flush_gently(1)) {
		error("server: cannot flush result to client");
		ec = GH__ERROR_CODE__SUBPROCESS_SYNTAX;
		goto cleanup;
	}

cleanup:
	oidset_clear(&oids);
	string_list_clear(&result_list, 0);
	gh__response_status__release(&status);

	return ec;
}

typedef enum gh__error_code (fn_subprocess_cmd)(void);

struct subprocess_capability {
	const char *name;
	int client_has;
	fn_subprocess_cmd *pfn;
};

static struct subprocess_capability caps[] = {
	{ "get", 0, do_server_subprocess_get },
	{ NULL, 0, NULL },
};

/*
 * Handle the subprocess protocol handshake as described in:
 * [] Documentation/technical/protocol-common.txt
 * [] Documentation/technical/long-running-process-protocol.txt
 */
static int do_protocol_handshake(void)
{
#define OUR_SUBPROCESS_VERSION "1"

	char *line;
	int len;
	int k;
	int b_support_our_version = 0;

	len = packet_read_line_gently(0, NULL, &line);
	if (len < 0 || !line || strcmp(line, "gvfs-helper-client")) {
		error("server: subprocess welcome handshake failed: %s", line);
		return -1;
	}

	while (1) {
		const char *v;
		len = packet_read_line_gently(0, NULL, &line);
		if (len < 0 || !line)
			break;
		if (!skip_prefix(line, "version=", &v)) {
			error("server: subprocess version handshake failed: %s",
			      line);
			return -1;
		}
		b_support_our_version |= (!strcmp(v, OUR_SUBPROCESS_VERSION));
	}
	if (!b_support_our_version) {
		error("server: client does not support our version: %s",
		      OUR_SUBPROCESS_VERSION);
		return -1;
	}

	if (packet_write_fmt_gently(1, "gvfs-helper-server\n") ||
	    packet_write_fmt_gently(1, "version=%s\n",
				    OUR_SUBPROCESS_VERSION) ||
	    packet_flush_gently(1)) {
		error("server: cannot write version handshake");
		return -1;
	}

	while (1) {
		const char *v;
		int k;

		len = packet_read_line_gently(0, NULL, &line);
		if (len < 0 || !line)
			break;
		if (!skip_prefix(line, "capability=", &v)) {
			error("server: subprocess capability handshake failed: %s",
			      line);
			return -1;
		}
		for (k = 0; caps[k].name; k++)
			if (!strcmp(v, caps[k].name))
				caps[k].client_has = 1;
	}

	for (k = 0; caps[k].name; k++)
		if (caps[k].client_has)
			if (packet_write_fmt_gently(1, "capability=%s\n",
						    caps[k].name)) {
				error("server: cannot write capabilities handshake: %s",
				      caps[k].name);
				return -1;
			}
	if (packet_flush_gently(1)) {
		error("server: cannot write capabilities handshake");
		return -1;
	}

	return 0;
}

/*
 * Interactively listen to stdin for a series of commands and execute them.
 */
static enum gh__error_code do_sub_cmd__server(int argc, const char **argv)
{
	static struct option server_options[] = {
		OPT_UNSIGNED('b', "block-size", &gh__cmd_opts.block_size,
			     N_("number of objects to request at a time")),
		OPT_INTEGER('d', "depth", &gh__cmd_opts.depth,
			    N_("Commit depth")),
		OPT_END(),
	};

	enum gh__error_code ec = GH__ERROR_CODE__OK;
	char *line;
	int len;
	int k;

	trace2_cmd_mode("server");

	if (argc > 1 && !strcmp(argv[1], "-h"))
		usage_with_options(server_usage, server_options);

	argc = parse_options(argc, argv, NULL, server_options, server_usage, 0);
	if (gh__cmd_opts.depth < 1)
		gh__cmd_opts.depth = 1;

	finish_init(1);

	if (do_protocol_handshake()) {
		ec = GH__ERROR_CODE__SUBPROCESS_SYNTAX;
		goto cleanup;
	}

top_of_loop:
	while (1) {
		len = packet_read_line_gently(0, NULL, &line);
		if (len < 0 || !line) {
			/* use extra FLUSH as a QUIT */
			ec = GH__ERROR_CODE__OK;
			goto cleanup;
		}

		for (k = 0; caps[k].name; k++) {
			if (caps[k].client_has && !strcmp(line, caps[k].name)) {
				ec = (caps[k].pfn)();
				if (ec != GH__ERROR_CODE__OK)
					goto cleanup;
				goto top_of_loop;
			}
		}

		error("server: unknown command '%s'", line);
		ec = GH__ERROR_CODE__SUBPROCESS_SYNTAX;
		goto cleanup;
	}

cleanup:
	return ec;
}

static enum gh__error_code do_sub_cmd(int argc, const char **argv)
{
	if (!strcmp(argv[0], "get"))
		return do_sub_cmd__get(argc, argv);

	if (!strcmp(argv[0], "config"))
		return do_sub_cmd__config(argc, argv);

	if (!strcmp(argv[0], "server"))
		return do_sub_cmd__server(argc, argv);

	// TODO have "test" mode that could be used to drive
	// TODO unit testing.

	return GH__ERROR_CODE__USAGE;
}

/*
 * Communicate with the primary Git server or a GVFS cache-server using the
 * GVFS Protocol.
 *
 * https://github.com/microsoft/VFSForGit/blob/master/Protocol.md
 */
int cmd_main(int argc, const char **argv)
{
	static struct option main_options[] = {
		OPT_STRING('r', "remote", &gh__cmd_opts.remote_name,
			   N_("remote"),
			   N_("Remote name")),
		OPT_BOOL('f', "fallback", &gh__cmd_opts.try_fallback,
			 N_("Fallback to Git server if cache-server fails")),
		OPT_CALLBACK(0, "cache-server", NULL,
			     N_("cache-server"),
			     N_("cache-server=disable|trust|verify|error"),
			     option_parse_cache_server_mode),
		OPT_CALLBACK(0, "shared-cache", NULL,
			     N_("pathname"),
			     N_("Pathname to shared objects directory"),
			     option_parse_shared_cache_directory),
		OPT_BOOL('p', "progress", &gh__cmd_opts.show_progress,
			 N_("Show progress")),
		OPT_END(),
	};

	enum gh__error_code ec = GH__ERROR_CODE__OK;

	if (argc > 1 && !strcmp(argv[1], "-h"))
		usage_with_options(main_usage, main_options);

	trace2_cmd_name("gvfs-helper");

	setup_git_directory_gently(NULL);

	repo_config(the_repository, git_default_config, NULL);

	/* Set any non-zero initial values in gh__cmd_opts. */
	gh__cmd_opts.depth = 1;
	gh__cmd_opts.block_size = GH__DEFAULT_BLOCK_SIZE;
	gh__cmd_opts.show_progress = !!isatty(2);

	argc = parse_options(argc, argv, NULL, main_options, main_usage,
			     PARSE_OPT_STOP_AT_NON_OPTION);
	if (argc == 0)
		usage_with_options(main_usage, main_options);

	ec = do_sub_cmd(argc, argv);

	gh_http_cleanup();

	if (ec == GH__ERROR_CODE__USAGE)
		usage_with_options(main_usage, main_options);

	return ec;
}
