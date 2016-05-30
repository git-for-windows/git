#!/bin/sh

test_description='fetch using the flag to skip reachability and upload pack'

. ./test-lib.sh


test_expect_success setup '
	echo inital >a &&
	git add a &&
	git commit -m initial &&
	git clone . one
'

test_expect_success "fetch test" '
	cd one &&
	git config core.gvfs 16 &&
	rm -rf .git/objects/* &&
	git -C .. cat-file commit HEAD | git hash-object -w --stdin -t commit &&
	git fetch &&
	test_must_fail git rev-parse --verify HEAD^{tree}
'

test_done