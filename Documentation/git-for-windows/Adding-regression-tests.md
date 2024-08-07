Git comes with an extensive regression test suite. It lives in the [`t/` subdirectory](https://github.com/git/git/tree/HEAD/t) of the source code repository and is organized into test scripts, e.g. `t8002-blame.sh`, which contain multiple test cases. The test scripts themselves are shell scripts.

The best documentation how to add tests is the test suite itself, by example.

# Adding test cases

You will want to add test cases either to demonstrate bugs, or to ensure that certain operations work as expected (e.g. when you just implemented such an operation or fixed a bug).

In the most common case, you will want to add your test case to an existing test script. Use the file name and `git grep`'s output as an indicator which script is the best one to which to add your test case.

Each test case is written in the form

```sh
test_expect_success 'title' '
    # Here come the shell script statements
'
```

If you want to demonstrate a breakage, use `test_expect_failure` instead, and write the rest of the test case as if the bug was fixed, i.e. the way you want to see it succeed.

Inside the script part, you need to connect everything into an ‘&&’ chain, i.e.

```sh
    test_commit this-will-write-a-file-and-commit &&
    echo New file >new-file.txt &&
    git status --porcelain --verbose --verbose >output &&
    grep new-file.txt output
```

If you need to test anything Windows-specific, you can put `MINGW` before the case’s title, as a prerequisite. Likewise, you can use `!MINGW` to test only on non-Windows.

Every script creates a test repository with a test working tree in a new directory called `t/trash directory.<basename>`, e.g. `t/trash directory.t8002-blame/`.

Traditionally, the first “test case” is the setup, where you set up files and commits common to multiple test cases in the same script.

There are a bunch of really useful functions for use in test scripts, e.g. `test_commit`, which not only abbreviates the common “write a file, add it, commit it, tag the commit” stanza, but also increments the timestamp from a fixed first timestamp, so that the commits are reproducible (read: so you can reliably debug even if the bug depends on some side effect of some compression or some such). You will find a comprehensive list in [`t/README`](https://github.com/git/git/blob/HEAD/t/README) under the “Test harness library” heading.

There are also a bunch of useful test helpers in `t/helper/` (automatically added to the `PATH`) e.g. `test-chmtime`. If you need to test native functions, you can introduce a new test helper, or piggy-back onto an existing one.

As you will most likely add a test case to an existing script, you can use whatever the test repository’s current state is. To find out, just run the script with the `-d` option, which will leave the trash directory behind instead of removing it after a successful conclusion of the script: `cd t && sh t8002-blame.sh -d`.

In addition to `-d`, I found the options `-i` (stop upon first failing test case), `-x` (show the tests’ commands before they are executed) and `-v` (show the entire output instead of suppressing it) useful.

If you want to use `-x`, it is best to run through bash instead of sh, as some tests require a bash feature where the trace is printed to a different file descriptor than stderr, although in Git for Windows’ SDK, sh still refers to bash, so it is the same.
