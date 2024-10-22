The release process of Git for Windows is ever-evolving. For now, it consists of these steps:

- Making sure that there are no unaddressed issues
- Rebasing Git for Windows' patches
- Opening a PR to kick off a PR build (and waiting for it to succeed)
- Kicking off the `/git-artifacts` slash command
- Verifying that the resulting installer works
- Kicking off the `/release` slash command that publishes the release
- Pushing directly to `main` to close the PR and set the stage for the Azure Pipeline (at some stage during the `-rc<N>` cycle)

Note: the hardest part is traditonally preparing `-rc0` of every major release.

# Making sure that there are no unaddressed issues

There are essentially three-and-a-half venues where Git for Windows bugs are reported:

- The Git for Windows bug tracker at https://github.com/git-for-windows/git/issues
- The Git for Windows mailing list at https://groups.google.com/forum/#!forum/git-for-windows
- The Git mailing list at https://public-inbox.org/git/
- Twitter. Yep: ugh. Git for Windows is actually that popular that a couple of its users seriously think that Twitter would make for a perfectly fine way to report bugs. As if 280 characters were enough to make for a good bug report. Therefore, sometimes, when the Git for Windows maintainer feels particularly masochistic, they go to https://twitter.com/search?q=%23git-for-windows and see whether there is any valid bug report (one really has to pick out the needles between all that hay).

## In case last-minute component updates are needed

What if, say, a new Git Credential Manager Core version needs to be snuck in at the last minute (where "last minute" is anything within the past 24h)?

