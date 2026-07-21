#!/bin/sh

test_description='checkout a symlink nested through another symlink on Windows

A phantom symlink may target a path that goes through another
symlink. Ensures that such a symlink is still upgraded to a directory
symlink once its real target appears, even when that target directory
is only populated after the leading symlink has already resolved.'

# Tell MSYS to create native symlinks. Without this flag test-lib's
# prerequisite detection for SYMLINKS doesn't detect the right thing.
MSYS=winsymlinks:nativestrict && export MSYS

. ./test-lib.sh

if ! test_have_prereq MINGW,SYMLINKS
then
	skip_all='skipping $0: MinGW-only test, which requires symlink support.'
	test_done
fi

# Adds a symlink to the index without clobbering the work tree.
cache_symlink () {
	sha=$(printf '%s' "$1" | git hash-object --stdin -w) &&
	git update-index --add --cacheinfo 120000,$sha,"$2"
}

# Adds a regular file to the index without clobbering the work tree.
cache_file () {
	sha=$(printf '%s' "$1" | git hash-object --stdin -w) &&
	git update-index --add --cacheinfo 100644,$sha,"$2"
}

test_expect_success 'symlink nested through another symlink resolves' '
	test_when_finished "rm -rf chained" &&
	mkdir chained &&
	(
		cd chained &&
		git init -q &&

		cache_symlink realdir leading &&
		cache_symlink leading/sub nested &&
		cache_file content realdir/sub/file &&

		git checkout -- . &&

		# "cmd.exe /c dir" marks directory symlinks as <SYMLINKD>
		# and file symlinks as <SYMLINK>; unlike opening a path
		# through the symlink, this distinguishes the two even
		# though both resolve identically for plain reads.
		cmd.exe //c dir . >dir-listing &&
		grep "SYMLINKD.*nested" dir-listing
	)
'

test_done
