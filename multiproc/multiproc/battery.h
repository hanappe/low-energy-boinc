#ifndef _BATTERY_H_
#define _BATTERY_H_

#include <windows.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

int battery_init();


HANDLE battery_init2(LPWSTR device);

// Temp function to viez battery info
int battery_print_info(HANDLE hbat);

// Assuming computer have only one battery
int battery_get_info(
	float* current_capacity,
	float* max_capacity,
	float* percent_charge
);


int battery_stop();


#ifdef __cplusplus
}
#endif

#endif // _BATTERY_H_
