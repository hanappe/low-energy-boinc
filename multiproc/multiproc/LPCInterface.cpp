#include <stdio.h>
#include "LPCInterface.h"

static void LPCSetErrorMessage(LPCInterface* lpci, char* s);
static int LPCAllocStats(LPCInterface* lpci);

LPCInterface* LPCInterfaceNew()
{
	LPCInterface* lpci = (LPCInterface*) malloc(sizeof(LPCInterface));
	if (lpci == NULL) 
		return NULL;

	lpci->error = 0;
	lpci->errorMessage[0] = 0;
	lpci->stats = NULL;
	lpci->statsSize = 0;

	lpci->file = CreateFile("\\\\.\\SFltrl0", MAXIMUM_ALLOWED, 0, 0, OPEN_EXISTING, 0, 0); 
	if (lpci->file == INVALID_HANDLE_VALUE) {
		free(lpci);
		return NULL;
	}

	return lpci;
}

void LPCInterfaceDelete(LPCInterface* lpci)
{
	if (lpci != NULL) {
		if (lpci->file != INVALID_HANDLE_VALUE)
			CloseHandle(lpci->file);
		if (lpci->stats != NULL)
			free(lpci->stats);
		free(lpci);
	}
}

static void LPCSetErrorMessage(LPCInterface* lpci, char* s)
{
	lpci->error = 1;
	_snprintf_s(lpci->errorMessage, 256, 256, "%s", s);
	lpci->errorMessage[255] = 0;
}

char* LPCGetErrorMessage(LPCInterface* lpci)
{
	return lpci->errorMessage;
}

int LPCGetConfig(LPCInterface* lpci, LPCConfiguration* cfg)
{
	DWORD bytesback;
	BOOL success;

	lpci->error = 0;

	success = DeviceIoControl(lpci->file, IOCTL_LPC_FLTR_GET_CFG, 0, 0, cfg, sizeof(LPCConfiguration), &bytesback, NULL);	
	if (!success) {
		lpci->error = GetLastError();
		LPCSetErrorMessage(lpci, "LPCGetConfig: DeviceIoControl failed");
		return -1;
	}

	return 0;
}

int LPCSetPstate(LPCInterface* lpci, unsigned int cpu, unsigned int pstate)
{
	DWORD data[2];
	DWORD bytesback;
	BOOL success;

	data[0] = cpu;
	data[1] = pstate;

	success = DeviceIoControl(lpci->file, IOCTL_LPC_FLTR_SET_CPU_PSTATE, &data, 8, 0, 0, &bytesback, NULL);	
	if (!success) {
		lpci->error = GetLastError();
		LPCSetErrorMessage(lpci, "LPCSetPstate: DeviceIoControl failed");
		return -1;
	}

	return 0;
}	

static int LPCAllocStats(LPCInterface* lpci)
{
	DWORD bytesback;
	DWORD bufsize;
	BOOL success;
	DWORD error;

	success = DeviceIoControl(lpci->file, IOCTL_LPC_FLTR_GET_STATS, 0, 0, &bufsize, 4, &bytesback, NULL);
	error = GetLastError();

	if (!success && (error == ERROR_MORE_DATA)) {
			// alloc data as needed, bufsize contains the size 
			lpci->statsSize = bufsize;
			lpci->stats = (LPCPstateStats*) malloc(bufsize);
			if (lpci->stats == NULL) {
				LPCSetErrorMessage(lpci, "LPCAllocStats: Out of memory");
				return -1;
			}

	} else {
		lpci->error = GetLastError();
		LPCSetErrorMessage(lpci, "LPCAllocStats: DeviceIoControl failed");
		return -1;
	}

	return 0;
}

LPCPstateStats* LPCGetStats(LPCInterface* lpci)
{
	DWORD bytesback;
	BOOL success;

	lpci->error = 0;

	if (lpci->stats == NULL) {
		int ret = LPCAllocStats(lpci);
		if (ret != 0) return NULL;
	}

	success = DeviceIoControl(lpci->file, IOCTL_LPC_FLTR_GET_STATS, 0, 0, lpci->stats, lpci->statsSize, &bytesback, NULL);
	if (!success) {
		lpci->error = GetLastError();
		LPCSetErrorMessage(lpci, "LPCGetStats: DeviceIoControl failed");
		return NULL;
	}

	return lpci->stats;
}

