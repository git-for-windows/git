To do a *rebase* of the *Git for Windows* source onto a new [upstream](https://github.com/git/git) *Git* release, follow this guideline.

# Assumptions
1. You want to *rebase* onto a new [upstream](https://github.com/git/git) release tagged as `v2.3.4`
2. The latest *rebase* was done onto a [upstream](https://github.com/git/git) release tagged as `v2.3.3`
3. the `origin/main` below means remote https://github.com/git-for-windows/git, its `main` branch.
   You may have them named differently.

# Preconditions
1. A working [Git for Windows SDK](https://gitforwindows.org/#download-sdk).
2. A fetched build-extra repository.
    `cd /usr/src/build-extra`
    `git fetch`
    `git checkout main`
    (`git pull` if your branch is behind the upstream)
3. Working directory set to current *Git for Windows* source.
    `cd /usr/src/git`
4. Added [upstream](https://github.com/git/git) as a remote.
    `git remote add git https://github.com/git/git`
    `git fetch git`

# Starting the *rebase*
Run the `shears.sh` script to build up the actual *rebase* script.
    `/usr/src/build-extra/shears.sh --merging --onto v2.3.4 merging-rebase`
    Note: `v2.3.4` is a `tag` in the remote `git`.
    See the script if you are testing changes initiated in a local branch of the git upstream.

The *rebase* should start automatically and occasionally stop if it hits any merge conflicts. Resolve those conflicts (see also [Merge Conflicts Resolving and Remembering them](https://github.com/git-for-windows/git/blob/HEAD/Documentation/git-for-windows/Merge-Conflicts---Resolving-and-Remembering-them.md)) and then continue the rebase.
    `git rebase --continue`

Note: the `merging-rebase` argument is a special placeholder that is interpreted by the `shears.sh` script to find the commit that started the previous merging-rebase. You can find the commit yourself like this:
    `BASE="$(git rev-parse HEAD^{/^"Start the merging-rebase"})"`

# Verifying the *rebase*
1. Generate a *diff* of the previous state.
    `git diff v2.3.3..origin/main > prev.diff`
2. Generate a *diff* of the current state.
    `git diff v2.3.4..main > curr.diff`
3. *Diff* the *diffs*.
    `git diff --no-index prev.diff curr.diff`

    >Ideally, the resulting output will show changes only in the `@@` lines of `prev.diff` vs `curr.diff`.
    >It's a bit hard to read, though, because it is a diff of a diff.
    >So meta.
    >When there is a line that starts with a `-` or a `+` but then continues with a space, that's good!
    >It means that the context of our changes changed.

In addition to that, you can also use the `range-diff` command to verify the rebase:

```sh
r=^{/^Start.the.merging-rebase}
git range-diff HEAD$r^2$r..HEAD$r^2 HEAD$r..
```

## Explanation

`HEAD^{/^Start.the.merging-rebase}` (abbreviated in the example as `HEAD$r`) finds the latest commit which started a merging-rebase and which is reachable from `HEAD`.

That commit is a "fake" merge, i.e. it merges the pre-rebase commit history, but leaves the tree identical compared to the state before the merge. That is, its first parent is the commit onto which the patches were rebased, and its second parent is the tip of the patches that were rebased.

As such, `HEAD$r^2` refers to the tip of the pre-rebase commits, and `HEAD$r^2$r` to the bottom of the rebased commits (pre-rebase). Ergo: `HEAD$r^2$r..HEAD$r^2` specifies the commits *before* the rebase.

Likewise, `HEAD$r` refers to the bottom of the rebased commits (post-rebase), and `HEAD` is obviously the tip of those commits. That is, `HEAD$r..` specifies the commits *after* the rebase.

## How to read the output of the `range-diff`
Note that the output of the range-diff can be a bit daunting to read: it is a diff between diffs. As such, there are "outer" diff markers (`-`/`+`), denoting differences between the compared commit ranges, and "inner" diff markers which correspond to the patches of the individual commits instead.

To verify that everything worked as intended, you need to watch out for lines that start with *two* diff markers: those indicate that the pre-rebase and post-rebase commits modify the code in *different* ways.

The output is color-coded in two levels: the familiar green/red (as in `git diff`'s output) corresponds to added/removed lines. The second level is dim/bright: if green/red lines are dim, that indicates that this is the pre-rebase version of the patch; conversely, any bright green/red lines indicate post-rebase versions.

More notes on reading the output of this `range-diff` command:
- It is quite common that the context changes, e.g. when a new `#include` line was introduced both in the rebased patch as well as upstream (in which case, the upstream line would be marked with an outer diff marker, but the line itself would be uncolored because it is in the context of the post-rebase version of the patch).
- When whole commits are marked in red, they have been dropped during the rebase. This is quite normal when patches were "upstreamed".
- Sometimes, commits are marked in red, as if they were removed, but there are also green lines that "re-introduce" them. That happens when `range-diff` fails to detect that the pre-rebase and post-rebase versions of the commit correspond to one another. This can be fixed by passing a `--creation-factor` to the `range-diff` command (sensible values are between 60, the default, and 100).