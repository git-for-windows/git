#!/bin/sh
#
# Copyright (c) 2026 Johannes Schindelin
#

test_description='git archive with 4GB+ entries'

. ./test-lib.sh

if ! test_have_prereq EXPENSIVE,SIZE_T_IS_64BIT
then
	skip_all='expensive 4GB archive test; enable on 64-bit with GIT_TEST_LONG=true'
	test_done
fi

size_4gb=4294967296

tar_info_size () {
	"$TAR" tvf "$1" |
	awk 'NR == 1 { print $3 }'
}

tar_gz_info_size () {
	gzip -d -c <"$1" |
	"$TAR" tvf - |
	awk 'NR == 1 { print $3 }'
}

test_expect_success 'set up a 4GB file' '
	test_atexit "rm -f large large.zip large.tar large.tar.gz" &&
	# genrandom takes only an unsigned long...
	test-tool genrandom 123 $(($size_4gb - 1)) >large &&
	printf 1 >>large &&
	test $size_4gb = $(test_file_size large)
'

test_expect_success 'add and commit 4GB file' '
	git add large &&
	git commit -m large-file
'

test_expect_success UNZIP 'zip archive stores 4GB file size' '
	git archive -0 --format=zip HEAD >large.zip &&
	"$GIT_UNZIP" -t large.zip &&
	"$GIT_UNZIP" -l large.zip >large.zip.lst &&
	awk '\''$NF == "large" { print $1; exit }'\'' <large.zip.lst >actual &&
	echo $size_4gb >expect &&
	test_cmp expect actual
'

test_expect_success 'tar archive stores 4GB file size' '
	git archive --format=tar HEAD >large.tar &&
	tar_info_size large.tar >actual &&
	echo $size_4gb >expect &&
	test_cmp expect actual
'

test_expect_success GZIP 'tar.gz archive stores 4GB file size' '
	git archive --format=tar.gz HEAD >large.tar.gz &&
	tar_gz_info_size large.tar.gz >actual &&
	echo $size_4gb >expect &&
	test_cmp expect actual
'

test_done
