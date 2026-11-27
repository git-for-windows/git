#!/bin/sh

# wrap-for-bin.sh: Template for git executable wrapper scripts
# to run test suite against sandbox, but with only bindir-installed
# executables in PATH.  The Makefile copies this into various
# files in bin-wrappers, substituting
# @BUILD_DIR@, @TEMPLATE_DIR@ and @PROG@.

GIT_EXEC_PATH='@BUILD_DIR@'
if test -n "$NO_SET_GIT_TEMPLATE_DIR"
then
	unset GIT_TEMPLATE_DIR
else
	GIT_TEMPLATE_DIR='@TEMPLATE_DIR@'
	export GIT_TEMPLATE_DIR
fi
MERGE_TOOLS_DIR='@MERGE_TOOLS_DIR@'
GITPERLLIB='@GITPERLLIB@'"${GITPERLLIB:+:$GITPERLLIB}"
GIT_TEXTDOMAINDIR='@GIT_TEXTDOMAINDIR@'
PATH='@BUILD_DIR@/bin-wrappers:'"$PATH"

export MERGE_TOOLS_DIR GIT_EXEC_PATH GITPERLLIB PATH GIT_TEXTDOMAINDIR

if test "$GIT_TEST_WSL" = 1
then
	export Path="$PATH"
	if test -n "$TEST_OUTPUT_DIRECTORY"
	then
		safe=$(wslpath -m "$TEST_OUTPUT_DIRECTORY") || exit
		safe=$(printf '%s' "$safe" | sed "s/'/'\\\\''/g") || exit
		params="${GIT_CONFIG_PARAMETERS:+$GIT_CONFIG_PARAMETERS }"
		export GIT_CONFIG_PARAMETERS="$params'safe.directory'='$safe/*'"
	fi
	WSLENV="$WSLENV:HOME/p:GITPERLLIB/pl:LANG:LC_ALL:TZ:TERM"
	WSLENV="$WSLENV:EDITOR:PAGER:COLUMNS:$(env -0 | sed -zE '
		/^(TEST|GIT)_/!d
		s/=.*//
		s/^GIT_(EXEC_PATH|TEMPLATE_DIR|TEXTDOMAINDIR)$/&\/p/
		s/^GIT_CEILING_DIRECTORIES$/&\/pl/
	' | tr '\0' :)"
	export WSLENV
fi

case "$GIT_DEBUGGER" in
'')
	exec "@PROG@" "$@"
	;;
1)
	unset GIT_DEBUGGER
	exec gdb --args "@PROG@" "$@"
	;;
*)
	GIT_DEBUGGER_ARGS="$GIT_DEBUGGER"
	unset GIT_DEBUGGER
	exec ${GIT_DEBUGGER_ARGS} "@PROG@" "$@"
	;;
esac
