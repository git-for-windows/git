Work in Progress as I try and work out how to do this (06-04-2019)..

When building a new release for Git-for-Windows, the full range of Windows patches the maintainer applies need to be moved ([`rebase`](https://git-scm.com/docs/git-rebase)d) from being on top of the old upstream Git, to being on top of the new upstream release.

There is a [process](https://github.com/git-for-windows/git/blob/HEAD/Documentation/git-for-windows/Rebasing-Git-for-Windows.md), a script and the support of the merging-rebase code, but still there maybe conflicts, old and new, especially if some of the Windows code has been adopted upstream in a different form.

This note looks at how to remember those resolutions, reuse them, and to learn from previous Git-for-Windows rebases.
The magic is in the [REuse REcorded REsolution (rerere)](https://git-scm.com/docs/git-rerere) command and it's underlying database.

The documentation is terse, so let's also point to some on-line articles:
* [Fun with rerere](https://gitster.livejournal.com/41795.html) from the Git maintainer
* [Fix conflicts only once with git rerere](https://medium.com/@porteneuve/fix-conflicts-only-once-with-git-rerere-7d116b2cec67) Nice graphs and examples.
* [Do you even rerere?](https://blog.theodo.fr/2015/01/do-you-even-rerere/) Good discussion, including `rerere-train`.
* [rerere-train.sh](https://github.com/git/git/blob/HEAD/contrib/rerere-train.sh) use the 'blame' button for extra commit info.
* [Are there any downsides to enabling git rerere?](https://stackoverflow.com/q/5519244/717355) Not really, but read and learn.
* [Smarter rebase avoiding redundant work?](https://stackoverflow.com/q/10601541/717355) more rerere-train answers
* [7.9 Git Tools (book) - Rerere](https://git-scm.com/book/en/v2/Git-Tools-Rerere).

So, enable `rerere` - `git config --global rerere.enabled true`,
consider setting `git config --global rerere.autoupdate true` and away you go!

Determine a previous Git-for-Windows merging-rebase end points and run the `contrib/rerere-train.sh`

Remember the parameters passed to rerere-train are `<rev-list-args>` (apparently) for selecting the training set.

### From the git List
Threads about the internal workings:

- [saving and replaying multiple variants with rerere](https://public-inbox.org/git/1442275050-30497-1-git-send-email-gitster@pobox.com/) 2015-09-14
- [Should rerere auto-update a merge resolution?](https://public-inbox.org/git/CACPiFCJH7RSb_rz6M6ADuGi0q+oeWYhE1fNMQC0EUcCn_kCJwg@mail.gmail.com/) 2017-08-23
- [rebase: use OPT_RERERE_AUTOUPDATE()](https://public-inbox.org/git/20190319190317.6632-4-phillip.wood123@gmail.com/#r) 2019-03-19
- [git/Documentation/technical/rerere.txt](https://github.com/git/git/blob/HEAD/Documentation/technical/rerere.txt) committed on 5 Aug 2018
- [git rerere unresolve file](https://public-inbox.org/git/200911211958.40872.j6t@kdbg.org/) 2009-11-21 patch series
- [Make git-rerere a builtin](https://public-inbox.org/git/Pine.LNX.4.63.0612201738000.19693@wbgn013.biozentrum.uni-wuerzburg.de/) 2006-12-20 convert from a perl script
- [Add a test for git-rerere](https://public-inbox.org/git/Pine.LNX.4.63.0612201737190.19693@wbgn013.biozentrum.uni-wuerzburg.de/) 2006-12-20

### Patch series, Rebase and Merge conflicts: When and where

For the rerere (redo) database, it is worth taking a few moments to consider the difference between the conflict resolution that you would perform during a rebase when a patch fails to merge cleanly, and the conflict resolution for a merge.

For a subsequent rebase, of the repeating merging-rebase kind, it is easy to forget that the _patches_ being forward ported to the new base commit _already_ contain the previous resolution, so will not conflict next time, unless the upstream has changed, whereupon you have a _new_ conflict to resolve.

This partially extends to the merges _within_ the merging-rebase. If previous rebases have tidied the merging branches to avoid merge conflicts then there will be no resolutions to record. However if the cleanest approach was to resolve at merge, then the merge resolution can (should) be learned. Further, if you also had upstream changes and resolutions in the branches being merged, there may still be further residual merge resolutions to be performed. So unless the merges were clean then these are all **new** merge resolutions that should be learned (or relearned) for later reuse in redoing the next merging rebase.

Moreover, if you are rebasing a series that has temporary merges from upstream or other independent side merges (?Cousins?), then these resolutions will need to be remembered for future merging-rebase is repeated (after dropping those merges from the 'todo' list)

### merging-rebase, as used in Git for Windows.

When upstream provides a tagged release, we create a commit with duplicated content tree (same oid hash) that has parents of: the upstream release and our _previous_ release. This 'fake merge', with commit message title `Start the merging-rebase`, along with its predecessor fake merge, provides anchors for the rebase of the patch series.

A side effect is that, via the second parent line, we have many repetitions of the same patch series when searching via `blame` or `grep`, or doing a `git bisect`. The effect can be mitigated by inserting a temporary `git replace` of the `Start the merging-rebase`merge commit, by its first (upstream) parent, making the merge disappear, along with all those historical duplicate patches. It becomes: upstream Git, with Windows patches on top - simple.
