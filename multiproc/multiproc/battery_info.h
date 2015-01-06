#ifndef _BATTERY_H_
#define _BATTERY_H_

#include <windows.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

int battery_init();

// return BOOL
int battery_is_present();


HANDLE battery_init2(LPWSTR device);

// Temp function to viez battery info
int battery_print_info(HANDLE hbat);

// Assuming computer have only one battery
// Get max capacity
int battery_get_max_capacity(
	float* pmax_capacity
);

// Assuming computer have only one battery

int battery_get_current_info(
	float* pcapacity,
	float* plife_percent
);


int battery_stop();


#ifdef __cplusplus
}
#endif

#endif // _BATTERY_H_
