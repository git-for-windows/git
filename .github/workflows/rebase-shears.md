---
on:
  workflow_dispatch:
    inputs:
      branch:
        description: 'Shears branch to update (seen, next, main, maint, or all)'
        required: true
        type: choice
        options:
          - all
          - seen
          - next
          - main
          - maint
      push:
        description: 'Push the result after successful rebase'
        required: false
        type: boolean
        default: false
  schedule: daily around 6:00

runs-on: windows-latest

engine:
  id: copilot
  model: claude-opus-4
  agent: conflict-resolver

permissions:
  contents: read

safe-outputs:
  upload-asset:
    max: 1
  jobs:
    push-branch:
      description: "Push the rebased branch to origin"
      runs-on: windows-latest
      output: "Branch pushed successfully!"
      permissions:
        contents: write
      inputs:
        branch:
          description: "The shears branch name (seen, next, main, or maint)"
          required: true
          type: string
        worktree:
          description: "Path to the worktree containing the rebased branch"
          required: true
          type: string
      steps:
        - name: Push rebased branch
          env:
            BRANCH: "${{ inputs.branch }}"
            WORKTREE: "${{ inputs.worktree }}"
          run: |
            cd "$WORKTREE"
            git push --force origin "HEAD:shears/$BRANCH"

steps:
  - name: Setup Git for Windows SDK
    uses: git-for-windows/setup-git-for-windows-sdk@v1
    with:
      flavor: minimal
  - name: Install Copilot CLI
    shell: bash
    run: |
      curl -fsSL https://gh.io/copilot-install | bash
      echo "$HOME/.local/bin" >> $GITHUB_PATH
  - name: Configure git
    shell: bash
    run: |
      git config user.name "GitHub Actions"
      git config user.email "actions@github.com"
  - name: Add upstream remote
    shell: bash
    run: |
      git remote add upstream https://github.com/git/git.git || true
      git fetch upstream --no-tags

tools:
  edit:
  bash:
    - "git:*"
    - "echo"
    - "cat"
    - "head"
    - "tail"
    - "grep"
    - "wc"
    - "mktemp"
    - "pwd"
    - "copilot"
    - "make"
    - "nproc"
    - "tee"

safe-inputs:
  run-rebase:
    description: "Run the merging-rebase script for a shears branch"
    inputs:
      branch:
        type: string
        required: true
        description: "The shears branch to update (seen, next, main, maint)"
    run: |
      set -e
      BRANCH="${INPUT_BRANCH}"
      SCRIPTS_DIR="${GITHUB_WORKSPACE}/ci"
      
      # Map branch names to upstream
      case "$BRANCH" in
        main) UPSTREAM="upstream/master" ;;
        *)    UPSTREAM="upstream/$BRANCH" ;;
      esac
      
      # Fetch latest
      git fetch origin "shears/$BRANCH"
      
      # Check if anything needs to be done
      if test 0 = "$(git rev-list --count "origin/shears/$BRANCH..$UPSTREAM")"; then
        echo "Nothing to do: $UPSTREAM has no new commits"
        exit 0
      fi
      
      # Run the rebase script
      exec "$SCRIPTS_DIR/rebase-branch.sh" "origin/shears/$BRANCH" "$UPSTREAM" "$SCRIPTS_DIR"
    env:
      GITHUB_TOKEN: "${{ secrets.COPILOT_GITHUB_TOKEN }}"
    timeout: 3600

  run-all-rebases:
    description: "Run merging-rebase for all four shears branches sequentially"
    run: |
      set -e
      SCRIPTS_DIR="${GITHUB_WORKSPACE}/ci"
      
      for BRANCH in seen next main maint; do
        echo ""
        echo "========================================"
        echo "Processing shears/$BRANCH"
        echo "========================================"
        
        case "$BRANCH" in
          main) UPSTREAM="upstream/master" ;;
          *)    UPSTREAM="upstream/$BRANCH" ;;
        esac
        
        git fetch origin "shears/$BRANCH"
        
        if test 0 = "$(git rev-list --count "origin/shears/$BRANCH..$UPSTREAM")"; then
          echo "Nothing to do: $UPSTREAM has no new commits"
          continue
        fi
        
        "$SCRIPTS_DIR/rebase-branch.sh" "origin/shears/$BRANCH" "$UPSTREAM" "$SCRIPTS_DIR" || {
          echo "Rebase failed for shears/$BRANCH"
          exit 1
        }
      done
      
      echo ""
      echo "All branches processed successfully!"
    env:
      GITHUB_TOKEN: "${{ secrets.COPILOT_GITHUB_TOKEN }}"
    timeout: 7200
---

# Merging-Rebase Automation for Git for Windows

This workflow updates the `shears/*` branches by rebasing Git for Windows patches
onto the latest upstream Git branches.

## What This Workflow Does

1. Fetches the latest `origin/shears/${{ github.event.inputs.branch }}` and corresponding upstream branch
2. Creates a new merging-rebase marker commit
3. Rebases all downstream commits onto the new upstream
4. When conflicts occur, uses AI to determine resolution:
   - **Skip**: If the patch was already upstreamed
   - **Resolve**: If surgical conflict resolution is needed
5. Generates a detailed report of all conflict resolutions
6. Optionally pushes the result

## Execution

Run the `run-rebase` safe-input with the branch parameter:

```
Branch to update: "${{ github.event.inputs.branch }}"
Push after success: "${{ github.event.inputs.push }}"
```

Execute the rebase by calling the `run-rebase` tool with the branch name.

If the rebase completes successfully and push is enabled, push the result:
```bash
cd <worktree-path>
git push --force origin HEAD:shears/${{ github.event.inputs.branch }}
```

## Conflict Resolution Guidelines

When you encounter a merge conflict, follow the guidelines in the conflict-resolver agent:

1. Check `git range-diff` output to see if patch was upstreamed
2. If upstreamed: skip without editing
3. If not upstreamed: resolve surgically, stage, and continue
4. Document each resolution in the report

## Important Notes

- The rebase happens in a temporary worktree to avoid corrupting the main checkout
- All automation scripts are accessed from `$GITHUB_WORKSPACE`, not the worktree
- The workflow will fail if AI cannot resolve a conflict (exit code 2)
- Manual intervention may be needed for complex conflicts
