#!/bin/sh

test_description='verify that push respects `pack.usePathWalk`'

TEST_PASSES_SANITIZE_LEAK=true
. ./test-lib.sh
. "$TEST_DIRECTORY"/lib-pack.sh

test_expect_success 'setup bare repository and clone' '
	git init --bare -b main bare.git &&
	git --git-dir=bare.git config receive.unpackLimit 0 &&
	git --git-dir bare.git commit-tree -m initial $EMPTY_TREE >head_oid &&
	git --git-dir bare.git update-ref refs/heads/main $(cat head_oid) &&
	git clone --bare bare.git clone.git
'
test_expect_success 'avoid reusing deltified objects' '
	# construct two commits, one containing a file with the hex digits
	# repeated 16 times, the next reducing that to 8 times. The crucial
	# part is that the blob of the second commit is deltified _really_
	# badly and it is therefore easy to detect if a `git push` reused that
	# delta.
	x="0123456789abcdef" &&
	printf "$x$x$x$x$x$x$x$x" >x128 &&
	printf "$x$x$x$x$x$x$x$x$x$x$x$x$x$x$x$x" >x256 &&

	pack=clone.git/objects/pack/pack-tmp.pack &&
	pack_header 2 >$pack &&

	# add x256 as a non-deltified object, using an uncompressed zlib stream
	# for simplicity
	# 060 = OBJ_BLOB << 4, 0200 = size larger than 15,
	# 0 = lower 4 bits of size, 020 = bits 5-9 of size (size = 256)
	printf "\260\020" >>$pack &&
	# Uncompressed zlib stream always starts with 0170 1 1, followed
	# by two bytes encoding the size, little endian, then two bytes with
	# the bitwise-complement of that size, then the payload, and then the
	# Adler32 checksum. For some reason, the checksum is in big-endian
	# format.
	printf "\170\001\001\0\001\377\376" >>$pack &&
	cat x256 >>$pack &&
	# Manually-computed Adler32 checksum: 0xd7ae4621
	printf "\327\256\106\041" >>$pack &&

	# add x128 as a very badly deltified object
	# 0120 = OBJ_OFS_DELTA << 4, 0200 = total size larger than 15,
	# 4 = lower 4 bits of size, 030 = bits 5-9 of size
	# (size = 128 * 3 + 2 + 2)
	printf "\344\030" >>$pack &&
	# 0415 = size (i.e. the relative negative offset) of the previous
	# object (x256, used as base object)
	# encoded as 0200 | ((0415 >> 7) - 1), 0415 & 0177
	printf "\201\015" >>$pack &&
	# Uncompressed zlib stream, as before, size = 2 + 2 + 128 * 3 (i.e.
	# 0604)
	printf "\170\001\001\204\001\173\376" >>$pack &&
	# base object size = 0400 (encoded as 0200 | (0400 & 0177),
	# 0400 >> 7)
	printf "\200\002" >>$pack &&
	# object size = 0200 (encoded as 0200 | (0200 & 0177), 0200 >> 7
	printf "\200\001" >>$pack &&
	# massively badly-deltified object: copy every single byte individually
	# 0200 = copy, 1 = use 1 byte to encode the offset (counter),
	# 020 = use 1 byte to encode the size (1)
	printf "$(printf "\\\\221\\\\%03o\\\\001" $(test_seq 0 127))" >>$pack &&
	# Manually-computed Adler32 checksum: 0x99c369c4
	printf "\231\303\151\304" >>$pack &&

	pack_trailer $pack &&
	git index-pack -v $pack &&

	oid256=$(git hash-object x256) &&
	printf "100755 blob $oid256\thex\n" >tree &&
	tree_oid="$(git --git-dir=clone.git mktree <tree)" &&
	commit_oid=$(git --git-dir=clone.git commit-tree \
		-p $(git --git-dir=clone.git rev-parse main) \
		-m 256 $tree_oid) &&

	oid128=$(git hash-object x128) &&
	printf "100755 blob $oid128\thex\n" >tree &&
	tree_oid="$(git --git-dir=clone.git mktree <tree)" &&
	commit_oid=$(git --git-dir=clone.git commit-tree \
		-p $commit_oid \
		-m 128 $tree_oid) &&

	# Verify that the on-disk size of the delta object is suboptimal in the
	# clone (see below why 18 bytes or smaller is the optimal size):
	git index-pack --verify-stat clone.git/objects/pack/pack-*.pack >verify &&
	size="$(sed -n "s/^$oid128 blob *\([^ ]*\).*/\1/p" <verify)" &&
	test $size -gt 18 &&

	git --git-dir=clone.git update-ref refs/heads/main $commit_oid &&
	git --git-dir=clone.git -c pack.usePathWalk=true push origin main &&
	git index-pack --verify-stat bare.git/objects/pack/pack-*.pack >verify &&
	size="$(sed -n "s/^$oid128 blob *\([^ ]*\).*/\1/p" <verify)" &&
	# The on-disk size of the delta object should be smaller than, or equal
	# to, 18 bytes, as that would be the size if storing the payload
	# uncompressed:
	#   3 bytes: 0170 01 01
	# + 2 bytes: zlib stream size
	# + 2 bytes: but-wise complement of the zlib stream size
	# + 7 bytes: payload
	#   (= 2 bytes for the size of tbe base object
	#    + 2 bytes for the size of the delta command
	#    + 3 bytes for the copy command)
	# + 2 + 2 bytes: Adler32 checksum
	test $size -le 18
'

test_done
