Under certain circumstances, Git can become very slow on Windows. Here are a couple of hints to figure out why.

# Identify specific Git commands' performance issues

If the problem lies with a specific Git command, the most straight-forward way to investigate is to use profiling tools.

The canonical profiling tool in GCC's tool set is [`gprof`](https://sourceware.org/binutils/docs/gprof/). However, the MINGW version of the GCC tools offered by MSYS2 has been known to produce empty output on occasion.

An more convenient alternative is to use [Visual Studio's Performance Profiler](https://github.com/git-for-windows/git/blob/HEAD/Documentation/git-for-windows/Performance-profiling-with-Visual-Studio.md) (which is available in the free-of-cost Community version) which offers a powerful graphical user interface.

# Trace executions in the Bash startup

When starting Git Bash is already slow, edit the file `<GIT_HOME>/etc/profile` and insert a `set -x` somewhere at the top. This command will tell Bash to echo the commands it is executing so that you can find out which commands are slow and investigate more closely in that direction.

# Activate Git's own tracing

Git often executes subcommands, hence it is possible that you run a certain Git command but the tardiness stems from a *different* Git command. To find out which commands Git executes internally, set the environment variable `GIT_TRACE` like so:

```bash
GIT_TRACE=1 git stash
```

# Redirecting output to a file

Sometimes the output is too verbose – or the command-line window flashes and closes too quickly to read – to make direct diagnosing of the output practical. In this case, simply redirect the entire output of the Bash to a file, like so:

```bash
exec > "$HOME"/Desktop/debug.txt 2>&1
```

After diagnosing the problem, you *will* want to remove the redirection, otherwise you will not be able to interact normally with the Bash.

# Enable the filesystem cache

Windows' filesystem layer is inherently different from Linux' (for which Git's filesystem access is optimized). As a workaround, Git for Windows offers a filesystem cache which accelerates operations in many cases, after an initial "warm-up". This workaround can be activated in the installer, on the last wizard page, or you can activate the filesystem cache per-repository:

```bash
git config core.fscache true
```

# Avoid inspecting large working trees' modification times

When working with large working trees, Git's (frequent) checking whether files were modified since Git's internal index was last updated can lead to substantial lags. In such a case, it can make sense to switch off this check, but it comes at a price: it requires discipline on the developer's side to keep track which files were changed *and `git add` them explicitly for the next commit* (the output of `git status` will *no longer* identify modified files). You can disable the check per-repository thusly:

```bash
git config core.ignoreStat true
```