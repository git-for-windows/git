Scalar: Enabling Git at Scale
=============================

Scalar is a tool that helps Git scale to some of the largest Git repositories.
It achieves this by enabling some advanced Git features, such as:

* *Partial clone:* reduces time to get a working repository by not
  downloading all Git objects right away.

* *Background prefetch:* downloads Git object data from all remotes every
  hour, reducing the amount of time for foreground `git fetch` calls.

* *Sparse-checkout:* limits the size of your working directory.

* *File system monitor:* tracks the recently modified files and eliminates
  the need for Git to scan the entire worktree.

* *Commit-graph:* accelerates commit walks and reachability calculations,
   speeding up commands like `git log`.

* *Multi-pack-index:* enables fast object lookups across many pack-files.

* *Incremental repack:* Repacks the packed Git data into fewer pack-file
  without disrupting concurrent commands by using the multi-pack-index.

By running `scalar register` in any Git repo, Scalar will automatically enable
these features for that repo (except partial clone) and start running suggested
maintenance in the background using
[the `git maintenance` feature](https://git-scm.com/docs/git-maintenance).

Repos cloned with the `scalar clone` command use partial clone or the
[GVFS protocol](https://github.com/microsoft/VFSForGit/blob/HEAD/Protocol.md)
to significantly reduce the amount of data required to get started
using a repository. By delaying all blob downloads until they are required,
Scalar allows you to work with very large repositories quickly. The GVFS
protocol allows a network of _cache servers_ to serve objects with lower
latency and higher throughput. The cache servers also reduce load on the
central server.

Documentation
-------------

* [Getting Started](getting-started.md): Get started with Scalar.
  Includes `scalar register`, `scalar unregister`, `scalar clone`, and
  `scalar delete`.

* [Troubleshooting](troubleshooting.md):
  Collect diagnostic information or update custom settings. Includes
  `scalar diagnose` and `scalar cache-server`.

* [The Philosophy of Scalar](philosophy.md): Why does Scalar work the way
  it does, and how do we make decisions about its future?

* [Frequently Asked Questions](faq.md)
