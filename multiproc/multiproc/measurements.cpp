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
#define snprintf _snprintf_s
#endif

static HANDLE measurements_thread = NULL;
static int measurements_continue = 1;



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

		//wmi_get_battery_info();

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
		printf("TEST cpu_buffer: %s", buffer);
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