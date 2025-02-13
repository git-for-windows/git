As described in [Package management](Package-management.md), Git for Windows' SDK is a close derivative of the [MSYS2](https://msys2.github.io/) system, and as such, its components are built using `makepkg` and `makepkg-mingw` (borrowed from Arch Linux' [`pacman`](https://wiki.archlinux.org/index.php/Pacman)).

Most of those packages are actually built by the MSYS2 project and consumed by Git for Windows. However, a couple of components (including cURL and OpenSSH) _are_ built by Git for Windows, e.g. to be able to react faster to newly-available versions, or to allow for modifications specific to Git for Windows.

In general, the process to upgrade components to new versions is quite tedious.

First of all, the Git for Windows maintainer needs to be aware that a new version of a Git for Windows SDK component is available. The current process for this involves using a custom IFTTT applet to receive notifications when, say, [cURL's Atom feed](https://github.com/curl/curl/tags.atom) reports a new version.

The next step is to update the package definitions (i.e. Pacman's [`PKGBUILD`](https://www.archlinux.org/pacman/PKGBUILD.5.html) files), to reflect the new version and the checksum for the respective archive.

Git for Windows offers some automation for that, figuring out the current version of a specified component in [its administrative script called `please.sh`](https://github.com/git-for-windows/build-extra/blob/master/please.sh) and updating the respective `PKGBUILD`, then building and uploading the Pacman package. The idea is to call e.g. `sdk cd build-extra && ./please.sh upgrade curl` to upgrade cURL.

This script is, essentially, the backbone of the two Azure Pipelines used by the Git for Windows project to build new package versions: [Build and publish MINGW Pacman package](https://dev.azure.com/git-for-windows/git/_build?definitionId=32) and [Build and publish MSYS Pacman package](https://dev.azure.com/git-for-windows/git/_build?definitionId=33).

However, many a time this automation fails, for a variety of reasons, including, but not limited to:

- Download links do change. If this is the case, the `source` definition in the `PKGBUILD` needs to be adjusted.
- Patches that are applied as part of the `build` function in the `PKGBUILD` script might no longer apply to newer versions (see below).
- Spurious network errors. Those are the maintainer's favorite hiccups.

# Adjusting patches when they no longer apply to new versions

The way `makepkg` works is unfortunately not very integrated with Git (as it was invented _before_ Git), therefore the way it works is to specify the URL to the archive (or to the branch in a repository) as `source`, together with any patches needed to compile it in MSYS2. The archive will be unpacked in the `src/` directory automatically, but the patches need to be applied specifically as part of the `prepare` step in the `PKGBUILD`.

When these patches no longer apply after upgrading to a newer version of the component, it can be a challenge to fix that.

The strategy that worked best for the Git for Windows maintainer goes like this:

1. initialize a new Git repository in `src/playground`
2. use `/usr/src/git/contrib/fast-import/import-tars.perl` to import the _previous_ version (i.e. the one where the patches still apply)
3. create a new branch from the import tag (`git switch -c rebase-to-<version> import-tars`)
4. if the patches are actually in mbox format, they can be imported using `git am ../../*.patch`; otherwise they need to be manually applied and committed (something like `srcdir=../..; grep "patch -p" $srcdir/PKGBUILD | while read line; do patch="${line##*/}" && eval "$line" && git commit -m "$patch" || break; done`)
5. import the new version using `import-tars.perl`
6. create a new branch from the current one (`git switch -c rebase-to-<new-version>`)
7. rebase the patches (`git rebase -i --onto <component>-<new-version> <component>-<version>`)
8. export the patches (if they were in mbox format, `git format-patch -<N> -o ../..`, otherwise a variation of `for commit in $(git rev-list HEAD); do patch="$(git show -s --format=%s $commit)" && git diff $commit^! >../../"$patch" || break; done`)
9. test (`cd ../.. && sdk build`)
10. commit, push (and open a PR unless pushing directly to `main`)

Caveat: this strategy needs manual adjustments if the archive in question contains files with DOS line endings (I am looking at you, [Perl](https://github.com/git-for-windows/git/blob/HEAD/Documentation/git-for-windows/Upgrading-the-%60perl%60-component-to-a-new-version.md) _shakes fist_): `import-tars.perl` will happily import those verbatim, but the `bsdtar` used by `makepkg` to unpack the archive will convert them to Unix line endings, and if the patches expect DOS line endings, they won't apply.