# Git for Windows - Development Guide

## Background

Git for Windows is a fork of upstream Git that provides the necessary
adaptations to make Git work well on Windows. While the primary target is
Windows, the project also maintains working builds on other platforms (Linux,
macOS) because cross-platform builds often catch mistakes that might be missed
when testing only on Windows.

There are downstream projects that build on Git for Windows, such as Microsoft
Git, which adds features for large monorepos hosted on Azure DevOps.

## Overview

This document provides guidance for developing and debugging in
Git for Windows.

## Repository Structure

### Branch Naming Patterns

Based on actual repository usage:

- `main` - The primary development branch
- Feature branches use descriptive topic names, targeting the main branch

## Building and Testing

### Build

```bash
make -j$(nproc)
```

On Windows (in a Git for Windows SDK shell):

```bash
make -j15
```

### Run Specific Tests

```bash
cd t && sh t0001-init.sh      # Run normally
cd t && sh t0001-init.sh -v   # Verbose
cd t && sh t0001-init.sh -ivx # verbose, trace, fail-fast
```

Some tests are expensive and skipped by default. When a test exits immediately
with "skip all", check the test script header for `test_bool_env GIT_TEST_*`
to find which environment variable enables it.

## Git Source Code Structure

This section provides a bird's eye view of Git's source code layout. For
more details, see "A birds-eye view of Git's source code" in
`Documentation/user-manual.adoc`.

### Key Directories

| Directory        | Purpose                                            |
|------------------|----------------------------------------------------|
| `builtin/`       | Built-in command implementations (`cmd_<name>()`)  |
| `xdiff/`         | Low-level diff algorithms (libxdiff)               |
| `t/`             | Test suite (shell scripts, helpers, libraries)     |
| `Documentation/` | Man pages, guides, technical docs (AsciiDoc)       |
| `contrib/`       | Optional extras, not part of core Git              |
| `compat/`        | Platform compatibility shims                       |
| `refs/`          | Reference backends (files, reftable)               |
| `reftable/`      | Reftable format implementation                     |

### Built-in Commands

Built-in commands are implemented in `builtin/<name>.c` with a function
`cmd_<name>()`. To add a new built-in:

1. Create `builtin/<name>.c` implementing `cmd_<name>()`
2. Add entry to the `commands[]` array in `git.c`:
   ```c
   { "<name>", cmd_<name>, RUN_SETUP },
   ```
3. Add to `BUILTIN_OBJS` in `Makefile`
4. Add to `command-list.txt` with appropriate category
5. Run `make check-builtins` to verify consistency

### Object Data Model

Git stores four types of objects, defined in `object.h`:

```c
enum object_type {
    OBJ_COMMIT = 1,  /* Points to tree, has parent commits, metadata */
    OBJ_TREE = 2,    /* Directory listing: names -> blob/tree OIDs   */
    OBJ_BLOB = 3,    /* File contents                                */
    OBJ_TAG = 4,     /* Annotated tag pointing to another object     */
};
```

Objects are addressed by their SHA (OID) and stored in the Object Database.

### Object Database (ODB)

The ODB is defined in `odb.h` and implemented in `odb.c`:

- **`struct object_database`**: Top-level container, owned by a repository
  - `sources`: Linked list of `odb_source` (primary + alternates)
  - `replace_map`: Object replacements (see `git-replace(1)`)
  - `commit_graph`: Commit-graph cache for faster traversal

- **`struct odb_source`**: A single object store location
  - `path`: Directory (e.g., `.git/objects` or an alternate)
  - `loose`: Loose object cache
  - `packfiles`: Packfile store (idx + pack files)

Key functions:
- `odb_read_object()`: Read an object by OID
- `odb_write_object()`: Write an object, returns OID
- `odb_read_object_info()`: Get object type/size without reading content

### Documentation

Documentation lives in `Documentation/` as AsciiDoc (`.adoc`) files:

