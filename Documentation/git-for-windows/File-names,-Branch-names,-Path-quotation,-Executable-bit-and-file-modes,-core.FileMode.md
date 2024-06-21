# Case Preservation, Case insensitivity

Windows is case insensitive, but case preserving for filenames. Meanwhile Linux/Posix, for which Git is designed, is case sensitive.

In addition Git branch names are stored as filenames.

This means that in normal usage, Git will have difficulty when it meets a changed case file or branch. Lower case is common for Linux usage!

E.g. `Main` branch and `main` branch are different, even though, on Windows, Git will often mistake one for the other (because Windows will read the 'other' case version of the file.

Likewise Windows will not be able to check out both an upper case and lower case version of a file(s). Apparently the vim repository has this 'two versions' Vim/vim problem.

Windows also cannot checkout files which contain special characters, such as colon(:) etc. This can cause cross system management issues.

# Path Quotation

Spaces in filenames is common in Windows, but rare in Linux. Linux has, like Windows, multiple path quoting mechanisms. In addition Linux can accept any character in any filename, including 'special' characters. This means that the path conversion between Windows and Git is imperfect and dependant on the particular command structure and 'terminal' in use.

Many Git commands do not report errors in the way that maybe expected by Windows users - some will simply ignore invalid options and paths, leaving the unfamiliar user wondering what happened. The `git config` is one area where this may happen, especially as there can be quoting confusion (single, double, or none) between bash, cmd, and powershell. (e.g. see [thread](https://public-inbox.org/git/d9330ba54fbda54a92a9f4d9320836d88ce9a6e6.camel@mad-scientist.net/))

# Executable and Mode Bits

Linux attaches a mode word to each file, with bits that indicate if the file is executable, readable, writable, and other ownership aspects (`chmod`). This does not map well to Windows semantics, especially for the executable bit. The existing mode bits are retained in the repository tree meta data, which is then held locally in the index.

Users may need to directly manipulate the mode bits where necessary - the [stackoverflow page](https://stackoverflow.com/a/38285462/717355) shows how to use the `--chmod` option while adding or updating files.

# core.FileMode

The core.FileMode configuration records, for the locally cloned repository, the local file system semantics (i.e. is it case sensitive / case preserving or not). DO NOT change this, unless it is actually set wrong. Git will trust this setting and mis-setting it may cause unexpected effects and damage - Don't do it, the internet, as ever, is probably wrong.

From https://git-scm.com/docs/git-config#git-config-corefileMode
> Tells Git if the executable bit of files in the working tree is to be honored.

> Some filesystems lose the executable bit when a file that is marked as executable is checked out, or checks out a non-executable file with executable bit on. git-clone[1] or git-init[1] probe the filesystem to see if it handles the executable bit correctly and this variable is automatically set as necessary.

# Windows Drive (C:) paths vs Bash rooted (/) paths

In the Git for Windows bash, the classic `C:` drive letter is replaced by the the implicit `/c/` directory path. This conversion is commonly used in the `git config` paths that need to be absolute.
