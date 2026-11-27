#ifndef TRACE2_WIN32_PROCESS_INFO_H
#define TRACE2_WIN32_PROCESS_INFO_H

#include "../../hashmap.h"

struct strvec;

struct trace2_win32_process_entry {
	struct hashmap_entry ent;
	DWORD parent_pid;
	char name[FLEX_ARRAY];
};

void trace2_win32_get_ancestry(struct strvec *names,
			     const struct hashmap *processes, DWORD pid);

#endif
