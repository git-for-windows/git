# How to backport a patch (series) that was contributed to upstream Git

Git for Windows' [release cadence](https://github.com/git-for-windows/git/security/policy#supported-versions) closely follows the [release cycle of the "upstream" Git project](https://tinyurl.com/gitcal).

Sometimes patches that already made it into upstream Git, but that have not been released as part of an official version, are important enough for Git for Windows that they need to be "backported", i.e. they to be integrated into Git for Windows' `main` branch.

Contributions to the upstream Git project [are only accepted in the form of patches or patch series sent via mail to the Git mailing list](https://git-scm.com/docs/SubmittingPatches#send-patches). This makes backporting them a bit more challenging than if regular Git tools like `git pull` could be used.

## GitGitGadget contributions

If the contribution was sent to the mailing list via [GitGitGadget](https://gitgitgadget.github.io/) (which is a tool allowing to send GitHub PRs to the Git mailing list without having to fiddle with mail programs until they send the contribution in the precise form that the Git project wants them), the backporting process is comparatively hassle-free: GitGitGadget contributions have a fetch'able Git branch and the PRs contain other relevant information such as the date and the commit which integrated the patches into Git's main branch.

Take for example [the "ci: use a newer `github-script` version" patch that avoids warnings in CI runs about using deprecated Actions/node.js versions](https://lore.kernel.org/git/pull.1387.git.1667902408921.gitgitgadget@gmail.com/): That mail contains the information in the footer from which PR it originated (in this example, https://github.com/gitgitgadget/git/pull/1387). That PR receives labels depending on the contribution process: `seen` if it was integrated into the daily hodgepodge branch of Git, `next` once the contribution advanced to the "let's cook this for a bit" stage, and `master` once the contribution has been slated for the next official Git version.

That PR also receives a comment once the patches have been integrated into a tentative topic branch, in this example ["js/ci-set-output"](https://github.com/gitgitgadget/git/pull/1387#issuecomment-1307968188). Following the [`js/ci-set-output` link](https://github.com/gitgitgadget/git/commits/js/ci-set-output), you can see the Git commits of the topic branch in which for upstream Git tracks the patches. You can also see the original shape of the contribution by clicking on the link to the PR branch at the top of the PR page (in the example: ["dscho:upgrade-github-script-version"](https://github.com/dscho/git/tree/upgrade-github-script-version)).

When backporting patches, the Git for Windows project typically prefers the shape that was already accepted upstream, i.e. once it made it into `next`, we backport the _upstream topic branch_'s commits.

The typical way to backport such a branch is to first see whether it needs to be rebased at all, i.e. if there are any additional commits between Git for Windows' `main` branch and that topic branch. The quickest way is to direct a web browser to a URL like this: https://github.com/git-for-windows/git/compare/main...gitgitgadget:js/ci-set-output.

### Scenario: The upstream topic branch can be used as-is

If that comparison only shows the commits of the contribution, we're almost done: hit the "Create pull request" button on that page and populate the PR description with [informative, enjoyable content](https://github.blog/2022-06-30-write-better-commits-build-better-projects/), i.e. providing enough context and motivation to understand why the patches need to be backported, enough detail to understand the contribution, how it solves the issue(s) at hand and what other impact the patches have, and of course add links to the relevant discussions such as the Git mailing list thread, the relevant Git for Windows tickets (if any), etc

### Scenario: The upstream topic branch _cannot_ be used as-is

There seems to be little rhyme nor reason on top of which commit, precisely, the Git maintainer decides to apply the patches received via the Git mailing list. At times it seems rather arbitrary and is not always conducive to using the upstream topic branch in Git for Windows as-is. In the example mentioned above, the comparison between Git for Windows' `main` branch and the topic branch showed 386 commits instead of the single desired one, at the time of writing.

In such scenarios, a backport involves creating a local branch from the upstream topic branch, rebasing the patch on top of a more suitable base commit (typically, a Git tag, or a Git for Windows tag if necessary to avoid merge conflicts, falling back to the tip of Git for Windows' `main` branch), then pushing the result and opening a PR from that. In the example above:

1. `git fetch https://github.com/gitgitgadget/git js/ci-set-output`
2. `git switch -c js/ci-set-output FETCH_HEAD`
3. `git rebase -i --onto v2.38.0 HEAD~1 # increase 1 accordingly for multiple patches`
4. `git push <personal-fork> HEAD`

After that, as detailed above, open a PR with an [informative, enjoyable description](https://github.blog/2022-06-30-write-better-commits-build-better-projects/), i.e. providing enough context and motivation to understand why the patches need to be backported, enough detail to understand the contribution, how it solves the issue(s) at hand and what other impact the patches have, and of course add links to the relevant discussions such as the Git mailing list thread, the relevant Git for Windows tickets (if any), etc

## Non-GitGitGadget contributions

If the contribution that needs to be backported did _not_ use GitGitGadget, it can become a bit more daunting, in particular when going off of a bug report on the Git mailing list. The first step, then, is to find the relevant topic branch (if any).

### If there is not even an upstream topic branch yet

The most challenging scenario is when there exists a patch that needs to be backported but it has not been integrated into Git yet (not even into the `seen` branch), and all one has to go for is a thread on the Git mailing list (or even just a Message-Id, which can be turned into a link to the Git mailing list archive via https://lore.kernel.org/git/<message-id>).

The first order of business is to find the mail containing the patch. Often, these mails have either been linked to from a reply to the bug report, or they are sent as replies directly, with the mail's subject line starting with `[PATCH]`. If that is not the case, the mail containing the patch can be identified by scrolling all the way to the beginning of the mail thread at the bottom of the lore.kernel.org page that has the bug report, then click the "nested" link so that all mails in the thread are shown, and then using the browser's "Find" functionality to search for the needle "diff --git".

Once the mail containing the patch was found, the [`apply-from-lore.sh` script](https://github.com/git-for-windows/build-extra/blob/HEAD/apply-from-lore.sh) can be used with the mail's permalink to apply it to a local branch in a checkout of [the `git-for-windows/git` repository](https://github.com/git-for-windows/git/).

Then, continue as detailed in the first sections above.

### If there _is_ an upstream topic branch

Sometimes, contributors are asked to backport patches that have made it into upstream Git. If it is already clear what name the Git maintainer gave the topic branch, continue as outlined in the corresponding subsection of [the "GitGitGadget" section](#gitgitgadget-contributions) above.

If it is unclear what the name of that topic branch is, search for the first line of a commit message ("commit subject" or "oneline") of that contribution in the [What's cooking mail](https://github.com/git/git/blob/todo/whats-cooking.txt); The name of the topic branch will be mentioned above the line mentioning the commit subject.

If all else fails, you can also try to fetch the `seen` branch and search for the topic branch via `git show FETCH_HEAD^{/<regex>}` where the regular expression matches the commit subject in question.