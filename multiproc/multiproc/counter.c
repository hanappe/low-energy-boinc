#include <windows.h>
#include <winnt.h>

#include <stdio.h>

static double pcfreq = 0.0;
static __int64 counter_start = 0;

/*
static LARGE_INTEGER freq;
static LARGE_INTEGER t0;

int counter_init()
{
    LARGE_INTEGER li;
    if(!QueryPerformanceFrequency(&freq))
		return -1;

    //pcfreq = (double) li.QuadPart;

    QueryPerformanceCounter(&t0);
    //counter_start = li.QuadPart;
	return 0;
}

double counter_get()
{
	double elapsedTime;
    double resolution;

	LARGE_INTEGER tf, delta;
	QueryPerformanceCounter(&tf);
    delta.QuadPart = tf.QuadPart - t0.QuadPart;
    elapsedTime = delta.QuadPart / (double) freq.QuadPart;
    resolution = 1.0 / (double) freq.QuadPart;
    //printf("Your performance counter ticks %I64u times per second\n", freq.QuadPart);
    //printf("Resolution is %lf nanoseconds\n", resolution*1e9);
    printf("Code under test took %lf sec\n", elapsedTime);

	return elapsedTime;
}
*/

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
