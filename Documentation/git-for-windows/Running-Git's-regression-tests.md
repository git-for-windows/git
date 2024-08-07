# Running tests

Git's source code ships with a lot of [regression tests](http://en.wikipedia.org/wiki/Regression_testing). This gives us a pretty good indicator of the source code's health: if a regression test starts failing for a topic branch, it is safe to assume that there is some "room for improvement" in the changes introduced by said topic branch.

It is a good practice to add a regression test when fixing bugs. Such a test not only illustrates what is going wrong (and lets other developers verify that there is indeed a bug that needs fixing), it also ensures that it is much harder to re-introduce that bug by mistake in the future: the regression tests would sound an alarm.

## Running the complete test suite

Git's test suite can be run in various ways:

- by running `make test` in the directory where Git's sources were checked out (`/usr/src/git/` by default)
- by running `make` in the `t/` subdirectory of `/usr/src/git/`
- using Perl's `prove` test runner, by running `make prove` in the `t/` subdirectory of `/usr/src/git/`

It can take quite a while to run the test suite. To help, Git allows to run those tests in parallel, e.g. by passing the `-j` flag to `make`: `make -j5` would run up to five tests in parallel.

# Running individual tests

When some tests fail, it is best to run them individually. This can be done using `make`, of course, a much better way is to run them via `sh`, though, allowing for more fine-grained control:

- `sh t0001-*.sh -i` will run *t0001* and stop at the first failing test (as opposed to the default, to continue running the rest of the tests in the *t0001* test suite)
- `sh t0001-*.sh -v` will show the output of the programs called by the tests
- `sh t0001-*.sh -d` will keep the temporary directory in which the test was run, `trash directory.t0001-init/` (this is useful when comparing the output of tests between Git versions when the tests fail for one Git version but succeed for another).
- `sh t0001-*.sh -x` will print out the command-line of the programs before they are executed

These options can be combined, too.

## Further debugging techniques

### Piping the output to a file

Often there will be a ton of output and not all of that output is relevant to investigate test failures. In those cases, it makes sense to redirect the output to a file:

```bash
sh t0001-*.sh -i -v -x 2>&1 | tee log.out
```

Alternatively, the output can be redirected to a pager:

```bash
sh t0001-*.sh -i -v -x 2>&1 | less
```

To enable a quicker round-trip for running tests when running them repeatedly (typically changing the code a tad after each iteration), you can ask the `less` pager to fake key-presses just after starting. For example, the following command-line will always search for the first occurrence of the characters "ERROR:" in the output:

```bash
sh t0001-*.sh -i -v -x 2>&1 | less +/ERROR:
```

### `set -x`

The statement `set -x` in a shell script will turn on the execution trace (see the explanation of the `sh ... -x` invocation above), and `set +x` will turn it off. This is not only useful for adding debug output to the regression tests; It is also useful in the parts of Git that are still implemented as shell scripts (such as `git submodule`).

For even more sophisticated debugging, the `set -x` statement can be triggered conditionally. The following example would turn on execution tracing only when the current commit name starts with *cafebabe*:

```bash
case "$(git rev-parse HEAD 2> /dev/null)" in cafebabe*) set -x;; esac
```

### Tracing execution in Perl scripts

Some parts of Git are implemented as Perl scripts. To trigger execution tracing similar to Bash's `set -x` statement, you can add the `-d:Trace` parameter to the shebang line (i.e. to the first line of the Perl script, which reads "#!/usr/bin/perl"). This requires the `Devel::Trace` Perl package to be installed, e.g. by calling `perl -MCPAN -e 'install Devel::Trace'` once.

### `GIT_TRACE=1`

When Git sees that the environment variable `GIT_TRACE` is set, it will print out an internal execution trace when Git wants to call external executables and builtins. This is extremely helpful in particular when debugging posix-to-windows mangling issues with the MSYS2 runtime. All you do is to prefix the Git command to be executed in the test script with `GIT_TRACE=1 `, e.g. `GIT_TRACE=1 git difftool --extcmd "$2"`.

### Interactive Bash/GDB

Sometimes it is a pretty interesting option to investigate an intermediate state of a working directory in the middle of a test regression by starting an interactive shell right at that moment. There is just *one* problem with inserting `bash &&` into the test's code: Git automatically redirects `stdin`/`stdout`/`stderr`. This needs to be switched off explicitly by editing the `test_eval_` function in `t/test-lib.sh`, deleting all those redirections.

To run the GNU debugger (`gdb`), these redirections need to be disabled, too, and in addition it is a good idea to recompile the entire Git sources after removing the `-O2` flag from the `CFLAGS` in the `Makefile`: For years, gdb has problems to identify exact code locations when the code was compiled with optimizations.

### Plain old debug print statements

When all else fails, and in particular when no interactive debugger is available, the only remaining debugging technique is to output print statements, i.e. insert `fprintf(stderr, ...);` (or conveniently `error(...)`) into C code, `echo ... >&2` into shell code, `print(STDERR, ...);` into Perl code etc.

It gets even trickier when there is no `stdout`/`stderr` available (e.g. when debugging issues with the "remote" side of a push/fetch), in which case the debug information should be written (appended) into a file. In C, you would do this via `{ FILE *f = fopen("C:\\log.out", "a"); fprintf(f, ...); fclose(f); }`, in shell via `echo ... >> /c/log.out` and in Perl via `{ my $fh = open(">>c:\\log.out"); print($fh ...); close($fh); }`.

While it is a little cumbersome to add such debug print statements (after all, you typically have to rebuild the executables and run the tests from the top), there is also a big benefit to this technique over single-stepping: the debug output can be made conditional upon the particulars of the problem to be debugged. For example, instead of writing out information *every* time, say, `get_sha1_hex()` is called, it can be written out *only* the third time it is called with an SHA-1 beginning with two specific byte values. This technique can also be combined with single-stepping, by setting a breakpoint on that conditional debug output, saving a lot of time to get back to the same point after modifying and recompiling the source code.

There exist even more problematic situations when working on Git for Windows, though: when trying to figure out issues in the MSYS2 runtime, there might not be any `fprintf(...)` functionality available at that point of execution (yet). In this case, you need to revert to the plain Win32 API to write into a file:

```c
...
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef ERROR
...
{ HANDLE h = CreateFile("C:\\log.out", FILE_APPEND_DATA, FILE_SHARE_READ, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL); if (h) {
#undef DDD
#define DDD(str) WriteFile(h, str, strlen(str), NULL, 0);
DDD("Hello");
DDD(...);
DDD("\n");
CloseHandle(h);
} }
```

Of course, if it is possible to write to `stderr` instead, the code should use `HANDLE h = GetStdHandle(STD_ERROR_HANDLE);` instead of the `CreateFile()` call instead, and *not* close the handle...
