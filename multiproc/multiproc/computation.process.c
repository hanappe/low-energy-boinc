#include "computation.h"
#include <string.h>

void linpack_main(int arsize, FILE* out)

enum {
	NO_PROCESS = 0,
	RUNNING,
	SUSPENDED
};

typedef struct _computation_t {
	int state;
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
} computation_t;

#define MAX_COMPUTATIONS 8

computation_t computations[MAX_COMPUTATIONS];

void computation_init()
{
    ZeroMemory(&computations[0], MAX_COMPUTATIONS * sizeof(computation_t));
}

void computation_cleanup()
{
		// TODO
}

int computation_start(int index)
{
	if ((index < 0) || (index >= MAX_COMPUTATIONS))
		return -1;
	if (computations[index].state != NO_PROCESS)
		return 0;

    ZeroMemory(&computations[index].si, sizeof(STARTUPINFO));
    computations[index].si.cb = sizeof(STARTUPINFO);
    ZeroMemory(&computations[index].pi, sizeof(PROCESS_INFORMATION));

    // Start the child process. 
    if (!CreateProcess(L"C:\\linpack\\linpack_11.2.0\\benchmarks\\linpack\\linpack_xeon64.exe",   // No module name (use command line)
        L"C:\\linpack\\linpack_11.2.0\\benchmarks\\linpack\\lininput_xeon64",        // Command line
        NULL,           // Process handle not inheritable
        NULL,           // Thread handle not inheritable
        FALSE,          // Set handle inheritance to FALSE
        0,              // No creation flags
        NULL,           // Use parent's environment block
        L"C:\\linpack\\linpack_11.2.0\\benchmarks\\linpack",           // Use parent's starting directory 
        &computations[index].si,            // Pointer to STARTUPINFO structure
        &computations[index].pi )           // Pointer to PROCESS_INFORMATION structure
    ) 
    {
        printf("CreateProcess failed (%d).\n", GetLastError());
        return -1;
    }

	computations[index].state = RUNNING;
	return 0;
}

int computation_is_finished(int index)
{
	DWORD exit_code = 0;
	if (computations[index].state == NO_PROCESS)
		return 1;
	BOOL ret = GetExitCodeProcess(computations[index].pi.hProcess, &exit_code);
	if (exit_code == STILL_ACTIVE)
		return 0;
	else
		return 1;	
}

static void computation_cleanup(int index)
{
	if (computations[index].state == NO_PROCESS)
		return;
    CloseHandle(computations[index].pi.hProcess);
    CloseHandle(computations[index].pi.hThread);
	computations[index].state = NO_PROCESS;
}

void computation_wait(int index)
{
	if (computations[index].state != RUNNING)
		return;
    WaitForSingleObject(computations[index].pi.hProcess, INFINITE);
	computation_cleanup(index);		
}

void computation_terminate(int index)
{
	if (computations[index].state != RUNNING)
		return;
    TerminateProcess(computations[index].pi.hProcess, 0);
	computation_cleanup(index);
}

void computation_suspend(int index)
{
	if (computations[index].state != RUNNING)
		return;
	SuspendThread(computations[index].pi.hThread);
	computations[index].state = SUSPENDED;
}

void computation_resume(int index)
{
	if (computations[index].state != SUSPENDED)
		return;
	ResumeThread(computations[index].pi.hThread);
	computations[index].state = RUNNING;
}