- `git-<cmd>.adoc` - Man pages for commands
- `config/<name>.adoc` - Config option documentation (included by others)
- `technical/` - Technical specifications and internals

To build documentation:
```bash
make -C Documentation html   # Build HTML docs
make -C Documentation man    # Build man pages
```

To add documentation for a new config option, add it to the appropriate
file in `Documentation/config/`. These are included by other docs.

To lint documentation:
```bash
make -C Documentation lint-docs
```

## Debugging Techniques

### Debugging Philosophy

Debugging is not about guessing fixes and seeing if they work. It is about
building a complete understanding of the problem before attempting any fix.
The goal is not speed to a "fix" but confidence that you understand and have
addressed the root cause.

**Respect turnaround time.** If seeing the result of an attempted fix takes
7-10 minutes (e.g., a CI workflow run), you cannot afford to guess. Each
iteration costs human time and attention. Before pushing any change:

1. Ask: "What information am I missing to competently assess this situation?"
2. Add diagnostic output that will provide that information if the fix fails.
3. Consider whether you can reproduce the issue locally where turnaround is
   seconds, not minutes.

**Understand before acting.** Before attempting any fix:

1. When investigating a regression between two versions, start by examining
   the code diff. Analyze what actually changed before running any tests.
   Tests confirm hypotheses; reading the diff gives you the hypothesis.
2. Trace the code flow completely. Read the relevant Makefiles, scripts, and
   source files. Understand what each component does and how they interact.
3. Identify all changes that could have contributed: upstream commits,
   downstream patches, infrastructure changes (CI runner updates, dependency
   upgrades).
4. For each potential cause, find the specific commit, its date, its intent,
   and how it interacts with other components.
5. Build a hypothesis. Then ask: "How would I confirm or disprove this?"

**Do not assume root cause from symptoms.** A symptom appearing on one
platform does not mean the bug is platform-specific. The cause may be in
shared code that manifests differently across platforms. Similarly, a passing
test on one platform when it fails on another is data to investigate, not
grounds to conclude "works for me."

**When a fix does not work, investigate why.** If you expected a fix to work
and it did not, that is valuable information. Do not abandon that line of
thinking and try something else. Instead:

1. Ask: "Why didn't that work? What does this tell me about my understanding?"
2. Add more targeted diagnostics to understand the discrepancy.
3. Re-examine your assumptions. Something you believed to be true is false.

**Add diagnostics proactively.** Before pushing a fix attempt, add diagnostic
output that will:

1. Confirm the state you expect to see if the fix works.
2. Reveal the actual state if it does not.
3. Provide enough context to understand the next step without another round
   trip.

For build failures, this might include: library paths, compiler flags,
architecture information, symbol tables, file existence checks, environment
variables.

**Build confidence before pushing.** A fix should not be a guess. You should
be able to explain:

1. What was the root cause?
2. Why does this fix address it?
3. What other ways could this problem be solved?
4. Am I choosing the "most correct" or "most effective" approach?
5. What evidence confirms your understanding?
6. What could still go wrong, and how would you detect it?

### Searching the Codebase

In particular when debugging failures that printed error messages, it is often
a useful thing to search for those error messages; If parts of the message seem
mutable (e.g. commit OIDs), those will not be hard-coded and the search needs
to accommodate for that by using regular expressions or prefix matches.

Use `git grep` for fast code searches:

```bash
git grep -n -i "pattern"            # Case-insensitive search with line numbers
git grep -n -w "word"                 # Whole-word matches only
git grep -n -i "pattern" -- "*.c"     # Search only C files
```

### Trace2

Enable tracing to see command execution patterns:
```bash
GIT_TRACE2_EVENT=/path/to/trace.txt git <command>
```

### Comparing Branches After Rebase

