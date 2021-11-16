Getting Started
===============

Registering existing Git repos
------------------------------

To add a repository to the list of registered repos, run `scalar register [<path>]`.
If `<path>` is not provided, then the "current repository" is discovered from
the working directory by scanning the parent paths for a path containing a `.git`
folder, possibly inside a `src` folder.

To see which repositories are currently tracked by the service, run
`scalar list`.

Run `scalar unregister [<path>]` to remove the repo from this list.

Creating a new Scalar clone
---------------------------------------------------

The `clone` verb creates a local enlistment of a remote repository using the
partial clone feature available e.g. on GitHub.


```
scalar clone [options] <url> [<dir>]
```

Create a local copy of the repository at `<url>`. If specified, create the `<dir>`
directory and place the repository there. Otherwise, the last section of the `<url>`
will be used for `<dir>`.

At the end, the repo is located at `<dir>/src`. By default, the sparse-checkout
feature is enabled and the only files present are those in the root of your
Git repository. Use `git sparse-checkout set` to expand the set of directories
you want to see, or `git sparse-checkout disable` to expand to all files. You
can explore the subdirectories outside your sparse-checkout specification using
`git ls-tree HEAD`.

### Sparse Repo Mode

By default, Scalar reduces your working directory to only the files at the
root of the repository. You need to add the folders you care about to build up
to your working set.

* `scalar clone <url>`
  * Please choose the **Clone with HTTPS** option in the `Clone Repository` dialog in Azure Repos, not **Clone with SSH**.
* `cd <root>\src`
* At this point, your `src` directory only contains files that appear in your root
  tree. No folders are populated.
* Set the directory list for your sparse-checkout using:
	1. `git sparse-checkout set <dir1> <dir2> ...`
	2. `git sparse-checkout set --stdin < dir-list.txt`
* Run git commands as you normally would.
* To fully populate your working directory, run `git sparse-checkout disable`.

If instead you want to start with all files on-disk, you can clone with the
`--full-clone` option. To enable sparse-checkout after the fact, run
`git sparse-checkout init --cone`. This will initialize your sparse-checkout
patterns to only match the files at root.

If you are unfamiliar with what directories are available in the repository,
then you can run `git ls-tree -d --name-only HEAD` to discover the directories
at root, or `git ls-tree -d --name-only HEAD <path>` to discover the directories
in `<path>`.

### Options

These options allow a user to customize their initial enlistment.

* `--full-clone`: If specified, do not initialize the sparse-checkout feature.
  All files will be present in your `src` directory. This uses a Git partial
  clone: blobs are downloaded on demand.

* `--branch=<ref>`: Specify the branch to checkout after clone.

### Advanced Options

The options below are not intended for use by a typical user. These are
usually used by build machines to create a temporary enlistment that
operates on a single commit.

* `--single-branch`: Use this option to only download metadata for the branch
  that will be checked out. This is helpful for build machines that target
  a remote with many branches. Any `git fetch` commands after the clone will
  still ask for all branches.

Removing a Scalar Clone
-----------------------

Since the `scalar clone` command sets up a file-system watcher (when available),
that watcher could prevent deleting the enlistment. Run `scalar delete <path>`
from outside of your enlistment to unregister the enlistment from the filesystem
watcher and delete the enlistment at `<path>`.
