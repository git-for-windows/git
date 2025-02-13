Please note that it is a best practice embraced by us to craft elegant, well-separated commits. Each commit is kind of its own little story, so if you make a typo fix, that is its own commit. If you add documentation, that is another commit. If you have multiple typo fixes, you could consider to put them into the same commit, depending on your taste.

In short, make sure that commits are logical units of changes.

# Commit messages

When it comes to the commit message, it is good practice to avoid repeating what is easily deduced from the code changes. Instead, try to describe ...
- ... the *why* and the *what* rather than the *how*;
- ... aspects of the solved problem or feature, that the reader might not be aware of;
- ... aspects that you would want any reviewer to know in addition to the actual code change.

If you made a change whose benefit is not obvious from reading the code changes associated with the commit, it is a wonderful idea to outline the benefit in the commit message.

When the commit is the result of a public discussion, it is good practice to summarize it and then add the hyper link to said discussion.

If you tried several strategies to solve your problem, you will want to describe what you tried and why it did not work, unless it appears too obvious in hindsight.

Also important:
- The commit message should always start with a single, short line summarizing the change.
- [Sign off](https://github.com/git/git/blob/v2.8.1/Documentation/SubmittingPatches#L234-L286) on your change (i.e. state that you release this patch as Open Source, if applicable with your employer's consent).
- Format the commit message to wrap at 76 columns per line.

A good example for a commit message is [git-for-windows/build-extra@a64fe115](github.com/git-for-windows/build-extra/commit/a64fe115d901cab775c881bd1624218ac28de4d4):

```
Install 7-Zip without confirmation if it is missing

This is to allow unattended CI builds for the SFX installers. Once we have
released a new net-installer for the SDK we can remove the check for 7za
altogether as the SDK ships 7-Zip by default since the previous commit.

Signed-off-by: Sebastian Schuberth <sschuberth@gmail.com>
```

Please note how it summarizes nicely *what* the change is about ("Install ... without confirmation") instead describing *how* it does it ("Use --noconfirm" would be a counter example), and how the main body provides the context to understand *why* the change is desirable.

Further reading on commit messages:

- http://tbaggery.com/2008/04/19/a-note-about-git-commit-messages.html
- http://gitforteams.com/resources/commit-granularity.html
- http://chris.beams.io/posts/git-commit/
- http://ablogaboutcode.com/2011/03/23/proper-git-commit-messages-and-an-elegant-git-history