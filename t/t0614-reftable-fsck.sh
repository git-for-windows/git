#!/bin/sh

test_description='Test reftable backend consistency check'

GIT_TEST_DEFAULT_REF_FORMAT=reftable
export GIT_TEST_DEFAULT_REF_FORMAT

. ./test-lib.sh

for TABLE_NAME in "foo-bar-e4d12d59.ref" \
	"0x00000000zzzz-0x00000000zzzz-e4d12d59.ref" \
	"0x000000000001-0x000000000002-e4d12d59.abc" \
	"0x000000000001-0x000000000002-e4d12d59.refabc"; do
	test_expect_success "table name $TABLE_NAME should be checked" '
		test_when_finished "rm -rf repo" &&
		git init repo &&
		(
			cd repo &&
			git commit --allow-empty -m initial &&

			git refs verify 2>err &&
			test_must_be_empty err &&

			touch ".git/reftable/$TABLE_NAME" &&

			git refs verify 2>err &&
			cat >expect <<-EOF &&
			warning: ${TABLE_NAME}: badReftableTableName: file with invalid table name
			EOF
			test_cmp expect err
		)
	'
done

test_expect_success "invalid file type should be checked" '
	test_when_finished "rm -rf repo" &&
	git init repo &&
	(
		cd repo &&
		git commit --allow-empty -m initial &&

		git refs verify 2>err &&
		test_must_be_empty err &&

		mkdir ".git/reftable/foo" &&

		test_must_fail git refs verify 2>err &&
		cat >expect <<-EOF &&
		error: foo: badReftableFiletype: file with unexpected type
		EOF
		test_cmp expect err
	)
'

test_done
