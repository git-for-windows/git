#!/bin/sh

test_description='racy GIT'

. ./test-lib.sh

# This test can give false success if your machine is sufficiently
# slow or your trial happened to happen on second boundary.

for trial in 0 1 2 3 4
do
	test_expect_success "Racy git trial #$trial part A" '
		rm -f .git/index &&
		echo frotz >infocom &&
		git update-index --add infocom &&
		echo xyzzy >infocom &&

		git diff-files -p >out &&
		test_file_not_empty out
	'
	sleep 1

	test_expect_success "Racy git trial #$trial part B" '
		echo xyzzy >cornerstone &&
		git update-index --add cornerstone &&

		git diff-files -p >out &&
		test_file_not_empty out
	'
done

test_expect_success 'notice a rewrite that keeps size and mtime' '
	echo frotz >rewritten &&
	test-tool chmtime =1700000000 rewritten &&
	git update-index --add rewritten &&

	# ctime may only be compared to the second
	sleep 1 &&
	echo xyzzy >rewritten &&
	test-tool chmtime =1700000000 rewritten &&
	echo "AM rewritten" >expect &&
	git -c core.fscache=false status --porcelain rewritten >actual &&
	test_cmp expect actual &&
	git -c core.fscache=true status --porcelain rewritten >actual &&
	test_cmp expect actual
'

test_done
