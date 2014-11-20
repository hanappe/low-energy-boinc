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
	float max_capacity = 0;
	char buffer[1024];
	
	battery_get_max_capacity(&max_capacity);
	
	snprintf(buffer, sizeof(buffer), 
			"setcompinfo cpu_vendor %s\n"
			"setcompinfo cpu_model %s\n"
			"setcompinfo cpu_number %d\n"
			"setcompinfo os_name %s\n"
			"setcompinfo os_version %s\n"
			"setcompinfo bat_max_capacity %f\n"
			//"set memory %f\n"
			//"set swap %f\n"
			"updatecompinfo\n",
			h->p_vendor, h->p_model, h->p_ncpus,
			h->os_name, h->os_version,
			max_capacity);
			//h->m_nbytes, h->m_swap);
	buffer[sizeof(buffer)-1] = 0;

	winsocket_send(buffer);

	//Sleep(500000);

	if (h) {
	  delete h;
	}
	
	while (measurements_continue) {

		// Cpu Load

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

		

		//printf("Batter: \n current_capacity: %f, max_capacity: %f, life_percent: %f", current_capacity, max_capacity, life_percent);

		//wmi_get_battery_info();

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
		float on_battery = 0;
		float current_capacity = 0;
		float life_percent = 0;

		if (battery_is_present()) {
			on_battery = (is_running_on_batteries() == 0) ? 1.0f : 0.0f;
			battery_get_current_info(&current_capacity, &life_percent);
		}

		snprintf(buffer, 1024, 
			"set cpu_load %f\n"
			"set cpu_load_comp %f\n"
			"set cpu_load_user %f\n"

			"set cpu_load_sys %f\n"
			"set cpu_load_idle %f\n"
			"set cpu_frequency %f\n"

			"set on_battery %f\n"
			"set bat_cur_capacity %f\n"
			"set bat_life_percent %f\n"

			"update\n",

			cpu_load, cpu_load_comp, cpu_load_user,
			cpu_load_sys, cpu_load_idle, cpu_frequency,
			on_battery, current_capacity, life_percent
		);

		//printf("TEST BATTERY CAPACITY: %f\n", current_capacity);
		//printf("TEST LIFE PERCENT: %f\n", life_percent);

		buffer[sizeof(buffer)-1] = 0;

		if (winsocket_send(buffer) == -1) {
			log_err("measurements_proc error: cpu data not sent");
			return -1;
		}
		/*
		/// Battery
		if (battery_is_present()) {
			float current_capacity = 0;
			float life_percent = 0;

			battery_get_current_info(
				&current_capacity,
				&life_percent
			);

			snprintf(buffer, sizeof(buffer),
				"setbattery on_battery %f\n"
				"setbattery bat_cur_capacity %f\n"
				"setbattery bat_life_percent %f\n"
				"updatebattery\n",
				
				
			);
			buffer[sizeof(buffer)-1] = 0;

			//printf("TEST BATTERY CAPACITY: %f\n", current_capacity);
			//printf("TEST LIFE PERCENT: %f\n", life_percent);
			
			if (winsocket_send(buffer) == -1) {
				log_err("measurements_proc error: battery data not sent");
				return -1;
			}
		}*/

		Sleep(1000);
	}

	return 0;
}

int measurements_start() 
{
	if (battery_init()) {
		log_warn("measurements_start: battery_init failed badly, not running measurements");
		return -1;
	}
	
	wmi_initialize();

	DWORD thread_id;

	measurements_thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) measurements_proc, NULL, 0, &thread_id);

	if (measurements_thread == NULL) {
		log_err("measurements_start error, measurements_thread has not been created");
		return -1;
	}
	
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