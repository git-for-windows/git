#ifndef EXIT_PROCESS_H
#define EXIT_PROCESS_H

/*
 * This file contains functions to terminate a Win32 process, as gently as
 * possible.
 *
 * At first, we will attempt to inject a thread that calls ExitProcess(). If
 * that fails, we will fall back to terminating the entire process tree.
 *
 * For simplicity, these functions are marked as file-local.
 */

#include <tlhelp32.h>

/*
 * Terminates the process corresponding to the process ID and all of its
 * directly and indirectly spawned subprocesses.
 *
 * This way of terminating the processes is not gentle: the processes get
 * no chance of cleaning up after themselves (closing file handles, removing
 * .lock files, terminating spawned processes (if any), etc).
 */
struct process_info {
	DWORD pid;
	FILETIME creation_time;
	HANDLE handle;
};

static int add_process(struct process_info *processes, int len, int parent,
		       const PROCESSENTRY32 *entry)
{
	HANDLE process;
	FILETIME creation_time;

	if (processes[parent].pid != entry->th32ParentProcessID)
		return 0;

	process = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_TERMINATE,
			      FALSE, entry->th32ProcessID);
	if (!process)
		return 0;

	if (!mingw_process_is_child_of(process, processes[parent].pid,
					       &processes[parent].creation_time,
					       &creation_time)) {
		CloseHandle(process);
		return 0;
	}

	processes[len].pid = entry->th32ProcessID;
	processes[len].creation_time = creation_time;
	processes[len].handle = process;
	return 1;
}

static int terminate_process_tree(HANDLE main_process, int exit_status)
{
	HANDLE snapshot;
	PROCESSENTRY32 entry;
	int max_len = 16384, i, len, ret = 0;
	struct process_info *processes = xcalloc(max_len, sizeof(*processes));

	/*
	 * A process's PID can be reused after it exits. Do not mistake an older
	 * orphan whose former parent happens to have the same PID for a child.
	 *
	 * Keep a handle for every accepted child. This both gives us the handle for
	 * termination and keeps the process object alive so its PID cannot be
	 * reused while walking the rest of the tree.
	 */
	if (!mingw_process_started_after(main_process, NULL,
					 &processes[0].creation_time))
		goto terminate_main_process;
	processes[0].pid = GetProcessId(main_process);
	processes[0].handle = main_process;
	len = 1;
	snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

	/*
	 * Even if Process32First()/Process32Next() seem to traverse the
	 * processes in topological order (i.e. parent processes before
	 * child processes), there is nothing in the Win32 API documentation
	 * suggesting that this is guaranteed.
	 *
	 * Therefore, run through them at least twice and stop when no more
	 * process IDs were added to the list.
	 */
	if (snapshot != INVALID_HANDLE_VALUE) {
		for (;;) {
			int orig_len = len;

			memset(&entry, 0, sizeof(entry));
			entry.dwSize = sizeof(entry);

			if (!Process32First(snapshot, &entry))
				break;

			do {
				for (i = len - 1; i >= 0; i--) {
					if (processes[i].pid == entry.th32ProcessID)
						break;
					if (processes[i].pid != entry.th32ParentProcessID)
						continue;
					if (add_process(processes, len, i, &entry))
						len++;
					break;
				}
			} while (len < max_len && Process32Next(snapshot, &entry));

			if (orig_len == len || len >= max_len)
				break;
		}
		CloseHandle(snapshot);
	}

	for (i = len - 1; i >= 0; i--) {
		if (!TerminateProcess(processes[i].handle, exit_status))
			ret = -1;
		CloseHandle(processes[i].handle);
	}
	free(processes);

	return ret;

terminate_main_process:
	if (!TerminateProcess(main_process, exit_status))
		ret = -1;
	CloseHandle(main_process);
	free(processes);
	return ret;
}

/**
 * Determine whether a process runs in the same architecture as the current
 * one. That test is required before we assume that GetProcAddress() returns
 * a valid address *for the target process*.
 */
static inline int process_architecture_matches_current(HANDLE process)
{
	static BOOL current_is_wow = -1;
	BOOL is_wow;

	if (current_is_wow == -1 &&
	    !IsWow64Process (GetCurrentProcess(), &current_is_wow))
		current_is_wow = -2;
	if (current_is_wow == -2)
		return 0; /* could not determine current process' WoW-ness */
	if (!IsWow64Process (process, &is_wow))
		return 0; /* cannot determine */
	return is_wow == current_is_wow;
}

/**
 * Inject a thread into the given process that runs ExitProcess().
 *
 * Note: as kernel32.dll is loaded before any process, the other process and
 * this process will have ExitProcess() at the same address.
 *
 * This function expects the process handle to have the access rights for
 * CreateRemoteThread(): PROCESS_CREATE_THREAD, PROCESS_QUERY_INFORMATION,
 * PROCESS_VM_OPERATION, PROCESS_VM_WRITE, and PROCESS_VM_READ.
 *
 * The idea comes from the Dr Dobb's article "A Safer Alternative to
 * TerminateProcess()" by Andrew Tucker (July 1, 1999),
 * http://www.drdobbs.com/a-safer-alternative-to-terminateprocess/184416547
 *
 * If this method fails, we fall back to running terminate_process_tree().
 */
static int exit_process(HANDLE process, int exit_code)
{
	DWORD code;

	if (GetExitCodeProcess(process, &code) && code == STILL_ACTIVE) {
		static int initialized;
		static LPTHREAD_START_ROUTINE exit_process_address;
		PVOID arg = (PVOID)(intptr_t)exit_code;
		DWORD thread_id;
		HANDLE thread = NULL;

		if (!initialized) {
			HINSTANCE kernel32 = GetModuleHandleA("kernel32");
			if (!kernel32)
				die("BUG: cannot find kernel32");
			exit_process_address =
				(LPTHREAD_START_ROUTINE)(void (*)(void))
				GetProcAddress(kernel32, "ExitProcess");
			initialized = 1;
		}
		if (!exit_process_address ||
		    !process_architecture_matches_current(process))
			return terminate_process_tree(process, exit_code);

		thread = CreateRemoteThread(process, NULL, 0,
					    exit_process_address,
					    arg, 0, &thread_id);
		if (thread) {
			CloseHandle(thread);
			/*
			 * If the process survives for 10 seconds (a completely
			 * arbitrary value picked from thin air), fall back to
			 * killing the process tree via TerminateProcess().
			 */
			if (WaitForSingleObject(process, 10000) ==
			    WAIT_OBJECT_0) {
				CloseHandle(process);
				return 0;
			}
		}

		return terminate_process_tree(process, exit_code);
	}

	CloseHandle(process);
	return 0;
}

#endif
