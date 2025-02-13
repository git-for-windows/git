# The quick way

Download the [Git for Windows SDK](https://gitforwindows.org/#download-sdk), install it and run `sdk build installer`.

If you need to include a Git version built with custom patches, call `sdk cd git`, then make sure that it is at the commit you want, then call `sdk build git-and-installer`.

## Debugging/Developing a single installer "wizard page"

Sometimes the wording needs to be changed, or the layout, or something, in a single page of the installer. To avoid having to build the entire installer (and compressing all of Git for Windows), run `sdk cd installer` and run `./release.sh -d <page>`. For example, to modify the page where Git's default editor can be configured, run this:

```
sdk cd installer
./release.sh -d Editor
```

# Detailed overview

As the installer is made by putting together files from the Git SDK, it is advisable to update those files first. To update the packages, call `pacman -Syu` ([caveats apply](https://github.com/git-for-windows/git/blob/HEAD/Documentation/git-for-windows/Package-management.md#updating-msys2-runtime-pacman-and-bash)).

If you want to test Git changes or make an installer from your own Git fork, you need to call

```bash
cd /usr/src/git
make install
```
for documentation changes:

```bash
make install-html
```

and *afterwards*

```bash
prefix=/mingw64 make -C contrib/subtree install-html
```

>without this second `make` you get an error message about a missing `git-subtree.html` file).

To make an installer, install the [SDK](https://gitforwindows.org/#download-sdk) and run it

>Git for Windows SDK MinGW shell can be run by double-clicking either the Shortcut on the desktop Git SDK 32-bit.lnk or by double-clicking mingw32_shell.bat in the install folder. That is Git SDK 64-bit.lnk and mingw64_shell.bat for the 64bit SDK installer.

then issue the following commands:

```bash
cd /usr/src/build-extra
git pull main
./installer/release.sh <version>-test
```

where `<version>` is the Git version (please note that the `<version>` cannot contain dots after the numerical version: `2.7.2-hello-world` is okay while `2.7.2.hello.world` is not okay).

# Why is this installer so much larger than the official installer?

Short version: you will need to run `make strip` in `/usr/src/git` before `make install`, and you will also need to run to call `pacman -S mingw-w64-x86_64-git-extra` afterwards (replace `x86_64` with `i686` for 32-bit builds and with `clang-aarch64` for Windows/ARM64 builds).

## Why `make strip`?

The `make strip` command removes the debug information from the `.exe` files. That is not quite the same as the official release process does: it calls `cv2pdb` which splits out the debug information from the `.exe` files into separate `.pdb` files, but that processing is not defined in `/usr/src/git/Makefile`, but instead in `/usr/src/MINGW-packages/mingw-w64-git/PKGBUILD`.

## Why `pacman -S mingw-w64-x86_64-git-extra`?

The `pacman -S mingw-w64-x86_64-git-extra` part is defined elsewhere yet: in `/usr/src/build-extra/please.sh`. Its purpose is to replace many hardlinked copies in `/mingw64/libexec/git-core/git-*.exe` with what Git for Windows calls the "Git wrapper": a small executable that does nothing else but call `git.exe` with the appropriate subcommand name.

You see, many "built-in" commands are implemented right there in `git.exe`, with no need to actually execute anything else. For example, when you call `git show`, the show part is executed inside `git.exe` itself.

This is in contrast to, say, `git send-email`, which actually executes `/mingw64/libexec/git-core/git-send-email` (a Perl script). For historical reasons, Git wants to provide also `git-show.exe` in `/mingw64/libexec/git-core/`, even if that is not at all necessary because the show command is a built-in command. Therefore, Git has this quirk where it first looks at the name of the executable via which it was called (e.g. if it was called via `/mingw64/libexec/git-core/git-show.exe`, it extracts the show part and knows that it should internally execute `git show` instead. And then, Git simply installs hardlinked copies of `git.exe` in `/mingw64/libexec/git-core/` for all built-in commands.

Since they are hard-links, they only use a minuscule amount of disk space. However, too many Git for Windows users looked at the disk usage of those hard-linked copies using the Windows Explorer, which historically completely ignored the fact that hard-linked copies did not use additional disk space (apart from the directory entry). As a consequence, we had many reports that Git for Windows occupied way too much space. That was not true, of course, but those reports took so much time to address that it was easier to install the Git wrapper (which is copied, but it is tiny compared to git.exe).