```bash
# See what patches exist in a new branch but not old
git log --oneline old-branch..new-branch
# or
git range-diff -s --right-only old-branch...new-branch

# Compare specific files between branches
git diff old-branch..new-branch -- path/to/file.c
# or
git log -p old-branch..new-branch -- path/to/file.c
# or even
git log -L start-line,end-line:path/to/file.c old-branch..new-branch --

# Find upstream changes between tags
git log --oneline --first-parent v2.52.0..v2.53.0
```

### Test Failure Investigation

1. **Reproduce with tracing**: Run test with `-ivx` flags
2. **Check timestamps**: Look at `t_abs` in trace to understand ordering
3. **Compare with working version**: Build and test the previous version
4. **Bisect if needed**: Use `git bisect` to find the breaking commit

Bisecting failures introduced by upstream commits require some stunts to
apply the downstream changes for every bisection step. This can be done by
squashing all downstream changes into one throw-away commit and then
cherry-picking that (typically, there will be merge conflicts the farther
away from the original branch point the commit is cherry-picked to, so it
often makes sense to squash both old and new downstream changes, and then
to "interpolate" between them when encountering merge conflicts).

### CI/Workflow Failure Investigation

When a CI workflow fails, the debugging process has a high cost per iteration.
Approach these failures methodically:

**1. Establish what changed.** Before looking at the error, identify:

- What was the last successful run? What version/commit was it based on?
- What changed between then and now? (upstream commits, downstream patches,
  runner image updates, dependency changes)
- Use the GitHub API to retrieve run metadata and compare.

**2. Analyze the error deeply.** Read the full error message and surrounding
context. Understand:

- What command failed?
- What were its inputs (flags, environment, paths)?
- What did it expect vs. what did it get?

**3. Trace the code flow locally.** Before making any CI changes:

- Read the workflow YAML, Makefiles, and scripts involved.
- Understand how variables flow from one to another.
- Identify where the failing values come from.

**4. Reproduce locally if possible.** Many CI failures can be reproduced
locally with faster turnaround:

- For build failures: replicate the build environment and commands.
- For macOS issues: if you lack a Mac, at least trace the Makefile logic
  to understand what flags should be set and why.

**5. Add comprehensive diagnostics on first attempt.** If you must push to
CI to test, make that push count:

- Add diagnostic output for every hypothesis you have.
- Print the values of key variables, paths, flags.
- Show the state before and after key operations.
- Design diagnostics to distinguish between your hypotheses.

**6. Do not remove diagnostics until the problem is solved.** Keep them in
"drop!" commits so they can be easily removed later but provide information
if subsequent fixes also fail.

**7. When a fix fails, treat it as data.** The failure tells you something.
Your mental model was wrong. Figure out what before trying again.

## Git Workflow

This repository is a shared development environment, not a sandbox. Exercise
caution with all Git operations.

### Committing Changes

Never use `git add -A` or `git add .` - these commands will stage untracked
build artifacts, editor swap files, and other detritus that should not be
committed. Always specify pathspecs explicitly:

```bash
# Good: stage and commit specific files
git commit -sm "your message here" path/to/file.c other/file.h

# Bad: stages everything, including untracked garbage
git add -A && git commit -m "message"
```

The `-s` flag adds a Signed-off-by trailer, which is required for this
project.

When AI assistance is used to author or co-author a commit, add a
Co-authored-by trailer identifying the model:

```bash
git commit -s --trailer "Co-authored-by: <model-name>" -m "message" file.c
```

### Pushing Changes

Never push without explicit user permission. The user controls when and
where changes are pushed. This is especially critical because:

- The repository has multiple remotes with different purposes
- Force-pushing to the wrong remote can cause significant damage
- Tags require special handling (`git push --tags` or explicit tag pushes)

Wait for the user to push, or ask explicitly before pushing.

### Making Code Changes

**Minimal, surgical changes.** Make the smallest possible change to achieve
the goal. Do not rewrite entire files or functions when a targeted edit
suffices. When removing functionality:

