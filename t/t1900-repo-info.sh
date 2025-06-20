#!/bin/sh

test_description='test git repo-info'
GIT_TEST_DEFAULT_INITIAL_BRANCH_NAME=main
export GIT_TEST_DEFAULT_INITIAL_BRANCH_NAME

. ./test-lib.sh

DEFAULT_NUMBER_OF_FIELDS=3

parse_json () {
	tr '\n' ' ' | "$PERL_PATH" "$TEST_DIRECTORY/t0019/parse_json.perl"
}

test_lazy_prereq PERLJSON '
	perl -MJSON -e "exit 0"
'

# Test if a field is correctly returned in both plaintext and json formats.
#
# Usage: test_repo_info <label> <init command> <key> <expected value>
#
# Arguments:
#   label: the label of the test
#   init command: a command that creates a repository called 'repo', configured
#      accordingly to what is being tested
#   key: the key of the field that is being tested
#   expected value: the value that the field should contain
test_repo_info () {
	label=$1
	init_command=$2
	key=$3
	expected_value=$4

	test_expect_success PERLJSON "json: $label" "
		test_when_finished 'rm -rf repo' &&
		'$SHELL_PATH' -c '$init_command' &&
		cd repo &&
		echo '$expected_value' >expect &&
		git repo-info '$key' >output &&
		cat output | parse_json >parsed &&
		grep -F 'row[0].$key' parsed | cut -d ' ' -f 2 >value &&
		cat value | sed 's/^0$/false/' | sed 's/^1$/true/' >actual &&
		test_cmp expect actual
	"

	test_expect_success "plaintext: $label" "
		test_when_finished 'rm -rf repo' &&
		'$SHELL_PATH' -c '$init_command' &&
		cd repo &&
		echo '$expected_value' >expect &&
		git repo-info --format=plaintext '$key' >output &&
		cat output | cut -d '=' -f 2 >actual &&
		test_cmp expect actual
	"
}

test_expect_success PERLJSON 'json: returns empty output with allow-empty' '
	git repo-info --allow-empty --format=json >output &&
	test_line_count = 2 output
'

test_expect_success 'plaintext: returns empty output with allow-empty' '
	git repo-info --allow-empty --format=plaintext >output &&
	test_line_count = 0 output
'

test_repo_info 'ref format files is retrieved correctly' '
	git init --ref-format=files repo' 'references.format' 'files'

test_repo_info 'ref format reftable is retrieved correctly' '
	git init --ref-format=reftable repo' 'references.format' 'reftable'

test_repo_info 'bare repository = false is retrieved correctly' '
	git init repo' 'layout.bare' 'false'

test_repo_info 'bare repository = true is retrieved correctly' '
	git init --bare repo' 'layout.bare' 'true'

test_repo_info 'shallow repository = false is retrieved correctly' '
	git init repo' 'layout.shallow' 'false'

test_repo_info 'shallow repository = true is retrieved correctly' '
	git init remote &&
	cd remote &&
	echo x >x &&
	git add x &&
	git commit -m x &&
	cd .. &&
	git clone --depth 1 "file://$PWD/remote" repo &&
	rm -rf remote
	' 'layout.shallow' 'true'

test_expect_success 'plaintext: output all default fields' "
	git repo-info --format=plaintext >actual &&
	test_line_count = $DEFAULT_NUMBER_OF_FIELDS actual
"

test_expect_success PERLJSON 'json: output all default fields' "
	git repo-info --format=json > output &&
	cat output | parse_json | grep '.*\..*\..*' >actual &&
	test_line_count = $DEFAULT_NUMBER_OF_FIELDS actual
"

test_done
