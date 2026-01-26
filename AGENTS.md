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