1. Remove the code paths that invoke the unwanted functionality
2. Compile to identify what is now unused
3. Remove the unused functions one at a time
4. Repeat until clean

**No fly-by changes.** Do not make changes that were not requested, even if
they seem like improvements (renaming variables, reformatting untouched code,
"fixing" things not part of the task). If you believe a change would be
beneficial but it was not requested, ask for permission first.

**The human is the driver.** Execute what is asked. If you think something
should be done differently, ask---do not just do it.

### Commit Message Quality

Good commit messages use flowing English prose, not bullet points. They
clearly state:

- **Context**: What situation prompted this change? Include URLs to failing
  CI runs, issue numbers, or other references that future readers will need.
- **Intent**: What is this change trying to accomplish?
- **Justification**: Why is this the right approach? What alternatives were
  considered? When choosing between approaches based on performance,
  include measured timings so future readers understand the tradeoffs.
- **Implementation**: How does the change work? (Only for non-obvious parts;
  don't describe what's clear from the diff.)

Include exact error messages rather than vague descriptions. If a build
failed with `Undefined symbols for architecture arm64: "_iconv"`, put that
in the commit message - don't just say "fixed a linker error."

Wrap commit messages at 76 columns per line.

### Commit Prefixes for Rebase Workflows

This repository uses interactive rebase with autosquash. Commit prefixes
signal intent:

- **`fixup! <original title>`**: Will be squashed into the referenced commit
  during rebase. The title after `fixup!` must match the original commit's
  title exactly.
- **`drop!`**: Indicates a commit that should be dropped before the final
  merge. Used for debugging, temporary workarounds, or experiments.

To find the correct title for a fixup commit:

```bash
git log --oneline path/to/changed/file | head -10
```

Then use the exact title:

```bash
git commit -sm "fixup! release: add Mac OSX installer build" path/to/file
```

## Rebasing Workflow

Rebases are the bread and butter of Git for Windows: topic branches are
rebased every time upstream Git releases a new version. This section covers
the workflow for managing downstream patches through repeated rebases.

### Merging-Rebases

Git for Windows uses "merging-rebases" to maintain downstream patches. Unlike
a flat series of commits, the downstream changes are organized as topic
branches merged together, preserving the logical grouping of related changes.

Each integration branch (`main`, `shears/next`, `shears/seen`) contains a
marker commit with the message "Start the merging-rebase to \<version\>". This
commit separates upstream history from downstream patches. Reference it with:

```bash
# Find the marker commit
git log --oneline --grep="Start the merging-rebase" -1

# Reference it using commit message search syntax
origin/main^{/Start.the.merging-rebase}
```

When working with merging-rebases:

- **Downstream patches start after the marker**: Use
  `origin/main^{/Start.the.merging-rebase}..origin/main` to see all
  downstream commits
- **Topic branches are merged, not rebased flat**: Each logical feature or
  fix is a branch merged into the integration branch
- **Merge commits are preserved**: The rebase recreates the merge structure
  on top of the new upstream base

To compare downstream patches before and after a rebase:

```bash
# Compare the old and new downstream patch series
git range-diff \
  old-base^{/Start.the.merging-rebase}..old-branch \
  new-base^{/Start.the.merging-rebase}..new-branch
```

### Starting a Merging-Rebase

To rebase the downstream patches onto a new upstream version, create a marker
commit and use it as the base for an interactive rebase:

```bash
# Variables for the commit message
tag=v2.53.0
# The previous marker - this becomes the exclusion point for --onto
previousMergeOid=$(git rev-parse origin/main^{/Start.the.merging-rebase})
tagOid=$(git rev-parse "$tag")
tipOid=$(git rev-parse origin/main)

# Create the marker commit with two parents: the tag and the current tip
markerOid=$(git commit-tree "$tag^{tree}" -p "$tag" -p "$tipOid" -m "Start the merging-rebase to $tag

This commit starts the rebase of $previousMergeOid to $tagOid")

# Graft the marker to appear as if it has only the tag as parent
git replace --graft "$markerOid" "$tag"

# Use the marker as the base for rebasing (only commits after previousMergeOid)
git rebase -r --onto "$markerOid" "$previousMergeOid" origin/main

# After the rebase completes, delete the replace ref
git replace -d "$markerOid"
```

