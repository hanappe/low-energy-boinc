#include <windows.h>
#include <winnt.h>

static double pcfreq = 0.0;
static __int64 counter_start = 0;

int counter_init()
{
    LARGE_INTEGER li;
    if(!QueryPerformanceFrequency(&li))
		return -1;

    pcfreq = (double) li.QuadPart;

    QueryPerformanceCounter(&li);
    counter_start = li.QuadPart;
	return 0;
}

double counter_get()
{
    LARGE_INTEGER li;
    QueryPerformanceCounter(&li);
    return (double)(li.QuadPart-counter_start) / pcfreq;

	//return (double) GetTickCount64() / 1000.0;
}
