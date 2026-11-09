#include "unit-test.h"

#ifdef GIT_WINDOWS_NATIVE
#include "compat/win32/trace2_win32_process_info.h"
#include "strvec.h"

static struct hashmap processes;
static struct strvec ancestors = STRVEC_INIT;

static void add_process(DWORD pid, DWORD parent_pid, const char *name)
{
	struct trace2_win32_process_entry *entry;

	FLEX_ALLOC_STR(entry, name, name);
	hashmap_entry_init(&entry->ent, pid);
	entry->parent_pid = parent_pid;
	hashmap_add(&processes, &entry->ent);
}

static void check_ancestors(const char **expected, size_t nr)
{
	cl_assert_equal_i(ancestors.nr, nr);
	for (size_t i = 0; i < nr; i++)
		cl_assert_equal_s(ancestors.v[i], expected[i]);
}
#endif

void test_mingw__initialize(void)
{
#ifdef GIT_WINDOWS_NATIVE
	hashmap_init(&processes, NULL, NULL, 0);
#endif
}

void test_mingw__cleanup(void)
{
#ifdef GIT_WINDOWS_NATIVE
	hashmap_clear_and_free(&processes,
			      struct trace2_win32_process_entry, ent);
	strvec_clear(&ancestors);
#endif
}

void test_mingw__ancestry_missing_process(void)
{
#ifdef GIT_WINDOWS_NATIVE
	trace2_win32_get_ancestry(&ancestors, &processes, 1);
	cl_assert_equal_i(ancestors.nr, 0);

	add_process(1, 2, "git.exe");
	trace2_win32_get_ancestry(&ancestors, &processes, 1);
	cl_assert_equal_i(ancestors.nr, 0);
#else
	cl_skip();
#endif
}

void test_mingw__ancestry_unordered_collisions(void)
{
#ifdef GIT_WINDOWS_NATIVE
	const char *expected[] = { "parent.exe", "root.exe" };
	DWORD bucket_count = processes.tablesize;
	DWORD parent_pid = bucket_count + 1;
	DWORD child_pid = 2 * bucket_count + 1;

	add_process(0, 2, "root.exe");
	add_process(child_pid, parent_pid, "git.exe");
	add_process(parent_pid, 0, "parent.exe");
	cl_assert_equal_i(hashmap_bucket(&processes, child_pid),
			  hashmap_bucket(&processes, parent_pid));

	trace2_win32_get_ancestry(&ancestors, &processes, child_pid);
	check_ancestors(expected, ARRAY_SIZE(expected));

	for (DWORD i = 0; i < 2 * bucket_count; i++)
		add_process(child_pid + i + 1, 2, "unrelated.exe");
	cl_assert(processes.tablesize > bucket_count);
	strvec_clear(&ancestors);
	trace2_win32_get_ancestry(&ancestors, &processes, child_pid);
	check_ancestors(expected, ARRAY_SIZE(expected));
#else
	cl_skip();
#endif
}

void test_mingw__ancestry_high_pid(void)
{
#ifdef GIT_WINDOWS_NATIVE
	const char *expected[] = { "parent.exe" };

	add_process(0x80000001, 1, "git.exe");
	add_process(1, 2, "parent.exe");

	trace2_win32_get_ancestry(&ancestors, &processes, 0x80000001);
	check_ancestors(expected, ARRAY_SIZE(expected));
#else
	cl_skip();
#endif
}

void test_mingw__ancestry_cycle(void)
{
#ifdef GIT_WINDOWS_NATIVE
	const char *expected[] = { "parent.exe", "git.exe", "(cycle)" };

	add_process(1, 2, "git.exe");
	add_process(2, 1, "parent.exe");

	trace2_win32_get_ancestry(&ancestors, &processes, 1);
	check_ancestors(expected, ARRAY_SIZE(expected));
#else
	cl_skip();
#endif
}

void test_mingw__ancestry_truncated(void)
{
#ifdef GIT_WINDOWS_NATIVE
	const char *expected[] = {
		"p1", "p2", "p3", "p4", "p5", "p6", "p7", "p8", "p9",
		"p10", "(truncated)"
	};

	for (DWORD pid = 0; pid < 12; pid++) {
		char name[16];

		xsnprintf(name, sizeof(name), "p%lu", (unsigned long)pid);
		add_process(pid, pid + 1, name);
	}

	trace2_win32_get_ancestry(&ancestors, &processes, 0);
	check_ancestors(expected, ARRAY_SIZE(expected));
#else
	cl_skip();
#endif
}

#if defined(GIT_WINDOWS_NATIVE) && !defined(_UCRT)
#undef strerror
int errnos_contains(int);
static int errnos [53]={
    /* errnos in err_win_to_posix */
    EACCES, EBUSY, EEXIST, ERANGE, EIO, ENODEV, ENXIO, ENOEXEC, EINVAL, ENOENT,
    EPIPE, ENAMETOOLONG, ENOSYS, ENOTEMPTY, ENOSPC, EFAULT, EBADF, EPERM, EINTR,
    E2BIG, ESPIPE, ENOMEM, EXDEV, EAGAIN, ENFILE, EMFILE, ECHILD, EROFS,
    /* errnos only in winsock_error_to_errno */
    EWOULDBLOCK, EINPROGRESS, EALREADY, ENOTSOCK, EDESTADDRREQ, EMSGSIZE,
    EPROTOTYPE, ENOPROTOOPT, EPROTONOSUPPORT, EOPNOTSUPP, EAFNOSUPPORT,
    EADDRINUSE, EADDRNOTAVAIL, ENETDOWN, ENETUNREACH, ENETRESET, ECONNABORTED,
    ECONNRESET, ENOBUFS, EISCONN, ENOTCONN, ETIMEDOUT, ECONNREFUSED, ELOOP,
    EHOSTUNREACH
    };

int errnos_contains(int errnum)
{
    for(int i=0;i<53;i++)
	if(errnos[i]==errnum)
	    return 1;
    return 0;
}
#endif

void test_mingw__no_strerror_shim_on_ucrt(void)
{
#if defined(GIT_WINDOWS_NATIVE) && defined(_UCRT)
    cl_assert_(strerror != mingw_strerror,
	"mingw_strerror is unnescessary when building against UCRT");
#else
    cl_skip();
#endif
}

void test_mingw__strerror(void)
{
#if defined(GIT_WINDOWS_NATIVE) && !defined(_UCRT)
    for(int i=0;i<53;i++)
    {
	char *crt;
	char *mingw;
	mingw = mingw_strerror(errnos[i]);
	crt = strerror(errnos[i]);
	cl_assert_(!strcasestr(mingw, "unknown error"),
	    "mingw_strerror should know all errno values we care about");
	if(!strcasestr(crt, "unknown error"))
	    cl_assert_equal_s(crt,mingw);
    }
#else
    cl_skip();
#endif
}

void test_mingw__errno_translation(void)
{
#if defined(GIT_WINDOWS_NATIVE) && !defined(_UCRT)
    /* GetLastError() return values are currently defined from 0 to 15841,
    testing up to 20000 covers some room for future expansion */
    for (int i=0;i<20000;i++)
    {
	if(i!=ERROR_SUCCESS)
	    cl_assert_(errnos_contains(err_win_to_posix(i)),
		"all err_win_to_posix return values should be tested against mingw_strerror");
	/* ideally we'd test the same for winsock_error_to_errno, but it's static */
    }
#else
    cl_skip();
#endif
}
