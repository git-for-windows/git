#!/bin/sh

test_description='post-command hook'

. ./test-lib.sh

test_expect_success 'with no hook' '
	echo "first" > file &&
	git add file &&
	git commit -m "first"
'

test_expect_success 'with succeeding hook' '
	mkdir -p .git/hooks &&
	write_script .git/hooks/post-command <<-EOF &&
	echo "\$*" | sed "s/ --git-pid=[0-9]*//" \
		>\$(git rev-parse --git-dir)/post-command.out
	EOF
	echo "second" >> file &&
	git add file &&
	test "add file --exit_code=0" = "$(cat .git/post-command.out)"
'

test_expect_success 'with failing pre-command hook' '
	test_when_finished rm -f .git/hooks/pre-command &&
	write_script .git/hooks/pre-command <<-EOF &&
	exit 1
	EOF
	echo "third" >> file &&
	test_must_fail git add file &&
	test_path_is_missing "$(cat .git/post-command.out)"
'

test_expect_success 'with post-index-change config' '
	mkdir -p internal-hooks &&
	write_script internal-hooks/post-command <<-EOF &&
	echo ran >post-command.out
	EOF
	write_script internal-hooks/post-index-change <<-EOF &&
	echo ran >post-index-change.out
	EOF

	# prevent writing of sentinel files to this directory.
	test_when_finished chmod 775 internal-hooks &&
	chmod a-w internal-hooks &&

	git config core.hooksPath internal-hooks &&

	# First, show expected behavior.
	echo ran >expect &&
	rm -f post-command.out post-index-change.out &&

	# rev-parse leaves index intact, but runs post-command.
	git rev-parse HEAD &&
	test_path_is_missing post-index-change.out &&
	test_cmp expect post-command.out &&
	rm -f post-command.out &&

	echo stuff >>file &&
	# add updates the index and runs post-command.
	git add file &&
	test_cmp expect post-index-change.out &&
	test_cmp expect post-command.out &&

	# Now, show configured behavior
	git config postCommand.strategy worktree-change &&

	# rev-parse leaves index intact and thus skips post-command.
	rm -f post-command.out post-index-change.out &&
	git rev-parse HEAD &&
	test_path_is_missing post-index-change.out &&
	test_path_is_missing post-command.out &&

	echo stuff >>file &&
	# add keeps the worktree the same, so does not run post-command.
	rm -f post-command.out post-index-change.out &&
	git add file &&
	test_cmp expect post-index-change.out &&
	test_path_is_missing post-command.out &&

	echo stuff >>file &&
	# reset --hard updates the worktree.
	rm -f post-command.out post-index-change.out &&
	git reset --hard &&
	test_cmp expect post-index-change.out &&
	test_cmp expect post-command.out
'

test_done
