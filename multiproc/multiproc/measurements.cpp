/*
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
*/

#include "measurements.h"

#include <windows.h>
#include <tchar.h>
#include <stdio.h>
#include <comdef.h>
#include <WbemIdl.h>
#include <intrin.h>

#include "counter.h"
#include "log.h"
#include "winsocket.h"

#include "cpu_info.h"
#include "battery.h"
#include "wmi.h"
// Kev
#ifdef _MSC_VER
#define snprintf _snprintf
#endif

// Cpu frequency 3
#pragma warning ( push )
#pragma warning ( disable : 4035 )

#define	CCPU_I32TO64(high, low)	(((__int64)high<<32)+low)
typedef	void	(*CCPU_FUNC)(unsigned int param);

static HANDLE measurements_thread = NULL;
static int measurements_continue = 1;


// Kev : Temp code 
namespace {
    static ULARGE_INTEGER lastCPU, lastSysCPU, lastUserCPU;
    static int numProcessors;
    static HANDLE self;
    

    void CpuLoadinit() {
        SYSTEM_INFO sysInfo;
        FILETIME ftime, fsys, fuser;
    

        GetSystemInfo(&sysInfo);
        numProcessors = sysInfo.dwNumberOfProcessors;
    

        GetSystemTimeAsFileTime(&ftime);
        memcpy(&lastCPU, &ftime, sizeof(FILETIME));
    

        self = GetCurrentProcess();
        GetProcessTimes(self, &ftime, &ftime, &fsys, &fuser);
        memcpy(&lastSysCPU, &fsys, sizeof(FILETIME));
        memcpy(&lastUserCPU, &fuser, sizeof(FILETIME));
    }
    

    double GetCurrentProcessCpuLoad() {
        FILETIME ftime, fsys, fuser;
        ULARGE_INTEGER now, sys, user;
        double percent;
    

        GetSystemTimeAsFileTime(&ftime);
        memcpy(&now, &ftime, sizeof(FILETIME));
    

        GetProcessTimes(self, &ftime, &ftime, &fsys, &fuser);
        memcpy(&sys, &fsys, sizeof(FILETIME));
        memcpy(&user, &fuser, sizeof(FILETIME));
        percent = (sys.QuadPart - lastSysCPU.QuadPart) +
            (user.QuadPart - lastUserCPU.QuadPart);
        percent /= (now.QuadPart - lastCPU.QuadPart);
        percent /= numProcessors;
        lastCPU = now;
        lastUserCPU = user;
        lastSysCPU = sys;
  

        return percent * 100;
    }

	float GetCpuFrequency()
	{
		/*
		RdTSC:
		It's the Pentium instruction "ReaD Time Stamp Counter". It measures the
		number of clock cycles that have passed since the processor was reset, as a
		64-bit number. That's what the <CODE>_emit</CODE> lines do.*/
		#define RdTSC __asm _emit 0x0f __asm _emit 0x31

		// variables for the clock-cycles:
		__int64 cyclesStart = 0, cyclesStop = 0;
		// variables for the High-Res Preformance Counter:
		unsigned __int64 nCtr = 0, nFreq = 0, nCtrStop = 0;


			// retrieve performance-counter frequency per second:
			if(!QueryPerformanceFrequency((LARGE_INTEGER *) &nFreq)) return 0;

			// retrieve the current value of the performance counter:
			QueryPerformanceCounter((LARGE_INTEGER *) &nCtrStop);

			// add the frequency to the counter-value:
			nCtrStop += nFreq;

			_asm
				{// retrieve the clock-cycles for the start value:
					RdTSC
					mov DWORD PTR cyclesStart, eax
					mov DWORD PTR [cyclesStart + 4], edx
				}

				do{
				// retrieve the value of the performance counter
				// until 1 sec has gone by:
					 QueryPerformanceCounter((LARGE_INTEGER *) &nCtr);
				  }while (nCtr < nCtrStop);

			_asm
				{// retrieve again the clock-cycles after 1 sec. has gone by:
					RdTSC
					mov DWORD PTR cyclesStop, eax
					mov DWORD PTR [cyclesStop + 4], edx
				}

		// stop-start is speed in Hz divided by 1,000,000 is speed in MHz
		return ((float)cyclesStop - (float)cyclesStart) / 1000000;
	}

	DWORD GetCpuFrequency2() {
	char Buffer[_MAX_PATH];
	DWORD BufSize = _MAX_PATH;
	DWORD dwMHz = _MAX_PATH;
	HKEY hKey;

	// open the key where the proc speed is hidden:
	long lError = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
							"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
							0,
							KEY_READ,
							&hKey);
    
