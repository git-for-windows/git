Modern software development relies heavily on a way to manage dependencies, i.e. to keep track of required software libraries and their versions. Examples are `apt` for Linux, `homebrew` for macOS, `maven` for Java and `pip` for Python.

Git for Windows is based on [MSYS2](https://msys2.github.io/) which bundles the [Pacman](https://wiki.archlinux.org/index.php/Pacman) tool (known from Arch Linux) for dependency management.

# How to use `pacman`

There is a `man` page for `pacman` and the tool also sports a `--help` option. These resources are recommended to address questions not covered by the following, brief descriptions.

## Install/upgrade packages

To install a package, run

```bash
pacman -S <package-name>
```

To ensure that the newest package version is installed, it is recommended to pass the `-y` option, too, which asks Pacman to download the newest package list:

```bash
pacman -Sy <package-name>
```

To upgrade all packages to their newest versions, call

```bash
pacman -Syu
```

### Updating `msys2-runtime`, `pacman` and `bash`

As `pacman.exe` is itself an MSYS2 executable, it is strongly suggested to update `msys2-runtime`, `bash` and `pacman` packages separately from other packages if they need to be updated, and let `pacman` quit *immediately* afterwards.

Likewise, if you run `pacman` from a `bash` -- an MSYS2 program, too -- you should quit the shell *immediately* (it might show an infinite stream of heap messages instead of quitting, requiring to be force-quit).

## Remove packages

```bash
pacman -R <package-name>
```

## List packages

To list the installed packages, call

```bash
pacman -Q
```

To list the contents of a package, call

```bash
pacman -Ql <package-name>
```

To find out what package a file belongs to, call

```bash
pacman -Qo <file-name>
```

# Technical details

## Rebuild packages

If you want to rebuild a package, the first order of business is to know which repository has the metadata for the package. Git for Windows has three repositories containing such metadata:

- [build-extra](https://github.com/git-for-windows/build-extra) contains the `mingw-w64-git-extra` package information,
- [MINGW-packages](https://github.com/git-for-windows/MINGW-packages) contains the information for the MINGW packages, i.e. packages that do not require any POSIX emulation; by convention, their package name have the `mingw-w64-` prefix, and
- [MSYS2-packages](https://github.com/git-for-windows/MSYS2-packages) contains the information for all packages that require a POSIX emulation, such as Bash, OpenSSH, etc. The `MSYS2-packages` repository also contains the information of the package *providing* the POSIX emulation: [`msys2-runtime`](https://github.com/git-for-windows/msys2-runtime) (see also [Building msys2-runtime](Building-msys2-runtime)).

To build MINGW packages, start a shell (a *MINGW* shell is recommended), clone the [`MINGW-packages`](https://github.com/git-for-windows/MINGW-packages) repository (recommended location: `/usr/src/MINGW-packages`), `cd` to the appropriate subdirectory and call

```bash
makepkg-mingw -s
```

(The `-s` flag tells `makepkg` that it should install dependencies automatically as needed.)

To build MSYS packages, start a shell (the *MSYS* shell is recommended), clone the [`MSYS2-packages`](https://github.com/git-for-windows/MSYS2-packages) repository (recommended location: `/usr/src/MSYS2-packages`), `cd` to the appropriate subdirectory and call

```bash
makepkg -s
```

If you have modified any source files you need to update the checksums using the `updpkgsums` command before running `makepkg` or `makepkg-mingw`.

*Note*: Before building the first package, as per [MSYS2's own documentation](https://github.com/msys2/msys2/wiki/Creating-Packages) you need to install the development packages for development:

```sh
pacman -Sy base-devel msys2-devel mingw-w64-x86_64-toolchain mingw-w64-i686-toolchain
```

### Build packages from locally-patched sources

When testing Pull Requests or debugging certain issues, it is convenient to build packages from source code other than the canonical one listed in the `PKGBUILD` file. This can be achieved by switching to the subdirectory of `/usr/src/MINGW-packages` or `/usr/src/MSYS2-packages`, respectively, corresponding to the package you want to build, ensure that the `src/` directory is populated (and call `makepkg-mingw --nobuild -s` or `makepkg --nobuild -s` otherwise), then patch the source code in the `src/` subdirectory and after that call

```bash
makepkg-mingw --noextract --noprepare
```

or

```bash
makepkg --noextract --noprepare
```

to build the package.

### More details about rebuilding packages

The `makepkg` script is part of the `pacman` package itself. It expects a [`PKGBUILD`](https://wiki.archlinux.org/index.php/PKGBUILD) file in the current directory that contains metadata about the package and functions specifying how to prepare the source code, build the executables, and package all the files.

## Perl package management

Perl packages are managed outside of the `pacman` realm, but instead with [CPAN](http://www.cpan.org/):

```bash
perl -MCPAN -e 'install <package-name>'
```

CPAN also offers an interactive shell:

```bash
perl -MCPAN -e shell
```

## Repository structure

Pacman repositories are served via HTTP, as static files in a single directory. The most important file in that directory is the *package index*, called `<name>.db.tar.xz` by convention. This package index can be updated via `repo-add <package-index> <package-file>...` (this updated *only* the package index, it does *not* copy the package files into the same directory). Pacman expects to find the package files referenced in the package index in the same directory as the index.

The *Git for Windows*-specific packages are served by [Azure Blob Storage](https://azure.microsoft.com/en-us/services/storage/blobs/), see below. We ship MSYS2 and MINGW packages for two architectures, `i686` and `x86_64`. Pacman is configured to use these in `/etc/pacman.conf`.

# Further reading

- [Building new package versions](Building-new-package-versions)
- [Upgrading the `perl` component to a new version](Upgrading-the-%60perl%60-component-to-a-new-version)
