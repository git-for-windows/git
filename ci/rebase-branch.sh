#!/usr/bin/bash
#
# Rebase a branch using the merging-rebase strategy with AI-powered conflict resolution.
#
# Usage: rebase-branch.sh <shears-branch> <upstream-branch> [<scripts-dir>]
#
# Parameters:
#   shears-branch   - The branch to rebase (e.g., shears/seen)
#   upstream-branch - The upstream branch to rebase onto (e.g., upstream/seen)
#   scripts-dir     - Optional: directory containing this script and agents
#                     (defaults to the directory containing this script)
#
# Preconditions:
#   - Must be run from a git repository
#   - origin/<shears-branch> and <upstream-branch> must be fetched
#   - GH_TOKEN or GITHUB_TOKEN must be set for AI resolution
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

set -ex

die () {
	echo "error: $*" >&2
	exit 1
}

usage () {
	sed -n '3,/^#$/s/^# //p' "$0"
	exit 1
}

# Function to generate a correspondence map between two commit ranges
# Usage: generate_correspondence_map <our-range> <their-range> <output-file>
# The map file contains range-diff output that can be searched for correspondences
generate_correspondence_map () {
	local our_range=$1 their_range=$2 output_file=$3
	git -c core.abbrev=false range-diff --no-color "$our_range" "$their_range" >"$output_file" 2>/dev/null || :
}

# Function to find a corresponding commit in a map file
# Usage: find_correspondence <oid> <map-file>
# Returns: corresponding OID on stdout, exit 0 if found (= or ! match), 1 if not
# Sets CORRESPONDENCE_TYPE to "=" (identical) or "!" (modified)
find_correspondence () {
	local oid=$1 map_file=$2
	test -s "$map_file" || return 1
	
	# Look for this OID in the left side of range-diff output
	# Format: "N: <oid> = M: <oid>" (identical) or "N: <oid> ! M: <oid>" (modified)
	local match=$(sed -n "s/^[0-9]*: $oid \([!=]\) [0-9]*: \([0-9a-f]*\).*/\1 \2/p" "$map_file" | head -1)
	test -n "$match" || return 1
	
	CORRESPONDENCE_TYPE=${match% *}
	echo "${match#* }"
}