		if(lError != ERROR_SUCCESS)
		  {// if the key is not found, tell the user why:
			   /*FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM,
							 NULL,
							 lError,
							 0,
							 Buffer,
							 _MAX_PATH,
							 0);
				   AfxMessageBox(Buffer);*/
			   return 0;//"N/A";
		   }

			// query the key:
			RegQueryValueEx(hKey, "~MHz", NULL, NULL, (LPBYTE) &dwMHz, &BufSize);

		// convert the DWORD to a CString:
		//sMHz.Format("%i", dwMHz);

	//return sMHz;
	return dwMHz;
}



	
	__int64	GetCyclesDifference(CCPU_FUNC func, unsigned int param)
	{
		unsigned	int	r_edx1, r_eax1;
		unsigned	int	r_edx2, r_eax2;

		//
		// Calculation
		//
		__asm
		{
			push	param			;push parameter param
			mov	ebx, func	;store func in ebx

			_emit	0Fh			;RDTSC
			_emit	31h

			mov	esi, eax		;esi=eax
			mov	edi, edx		;edi=edx

			call	ebx			;call func

			_emit	0Fh			;RDTSC
			_emit	31h

			pop	ebx

			mov	r_edx2, edx	;r_edx2=edx
			mov	r_eax2, eax	;r_eax2=eax

			mov	r_edx1, edi	;r_edx2=edi
			mov	r_eax1, esi	;r_eax2=esi
		}

		return(CCPU_I32TO64(r_edx2, r_eax2) - CCPU_I32TO64(r_edx1, r_eax1));
	}

	void _Delay(unsigned int ms)
	{
		LARGE_INTEGER	freq, c1, c2;
		__int64			x;

		if (!QueryPerformanceFrequency(&freq))
			return;
		x = freq.QuadPart/1000*ms;

		QueryPerformanceCounter(&c1);
		do
		{
			QueryPerformanceCounter(&c2);
		}while(c2.QuadPart-c1.QuadPart < x);
	}
	void	_DelayOverhead(unsigned int ms)
	{
		LARGE_INTEGER	freq, c1, c2;
		__int64			x;

		if (!QueryPerformanceFrequency(&freq))
			return;
		x = freq.QuadPart/1000*ms;

		QueryPerformanceCounter(&c1);
		do
		{
			QueryPerformanceCounter(&c2);
		}while(c2.QuadPart-c1.QuadPart == x);
	}

	static int GetCpuFrequency3(unsigned int uTimes=1, unsigned int uMsecPerTime=50, 
							int nThreadPriority=THREAD_PRIORITY_TIME_CRITICAL,
							DWORD dwPriorityClass=REALTIME_PRIORITY_CLASS)
	{
		__int64	total=0, overhead=0;

		if (!uTimes || !uMsecPerTime)
			return(0);

		DWORD		dwOldPC;												// old priority class
		int		nOldTP;												// old thread priority

		// Set Process Priority Class
		if (dwPriorityClass != 0)									// if it's to be set
		{
			HANDLE	hProcess = GetCurrentProcess();			// get process handle
			
			dwOldPC = GetPriorityClass(hProcess);				// get current priority
			if (dwOldPC != 0)											// if valid
				SetPriorityClass(hProcess, dwPriorityClass);	// set it
			else															// else
				dwPriorityClass = 0;									// invalidate
		}
		// Set Thread Priority
		if (nThreadPriority != THREAD_PRIORITY_ERROR_RETURN)	// if it's to be set
		{
			HANDLE	hThread = GetCurrentThread();				// get thread handle
			
			nOldTP = GetThreadPriority(hThread);				// get current priority
			if (nOldTP != THREAD_PRIORITY_ERROR_RETURN)		// if valid
				SetThreadPriority(hThread, nThreadPriority);	// set it
			else															// else
				nThreadPriority = THREAD_PRIORITY_ERROR_RETURN;	// invalidate
		}

		for(unsigned int i=0; i<uTimes; i++)
		{
			total += GetCyclesDifference(_Delay, uMsecPerTime);
			overhead += GetCyclesDifference(_DelayOverhead, uMsecPerTime);
		}

		// Restore Thread Priority
		if (nThreadPriority != THREAD_PRIORITY_ERROR_RETURN)	// if valid
			SetThreadPriority(GetCurrentThread(), nOldTP);		// set the old one
		// Restore Process Priority Class
		if (dwPriorityClass != 0)										// if valid
			SetPriorityClass(GetCurrentProcess(), dwOldPC);		// set the old one


		total -= overhead;
		total /= uTimes;
		total /= uMsecPerTime;
		total /= 1000;

		return((int)total);
	}


	/*
	double GetPerformanceRatio()
	{
		BOOL bRc;
		ULONG bytesReturned;

		int input = 0;
		unsigned __int64 output[2];
		memset(output, 0, sizeof(unsigned __int64) * 2);

		//printf("InputBuffer Pointer = %p, BufLength = %d\n", &input, sizeof(&input));
		//printf("OutputBuffer Pointer = %p BufLength = %d\n", &output, sizeof(&output));

		//
		// Performing METHOD_BUFFERED
		//

		//printf("\nCalling DeviceIoControl METHOD_BUFFERED:\n");

		bRc = DeviceIoControl(hDevice,
			(DWORD)IOCTL_SIOCTL_METHOD_BUFFERED,
			&input,
			sizeof(&input),
			output,
			sizeof(unsigned __int64)*2,
			&bytesReturned,
			NULL
			);

		if (!bRc) {
			//printf("Error in DeviceIoControl : %d", GetLastError());
			return 0;

		}
		//printf("    OutBuffer (%d): %d\n", bytesReturned, output);
		if (output[1] == 0) {
			return 0;
		} else {
			return (float)output[0] / (float)output[1];
		}
	}
	

	int GetNumberOfProcessorCores()
	{
		SYSTEM_INFO sysinfo;
		GetSystemInfo(&sysinfo);
		return sysinfo.dwNumberOfProcessors;
	}

	float GetCpuFrequency4()
	{
		// __rdtsc: Returns the processor time stamp which records the number of clock cycles since the last reset.
		// QueryPerformanceCounter: Returns a high resolution time stamp that can be used for time-interval measurements.
		// Get the frequency which defines the step size of the QueryPerformanceCounter method.
		LARGE_INTEGER frequency;
		QueryPerformanceFrequency(&frequency);
		// Get the number of cycles before we start.
		ULONG cyclesBefore = __rdtsc();
		// Get the Intel performance ratio at the start.
		float ratioBefore = GetPerformanceRatio();
		// Get the start time.
		LARGE_INTEGER startTime;
		QueryPerformanceCounter(&startTime);
		// Give the CPU cores enough time to repopulate their __rdtsc and QueryPerformanceCounter registers.
		Sleep(1000);
		ULONG cyclesAfter = __rdtsc();
		// Get the Intel performance ratio at the end.
		float ratioAfter = GetPerformanceRatio();
		// Get the end time.
		LARGE_INTEGER endTime;
		QueryPerformanceCounter(&endTime);
		// Return the number of MHz. Multiply the core's frequency by the mean MSR (model-specific register) ratio (the APERF register's value divided by the MPERF register's value) between the two timestamps.
		return ((ratioAfter + ratioBefore) / 2)*(cyclesAfter - cyclesBefore)*pow(10, -6) / ((endTime.QuadPart - startTime.QuadPart) / frequency.QuadPart);
	}
	*/
	double GetCpuFrequency5() {
		DWORD ms_begin,ms_end;
		LARGE_INTEGER count_begin, count_end;
		double ticks_per_second;
		ms_begin = timeGetTime();
		QueryPerformanceCounter(&count_begin);
		Sleep(1000);
		ms_end = timeGetTime();
		QueryPerformanceCounter(&count_end);
		return ticks_per_second = (double)(count_end.QuadPart - count_begin.QuadPart)/(ms_end - ms_begin);
	}


	    // Spin in a loop for 50*spinCount cycles
    // On out-of-order CPUs the sub and jne will not add
    // any execution time.
    static void SpinALot(int spinCount)
    {
        __asm
        {
            mov ecx, spinCount
    start:
            add eax, eax // Repeat 50 times

            sub ecx,1
            jne start
        }
    }

    // Calculate the current CPU frequency in GHz, one time.
    float GetFrequency()
    {
        //double start = GetTime();
		double start = timeGetTime();
		printf("start: %f\n", start);
        // Spin for 500,000 cycles (50 * spinCount). This should
        // be fast enough to usually avoid any interrupts.
        SpinALot(10000);
        //double elapsed = GetTime() – start;
		const double end =  timeGetTime();
		double elapsed = end - start;
		printf("end: %f\n", end);
		printf("elapsed: %f\n", elapsed);
        double frequency = (500000 / elapsed) / 1e9;
        return (float)frequency;
    }

    // Calculate the current CPU frequency multiple times
    // and return the largest frequency seen.
    float GetSampledFrequency(int iterations)
    {
        float maxFrequency = 0.0;
        for (int i = 0; i < iterations; ++i)
        {
            float frequency = GetFrequency();
            if (frequency > maxFrequency)
                maxFrequency = frequency;
        }

        return maxFrequency;
    }

	double GetCpuFrequency6() {
		const int arbitrary_value = 5;
		return GetSampledFrequency(arbitrary_value);
	}

	LARGE_INTEGER GetCpuFrequency7() {
		LARGE_INTEGER frequency;
		QueryPerformanceFrequency(&frequency);
		return frequency;
	}

	LONGLONG GetCpuFrequency8() {

		LARGE_INTEGER StartingTime, EndingTime, ElapsedMicroseconds;
		LARGE_INTEGER frequency;

		QueryPerformanceFrequency(&frequency);
		QueryPerformanceCounter(&StartingTime);

		// Activity to be timed
		Sleep(1000);

		QueryPerformanceCounter(&EndingTime);
		//ElapsedMicroseconds.QuadPart = EndingTime.QuadPart - StartingTime.QuadPart;
		return (EndingTime.QuadPart - StartingTime.QuadPart);
	}

} // end anonymous namespace