The marker commit is created with two parents: the upstream tag and the
current branch tip. The `git replace --graft` makes Git see only the tag as
parent during the rebase, allowing the downstream commits to be cleanly
rebased onto the new upstream. After the rebase completes, the replace ref
is deleted to clean up.

#### The shears/* Branches

Upstream Git has four integration branches: `seen`, `next`, `master`, and
`maint`. Git for Windows maintains a corresponding `shears/*` branch for each
(`shears/seen`, `shears/next`, `shears/master`, `shears/maint`) that
continuously rebases Git for Windows' `main` onto the respective upstream
branch.

These branches are updated incrementally rather than from scratch, avoiding
re-resolution of merge conflicts. The update process leverages reachability:

1. **Integrate new downstream commits**: If `origin/main` has commits not yet
   in the shears branch, rebase them on top (using `-r` to preserve branch
   structure). Update the marker commit's message and second parent.

2. **Integrate new upstream commits**: If the upstream branch has commits not
   yet integrated, rebase onto the new upstream tip. Update the marker commit
   accordingly.

The marker commit's second parent always points to the current `origin/main`
tip, making it trivial to identify what downstream commits are included.
Similarly, the marker's first parent (the upstream base) shows exactly which
upstream version is integrated.

### When to Skip a Patch

Use `git rebase --skip` when the patch is already in the new base:

- **Upstreamed**: The patch was accepted upstream and is now in `seen`
- **Backported**: A fix we backported is now included in the upstream base
- **Superseded**: HEAD already contains evolved code that includes this
  change

Signs to skip rather than resolve: HEAD has the functionality, the
conflict would discard the patch entirely, or `git range-diff` shows
the downstream and upstream patches are equivalent.

To find the corresponding upstream commit for a conflicting patch:

```bash
git range-diff --left-only REBASE_HEAD^! REBASE_HEAD..
```

### Resolving Merge Conflicts

When resolving merge conflicts during a rebase (especially when squashing
fixups), the goal is to **apply the minimal surgical change** that the
patch intended, not to reconstruct entire functions or add duplicate code.

#### 1. Understand What the Patch Wants

First, examine the patch being applied:

```bash
git show REBASE_HEAD
```

Look at the actual changes (lines starting with `-` and `+`):
- What lines are being removed?
- What lines are being added?
- What is the context (function name, nearby code)?

**Key insight**: The patch shows the *intent*---a specific small change to
make. Focus on this, not on the conflict markers' content.

**Code movement detection**: If the patch shows large changes, check with
`--ignore-space-change`:

```bash
git show <conflicted-commit> --ignore-space-change
```

This reveals whether the commit is primarily **moving code** (lots of
whitespace changes) or making **logic changes** (actual code modifications).
When code was moved and re-indented, focus only on the non-whitespace
changes when resolving the conflict.

#### 2. Understand Where the Code Is Now

The conflict occurred because the code moved or changed since the patch was
created. Find where that code actually exists now:

```bash
# If the patch was changing a specific pattern, find all occurrences
git grep -n "pattern from patch"

# View the conflicted file around those locations
```

**Common mistake**: Assuming the conflict markers show you what to do. They
do not---they just show where Git got confused.

#### 3. Apply the Surgical Change

Make **only** the change the patch intended, but in the current location:

- If the patch adds `--abbrev=12` to a range-diff call, find where that
  range-diff call is NOW and add it there
- If the patch changes a `.split()` pattern, find where that pattern is NOW
  and change it
- Do not copy entire functions from the conflict markers
- Do not create duplicates

