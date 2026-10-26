#!/bin/sh

test_description='checkout long paths on Windows

Ensures that Git for Windows can deal with long paths (>260) enabled via core.longpaths'

. ./test-lib.sh

if test_have_prereq !MINGW
then
	skip_all='skipping MINGW specific long paths test'
	test_done
fi

test_expect_success setup '
	p=longpathxx && # -> 10
	p=$p$p$p$p$p && # -> 50
	p=$p$p$p$p$p && # -> 250

	path=${p}/longtestfile && # -> 263 (MAX_PATH = 260)

	blob=$(echo foobar | git hash-object -w --stdin) &&

	printf "100644 %s 0\t%s\n" "$blob" "$path" |
	git update-index --add --index-info &&
	git commit -m initial -q
'

test_expect_success 'checkout of long paths without core.longpaths fails' '
	git config core.longpaths false &&
	test_must_fail git checkout -f 2>error &&
	test_grep -q "Filename too long" error &&
	test ! -d longpa*
'

test_expect_success 'checkout of long paths with core.longpaths works' '
	git config core.longpaths true &&
	git checkout -f &&
	test_path_is_file longpa*/longtestfile
'

test_expect_success 'update of long paths' '
	echo frotz >>$(ls longpa*/longtestfile) &&
	echo $path > expect &&
	git ls-files -m > actual &&
	test_cmp expect actual &&
	git add $path &&
	git commit -m second &&
	git grep "frotz" HEAD -- $path
'

test_expect_success cleanup '
	# bash cannot delete the trash dir if it contains a long path
	# lets help cleaning up (unless in debug mode)
	if test -z "$debug"
	then
		rm -rf longpa~1
	fi
'

# check that the template used in the test won't be too long:
abspath="$(pwd)"/testdir
test ${#abspath} -gt 230 ||
test_set_prereq SHORTABSPATH

test_expect_success SHORTABSPATH 'clean up path close to MAX_PATH' '
	p=/123456789abcdef/123456789abcdef/123456789abcdef/123456789abc/ef &&
	p=y$p$p$p$p &&
	subdir="x$(echo "$p" | tail -c $((253 - ${#abspath})) - )" &&
	# Now, $abspath/$subdir has exactly 254 characters, and is inside CWD
	p2="$abspath/$subdir" &&
	test 254 = ${#p2} &&

	# Be careful to overcome path limitations of the MSys tools and split
	# the $subdir into two parts. ($subdir2 has to contain 16 chars and a
	# slash somewhere following; that is why we asked for abspath <= 230 and
	# why we placed a slash near the end of the $subdir template.)
	subdir2=${subdir#????????????????*/} &&
	subdir1=testdir/${subdir%/$subdir2} &&
	mkdir -p "$subdir1" &&
	i=0 &&
	# The most important case is when absolute path is 258 characters long,
	# and that will be when i == 4.
	while test $i -le 7
	do
		mkdir -p $subdir2 &&
		touch $subdir2/one-file &&
		mv ${subdir2%%/*} "$subdir1/" &&
		subdir2=z${subdir2} &&
		i=$(($i+1)) ||
		exit 1
	done &&

	# now check that git is able to clear the tree:
	(cd testdir &&
	 git init &&
	 git config core.longpaths yes &&
	 git clean -fdx) &&
	test ! -d "$subdir1"
'

test_expect_success SYMLINKS_WINDOWS 'leave drive-less, short paths intact' '
	printf "/Program Files" >symlink-target &&
	symlink_target_oid="$(git hash-object -w --stdin <symlink-target)" &&
	git update-index --add --cacheinfo 120000,$symlink_target_oid,PF &&
	git -c core.symlinks=true checkout -- PF &&
	cmd //c dir >actual &&
	test_grep "<SYMLINKD\\?> *PF *\\[\\\\Program Files\\]" actual
'

test_expect_success SYMLINKS_WINDOWS 'set up long symlink targets' '
	ascii_target=$(printf "%0128d" 0) &&
	ascii_target=$ascii_target/$ascii_target &&
	utf8_char=$(printf "\344\270\255") &&
	utf8_target=$(test_seq 1 85 | sed "s/.*/$utf8_char/" | tr -d "\n") &&
	utf8_target=$utf8_target/a
'

for encoding in ascii utf8
do
	for suffix in aa aaa aaaa
	do
		# Both prefixes are 257 UTF-8 bytes: test 259, 260 and 261.
		bytes=$((257 + ${#suffix}))
		test_expect_success SYMLINKS_WINDOWS \
			"read $encoding symlink target of $bytes bytes" '
			test_config core.symlinks true &&
			test_config core.longpaths false &&
			test_config core.fscache false &&
			test_when_finished "rm -f long-link" &&
			test_when_finished \
				"git update-index --force-remove long-link" &&
			case $encoding in
			ascii) target=$ascii_target$suffix ;;
			utf8) target=$utf8_target$suffix ;;
			esac &&
			oid=$(printf "%s" "$target" |
				git hash-object -w --stdin) &&
			git update-index --add \
				--cacheinfo 120000,$oid,long-link &&
			win_target=$(printf "%s" "$target" | tr / "\\\\") &&
			cmd //c "mklink long-link $win_target" &&
			printf "%s\n" "$target" >expect &&
			test-tool path-utils readlink long-link >actual &&
			test_cmp expect actual &&
			git diff --exit-code -- long-link >actual 2>err &&
			test_must_be_empty actual &&
			test_must_be_empty err
		'
	done
done

test_done
