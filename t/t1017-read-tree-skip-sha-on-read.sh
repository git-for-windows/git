#!/bin/sh

test_description='check that read-tree works with core.gvfs config value'

. ./test-lib.sh
. "$TEST_DIRECTORY"/lib-read-tree.sh

test_expect_success setup '
	echo one >a &&
	git add a &&
	git commit -m initial
'
test_expect_success 'read-tree without core.gvsf' '
	read_tree_u_must_succeed -m -u HEAD
'

test_expect_success 'read-tree with core.gvfs set to 1' '
	git config core.gvfs 1 &&
	read_tree_u_must_succeed -m -u HEAD
'

test_done
