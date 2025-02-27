Git for Windows' `main` branch is kept in an always-releasable state as much as possible.

For example, in case that a critical bug is discovered that really needs to be fixed within the same day, Git for Windows does not have a maintenance branch (or, "stable" branch). Such a fix would be applied on top of `main` and a new version would be created from that state.

To allow users to help verifying that "always-releasable state", as well as to allow users to verify fixes introduced via PRs, Git for Windows builds "snapshot" releases (published [here](https://wingit.blob.core.windows.net/files/index.html)) whenever the `main` branch advances.

Those snapshot releases are considered robust. They are built using [the exact same Azure Pipeline](https://dev.azure.com/git-for-windows/git/_build?definitionId=34&_a=summary) that also builds official Git for Windows versions (as well as the `-rc` pre-releases leading up to every major version). Snapshot versions come in the same flavors as full Git for Windows versions: 32-bit and 64-bit, installers, portable Gits, MinGits.

Snapshot versions are signed the same way as full Git for Windows versions, too. Apart from the version number and from the fact that they are not uploaded as [full GitHub Releases](github.com/git-for-windows/git/releases), they are pretty much indistinguishable from official Git for Windows versions.