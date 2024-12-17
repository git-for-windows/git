# The problem

Windows does not properly support files and directories longer than 260 characters. This applies to Windows Explorer, `cmd.exe` and many other applications (including many IDEs as well as bash, perl and tcl that come with Git for Windows).

# Opt-In configuration

For this reason, long paths support in Git for Windows is disabled by default.

That being said, long paths support for C-based git commands can be enabled by setting the `core.longpaths` option to true. Scripted git commands may still fail with this option, so use at your own risk. Example:

```sh
git config core.longpaths true
```

# Technical background

The root cause of the technical limitation of 260 chars lies deeply within the Win32 API.
Microsoft's online article [Naming Files, Paths, and Namespaces](http://msdn.microsoft.com/en-us/library/windows/desktop/aa365247(v=vs.85).aspx#maxpath) describes the reasons.
Git's source code itself does not have the limitation, it is Windows-only.

# Implementation details

Originally [introduced into Git for Windows 1.9.0](https://github.com/msysgit/git/pull/122), the idea to overcome the length limitation is to convert them into UNC paths (i.e. network paths of the form `\\?\...`). Such paths are accepted by many of the Win32 API's functions accepting UTF-16 strings (many functions in the Win32 API are available as ASCII and UTF-16 functions, marked by an `A` and a `W` suffix, respectively) and are allowed to have a length up to 32,767 wide characters. See e.g. [this commit](https://github.com/git-for-windows/git/commit/38b94fe15fb60e3871a166eec8cfd4265fee727f) for more details.
