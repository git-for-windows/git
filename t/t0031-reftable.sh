#!/bin/sh
#
# Copyright (c) 2020 Google LLC
#

test_description='reftable basics'

. ./test-lib.sh

INVALID_SHA1=aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa

git_init () {
	git init -b primary "$@"
}

initialize ()  {
	rm -rf .git &&
	git_init --ref-storage=reftable &&
	mv .git/hooks .git/hooks-disabled
}

test_expect_success 'SHA256 support, env' '
	rm -rf .git &&
	GIT_DEFAULT_HASH=sha256 && export GIT_DEFAULT_HASH &&
	git_init --ref-storage=reftable &&
	mv .git/hooks .git/hooks-disabled &&
	test_commit file
'

test_expect_success 'SHA256 support, option' '
	rm -rf .git &&
	git_init --ref-storage=reftable --object-format=sha256 &&
	mv .git/hooks .git/hooks-disabled &&
	test_commit file
'

test_expect_success 'delete ref' '
	initialize &&
	test_commit file &&
	SHA=$(git show-ref -s --verify HEAD) &&
	test_write_lines "$SHA refs/heads/primary" "$SHA refs/tags/file" >expect &&
	git show-ref >actual &&
	! git update-ref -d refs/tags/file $INVALID_SHA1 &&
	test_cmp expect actual &&
	git update-ref -d refs/tags/file $SHA  &&
	test_write_lines "$SHA refs/heads/primary" >expect &&
	git show-ref >actual &&
	test_cmp expect actual
'


test_expect_success 'clone calls transaction_initial_commit' '
	test_commit message1 file1 &&
	git clone . cloned &&
	(test  -f cloned/file1 || echo "Fixme.")
'

test_expect_success 'basic operation of reftable storage: commit, show-ref' '
	initialize &&
	test_commit file &&
	test_write_lines refs/heads/primary refs/tags/file >expect &&
	git show-ref &&
	git show-ref | cut -f2 -d" " >actual &&
	test_cmp actual expect
'

test_expect_success 'reflog, repack' '
	initialize &&
	for count in $(test_seq 1 10)
	do
		test_commit "number $count" file.t $count number-$count ||
		return 1
	done &&
	git pack-refs &&
	ls -1 .git/reftable >table-files &&
	test_line_count = 2 table-files &&
	git reflog refs/heads/primary >output &&
	test_line_count = 10 output &&
	grep "commit (initial): number 1" output &&
	grep "commit: number 10" output &&
	git gc &&
	git reflog refs/heads/primary >output &&
	test_line_count = 0 output
'

test_expect_success 'branch switch in reflog output' '
	initialize &&
	test_commit file1 &&
	git checkout -b branch1 &&
	test_commit file2 &&
	git checkout -b branch2 &&
	git switch - &&
	git rev-parse --symbolic-full-name HEAD >actual &&
	echo refs/heads/branch1 >expect &&
	test_cmp actual expect
'


# This matches show-ref's output
print_ref() {
	echo "$(git rev-parse "$1") $1"
}

test_expect_success 'peeled tags are stored' '
	initialize &&
	test_commit file &&
	git tag -m "annotated tag" test_tag HEAD &&
	{
		print_ref "refs/heads/primary" &&
		print_ref "refs/tags/file" &&
		print_ref "refs/tags/test_tag" &&
		print_ref "refs/tags/test_tag^{}"
	} >expect &&
	git show-ref -d >actual &&
	test_cmp expect actual
'

test_expect_success 'show-ref works on fresh repo' '
	initialize &&
	rm -rf .git &&
	git_init --ref-storage=reftable &&
	>expect &&
	! git show-ref >actual &&
	test_cmp expect actual
'

test_expect_success 'checkout unborn branch' '
	initialize &&
	git checkout -b primary
'


test_expect_success 'dir/file conflict' '
	initialize &&
	test_commit file &&
	! git branch primary/forbidden
'


test_expect_success 'do not clobber existing repo' '
	rm -rf .git &&
	git_init --ref-storage=files &&
	cat .git/HEAD >expect &&
	test_commit file &&
	(git_init --ref-storage=reftable || true) &&
	cat .git/HEAD >actual &&
	test_cmp expect actual
'

# cherry-pick uses a pseudo ref.
test_expect_success 'pseudo refs' '
	initialize &&
	test_commit message1 file1 &&
	test_commit message2 file2 &&
	git branch source &&
	git checkout HEAD^ &&
	test_commit message3 file3 &&
	git cherry-pick source &&
	test -f file2
'

# cherry-pick uses a pseudo ref.
test_expect_success 'rebase' '
	initialize &&
	test_commit message1 file1 &&
	test_commit message2 file2 &&
	git branch source &&
	git checkout HEAD^ &&
	test_commit message3 file3 &&
	git rebase source &&
	test -f file2
'

test_expect_success 'worktrees' '
	git_init --ref-storage=reftable start &&
	(cd start && test_commit file1 && git checkout -b branch1 &&
	git checkout -b branch2 &&
	git worktree add  ../wt
	) &&
	cd wt &&
	git checkout branch1 &&
	git branch
'

test_expect_success 'worktrees 2' '
	initialize &&
	test_commit file1 &&
	mkdir existing_empty &&
	git worktree add --detach existing_empty primary
'

test_expect_success 'FETCH_HEAD' '
	initialize &&
	test_commit one &&
	(git_init sub && cd sub && test_commit two) &&
	git --git-dir sub/.git rev-parse HEAD >expect &&
	git fetch sub &&
	git checkout FETCH_HEAD &&
	git rev-parse HEAD >actual &&
	test_cmp expect actual
'

test_done
