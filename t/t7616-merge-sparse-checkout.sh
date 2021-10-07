#!/bin/sh

test_description='merge can handle sparse-checkout'

. ./test-lib.sh

# merges with conflicts

test_expect_success 'setup' '
	git branch -M main &&
	test_commit a &&
	test_commit file &&
	git checkout -b delete-file &&
	git rm file.t &&
	test_tick &&
	git commit -m "remove file" &&
	git checkout main &&
	test_commit modify file.t changed
'

test_expect_success 'merge conflict deleted file and modified' '
	echo "/a.t" >.git/info/sparse-checkout &&
	test_config core.sparsecheckout true &&
	git checkout -f &&
	test_path_is_missing file.t &&
	test_must_fail git merge delete-file &&
	test_path_is_file file.t &&
	test "changed" = "$(cat file.t)"
'

test_done