# Generate git log -L commands for conflicting hunks
generate_log_l_commands () {
	local file=$1
	local commands=""

	# Extract line ranges from diff hunk headers
	while IFS= read -r hunk_header; do
		# Extract the "new" side line range (current branch)
		local line_range=$(echo "$hunk_header" | sed -n 's/^@@ -[0-9,]* +\([0-9]*\),*\([0-9]*\) @@.*/\1 \2/p')
		if test -n "$line_range"; then
			local start count end
			start=${line_range% *}
			count=${line_range#* }
			count=${count:-1}
			end=$((start + count - 1))
			test $end -lt $start && end=$start
			commands="${commands}git log -L $start,$end:$file REBASE_HEAD..HEAD
"
		fi
	done < <(git diff -- "$file" | grep '^@@ ')

	echo "$commands"
}

# Function to resolve a single conflict with AI
# Usage: resolve_conflict_with_ai [<tried-correspondences>]
resolve_conflict_with_ai () {
	local tried_correspondences=$1
	
	# Get REBASE_HEAD info once
	local rebase_head_oid rebase_head_ref rebase_head_oneline
	rebase_head_oid=$(git rev-parse REBASE_HEAD)
	rebase_head_ref=$(git show --no-patch --format=reference REBASE_HEAD)
	rebase_head_oneline=$(git show --no-patch --format='%h %s' REBASE_HEAD)

	echo "Conflict detected at: $rebase_head_ref"

	# Collect minimal info for the prompt
	local conflicting_files log_l_commands
	conflicting_files=$(git diff --name-only --diff-filter=U)

	# Generate git log -L commands for each conflicting file
	log_l_commands=""
	for file in $conflicting_files; do
		log_l_commands="${log_l_commands}$(generate_log_l_commands "$file")"
	done

	# Add context about tried correspondences
	local correspondence_context=""
	if test -n "$tried_correspondences"; then
		correspondence_context="
Note: We found corresponding commits from previous/sibling rebases but they did not apply cleanly:
$tried_correspondences
You may want to examine these with 'git show <oid>' for hints on how to resolve."
	fi

	# Build the prompt - minimal, letting LLM discover context
	local prompt="Resolve merge conflict during rebase of commit REBASE_HEAD.

Conflicting files: $conflicting_files
$correspondence_context
Investigation commands:
- See the patch: git show REBASE_HEAD
- See conflict markers: view <file>
- Check if upstreamed: git range-diff REBASE_HEAD^! REBASE_HEAD..
- Try higher creation factor: git range-diff --creation-factor=200 REBASE_HEAD^! REBASE_HEAD..
- See upstream changes to conflicting lines:
${log_l_commands}
Decision rules:
1. If range-diff shows correspondence (e.g. '1: abc = 1: def'), output: skip <upstream-oid>
2. If patch needs surgical resolution, edit files, stage with 'git add', output: continue
3. If unresolvable, output: fail

Your FINAL line must be exactly: skip <oid>, continue, or fail"

	echo "Invoking AI for conflict resolution..."
	local ai_output=$(copilot -p "$prompt" \
		${COPILOT_MODEL:+--model "$COPILOT_MODEL"} \
		--allow-tool 'view' \
		--allow-tool 'edit' \
		--allow-tool 'shell(git show)' \
		--allow-tool 'shell(git diff)' \
		--allow-tool 'shell(git log)' \
		--allow-tool 'shell(git range-diff)' \
		--allow-tool 'shell(git add)' \
		--allow-tool 'shell(git grep)' \
		--allow-tool 'shell(git rev-list)' \
		--allow-tool 'shell(git checkout)' \
		--allow-tool 'shell(grep)' \
		--allow-tool 'shell(head)' \
		--allow-tool 'shell(tail)' \
		--allow-tool 'shell(sed)' \
		--allow-tool 'shell(cat)' \
		--allow-tool 'shell(awk)' \
		2>&1 | tee /dev/stderr)
	local ai_exit_code=$?

	# Log the AI output in a collapsible group
	echo "::group::AI Output for $rebase_head_oneline"
	echo "$ai_output"
	if test $ai_exit_code -ne 0; then
		echo "::warning::Copilot exited with code $ai_exit_code"
	fi
	echo "::endgroup::"

	# Extract the decision - look for continue/skip/fail before the stats trailer
	local last_line decision upstream_oid
	last_line=$(echo "$ai_output" | sed -n '
		/^continue$/b found
		/^skip [0-9a-f][0-9a-f]*$/b found
		/^fail$/b found
		b
		:found
		h
		# If this is the last line, output it
		${ p; q }
		# Read next line
		n
		# If not empty, this was not the decision line before stats
		/^$/!b
		# Empty lines before stats
		:emptyloop
		n
		/^$/b emptyloop
		# Stats loop - no empty lines allowed after first stats line
		:stats
		/[A-Za-z][^:]\{0,30\}:$/{ n; /^ /!b; :ind; ${ g; p; q }; n; /^ /b ind; b stats }
		/^[^:]\{1,30\}: /!b
		${ g; p; q }
		n
		b stats
	')
	decision=$(echo "$last_line" | awk '{print tolower($1)}')

	case "$decision" in
	skip)
		upstream_oid=$(echo "$last_line" | awk '{print $2}')
		if test -n "$upstream_oid"; then
			echo "$rebase_head_oid $upstream_oid" >>"$SKIPPED_MAP_FILE"
			echo "::notice::Skipping commit (upstream: $upstream_oid): $rebase_head_oneline"
			cat >>"$REPORT_FILE" <<SKIP_EOF

### Skipped: $rebase_head_ref

Upstream equivalent: $(git show --no-patch --format=reference "$upstream_oid" || echo "$upstream_oid")

<details>
<summary>Range-diff</summary>

\`\`\`
$(git range-diff --creation-factor=99 "$rebase_head_oid^!" "$upstream_oid^!" || echo "Unable to generate range-diff")
\`\`\`

</details>

SKIP_EOF
		else
			echo "::notice::Skipping commit (already upstream): $rebase_head_oneline"
		fi
		CONFLICTS_SKIPPED=$((CONFLICTS_SKIPPED + 1))
		git rebase --skip
		;;
	continue)
		echo "::notice::Resolved conflict surgically: $rebase_head_oneline"
		CONFLICTS_RESOLVED=$((CONFLICTS_RESOLVED + 1))
		git rebase --continue
		
		# Verify build after surgical resolution
		echo "::group::Verifying build"
		if ! make -j$(nproc) 2>&1 | tee make.log; then
			echo "::endgroup::"
			echo "::warning::Build failed after conflict resolution, giving AI another chance"
			
			local retry_prompt="Build failed after your conflict resolution. Fix the compilation error.

Files you modified: $(git diff --name-only HEAD^)

Investigation:
- See full build log: view make.log
- See your changes: git diff HEAD^
- Edit files to fix, then: git add <file> && git commit --amend --no-edit

Build errors (last 15 lines):
$(tail -15 make.log)

Output 'continue' when fixed, or 'fail' if you cannot fix it.
Your FINAL line must be exactly: continue or fail"

			local retry_output=$(copilot -p "$retry_prompt" \
				${COPILOT_MODEL:+--model "$COPILOT_MODEL"} \
				--allow-tool 'view' \
				--allow-tool 'edit' \
				--allow-tool 'shell(git show)' \
				--allow-tool 'shell(git diff)' \
				--allow-tool 'shell(git add)' \
				--allow-tool 'shell(git commit --amend)' \
				2>&1 | tee /dev/stderr)
			local retry_exit_code=$?
			
			echo "::group::AI Retry Output"
			echo "$retry_output"
			if test $retry_exit_code -ne 0; then
				echo "::warning::Copilot exited with code $retry_exit_code"
			fi
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
				exit 2
			fi
			echo "::endgroup::"
		else
			echo "::endgroup::"
		fi
		rm -f make.log
		;;
	fail)
		echo "::error::AI could not resolve conflict: $rebase_head_oneline"
		cat >>"$REPORT_FILE" <<EOF

### FAILED: $rebase_head_ref

AI could not resolve this conflict. Full output:

\`\`\`
$ai_output
\`\`\`

EOF
		exit 2
		;;
	*)
		echo "::error::Unexpected AI decision '$decision': $rebase_head_oneline"
		cat >>"$REPORT_FILE" <<EOF

### FAILED: $rebase_head_ref

Unexpected AI decision: '$decision'. Full output:

\`\`\`
$ai_output
\`\`\`

EOF
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

		# Get REBASE_HEAD info once
		local rebase_head_oid rebase_head_ref rebase_head_oneline
		rebase_head_oid=$(git rev-parse REBASE_HEAD)
		rebase_head_ref=$(git show --no-patch --format=reference REBASE_HEAD)
		rebase_head_oneline=$(git show --no-patch --format='%h %s' REBASE_HEAD)

		# Check upstream correspondence (= means identical, skip it)
		if corresponding_oid=$(find_correspondence "$rebase_head_oid" "$UPSTREAM_MAP") &&
		   test "$CORRESPONDENCE_TYPE" = "="; then
			echo "$rebase_head_oid $corresponding_oid" >>"$SKIPPED_MAP_FILE"
			echo "::notice::Trivial skip (upstream: $corresponding_oid): $rebase_head_oneline"
			cat >>"$REPORT_FILE" <<TRIVIAL_SKIP_EOF

### Skipped (trivial): $rebase_head_ref

Upstream equivalent: $(git show --no-patch --format=reference "$corresponding_oid" 2>/dev/null || echo "$corresponding_oid")

Detected via exact range-diff match (no AI needed).

TRIVIAL_SKIP_EOF
			CONFLICTS_SKIPPED=$((CONFLICTS_SKIPPED + 1))
			git rebase --skip
			continue
		fi

		# Try previous/sibling correspondences (reuse their resolution via merge-tree)
		local tried_correspondences=""
		for map_file in "$PREVIOUS_MAP" "$SIBLING_MAP"; do
			test -s "$map_file" || continue
			corresponding_oid=$(find_correspondence "$rebase_head_oid" "$map_file") || continue
			
			echo "::notice::Found correspondence: $corresponding_oid for $rebase_head_oneline"
			tried_correspondences="${tried_correspondences:+$tried_correspondences }$corresponding_oid"
			# Try merge-tree: if clean, use the resulting tree
			if result_tree=$(git merge-tree --write-tree HEAD^ REBASE_HEAD "$corresponding_oid" 2>/dev/null) &&
			   git read-tree --reset -u "$result_tree" &&
			   git commit -C REBASE_HEAD; then
				echo "::notice::Used resolution from: $corresponding_oid"
				cat >>"$REPORT_FILE" <<RESOLVED_EOF

### Resolved via correspondence: $rebase_head_ref

Used resolution from: $(git show --no-patch --format=reference "$corresponding_oid" 2>/dev/null || echo "$corresponding_oid")

RESOLVED_EOF
				CONFLICTS_RESOLVED=$((CONFLICTS_RESOLVED + 1))
				git rebase --continue
				continue 2
			fi
		done

		# Non-trivial conflict - invoke AI (pass tried correspondences as context)
		resolve_conflict_with_ai "$tried_correspondences"
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

# Validate branches exist (shears branch is fetched as origin/<branch>)
git rev-parse --verify "origin/$SHEARS_BRANCH" >/dev/null 2>&1 ||
	die "Branch not found: origin/$SHEARS_BRANCH"
git rev-parse --verify "$UPSTREAM_BRANCH" >/dev/null 2>&1 ||
	die "Branch not found: $UPSTREAM_BRANCH"

# Set up worktree in current directory, named after the branch
WORKTREE_DIR="$PWD/rebase-worktree-${SHEARS_BRANCH##*/}"
REPORT_FILE="$WORKTREE_DIR/conflict-report.md"

# Output worktree path early so recovery steps can find it on failure
if test -n "$GITHUB_OUTPUT"; then
	echo "worktree=$WORKTREE_DIR" >>"$GITHUB_OUTPUT"
fi

echo "::group::Setup worktree"
echo "Creating worktree at $WORKTREE_DIR..."
git worktree add -B "$SHEARS_BRANCH" "$WORKTREE_DIR" "origin/$SHEARS_BRANCH"
cd "$WORKTREE_DIR"
echo "::endgroup::"

# Find the old marker
OLD_MARKER=$(git rev-parse "HEAD^{/Start.the.merging-rebase}") ||
	die "Could not find merging-rebase marker in $SHEARS_BRANCH"
OLD_UPSTREAM=$(git rev-parse "$OLD_MARKER^1")
NEW_UPSTREAM=$(git rev-parse "$UPSTREAM_BRANCH")
TIP_OID=$(git rev-parse HEAD)

# Save original values for the final range-diff (before any sync/adoption)
ORIG_OLD_MARKER=$OLD_MARKER
ORIG_TIP_OID=$TIP_OID

echo "::notice::Old marker: $OLD_MARKER"
echo "::notice::Old upstream: $OLD_UPSTREAM"
echo "::notice::New upstream: $NEW_UPSTREAM"
echo "::notice::Current tip: $TIP_OID"

# Check if shears branch is behind GfW main (commits in main not in shears)
# The second parent of the marker points to the GfW branch tip at marker creation time
GFW_MAIN_BRANCH="origin/main"
BEHIND_COUNT=$(git rev-list --count "$TIP_OID..$GFW_MAIN_BRANCH" || echo "0")
PREVIOUS_MAP=""

if test "$BEHIND_COUNT" -gt 0; then
	if git rev-list --grep='^Start the merging-rebase' "$TIP_OID..$GFW_MAIN_BRANCH" | grep -q .; then
		# origin/main was rebased - generate correspondence before adopting
		PREVIOUS_MAP="$WORKTREE_DIR/previous-correspondence.map"
		MAIN_MARKER=$(git rev-parse "$GFW_MAIN_BRANCH^{/Start.the.merging-rebase}")
		generate_correspondence_map "$MAIN_MARKER..$GFW_MAIN_BRANCH" "$OLD_MARKER..$TIP_OID" "$PREVIOUS_MAP"
		
		echo "::notice::origin/main was rebased, adopting its $BEHIND_COUNT commits"
		git checkout -B "$SHEARS_BRANCH" "$GFW_MAIN_BRANCH"
		TIP_OID=$(git rev-parse HEAD)
		OLD_MARKER=$(git rev-parse "HEAD^{/Start.the.merging-rebase}")
		OLD_UPSTREAM=$(git rev-parse "$OLD_MARKER^1")
	else
		echo "::notice::Syncing $BEHIND_COUNT commits from $GFW_MAIN_BRANCH"
		echo "::group::Rebasing $BEHIND_COUNT commits from $GFW_MAIN_BRANCH on top of $SHEARS_BRANCH"
		run_rebase_with_ai -r HEAD "$GFW_MAIN_BRANCH"
		git checkout -B "$SHEARS_BRANCH"
		TIP_OID=$(git rev-parse HEAD)
		echo "::endgroup::"
	fi
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

# Generate correspondence maps for conflict resolution
UPSTREAM_MAP="$WORKTREE_DIR/upstream-correspondence.map"
SIBLING_MAP="$WORKTREE_DIR/sibling-correspondence.map"

# Map 1: Our commits vs upstream (for trivial skips)
generate_correspondence_map "$OLD_MARKER..$TIP_OID" "$OLD_UPSTREAM..$NEW_UPSTREAM" "$UPSTREAM_MAP"

# Map 2: Our commits vs sibling branch (seen→next→main hierarchy)
case "${SHEARS_BRANCH##*/}" in
main) SIBLING_BRANCH="origin/shears/next" ;;
next) SIBLING_BRANCH="origin/shears/seen" ;;
*)    SIBLING_BRANCH="" ;;
esac
if test -n "$SIBLING_BRANCH" && git rev-parse --verify "$SIBLING_BRANCH" >/dev/null 2>&1; then
	SIBLING_MARKER=$(git rev-parse "$SIBLING_BRANCH^{/Start.the.merging-rebase}" 2>/dev/null) || SIBLING_MARKER=""
	if test -n "$SIBLING_MARKER"; then
		generate_correspondence_map "$OLD_MARKER..$TIP_OID" "$SIBLING_MARKER..$SIBLING_BRANCH" "$SIBLING_MAP"
	fi
fi

# Create new marker with two parents: upstream + origin/main
echo "::group::Creating marker and running rebase"
MARKER_OID=$(git commit-tree "$UPSTREAM_BRANCH^{tree}" \
	-p "$UPSTREAM_BRANCH" \
	-p "$GFW_MAIN_BRANCH" \
	-m "Start the merging-rebase to $UPSTREAM_BRANCH

This commit starts the rebase of $OLD_MARKER to $NEW_UPSTREAM")

git replace --graft "$MARKER_OID" "$UPSTREAM_BRANCH"
REBASE_TODO_COUNT=$(git rev-list --count "$OLD_MARKER..$TIP_OID")
echo "Rebasing $REBASE_TODO_COUNT commits onto $MARKER_OID"

run_rebase_with_ai -r --onto "$MARKER_OID" "$OLD_MARKER"
echo "::endgroup::"

# Clean up graft and verify
git replace -d "$MARKER_OID"
MARKER_IN_RESULT=$(git rev-parse "HEAD^{/Start.the.merging-rebase}")
PARENT_COUNT=$(git rev-list --parents -1 "$MARKER_IN_RESULT" | wc -w)
test "$PARENT_COUNT" -eq 3 || # commit itself + 2 parents
	die "Marker should have 2 parents, found $((PARENT_COUNT - 1))"

# Generate range-diff (always use markers as base, never upstream branches)
# Compare original patches (before rebase) with rebased patches
RANGE_DIFF=$(git range-diff "$ORIG_OLD_MARKER..$ORIG_TIP_OID" "$MARKER_IN_RESULT..HEAD" || echo "Unable to generate range-diff")

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

echo "Rebase completed: $(git rev-parse --short HEAD)"
cat "$REPORT_FILE"
echo "To push: git push --force origin $(git rev-parse HEAD):$SHEARS_BRANCH"

# For GitHub Actions: output report path
if test -n "$GITHUB_OUTPUT"; then
	echo "report=$REPORT_FILE" >>"$GITHUB_OUTPUT"
fi
