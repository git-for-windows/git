#!/bin/sh

test_description='Trace2 process ancestry overhead'

. ./perf-lib.sh

test_perf 'git version without Trace2' '
	GIT_TRACE2=0 GIT_TRACE2_PERF=0 GIT_TRACE2_EVENT=0 \
		git version >/dev/null
'

test_perf 'git version with Trace2 ancestry' '
	GIT_TRACE2=0 GIT_TRACE2_PERF=0 GIT_TRACE2_EVENT=2 \
		git version >/dev/null 2>/dev/null
'

test_done