#### 4. Remove ALL Conflict Markers

Conflict markers make the file invalid code:
```
<<<<<<< HEAD
=======
>>>>>>> commit-hash
```

**All three types of markers must be completely removed.**

#### 5. Verify the Resolution

**Critical**: After staging your resolution, verify it matches the patch
intent:

```bash
# Compare your staged changes to the original patch
git diff --cached
git rebase --show-current-patch

# Or more directly, compare to REBASE_HEAD
git diff --cached
git show REBASE_HEAD

# For code that was moved/re-indented, ignore whitespace
git diff --cached --ignore-space-change
git show REBASE_HEAD --ignore-space-change
```

**Verify, verify, verify**: The output of `git diff --cached` should
correspond closely to the diff in `git show REBASE_HEAD`. The line numbers
and context will differ (because code moved), but the actual changes (the
`-` and `+` lines) should match the patch intent.

**After completing a rebase**, always verify the final result:

```bash
# Compare tree before and after rebase
git diff @{1}

# Shows what changed in each rebased commit
git range-diff @{1}...
```

If the rebase was onto the same base commit (e.g., squashing fixups), the
`git diff @{1}` should be empty---this proves the rebase only reorganized
commits without changing the end result. If the rebase was onto a new base
commit (e.g., rebasing onto a new upstream release), the diff should match
the difference between the old and new base commits, modulo any changes
from upstreamed or backported patches. The `git range-diff @{1}...` shows
the intended amendments (like adding `--abbrev=12`) were correctly applied
to each commit.

### Conflict Resolution Red Flags

These indicate you are doing it wrong:

- Your diff adds hundreds of lines when the patch only changed 3
- Conflict markers remain in the file
- Functions appear twice in the file
- You added `<<<<<<< HEAD` or `=======` to the staged changes
- Syntax check fails after resolution

### Key Conflict Resolution Lessons

1. **Context changes, intent does not** - The patch's line numbers are
   wrong, but the change is right
2. **Conflict markers lie** - They show you where Git got confused, not
   what you should do
3. **One change at a time** - If the patch adds one line, your resolution
   should add one line
4. **Verify, verify, verify** - `git diff --cached` should match
   `git show REBASE_HEAD` (modulo context)
5. **Post-rebase verification** - `git diff @{1}` (empty) and
   `git range-diff @{1}...` (shows amendments)
6. **Ignore whitespace for code moves** - Use `--ignore-space-change` to
   see the actual logic changes when code was moved and re-indented
7. **When in doubt, look at the range-diff** - `git range-diff` shows if
   you matched the intent

### Useful Rebase Tools

- `git rebase --show-current-patch` - See what change is being applied
- `git show REBASE_HEAD` - Alternative to above, works better with
  `--ignore-space-change`
- `git show <commit> --ignore-space-change` - See only logic changes, not
  whitespace/indentation
- `git grep -n "pattern"` - Find where code moved to
- `git log -L <start>,<end>:<file> REBASE_HEAD..HEAD` - See how upstream
  modified a line range since the original patch; invaluable for
  understanding how conflicting lines changed
- `git diff --cached` - After staging resolution, verify it matches
  REBASE_HEAD
- `git diff @{1}` - After rebase, compare tree before/after
- `git range-diff @{1}...` - After rebase, verify intended changes were made
- `git range-diff A^! B^!` - Compare original patch to your resolution

### Leveraging Rerere

Git's "reuse recorded resolution" (`rerere`) feature automatically records
how you resolve conflicts and replays those resolutions when the same
conflict recurs. This is invaluable for repeated rebases where the same
downstream patches conflict with similar upstream changes.

When you see `Staged 'file' using previous resolution`, Git has applied a
previously recorded resolution. Always verify these auto-resolutions are
still correct---upstream context may have changed enough that the old
resolution no longer applies cleanly.

To enable rerere:
```bash
git config --global rerere.enabled true
```

