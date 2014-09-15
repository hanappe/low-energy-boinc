#ifndef _LPCINTERFACE_H
#define _LPCINTERFACE_H

#include <windows.h>
#include <WINIOCTL.h>

typedef struct _LPCPstateStats
{
	ULONG StayAliveFailureHits;
	ULONG64 TimeStamp;
	ULONG NumCPUs;
	ULONG NumFrequencies;
} LPCPstateStats;  

#define LPCPStateStatsGetFreq(__p,__cpu,__freq) \
	((LPCCPUFrequency*) (((char*)__p) \
                             + sizeof(LPCPstateStats) \
                             + sizeof(LPCCPUFrequency) \
                             * (__cpu * __p->NumFrequencies + __freq)))

typedef struct _LPCConfiguration
{
        ULONG ThreadDelay;
        ULONG StatsSampleRate;
        ULONG StayAliveFailureLimit;
} LPCConfiguration; 

typedef struct _LPCInterface {
	unsigned int error;
	char errorMessage[256];
	HANDLE file;
	LPCPstateStats* stats;
	unsigned int statsSize;
} LPCInterface;


LPCInterface* LPCInterfaceNew();
void LPCInterfaceDelete(LPCInterface* lpci);

char* LPCGetErrorMessage(LPCInterface* lpci);

int LPCGetConfig(LPCInterface* lpci, LPCConfiguration* cfg);

int LPCSetPstate(LPCInterface* lpci, unsigned int cpu, unsigned int pstate);

LPCPstateStats* LPCGetStats(LPCInterface* lpci);

#endif // _LPCINTERFACE_H
