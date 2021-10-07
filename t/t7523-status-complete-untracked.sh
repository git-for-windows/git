#!/bin/sh

test_description='git status untracked complete tests'

. ./test-lib.sh

test_expect_success 'setup' '
	cat >.gitignore <<-\EOF &&
	*.ign
	ignored_dir/
	EOF

	mkdir tracked ignored_dir &&
	touch tracked_1.txt tracked/tracked_1.txt &&
	git add . &&
	test_tick &&
	git commit -m"Adding original file." &&
	mkdir untracked &&
	touch ignored.ign ignored_dir/ignored_2.txt \
	      untracked_1.txt untracked/untracked_2.txt untracked/untracked_3.txt
'

test_expect_success 'verify untracked-files=complete' '
	cat >expect <<-\EOF &&
	? expect
	? output
	? untracked/
	? untracked/untracked_2.txt
	? untracked/untracked_3.txt
	? untracked_1.txt
	! ignored.ign
	! ignored_dir/
	EOF

	git status --porcelain=v2 --untracked-files=complete --ignored >output &&
	test_cmp expect output
'

test_done