### Automation Tips

When running rebases in automated or scripted contexts, disable the pager
to avoid hangs:

```bash
GIT_PAGER=cat git range-diff ...
# or
git --no-pager log ...
```

### Fixup Commits

Downstream patches sometimes require adjustment due to changes in the
environment they operate in. These changes may come from:

- **Upstream code changes**: API modifications, struct field moves,
  declarations relocating between headers, or semantic changes in functions
  that downstream code depends on.
- **External environment changes**: CI runner image updates, toolchain
  upgrades, dependency version changes, or platform behavior shifts.

In both cases, create a `fixup!` commit that will be squashed into the
original downstream patch during the next interactive rebase. The commit
message body must precisely document the change that necessitated the fix:

- For upstream changes: reference the specific upstream commit (by OID or
  title) and explain what it changed.
- For external changes: include URLs to failing CI runs, document what
  changed in the environment (e.g., "GitHub Actions macos-latest runner
  upgraded from macOS 14 to macOS 15"), and note the exact error message.

This documentation is essential because the fixup will be squashed away,
and the context will be lost if not recorded in the commit message that
gets squashed into.

Run affected tests before finalizing.

### Common Adaptation Patterns

**Struct field moves**: When upstream moves fields between structs, update
all downstream code that accesses those fields.

**API changes**: When upstream changes function signatures, update callers
and verify semantics are preserved.

**New abstractions**: When upstream introduces new layers, ensure downstream
code uses the correct instance.

## Coding Conventions

The Git project maintains a charmingly old-school, Unix-greybeard aesthetic
when it comes to text encoding. In the spirit of the PDP-11 and Bell Labs
terminal sessions of yore:

- **ASCII only**: Avoid Unicode characters in source code, comments, and
  documentation. Use `->` instead of `→`, `--` instead of `—`, and so on.
  To verify your changes contain no non-ASCII characters:
  ```
  git diff | LC_ALL=C grep '[^ -~]'
  ```
- **80 columns per line**: The mailing list veterans will "kindly" remind you
  that lines should not exceed 80 characters (they do mean columns, but
  let's not split beards or hairs about wide glyphs).
  First, check for whitespace errors (trailing whitespace, mid-line tabs, etc.):
  ```
  git diff --check
  ```
  Once that passes, you know tabs only appear at line beginnings, so each
  tab equals exactly 8 columns. To find lines exceeding 80 columns:
  ```
  git diff --no-color | grep '^+' | sed 's/\t/        /g' | grep '.\{82\}'
  ```
  (We use 82 because diff output prefixes added lines with `+`.)
- **Tabs for indentation**: The codebase uses tabs, not spaces.
- **No trailing whitespace**: Clean up your lines.

See `Documentation/CodingGuidelines` for the full set of conventions.

### strbuf patterns

Use `strbuf_addf()` with string continuation for multi-line content instead
of multiple `strbuf_addstr()` calls:

```c
/* Good */
strbuf_addf(&buf,
            "tree %s\n"
            "author %s\n"
            "committer %s\n"
            "\ncommit message\n",
            tree_hex, author, committer);

/* Avoid */
strbuf_addstr(&buf, "tree ");
strbuf_addstr(&buf, tree_hex);
strbuf_addstr(&buf, "\nauthor ");
/* ... */
```

Choose descriptive variable names (`header` for pack headers, not generic
`buf`; use `buf` for the secondary strbuf if you cannot reuse the first).

## Platform Considerations

### Windows-specific issues

On Windows, `unsigned long` is 32 bits even on 64-bit systems. Use `size_t`
for sizes that may exceed 4GB. Be careful with format strings: use `PRIuMAX`
with a cast for `size_t` values.

## Resources

- [Git for Windows](https://gitforwindows.org/)
- [Git Internals](https://git-scm.com/book/en/v2/Git-Internals-Plumbing-and-Porcelain)
