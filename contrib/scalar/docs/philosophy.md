The Philosophy of Scalar
========================

The team building Scalar has **opinions** about Git performance. Scalar
takes out the guesswork by automatically configuring your Git repositories
to take advantage of the latest and greatest features. It is difficult to
say that these are the absolute best settings for every repository, but
these settings do work for some of the largest repositories in the world.

Scalar intends to do very little more than the standard Git client. We
actively implement new features into Git instead of Scalar, then update
Scalar only to configure those new settings. In particular, we ported
features like background maintenance to Git to make Scalar simpler and
make Git more powerful.

Services such as GitHub support partial clone , a standard adopted by the Git
project to download only part of the Git objects when cloning, and fetching
further objects on demand. If your hosting service supports partial clone, then
we absolutely recommend it as a way to greatly speed up your clone and fetch
times and to reduce how much disk space your Git repository requires. Scalar
will help with this!

Most of the value of Scalar can be found in the core Git client. However, most
of the advanced features that really optimize Git's performance are off by
default for compatibility reasons. To really take advantage of Git's latest and
greatest features, you either need to study the [`git config`
documentation](https://git-scm.com/docs/git-config) and regularly read [the Git
release notes](https://github.com/git/git/tree/master/Documentation/RelNotes).
Even if you do all that work and customize your Git settings on your machines,
you likely will want to share those settings with other team members. Or, you
can just use Scalar!

Using `scalar register` on an existing Git repository will give you these
benefits:

* Additional compression of your `.git/index` file.
* Hourly background `git fetch` operations, keeping you in-sync with your
  remotes.
* Advanced data structures, such as the `commit-graph` and `multi-pack-index`
  are updated automatically in the background.
* If using macOS or Windows, then Scalar configures Git's builtin File System
  Monitor, providing faster commands such as `git status` or `git add`.

Additionally, if you use `scalar clone` to create a new repository, then
you will automatically get these benefits:

* Use Git's partial clone feature to only download the files you need for
  your current checkout.
* Use Git's [sparse-checkout feature][sparse-checkout] to minimize the
  number of files required in your working directory.
  [Read more about sparse-checkout here.][sparse-checkout-blog]
* Create the Git repository inside `<repo-name>/src` to make it easy to
  place build artifacts outside of the Git repository, such as in
  `<repo-name>/bin` or `<repo-name>/packages`.

We also admit that these **opinions** can always be improved! If you have
an idea of how to improve our setup, consider
[creating an issue](https://github.com/microsoft/scalar/issues/new) or
contributing a pull request! Some [existing](https://github.com/microsoft/scalar/issues/382)
[issues](https://github.com/microsoft/scalar/issues/388) have already
improved our configuration settings and roadmap!

[gvfs-protocol]: https://github.com/microsoft/VFSForGit/blob/HEAD/Protocol.md
[microsoft-git]: https://github.com/microsoft/git
[sparse-checkout]: https://git-scm.com/docs/git-sparse-checkout
[sparse-checkout-blog]: https://github.blog/2020-01-17-bring-your-monorepo-down-to-size-with-sparse-checkout/