To integrate such an updated component, depending on the component type (MINGW or MSYS?) either the [`Build and publish MINGW Pacman package`](https://dev.azure.com/git-for-windows/git/_build?definitionId=32) or the [`Build and publish MSYS Pacman package` Pipeline](https://dev.azure.com/git-for-windows/git/_build?definitionId=33) need to be triggered (specifying the component name via the `package.to.build` variable at queue time).

Once the component has been built and uploaded to Git for Windows' Pacman repositories (i.e. once that triggered Pipeline finished), the `sync` workflows ([32-bit](https://github.com/git-for-windows/git-sdk-32/actions/workflows/sync.yml) and [64-bit](https://github.com/git-for-windows/git-sdk-64/actions/workflows/sync.yml])) must be triggered, manually. These workflows are responsible for installing the updated Pacman packages into the [`git-sdk-64`](https://github.com/git-for-windows/git-sdk-64) and [`git-sdk-32`](https://github.com/git-for-windows/git-sdk-64) repositories.

Note: Historically, we used the artifacts of the [`git-sdk-64-extra-artifacts`](https://dev.azure.com/git-for-windows/git/_build?definitionId=29&_a=summary) and the [`git-sdk-32-extra-artifacts` Pipeline](https://dev.azure.com/git-for-windows/git/_build?definitionId=30&_a=summary) Pipelines that were triggered automatically by pushes to `git-sdk-64` and `git-sdk-32`, respectively, and we had to wait for them to complete before proceeding. As of June 17th, 2022, this is no longer necessary.

# Rebasing Git for Windows' patches

Note: this section assumes that the reader is _very_ familiar with interactive rebases, in particular with the `--rebase-merges` variant thereof. Readers without much experience in this are highly advised to read up on https://git-scm.com/docs/git-rebase#_rebasing_merges before going any further.

The trick is not just to run `/usr/src/build-extra/shears.sh --merging --onto <version> merging-rebase`, it is even more important to verify that the result makes sense. To make sense, the result:

- Should not have any `fixup!`/`squash!` commits
- Should have the already-submitted and easily-submittable patches contained within the `ready-for-upstream` "sub thicket" (i.e. the patches/patch series that are merged at the top, via the `Merge branch "ready-for-upstream"` commit.
- In general, the commit topology should reflect the structure
  - Ready for upstream
  - Still in flux/not yet cleaned up enough
  - Never to be contributed to Git (e.g. Git for Windows' very own adjustments to README.md)
- Should be verified via `git range-diff` to the previous release, to make sure that no important patches were lost.

  The Git for Windows maintainer likes to use a custom alias for that:

  ```ini
  [alias]
	gfw-range-diff = "!sh -c 'a=; while case \"$1\" in -*) a=\"${a:+$a }$1\";; *) break;; esac; do shift; done; git range-diff --creation-factor=95 $a \"$1^{/^Start.the.merging-rebase}..$1\" \"$2^{/^Start.the.merging-rebase}..$2\"' -"
  ```

  With this alias, one can conveniently generate that range-diff. For example, after rebasing Git for Windows to v2.28.0, `git gfw-range-diff v2.27.0.windows.1 HEAD | clip.exe` would generate the range-diff and put it into the clipboard, ready to paste into the PR.
- To make extra certain that nothing important has been lost, call `git diff <git-version>..<git-for-windows-version> >prev` for the previous Git version, then `git diff <new-git-version>.. >cur` and compare with `git diff --no--index prev cur`, looking for lines starting with two diff markers (`/^[-+][-+]` in `less.exe`).

Note: internally, the `shears.sh` script runs `git rebase --rebase-merges --interactive` with one quirk: a first line is injected into the todo list that creates a "dummy" merge commit: it is a merge that pulls in the pre-rebase commits, but it uses `-s ours` so that the tree remains unchanged relative to the onto commit. The effect is that the patches got rebased, _and_ the end result still fast-forwards from the previous tip of the main branch.

# Opening a PR to kick off a PR build

This is easy: just push a branch to your fork and then go to https://github.com/git-for-windows/git/pull/new/main and select that branch from your fork.

The harder part is to include the range-diff (with `--creation-factor=95`; see the `gfw-range-diff` alias in the previous section) and discuss it.

To make it easier to see what happened to patches that are no longer part of Git for Windows' branch thicket, it is highly advised to use a variant of the shell script used in https://github.com/git-for-windows/git/pull/2626, and to perform a similarly thorough analysis, especially for any `-rc0` version:

```sh
$ x="$(git range-diff -s origin/main^{/^Start.the}..origin/main v2.27.0..v2.28.0-rc0 |
    sed -n 's/^[ 0-9]*:  \([0-9a-f][0-9a-f]*\) [=!] [ 0-9]*:  \([0-9a-f][0-9a-f]*\).*/-e "s\/\1\/\1 (upstream: \2)\/"/p')"

$ git gfw-range-diff origin/main HEAD |
  sed -e 's/^[ 0-9]*:  [0-9a-f]* [=!]/ &/' \
      -e 's/^[ 0-9]*:  [0-9a-f]* </-&/' \
      -e 's/^[ 0-9]*:  [0-9a-f]* >/+&/' |
  eval sed ${x:-\'\'} |
  clip.exe
```

For convenience, this can be automated via this alias:

```ini
[alias]
	pre-release-pr-range-diff = "!set -x && x=\"$(git range-diff -s origin/main^{/^Start.the}..origin/main origin/main^{/^Start.the}^..HEAD^{/^Start.the}^ | sed -n 's/^[ 0-9]*:  \\([0-9a-f][0-9a-f]*\\) [=!] [ 0-9]*:  \\([0-9a-f][0-9a-f]*\\).*/-e \"s\\/\\1\\/\\1 (upstream: \\2)\\/\"/p')\" && git gfw-range-diff origin/main HEAD | sed -e 's/^[ 0-9]*:  [0-9a-f]* [=!]/ &/' -e 's/^[ 0-9]*:  [0-9a-f]* </-&/' -e 's/^[ 0-9]*:  [0-9a-f]* >/+&/' | eval sed ${x:-\\'\\'} | clip.exe"
```

# Linking the "New git version" issue

When a new version is tagged in [upstream git](https://github.com/git/git), the "Monitor Component Updates" pipeline in Git for Windows creates an issue for the version ([example](https://github.com/git-for-windows/git/issues/3515)). For tracking & automation purposes, you should:

- link the issue to the pull request created in the previous step
- add the issue to the current "Next release" milestone ([example](https://github.com/git-for-windows/git/milestone/67))

# Kicking off the "Git Artifacts" automation

Add a PR comment with [the slash command `/git-artifacts`](https://github.com/git-for-windows/gfw-helper-github-app?tab=readme-ov-file#git-artifacts). This will kick off several GitHub workflow runs in https://github.com/git-for-windows/git-for-windows-automation: one to create the tag and adjust the release notes, and then one each for every CPU architecture supported by Git for Windows generating:

- The installer
- The Portable Git
- The `.tar.bz2` archive
- The MinGit
- The BusyBox-based MinGit
- The NuGet packages (x86_64-only)

# Verifying that the resulting installer works

The idea here is to download the `Git-<version>-64-bit.exe` artifact from the workflow run, install it, and run through the "pre-flight check list" at https://github.com/git-for-windows/build-extra/blob/HEAD/installer/checklist.txt.

# Kicking off the `/release` slash command that publishes the release

This one is _really_ easy (as long as nothing is broken...): add a PR comment with [the `/release` slash command](https://github.com/git-for-windows/gfw-helper-github-app?tab=readme-ov-file#release).

Note: The `pacman` upload always takes this long.

Sadly, things are broken a lot. In those cases, the logs have to be analyzed, and the GitHub workflow definition needs to be edited (on the `release` branch of https://github.com/git-for-windows/git-for-windows-automation), and the failing jobs of the workflow run have to be rerunExamples for failures that happened in the past:

- Timeouts while uploading the GitHub Release. In that case, the partially-populated draft release has to be deleted manually (first delete the assets, or it won't delete the release), and then re-deploy.

- Outage of github.com. In that case, simply re-deploying the stage that pushes the commits was sufficient.

- Something was pushed to the `main` branch in the meantime, and the Release Pipeline could not handle that. In this instance, the Pipeline was edited to `git pull && git push` if the `git push` failed. More complex scenarios might involve pulling a branch from a personal fork, e.g. if merge conflicts had to be resolved.

# Pushing directly to `main` to close the PR and set the stage for uploading the release as a new snapshot

This step is trivial: `git push origin <branch>:main` where `<branch>` is something like `rebase-to-v2.37.0`.

This will trigger another "Git artifacts" run, which will figure out that there is a GitHub Release for that commit, download those artifacts, then trigger a run of [the `Snapshots` Release Pipeline](https://dev.azure.com/git-for-windows/git/_release?definitionId=2&view=mine&_a=releases), which will then upload [the snapshot](https://wingit.blob.core.windows.net/files/index.html).

It is important to wait with pushing to `main` until there is a GitHub Release, otherwise the "Git artifacts" Pipeline would build _another_ set of artifacts and upload those, but we do want to use the same artifacts as were uploaded to GitHub Releases.

Note: The idea is to push to `main` relatively soon after the Release Pipeline finished, to keep the Pacman repository, the [snapshots](https://wingit.blob.core.windows.net/files/index.html) and the `main` branches as aligned as possible.


# How to release a quick-fix release

Outside of the `-rc<N>` phase, just follow the process as described above. This will publish one of those `(2)` versions, e.g. v2.24.1(2). Typically this happens _very_ close after an official release e.g. when really serious bugs crept into Git for Windows unnoticed, such as Git GUI crashing before even showing the window.

In the hypothetical case that a quick-fix release is necessary during the `-rc<N>` phase, _after_ `main` was pushed, i.e. needing to branch off of an earlier revision of `main`, all the above steps can be performed with ease (as the Pipelines do not run directly on `main`, but instead require `use.branch` to be set). The only tricky part is pushing the result to `main`. The recommended way is to use the next merging rebase to pull in the changes by inserting another fake merge before the one inserted by `shears.sh`.
