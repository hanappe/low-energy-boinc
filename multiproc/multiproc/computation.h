
#ifndef _COMPUTATION_H_
#define _COMPUTATION_H_

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef struct _computation_data_t {
	// input
	int size;
	FILE* out;
	// output
	double duration;
	double kflops;
} computation_data_t;

computation_data_t* new_computation_data(int size, FILE* out);
void delete_computation_data(computation_data_t* d);


void linpack_main(computation_data_t* d);


int computation_init();
void computation_cleanup();

int computation_start(int index, computation_data_t* d);

void computation_suspend(int index);
void computation_resume(int index);

int computation_is_finished(int index);
void computation_wait(int index);
void computation_terminate(int index);

#ifdef __cplusplus
}
#endif

#endif _COMPUTATION_H_
