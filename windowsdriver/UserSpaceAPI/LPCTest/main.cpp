#include <stdio.h>
#include "LPCInterface.h"

int main(int argc, char** argv)
{
	printf("LPCInterfaceNew: ");

	LPCInterface* lpci = LPCInterfaceNew();
	if (lpci == NULL) {
		printf("failed\n");
		return 1;
	}

	printf("done\n");


	printf("LPCGetConfig: ");

	LPCConfiguration cfg;
	int ret = LPCGetConfig(lpci, &cfg);
	if (ret != 0) {
		printf("%s\n", LPCGetErrorMessage(lpci));
		goto error_recovery;
	}

	printf("done\n");


	printf("ThreadDelay:           %d\n", cfg.ThreadDelay);
	printf("StatsSampleRate:       %d\n", cfg.StatsSampleRate);
	printf("StayAliveFailureLimit: %d\n", cfg.StayAliveFailureLimit);


	printf("LPCGetStats: ");

	LPCPstateStats* stats = LPCGetStats(lpci);
	if (stats == NULL) {
		printf("%s\n", LPCGetErrorMessage(lpci));
		goto error_recovery;
	}

	printf("done\n");

	printf("Timestamp %llu\n", stats->TimeStamp);
	for (unsigned int i = 0; i < stats->NumCPUs; i++) {
		printf("CPU %d:\n", i);
		for (unsigned int j = 0; j < stats->NumFrequencies; j++) {
			LPCCPUFrequency* freq = LPCPStateStatsGetFreq(stats, i, j);
			printf("  Frequency %d, Hits %llu\n", freq->CPUFrequency, freq->Time);
		}
	}


	printf("LPCSetPstate: ");

	for (unsigned int i = 0; i < stats->NumCPUs; i++) {
		int ret = LPCSetPstate(lpci, i, 0);
		if (ret != 0) {
			printf("%s\n", LPCGetErrorMessage(lpci));
			goto error_recovery;
		}
	}

	printf("done\n");

error_recovery:

	LPCInterfaceDelete(lpci);

	return 0;
}