Branches used in this repo
==========================

The document explains the branching structure that we are using in the VFSForGit repository as well as the forking strategy that we have adopted for contributing.

Repo Branches
-------------

1. `vfs-#`

    These branches are used to track the specific version that match Git for Windows with the VFSForGit specific patches on top.  When a new version of Git for Windows is released, the VFSForGit patches will be rebased on that windows version and a new gvfs-# branch created to create pull requests against.

    #### Examples

    ```
    vfs-2.27.0
    vfs-2.30.0
    ```

    The versions of git for VFSForGit are based on the Git for Windows versions.  v2.20.0.vfs.1 will correspond with the v2.20.0.windows.1 with the VFSForGit specific patches applied to the windows version.

2. `vfs-#-exp`

   These branches are for releasing experimental features to early adopters. They
   should contain everything within the corresponding `vfs-#` branch; if the base
   branch updates, then merge into the `vfs-#-exp` branch as well.

Tags
----

We are using annotated tags to build the version number for git.  The build will look back through the commit history to find the first tag matching `v[0-9]*vfs*` and build the git version number using that tag.

Full releases are of the form `v2.XX.Y.vfs.Z.W` where `v2.XX.Y` comes from the
upstream version and `Z.W` are custom updates within our fork. Specifically,
the `.Z` value represents the "compatibility level" with VFS for Git. Only
increase this version when making a breaking change with a released version
of VFS for Git. The `.W` version is used for minor updates between major
versions.

Experimental releases are of the form `v2.XX.Y.vfs.Z.W.exp`. The `.exp`
suffix indicates that experimental features are available. The rest of the
version string comes from the full release tag. These versions will only
be made available as pre-releases on the releases page, never a full release.

Forking
-------

A personal fork of this repository and a branch in that repository should be used for development.

These branches should be based on the latest vfs-# branch.  If there are work in progress pull requests that you have based on a previous version branch when a new version branch is created, you will need to move your patches to the new branch to get them in that latest version.

#### Example

```
git clone <personal fork repo URL>
git remote add ms https://github.com/Microsoft/git.git
git checkout -b my-changes ms/vfs-2.20.0 --no-track
git push -fu origin HEAD
```
