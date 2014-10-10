
#ifndef _WMI_H_
#define _WMI_H_

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

int wmi_initialize();

// Get total cpu load, cpu frequency current app cpu load, and user cpu load
int wmi_get_cpu_data(
	long long* cpu_load,
	long long* cpu_frequency,
	long long* cpu_load_comp,
	long long* cpu_load_user
);

int wmi_get_cpu_load_frequency(
	long long* cpu_load,
	long long* cpu_frequency
);

int wmi_get_cpu_comp_user(
	long long* cpu_load_comp,
	long long* cpu_load_user
);

// Test function
int wmi_get_cpu_comp_user2(
	long long* cpu_load_comp,
	long long* cpu_load_user
);

int wmi_get_cpu_comp(
	long long* cpu_load_comp
);

int wmi_get_cpu_user(
	long long* cpu_load_user
);

/// Float version

int wmi_get_cpu_dataf(
	float* cpu_load,
	float* cpu_frequency,
	float* cpu_load_comp,
	float* cpu_load_user
);

int wmi_get_cpu_frequency(
	float* cpu_frequency
);

int wmi_get_cpu_comp_user_sys(
	float* cpu_load_comp,
	float* cpu_load_user,
	float* cpu_load_sys
);

//int wmi_get_memory(...);
//int wmi_get_memory(...);

// Get Battery Info

int wmi_get_battery_info(
);

int wmi_get_battery_info2(
);

void wmi_free();

#ifdef __cplusplus
}
#endif

#endif // _WMI_H_
