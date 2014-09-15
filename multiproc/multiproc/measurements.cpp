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


// Socket

// Requires Win7 or Vista
// Link to Ws2_32.lib library
//#include <winsock2.h>
/*#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif*/
/*
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <stdio.h>
*/
//#include <ws2tcpip.h>
/*
#if defined(_WIN32) || defined(__WIN32__) || defined(_MSC_VER)

    #define OS_WIN32
    #define _WIN32_WINNT 0x0600

    

    // Requires Win7 or Vista
    // Link to Ws2_32.lib library
#endif*/

#include "cpu_info.h"
#include "wmi.h"
// Kev
#ifdef _MSC_VER
#define snprintf _snprintf
#endif

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



} // end anonymous namespace

static DWORD measurements_proc(LPVOID param)
{
	host_info_t *h = new host_info_t;
	clear_host_info(h);
	get_host_info(h);
	print_host_info(h);
	
	char info_buffer[1024];
	memset(info_buffer, 0, sizeof(info_buffer));
	
	snprintf(info_buffer, 1024, 
			"set cpu_vendor %s\n"
			"set cpu_model %s\n"
			"set cpu_number %d\n"
			"set os_name %s\n"
			"set os_version %s\n"
			"set memory %f\n"
			"set swap %f\n",
			h->p_vendor, h->p_model, h->p_ncpus,
			h->os_name, h->os_version,
			h->m_nbytes, h->m_swap);

	printf("TEST info_buffer: %s", info_buffer);

	//winsocket_send(buffer);

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
			"set cpu_frequency %f\n"
			"set cpu_load_comp %f\n"
			"set cpu_load_user %f\n"
			"set cpu_load_sys %f\n"
			"set cpu_load_idle %f\n"
			"set on_battery %d\n"
			"update\n",
			cpu_load, cpu_frequency, cpu_load_comp,
			cpu_load_user, cpu_load_sys, cpu_load_idle, is_running_on_batteries() == 0 ? 1 : 0);
		
		buffer[1023] = 0;

		printf("TEST cpu_buffer: %s", buffer);
		winsocket_send(buffer);

		Sleep(1000);
	}

	return 0;
}

int measurements_start() {

	//CpuLoadinit();
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
    WaitForSingleObject(measurements_thread, INFINITE);
    CloseHandle(measurements_thread);
	measurements_thread = NULL;
	return 0;
}