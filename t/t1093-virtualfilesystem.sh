#!/bin/sh

test_description='virtual file system tests'

. ./test-lib.sh

clean_repo () {
	rm .git/index &&
	git -c core.virtualfilesystem= reset --hard HEAD &&
	git -c core.virtualfilesystem= clean -fd &&
	touch untracked.txt &&
	touch dir1/untracked.txt &&
	touch dir2/untracked.txt
}

test_expect_success 'setup' '
	git branch -M main &&
	mkdir -p .git/hooks/ &&
	cat > .gitignore <<-\EOF &&
		.gitignore
		expect*
		actual*
	EOF
	mkdir -p dir1 &&
	touch dir1/file1.txt &&
	touch dir1/file2.txt &&
	mkdir -p dir2 &&
	touch dir2/file1.txt &&
	touch dir2/file2.txt &&
	git add . &&
	git commit -m "initial" &&
	git config --local core.virtualfilesystem .git/hooks/virtualfilesystem
'

test_expect_success 'test hook parameters and version' '
	clean_repo &&
	write_script .git/hooks/virtualfilesystem <<-\EOF &&
		if test "$#" -ne 1
		then
			echo "$0: Exactly 1 argument expected" >&2
			exit 2
		fi

		if test "$1" != 1
		then
			echo "$0: Unsupported hook version." >&2
			exit 1
		fi
	EOF
	git status &&
	write_script .git/hooks/virtualfilesystem <<-\EOF &&
		exit 3
	EOF
	test_must_fail git status
'

test_expect_success 'verify status is clean' '
	clean_repo &&
	write_script .git/hooks/virtualfilesystem <<-\EOF &&
		printf "dir2/file1.txt\0"
	EOF
	rm -f .git/index &&
	git checkout -f &&
	write_script .git/hooks/virtualfilesystem <<-\EOF &&
		printf "dir2/file1.txt\0"
		printf "dir1/file1.txt\0"
		printf "dir1/file2.txt\0"
	EOF
	git status > actual &&
	cat > expected <<-\EOF &&
		On branch main
		nothing to commit, working tree clean
	EOF
	test_cmp expected actual
'

test_expect_success 'verify skip-worktree bit is set for absolute path' '
	clean_repo &&
	write_script .git/hooks/virtualfilesystem <<-\EOF &&
		printf "dir1/file1.txt\0"
	EOF
	git ls-files -v > actual &&
	cat > expected <<-\EOF &&
		H dir1/file1.txt
		S dir1/file2.txt
		S dir2/file1.txt
		S dir2/file2.txt
	EOF
	test_cmp expected actual
'

test_expect_success 'verify skip-worktree bit is cleared for absolute path' '
	clean_repo &&
	write_script .git/hooks/virtualfilesystem <<-\EOF &&
		printf "dir1/file2.txt\0"
	EOF
	git ls-files -v > actual &&
	cat > expected <<-\EOF &&
		S dir1/file1.txt
		H dir1/file2.txt
		S dir2/file1.txt
		S dir2/file2.txt
	EOF
	test_cmp expected actual
'

test_expect_success 'verify folder wild cards' '
	clean_repo &&
	write_script .git/hooks/virtualfilesystem <<-\EOF &&
		printf "dir1/\0"
	EOF
	git ls-files -v > actual &&
	cat > expected <<-\EOF &&
		H dir1/file1.txt
		H dir1/file2.txt
		S dir2/file1.txt
		S dir2/file2.txt
	EOF
	test_cmp expected actual
'

test_expect_success 'verify folders not included are ignored' '
	clean_repo &&
	write_script .git/hooks/virtualfilesystem <<-\EOF &&
		printf "dir1/file1.txt\0"
		printf "dir1/file2.txt\0"
	EOF
	mkdir -p dir1/dir2 &&
	touch dir1/a &&
	touch dir1/b &&
	touch dir1/dir2/a &&
	touch dir1/dir2/b &&
	git add . &&
	git ls-files -v > actual &&
	cat > expected <<-\EOF &&
		H dir1/file1.txt
		H dir1/file2.txt
		S dir2/file1.txt
		S dir2/file2.txt
	EOF
	test_cmp expected actual
'

test_expect_success 'verify including one file doesnt include the rest' '
	clean_repo &&
	write_script .git/hooks/virtualfilesystem <<-\EOF &&
		printf "dir1/file1.txt\0"
		printf "dir1/file2.txt\0"
		printf "dir1/dir2/a\0"
	EOF
	mkdir -p dir1/dir2 &&
	touch dir1/a &&
	touch dir1/b &&
	touch dir1/dir2/a &&
	touch dir1/dir2/b &&
	git add . &&
	git ls-files -v > actual &&
	cat > expected <<-\EOF &&
		H dir1/dir2/a
		H dir1/file1.txt
		H dir1/file2.txt
		S dir2/file1.txt
		S dir2/file2.txt
	EOF
	test_cmp expected actual
'

test_expect_success 'verify files not listed are ignored by git clean -f -x' '
	clean_repo &&
	write_script .git/hooks/virtualfilesystem <<-\EOF &&
		printf "untracked.txt\0"
		printf "dir1/\0"
	EOF
	mkdir -p dir3 &&
	touch dir3/untracked.txt &&
	git clean -f -x &&
	test ! -f untracked.txt &&
	test -d dir1 &&
	test -f dir1/file1.txt &&
	test -f dir1/file2.txt &&
	test ! -f dir1/untracked.txt &&
	test -f dir2/file1.txt &&
	test -f dir2/file2.txt &&
	test -f dir2/untracked.txt &&
	test -d dir3 &&
	test -f dir3/untracked.txt
