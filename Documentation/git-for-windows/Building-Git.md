We build Git for Windows using an [MSYS2](https://msys2.github.io/) based *Git for Windows SDK*. This SDK can be installed via the [SDK installer](https://gitforwindows.org/#download-sdk)

# Installing a build environment

1. Just run the [SDK installer](https://gitforwindows.org/#download-sdk).

# Build Git

1. An initial `git clone` and `make` should have already occurred when running the [SDK installer](https://gitforwindows.org/#download-sdk).

2. Open the *Git for Windows SDK* *MinGW* shell by double clicking either the Shortcut on the desktop `Git SDK 32-bit.lnk` or by double clicking `mingw32_shell.bat` in the install folder. That is `Git SDK 64-bit.lnk` and `mingw64_shell.bat` for the `64bit` [SDK installer](https://gitforwindows.org/#download-sdk).

2. Change directory to the initial clone: `cd /usr/src/git`. If the directory is empty you may have the `master` branch checked out: `git checkout main`.

4. (Optional) build Git: `make install`

5. Run the test suite: `make test`. If you are a fan of statistics, you can use the following `prove` invocation to run the testsuite. But first we have to change to the test directory with the command `cd t`. After that you can issue `/usr/bin/time prove -j 15 --state=failed,save ./t[0-9]*.sh`. If *15* threads are too many for your system, you can provide the number of threads via the `-j <num>` (j for jobs) parameter.

## Build Git from a Pull Request or another branch

Once the SDK built Git, it is *very* easy to build another revision of Git, such as per a different branch or Pull Request.

1. open the Git Bash of the SDK unless it is still open: execute the `git-bash.exe` binary in the top-level directory of the SDK,
2. switch the working directory: `cd /usr/src/git`,
3. fetch the Pull Request's revision or the branch:
 1. if you want to test a Pull Request, call `git fetch origin refs/pull/<id>/head`, where `<id>` is the number of the Pull Request (e.g. if you want to test Pull Request #606, you would call `git fetch origin refs/pull/606/head`),
 2. if you want to test a custom branch, call `git fetch <url> <branch>` instead, where `<url>` is the URL of the repository and `<branch>` is the name of the branch to test (e.g. if you wanted to test @dscho's `cool-new-feature` branch, you would call `git fetch https://github.com/dscho/git cool-new-feature`),
4. check out the revision that was just fetched: `git checkout FETCH_HEAD`,
5. continue as [above](https://github.com/git-for-windows/git/blob/HEAD/Documentation/git-for-windows/Building-Git.md#build-git) either by `make install` or `make test`.

# Updating to the newest Git for Windows version

```bash
cd /usr/src/git
git pull
make -j15 install install-html
```

## Testing the new Git version

As before:

```bash
cd /usr/src/git/t
/usr/bin/time prove -j 5 --state=failed,save ./t[0-9]*.sh
```

## See also Regression Testing

Single tests, block of tests, or the whole test suite can be run, as detailed in
[Running Git's regression-tests](https://github.com/git-for-windows/git/blob/HEAD/Documentation/git-for-windows/Running-Git%27s-regression-tests.md)


---
TODO: mention good practices to develop using Eclipse, MSVC
