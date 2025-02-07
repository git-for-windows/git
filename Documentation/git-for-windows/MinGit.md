# MinGit is *Git for Windows Applications*

## Rationale

Git for Windows targets end users, and for a long time third-party applications that spawn `git.exe` in order to provide Git-specific functionality piggy-backed onto the end-user installation.

However, for third-party applications it can be quite important to target certain minimum Git versions, e.g. when features such as `git reset --stdin` are needed that were introduced only recently.

Bundling a complete portable Git for such cases may very well be overkill, as the applications that want to call Git usually do not require *all* of Git for Windows' functionality, *especially* not the interactive features including Git GUI and Git Bash.

[Enter MinGit](https://devblogs.microsoft.com/devops/whats-new-in-git-for-windows-2-10/#mingit-git-for-windows-applications).

MinGit is an intentionally minimal, non-interactive distribution of Git for Windows, with third-party applications as its intended audience.

## What is included in (or excluded from) MinGit?

MinGit bundles `git.exe` and supporting cast, with an eye on trying to keep the size as small as possible without breaking non-interactive Git usage. To reduce the size, MinGit excludes localized messages, interactive commands, help documents, executables that are not called by `git.exe`, and the likes.

As Tcl/Tk is only included in Git for Windows to support the Git GUI and `gitk` (both graphical, interactive components of Git), it is excluded from MinGit.

Further, a conscious decision was made to exclude Perl. It is a *large* contributor to Git for Windows' size, and only very few, non-essential functions of Git require it:

* `git add -i` (interactive, hence not needed)
* `git send-email` (highly unlikely that 3rd-party applications want to use *that*...)
* CVS/Subversion adapters

The only really contentious part of that list is the Subversion adapter, `git svn`. However, the space savings are substantial enough, and Subversion is no longer *that* prevalent enough, to ask third-party applications that *do* want to call `git svn` to bundle a full-fledged portable Git.

# Experimental: BusyBox-based MinGit

[BusyBox](https://busybox.net) is a project providing a single executable that implements small versions of a multitude of Unix utilities such as `sed`, `awk`, and even a Unix shell called `ash`. BusyBox tries to strike a balance between size and functionality, not aiming for complete POSIX compliance. [BusyBox-w32](https://github.com/rmyorston/busybox-w32) is a pure Win32 port of BusyBox, i.e. a single `.exe` file implementing those utilities without using the MSYS2 runtime.

There are two advantages to avoiding the MSYS2 runtime: speed and robustness. In order to emulate POSIX semantics like `fork()`, the MSYS2 runtime needs to play certain tricks at startup, which slows down each and every invocation of MSYS2 programs, including Bash. Those tricks also entail a fixed DLL base address of `msys-2.0.dll` which in turn caused conflicts in some Git for Windows installations. Also, POSIX semantics for uids and gids need to be emulated for all kinds of different account types, sometimes leading to puzzling failures of the MSYS2 runtime to start.

Naturally, using BusyBox-w32 instead of the MSYS2 Bash (and of the MSYS2 versions of the other Unix utilities required by Git's Unix shell scripts) would let us benefit from these advantages.

Using BusyBox-w32 also offers a size advantage, being a single, tiny `.exe` file rather than a collection of `.dll` files and `.exe` files including more functionality than is strictly needed for Git's purposes.

As it turns out, BusyBox' utilities provide all the functionality required to run Git's Unix shell scripts, and even almost all the functionality to run Git's test suite.

Git for Windows v2.14.0 features experimental BusyBox-based MinGit versions.

Note: the BusyBox-based MinGit versions are marked as experimental for at least the following reasons:
- BusyBox-based MinGit has not seen any serious, real-life testing,
- users might want to use more obscure options of the Unix utilities shipped with Git for Windows in their hooks or aliases, options that BusyBox may not (yet) support.

Please also note that even if MinGit eventually switches to BusyBox completely, Git for Windows' installers will still ship with MSYS2's Bash for the time being:
- BusyBox-w32 has no idea (yet) about Git Bash's interactive terminal, and therefore does not support any interactive usage,
- Git Bash promises a full-featured *Bash*, not BusyBox' `ash`,
- with Git for Windows, it is even more likely that users may want to use functionality in their hooks and/or aliases which is not supported by BusyBox.