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
	write_script .git/hooks/pre-command <<-EOF &&
	exit 1
	EOF
	echo "third" >> file &&
	test_must_fail git add file &&
	test_path_is_missing "$(cat .git/post-command.out)"
'

test_done