'

test_expect_success 'verify files not listed are ignored by git clean -f -d -x' '
	clean_repo &&
	write_script .git/hooks/virtualfilesystem <<-\EOF &&
		printf "untracked.txt\0"
		printf "dir1/\0"
		printf "dir3/\0"
	EOF
	mkdir -p dir3 &&
	touch dir3/untracked.txt &&
	git clean -f -d -x &&
	test ! -f untracked.txt &&
	test -d dir1 &&
	test -f dir1/file1.txt &&
	test -f dir1/file2.txt &&
	test ! -f dir1/untracked.txt &&
	test -f dir2/file1.txt &&
	test -f dir2/file2.txt &&
	test -f dir2/untracked.txt &&
	test ! -d dir3 &&
	test ! -f dir3/untracked.txt
'

test_expect_success 'verify folder entries include all files' '
	clean_repo &&
	write_script .git/hooks/virtualfilesystem <<-\EOF &&
		printf "dir1/\0"
	EOF
	mkdir -p dir1/dir2 &&
	touch dir1/a &&
	touch dir1/b &&
	touch dir1/dir2/a &&
	touch dir1/dir2/b &&
	git status -su > actual &&
	cat > expected <<-\EOF &&
		?? dir1/a
		?? dir1/b
		?? dir1/dir2/a
		?? dir1/dir2/b
		?? dir1/untracked.txt
	EOF
	test_cmp expected actual
'

test_expect_success 'verify case insensitivity of virtual file system entries' '
	clean_repo &&
	write_script .git/hooks/virtualfilesystem <<-\EOF &&
		printf "dir1/a\0"
		printf "Dir1/Dir2/a\0"
		printf "DIR2/\0"
	EOF
	mkdir -p dir1/dir2 &&
	touch dir1/a &&
	touch dir1/b &&
	touch dir1/dir2/a &&
	touch dir1/dir2/b &&
	git -c core.ignorecase=false status -su > actual &&
	cat > expected <<-\EOF &&
		?? dir1/a
	EOF
	test_cmp expected actual &&
	git -c core.ignorecase=true status -su > actual &&
	cat > expected <<-\EOF &&
		?? dir1/a
		?? dir1/dir2/a
		?? dir2/untracked.txt
	EOF
	test_cmp expected actual
'

test_expect_success 'on file created' '
	clean_repo &&
	write_script .git/hooks/virtualfilesystem <<-\EOF &&
		printf "dir1/file3.txt\0"
	EOF
	touch dir1/file3.txt &&
	git add . &&
	git ls-files -v > actual &&
	cat > expected <<-\EOF &&
		S dir1/file1.txt
		S dir1/file2.txt
		H dir1/file3.txt
		S dir2/file1.txt
		S dir2/file2.txt
	EOF
	test_cmp expected actual
'

test_expect_success 'on file renamed' '
	clean_repo &&
	write_script .git/hooks/virtualfilesystem <<-\EOF &&
		printf "dir1/file1.txt\0"
		printf "dir1/file3.txt\0"
	EOF
	mv dir1/file1.txt dir1/file3.txt &&
	git status -su > actual &&
	cat > expected <<-\EOF &&
		 D dir1/file1.txt
		?? dir1/file3.txt
	EOF
	test_cmp expected actual
'

test_expect_success 'on file deleted' '
	clean_repo &&
	write_script .git/hooks/virtualfilesystem <<-\EOF &&
		printf "dir1/file1.txt\0"
	EOF
	rm dir1/file1.txt &&
	git status -su > actual &&
	cat > expected <<-\EOF &&
		 D dir1/file1.txt
	EOF
	test_cmp expected actual
'

test_expect_success 'on file overwritten' '
	clean_repo &&
	write_script .git/hooks/virtualfilesystem <<-\EOF &&
		printf "dir1/file1.txt\0"
	EOF
	echo "overwritten" > dir1/file1.txt &&
	git status -su > actual &&
	cat > expected <<-\EOF &&
		 M dir1/file1.txt
	EOF
	test_cmp expected actual
'

test_expect_success 'on folder created' '
	clean_repo &&
	write_script .git/hooks/virtualfilesystem <<-\EOF &&
		printf "dir1/dir1/\0"
	EOF
	mkdir -p dir1/dir1 &&
	git status -su > actual &&
	cat > expected <<-\EOF &&
	EOF
	test_cmp expected actual &&
	git clean -fd &&
	test ! -d "/dir1/dir1"
'

test_expect_success 'on folder renamed' '
	clean_repo &&
	write_script .git/hooks/virtualfilesystem <<-\EOF &&
		printf "dir3/\0"
		printf "dir1/file1.txt\0"
		printf "dir1/file2.txt\0"
		printf "dir3/file1.txt\0"
		printf "dir3/file2.txt\0"
	EOF
	mv dir1 dir3 &&
	git status -su > actual &&
	cat > expected <<-\EOF &&
		 D dir1/file1.txt
		 D dir1/file2.txt
		?? dir3/file1.txt
		?? dir3/file2.txt
		?? dir3/untracked.txt
	EOF
	test_cmp expected actual
'

test_expect_success 'folder with same prefix as file' '
	clean_repo &&
	touch dir1.sln &&
	write_script .git/hooks/virtualfilesystem <<-\EOF &&
		printf "dir1/\0"
		printf "dir1.sln\0"
	EOF
	git add dir1.sln &&
	git ls-files -v > actual &&
	cat > expected <<-\EOF &&
		H dir1.sln
		H dir1/file1.txt
		H dir1/file2.txt
		S dir2/file1.txt
		S dir2/file2.txt
	EOF
	test_cmp expected actual
'

test_done
