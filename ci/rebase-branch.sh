#!/bin/bash
#
# Rebase a branch using the merging-rebase strategy with AI-powered conflict resolution.
#
# Usage: rebase-branch.sh <shears-branch> <upstream-branch> [<scripts-dir>]
#
# Parameters:
#   shears-branch   - The branch to rebase (e.g., origin/shears/seen)
#   upstream-branch - The upstream branch to rebase onto (e.g., upstream/seen)
#   scripts-dir     - Optional: directory containing this script and agents
#                     (defaults to the directory containing this script)
#
# Preconditions:
#   - Must be run from a git repository
#   - shears-branch and upstream-branch must be fetched
#   - GITHUB_TOKEN or COPILOT_GITHUB_TOKEN must be set for AI resolution
#
# Outputs:
#   - Updated HEAD in a worktree (ready for push)
#   - conflict-report.md with detailed resolution notes
#
# Exit codes:
#   0 - Success
#   1 - Usage error or precondition failure
#   2 - Conflict resolution failed (AI could not resolve)
#   3 - Rebase failed for other reasons

set -e

die () {
	echo "error: $*" >&2
	exit 1
}

usage () {
	sed -n '3,/^#$/s/^# //p' "$0"
	exit 1
}

# Function to gather conflict context for AI
gather_context () {
	local context_file=$1

	cat >"$context_file" <<EOF
# Merge Conflict During Rebase

## Commit Being Applied
$(git show --no-patch --format=reference REBASE_HEAD)
Author: $(git show --no-patch --format='%an <%ae>' REBASE_HEAD)
Date: $(git show --no-patch --format='%ai' REBASE_HEAD)

## Conflicting Files
$(git diff --name-only --diff-filter=U)

## Was This Patch Upstreamed?
\`\`\`
$(git range-diff --left-only REBASE_HEAD^! REBASE_HEAD.. 2>/dev/null || echo "Unable to determine")
\`\`\`

## Conflict Details
EOF

	# For each conflicting file, add the conflict and upstream history
	for file in $(git diff --name-only --diff-filter=U); do
		cat >>"$context_file" <<EOF

### File: $file

#### Current conflict state:
\`\`\`
$(cat "$file")
\`\`\`

#### Upstream changes to this file (recent commits):
\`\`\`
$(git log --oneline -10 REBASE_HEAD..HEAD -- "$file" 2>/dev/null || echo "No upstream changes found")
\`\`\`

EOF
		# Extract line ranges from diff hunk headers and show upstream history
		# Hunk headers look like: @@ -old_start,old_count +new_start,new_count @@
		while IFS= read -r hunk_header; do
			# Extract the "new" side line range (current branch)
			line_range=$(echo "$hunk_header" | sed -n 's/^@@ -[0-9,]* +\([0-9]*\),\([0-9]*\) @@.*/\1,\2/p')
			if test -n "$line_range"; then
				start=${line_range%,*}
				count=${line_range#*,}
				end=$((start + count))
				cat >>"$context_file" <<EOF
#### How upstream modified lines $start-$end:
\`\`\`
$(git log -L "$start,$end:$file" REBASE_HEAD..HEAD 2>/dev/null | head -100 || echo "Unable to get line history")
\`\`\`

EOF
			fi
		done < <(git diff -- "$file" 2>/dev/null | grep '^@@ ')
	done

	cat >>"$context_file" <<EOF

## Instructions

You are resolving a merge conflict during a Git rebase. Based on the context above:

1. If the patch was upstreamed (range-diff shows correspondence like "1: abc123 = 1: def456"),
   do NOT edit any files. Output: skip <upstream-oid>
   where <upstream-oid> is the OID shown on the right side of the "=" in the range-diff.

2. If the patch needs surgical resolution:
   - Edit the conflicting file(s) to resolve the conflict
   - Remove all conflict markers (<<<<<<<, =======, >>>>>>>)
   - Stage with: git add <file>
   - Then output: continue

3. If you cannot resolve the conflict, output: fail

IMPORTANT: Your final line of output MUST be exactly: skip <oid>, continue, or fail

After resolving, append a brief note about your resolution to: $REPORT_FILE
EOF
}

# Function to create recovery bundle on failure
create_recovery_archive () {
	local ai_output=$1
	local archive_dir
	archive_dir=$(mktemp -d)
	local bundle_file="$archive_dir/recovery.bundle"
	local archive_file="${WORKTREE_DIR}/recovery-archive.tar.gz"

	echo "::group::Creating recovery archive"
	echo "Saving progress for manual recovery..."

	# Collect refs: HEAD, REBASE_HEAD, and all refs/rewritten/*
	{
		echo HEAD
		echo REBASE_HEAD
		git for-each-ref --format='%(refname)' refs/rewritten/
	} >"$archive_dir/refs-to-bundle.txt"

	# Create bundle with all progress
	git bundle create "$bundle_file" --stdin <"$archive_dir/refs-to-bundle.txt" 2>/dev/null || true

	# Copy the conflict report
	cp "$REPORT_FILE" "$archive_dir/" 2>/dev/null || true

	# Save the AI output that caused failure
	echo "$ai_output" >"$archive_dir/ai-output.txt"

	# Save rebase state
	cp -r .git/rebase-merge "$archive_dir/" 2>/dev/null || true

	# Create archive with working tree and bundle
	tar -czf "$archive_file" \
		-C "$WORKTREE_DIR" . \
		-C "$archive_dir" recovery.bundle conflict-report.md ai-output.txt rebase-merge 2>/dev/null || \
	tar -czf "$archive_file" -C "$WORKTREE_DIR" . 2>/dev/null || true

	rm -rf "$archive_dir"

	echo "Recovery archive created: $archive_file"
	echo "::endgroup::"

	# Output for GitHub Actions artifact upload
	if test -n "$GITHUB_OUTPUT"; then
		echo "recovery_archive=$archive_file" >>"$GITHUB_OUTPUT"
	fi
}

# Function to resolve a single conflict with AI
resolve_conflict_with_ai () {
	echo ""
	echo "========================================"
	echo "Conflict detected at: $(git show --no-patch --format=reference REBASE_HEAD)"
	echo "========================================"

	local context_file
	context_file=$(mktemp)
	gather_context "$context_file"

	echo "Invoking AI for conflict resolution..."
	local ai_output
	ai_output=$(copilot -p "$(cat "$context_file")" \
		--allow-tool 'write' \
		--allow-tool 'shell(git add)' \
		--allow-tool 'shell(git rm)' \
		--deny-tool 'shell(git rebase)' \
		--deny-tool 'shell(git push)' \
		--deny-tool 'shell(git reset)' \
		--deny-tool 'shell(rm)' \
		2>&1) || true

	rm -f "$context_file"

	# Log the AI output in a collapsible group
	echo "::group::AI Output for $(git show --no-patch --format='%h %s' REBASE_HEAD)"
	echo "$ai_output"
	echo "::endgroup::"

	# Extract the decision from the last non-empty line
	local last_line decision upstream_oid
	last_line=$(echo "$ai_output" | grep -v '^$' | tail -1)
	decision=$(echo "$last_line" | awk '{print tolower($1)}')

	case "$decision" in
	skip)
		local original_oid
		original_oid=$(git rev-parse REBASE_HEAD)
		upstream_oid=$(echo "$last_line" | awk '{print $2}')
		if test -n "$upstream_oid"; then
			echo "$original_oid $upstream_oid" >>"$SKIPPED_MAP_FILE"
			echo "::notice::Skipping commit (upstream: $upstream_oid): $(git show --no-patch --format='%h %s' REBASE_HEAD)"
			cat >>"$REPORT_FILE" <<SKIP_EOF

### Skipped: $(git show --no-patch --format=reference REBASE_HEAD)

Upstream equivalent: $(git show --no-patch --format=reference "$upstream_oid" 2>/dev/null || echo "$upstream_oid")

<details>
<summary>Range-diff</summary>

\`\`\`
$(git range-diff --creation-factor=99 REBASE_HEAD^! "$upstream_oid^!" 2>/dev/null || echo "Unable to generate range-diff")
\`\`\`

</details>

SKIP_EOF
		else
			echo "::notice::Skipping commit (already upstream): $(git show --no-patch --format='%h %s' REBASE_HEAD)"
		fi
		CONFLICTS_SKIPPED=$((CONFLICTS_SKIPPED + 1))
		git rebase --skip
		;;
	continue)
		echo "::notice::Resolved conflict surgically: $(git show --no-patch --format='%h %s' REBASE_HEAD)"
		CONFLICTS_RESOLVED=$((CONFLICTS_RESOLVED + 1))
		git rebase --continue
		
		# Verify build after surgical resolution
		echo "::group::Verifying build"
		if ! make -j$(nproc) 2>&1 | tee make.log; then
			echo "::endgroup::"
			echo "::warning::Build failed after conflict resolution, giving AI another chance"
			
			# Create context with build failure
			local retry_context
			retry_context=$(mktemp)
			cat >"$retry_context" <<RETRY_EOF
# Build Failed After Conflict Resolution

The conflict resolution you provided does not compile. Please fix it.

## Build Output (last 100 lines)
\`\`\`
$(tail -100 make.log)
\`\`\`

## Files You Modified
$(git diff --name-only HEAD^)

## Current State of Modified Files
$(for f in $(git diff --name-only HEAD^); do echo "### $f"; echo "\`\`\`"; cat "$f"; echo "\`\`\`"; done)

## Instructions

Fix the compilation error by editing the affected file(s), then stage with \`git add\`.
Amend the previous commit with \`git commit --amend --no-edit\`.

Output \`continue\` when fixed, or \`fail\` if you cannot fix it.
RETRY_EOF

			local retry_output
			retry_output=$(copilot -p "$(cat "$retry_context")" \
				--allow-tool 'write' \
				--allow-tool 'shell(git add)' \
				--allow-tool 'shell(git commit --amend)' \
				--deny-tool 'shell(git rebase)' \
				--deny-tool 'shell(git push)' \
				--deny-tool 'shell(git reset)' \
				--deny-tool 'shell(rm)' \
				2>&1) || true
			rm -f "$retry_context"
			
			echo "::group::AI Retry Output"
			echo "$retry_output"
			echo "::endgroup::"
			
			# Verify build again
			echo "::group::Verifying build (retry)"
			if ! make -j$(nproc) 2>&1 | tee make.log; then
				echo "::endgroup::"
				echo "::error::Build still fails after retry"
				cat >>"$REPORT_FILE" <<BUILD_FAIL_EOF

### BUILD FAILED: $(git show --no-patch --format=reference HEAD)

Build failed after conflict resolution. Last 50 lines:

\`\`\`
$(tail -50 make.log)
\`\`\`

BUILD_FAIL_EOF
				create_recovery_archive "$retry_output"
				exit 2
			fi
			echo "::endgroup::"
		else
			echo "::endgroup::"
		fi
		rm -f make.log
		;;
	fail)
		echo "::error::AI could not resolve conflict: $(git show --no-patch --format='%h %s' REBASE_HEAD)"
		cat >>"$REPORT_FILE" <<EOF

### FAILED: $(git show --no-patch --format=reference REBASE_HEAD)

AI could not resolve this conflict. Full output:

\`\`\`
$ai_output
\`\`\`

EOF
		create_recovery_archive "$ai_output"
		exit 2
		;;
	*)
		echo "::error::Unexpected AI decision '$decision': $(git show --no-patch --format='%h %s' REBASE_HEAD)"
		cat >>"$REPORT_FILE" <<EOF

### FAILED: $(git show --no-patch --format=reference REBASE_HEAD)

Unexpected AI decision: '$decision'. Full output:

\`\`\`
$ai_output
\`\`\`

EOF
		create_recovery_archive "$ai_output"
		exit 2
		;;
	esac
}

# Function to run a rebase with AI-powered conflict resolution
# Usage: run_rebase_with_ai <rebase-args...>
run_rebase_with_ai () {
	while true; do
		if git rebase "$@" 2>&1; then
			break
		fi

		# Check if we have a conflict
		if ! git diff --name-only --diff-filter=U | grep -q .; then
			die "Rebase failed but no conflicts detected"
		fi

		resolve_conflict_with_ai
	done
}

# Parse arguments
test $# -ge 2 || usage
SHEARS_BRANCH=$1
UPSTREAM_BRANCH=$2
SCRIPTS_DIR=${3:-$(cd "$(dirname "$0")" && pwd)}
AGENTS_DIR=$(dirname "$SCRIPTS_DIR")/.github/agents

# Validate environment
command -v copilot >/dev/null 2>&1 || die "copilot CLI not found in PATH"
test -n "${GH_TOKEN:-${GITHUB_TOKEN:-}}" ||
	die "GH_TOKEN or GITHUB_TOKEN must be set"

# Validate branches exist
git rev-parse --verify "$SHEARS_BRANCH" >/dev/null 2>&1 ||
	die "Branch not found: $SHEARS_BRANCH"
git rev-parse --verify "$UPSTREAM_BRANCH" >/dev/null 2>&1 ||
	die "Branch not found: $UPSTREAM_BRANCH"

# Set up worktree
WORKTREE_DIR=$(mktemp -d)
REPORT_FILE="$WORKTREE_DIR/conflict-report.md"
trap 'git worktree remove --force "$WORKTREE_DIR" 2>/dev/null || true' EXIT

# Extract local branch name from origin/shears/foo -> shears/foo
LOCAL_BRANCH=${SHEARS_BRANCH#origin/}

echo "Creating worktree at $WORKTREE_DIR..."
git worktree add -b "$LOCAL_BRANCH" "$WORKTREE_DIR" "$SHEARS_BRANCH"
cd "$WORKTREE_DIR"

# Find the old marker
OLD_MARKER=$(git rev-parse "HEAD^{/Start.the.merging-rebase}") ||
	die "Could not find merging-rebase marker in $SHEARS_BRANCH"
OLD_UPSTREAM=$(git rev-parse "$OLD_MARKER^1")
NEW_UPSTREAM=$(git rev-parse "$UPSTREAM_BRANCH")
TIP_OID=$(git rev-parse HEAD)

echo "Old marker: $OLD_MARKER"
echo "Old upstream: $OLD_UPSTREAM"
echo "New upstream: $NEW_UPSTREAM"
echo "Current tip: $TIP_OID"

# Check if shears branch is behind GfW main (commits in main not in shears)
# The second parent of the marker points to the GfW branch tip at marker creation time
GFW_MAIN_BRANCH="origin/main"
BEHIND_COUNT=$(git rev-list --count "$TIP_OID..$GFW_MAIN_BRANCH" 2>/dev/null || echo "0")

if test "$BEHIND_COUNT" -gt 0; then
	echo "::group::Syncing $BEHIND_COUNT commits from $GFW_MAIN_BRANCH"
	echo "Shears branch is behind $GFW_MAIN_BRANCH by $BEHIND_COUNT commits"
	echo "Rebasing main commits onto shears branch..."
	
	run_rebase_with_ai -r HEAD "$GFW_MAIN_BRANCH"
	
	# Restore local branch to point to new HEAD
	git checkout -B "$LOCAL_BRANCH"
	
	# Update TIP_OID after the sync rebase
	TIP_OID=$(git rev-parse HEAD)
	echo "Updated tip after sync: $TIP_OID"
	echo "::endgroup::"
fi

# Initialize report
cat >"$REPORT_FILE" <<EOF
# Rebase Summary: ${SHEARS_BRANCH##*/}

**Date**: $(date -u +%Y-%m-%d)
**From**: $(git show --no-patch --format=reference "$OLD_MARKER")
**To**: $(git show --no-patch --format=reference "$NEW_UPSTREAM")

## Conflicts Resolved

EOF

CONFLICTS_SKIPPED=0
CONFLICTS_RESOLVED=0
SKIPPED_MAP_FILE="$WORKTREE_DIR/skipped-commits.map"
: >"$SKIPPED_MAP_FILE"

# Create new marker with two parents: upstream + current tip
echo "Creating new marker commit..."
MARKER_OID=$(git commit-tree "$UPSTREAM_BRANCH^{tree}" \
	-p "$UPSTREAM_BRANCH" \
	-p "$TIP_OID" \
	-m "Start the merging-rebase to $UPSTREAM_BRANCH

This commit starts the rebase of $OLD_MARKER to $NEW_UPSTREAM")

echo "New marker: $MARKER_OID"

# Apply graft to hide second parent during rebase
echo "Applying graft..."
git replace --graft "$MARKER_OID" "$UPSTREAM_BRANCH"

# Run the rebase
echo "Starting rebase..."
REBASE_TODO_COUNT=$(git rev-list --count "$OLD_MARKER..$TIP_OID")
echo "Rebasing $REBASE_TODO_COUNT commits..."

run_rebase_with_ai -r --onto "$MARKER_OID" "$OLD_MARKER" HEAD

# Clean up graft
echo "Rebase complete, cleaning up graft..."
git replace -d "$MARKER_OID"

# Verify the marker still has two parents
echo "Verifying marker integrity..."
MARKER_IN_RESULT=$(git rev-parse "HEAD^{/Start.the.merging-rebase}")
PARENT_COUNT=$(git rev-list --parents -1 "$MARKER_IN_RESULT" | wc -w)
test "$PARENT_COUNT" -eq 3 || # commit itself + 2 parents
	die "Marker should have 2 parents, found $((PARENT_COUNT - 1))"

# Generate range-diff
echo "Generating range-diff..."
RANGE_DIFF=$(git range-diff "$OLD_UPSTREAM..$OLD_MARKER^2" "$NEW_UPSTREAM..$MARKER_IN_RESULT^2" 2>/dev/null || echo "Unable to generate range-diff")

# Annotate range-diff with upstream OIDs for skipped commits
if test -s "$SKIPPED_MAP_FILE"; then
	SED_SCRIPT=$(sed 's/\([^ ]*\) \(.*\)/s,\1,\1 (upstream: \2),/' "$SKIPPED_MAP_FILE")
	RANGE_DIFF=$(echo "$RANGE_DIFF" | sed "$SED_SCRIPT")
fi

# Finalize report
cat >>"$REPORT_FILE" <<EOF

## Statistics

- Total conflicts: $((CONFLICTS_SKIPPED + CONFLICTS_RESOLVED))
- Skipped (upstreamed): $CONFLICTS_SKIPPED
- Resolved surgically: $CONFLICTS_RESOLVED

<details>
<summary>Range-diff (click to expand)</summary>

\`\`\`
$RANGE_DIFF
\`\`\`

</details>
EOF

echo ""
echo "=========================================="
echo "Rebase completed successfully!"
echo "Worktree: $WORKTREE_DIR"
echo "New HEAD: $(git rev-parse HEAD)"
echo "Report: $REPORT_FILE"
echo ""
cat "$REPORT_FILE"
echo ""
echo "To push:"
echo "  cd $WORKTREE_DIR && git push --force origin HEAD:${SHEARS_BRANCH##*/}"
echo "=========================================="

# For GitHub Actions: output variables
if test -n "$GITHUB_OUTPUT"; then
	echo "worktree=$WORKTREE_DIR" >>"$GITHUB_OUTPUT"
	echo "report=$REPORT_FILE" >>"$GITHUB_OUTPUT"
	echo "head=$(git rev-parse HEAD)" >>"$GITHUB_OUTPUT"
fi
