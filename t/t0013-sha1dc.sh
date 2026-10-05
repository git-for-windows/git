#!/bin/sh

test_description='test sha1 collision detection'

. ./test-lib.sh
TEST_DATA="$TEST_DIRECTORY/t0013"

test_lazy_prereq SHA1_IS_SHA1DC 'test-tool sha1-is-sha1dc'

if ! test_have_prereq SHA1_IS_SHA1DC
then
	skip_all='skipping sha1 collision tests, not using sha1collisiondetection'
	test_done
fi

test_lazy_prereq SHA1DC_RS '
	test rust = "$(GIT_CONFIG_PARAMETERS="${SQ}core.sha1dcBackend=rust${SQ}" \
		test-tool sha1-is-sha1dc --backend)"
'

test_expect_success 'test-sha1 detects shattered pdf' '
	test_must_fail test-tool sha1 <"$TEST_DATA/shattered-1.pdf" 2>err &&
	test_grep collision err &&
	test_grep 38762cf7f55934b34d179ae6a4c80cadccbb7f0a err &&
	if test_have_prereq SHA1DC_RS
	then
		test_must_fail env \
			GIT_CONFIG_PARAMETERS="${SQ}core.sha1dcBackend=c${SQ}" \
			test-tool sha1 <"$TEST_DATA/shattered-1.pdf" 2>err &&
		test_grep collision err &&
		test_grep 38762cf7f55934b34d179ae6a4c80cadccbb7f0a err
	fi
'

test_expect_success SHA1DC_RS 'select SHA1DC backend via config' '
	test rust = "$(test-tool sha1-is-sha1dc --backend)" &&
	test_config core.sha1dcBackend c &&
	test c = "$(test-tool sha1-is-sha1dc --backend)"
'

test_done