// Battery
namespace {

}


static DWORD measurements_proc(LPVOID param)
{
	host_info_t *h = new host_info_t;
	clear_host_info(h);
	get_host_info(h);
	//print_host_info(h);
	
	char info_buffer[1024];
	memset(info_buffer, 0, sizeof(info_buffer));
	
	snprintf(info_buffer, 1024, 
			"setcompinfo cpu_vendor %s\n"
			"setcompinfo cpu_model %s\n"
			"setcompinfo cpu_number %d\n"
			"setcompinfo os_name %s\n"
			"setcompinfo os_version %s\n"
			//"set memory %f\n"
			//"set swap %f\n"
			"updatecompinfo\n",
			h->p_vendor, h->p_model, h->p_ncpus,
			h->os_name, h->os_version);
			//h->m_nbytes, h->m_swap);

	winsocket_send(info_buffer);

	//Sleep(500000);

	if (h) {
	  delete h;
	}
	
	while (measurements_continue) {

		float cpu_frequency = 0;
		float cpu_load_comp = 0;
		float cpu_load_user = 0;
		float cpu_load_sys = 0;

		wmi_get_cpu_dataf(
			&cpu_frequency,
			&cpu_load_comp,
			&cpu_load_user,
			&cpu_load_sys
		);

		float cpu_load = cpu_load_comp + cpu_load_user + cpu_load_sys;
		float cpu_load_idle = 100 - cpu_load;

		wmi_get_battery_info();

		char buffer[1024];

		/*
		snprintf(buffer, 1024, 
			"set cpu_load %f\n"
			"set cpu_frequency %f\n"
			"set cpu_load_comp %f\n"
			"set cpu_load_user %f\n"
			"set cpu_load_sys %f\n"
			"set cpu_load_idle %f\n"
			"update\n",
			cpu_load, cpu_frequency, cpu_load_comp,
			cpu_load_user, cpu_load_sys, cpu_load_idle);
		*/
		snprintf(buffer, 1024, 
			"set cpu_load %f\n"
			"set cpu_load_comp %f\n"
			"set cpu_load_user %f\n"
			"set cpu_load_sys %f\n"
			"set cpu_load_idle %f\n"
			"set cpu_frequency %f\n"
			"set on_battery %d\n"
			"update\n",
			cpu_load, cpu_load_comp, cpu_load_user,
			cpu_load_sys, cpu_load_idle, cpu_frequency,
			is_running_on_batteries() == 0 ? 1 : 0);
		
		buffer[1023] = 0;

		//
		//printf("TEST cpu_buffer: %s", buffer);
		//printf("Cpu load: %f\n", cpu_load);
		//printf("Cpu freq: %f\n", cpu_frequency);

		
		if (winsocket_send(buffer) == -1) {
			return -1;
		}

		Sleep(1000);
	}

	return 0;
}

int measurements_start() {

	//CpuLoadinit();

	//battery_init();
	
	wmi_initialize();

	DWORD thread_id;

	measurements_thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) measurements_proc, NULL, 0, &thread_id);

	if (measurements_thread == NULL)
		return -1;
	
	if (!SetThreadPriority(measurements_thread, THREAD_PRIORITY_HIGHEST)) {
		log_debug("measurements thread has not been 'prioritized'");
	}

	return 0;
}

int measurements_stop()
{
	measurements_continue = 0;
	if (measurements_thread) {
		WaitForSingleObject(measurements_thread, INFINITE);
		CloseHandle(measurements_thread);
		measurements_thread = NULL;
	}
	return 0;
}