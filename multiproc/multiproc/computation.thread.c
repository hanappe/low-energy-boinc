#include <string.h>
#include <windows.h>
#include <tchar.h>
#include <stdio.h>
#include "computation.h"
#include "log.h"

static int _computation_mode = STANDARD;

computation_data_t* new_computation_data(int size, FILE* out)
{
	computation_data_t* d = (computation_data_t*) malloc(sizeof(computation_data_t));
	if (d == NULL)
		return NULL;
	memset(d, 0, sizeof(computation_data_t));
	d->size = size;
	d->out = out;
	return d;
}

void delete_computation_data(computation_data_t* d)
{
	if (d == NULL)
		return;
	if (d->out != NULL)
		fclose(d->out);
	free(d);
}

void set_computation_mode(int mode) {
	_computation_mode = mode;
}

int get_computation_mode() {
	return _computation_mode;
}


enum {
	NO_PROCESS = 0,
	RUNNING,
	SUSPENDED,
	FINISHED
};



typedef struct _computation_t {
	int state;
	HANDLE thread;
	DWORD thread_id;
	computation_data_t* d;
} computation_t;

#define MAX_COMPUTATIONS 60

computation_t computations[MAX_COMPUTATIONS];

int computation_init()
{
    ZeroMemory(&computations[0], MAX_COMPUTATIONS * sizeof(computation_t));
	return 0;
}

void computation_cleanup()
{
		// TODO
}

static DWORD computation_proc(LPVOID param)
{
	computation_t* c = (computation_t*) param;


	switch (_computation_mode) {
		case STANDARD: linpack_main(c->d); break;
		case DRIVER: linpack_lpc_main(c->d); break;
		default: {
			log_err("computation_proc: error, unkown computation mode: %d", _computation_mode);
		}
	}
	c->state = FINISHED;
	return 0;
}

int computation_start(int index, computation_data_t* d)
{
	log_info("computation_start: index %d, size %d", index, d->size);

	if ((index < 0) || (index >= MAX_COMPUTATIONS)) {
		log_err("computation_start: invalid index %d", index);
		return -1;
	}
	if (computations[index].state != NO_PROCESS) {
		log_warn("computation_start: index %d not available, state %d", index, computations[index].state);
		return 0;
	}

	computations[index].d = d;

	computations[index].thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) computation_proc,
			(LPVOID) &computations[index], 0, &computations[index].thread_id);

	if (computations[index].thread == NULL)
		return -1;

	computations[index].state = RUNNING;
	return 0;
}

int computation_is_finished(int index)
{
	if ((index < 0) || (index >= MAX_COMPUTATIONS)) {
		log_err("computation_is_finished: invalid index %d", index);
		return 1;
	}
	return ((computations[index].state == NO_PROCESS) 
		|| (computations[index].state == FINISHED));
}

static void computation_clear(int index)
{
	if ((index < 0) || (index >= MAX_COMPUTATIONS)) {
		log_err("computation_clear: invalid index %d", index);
		return;
	}
	if (computations[index].thread != NULL)
		CloseHandle(computations[index].thread);
	computations[index].state = NO_PROCESS;
	memset(&computations[index], 0, sizeof(computation_t));
}

void computation_wait(int index)
{
	if ((index < 0) || (index >= MAX_COMPUTATIONS)) {
		log_err("computation_wait: invalid index %d", index);
		return;
	}

	if (computations[index].state == NO_PROCESS) {
		log_err("computation_wait: index %d: no computation running", index);
		return;
	}
    WaitForSingleObject(computations[index].thread, INFINITE);
	computation_clear(index);		
}

void computation_terminate(int index)
{
	if ((index < 0) || (index >= MAX_COMPUTATIONS)) {
		log_err("computation_terminate: invalid index %d", index);
		return;
	}
	if (computations[index].state == NO_PROCESS) {
		log_err("computation_terminate: index %d: no computation running", index);
		return;
	}
    TerminateThread(computations[index].thread, 0);
	computation_clear(index);
}

void computation_suspend(int index)
{
	//printf("computation_suspend\n");

	if ((index < 0) || (index >= MAX_COMPUTATIONS)) {
		log_err("computation_suspend: invalid index %d", index);
		return;
	}
	if ((computations[index].state == NO_PROCESS)
		|| (computations[index].state == FINISHED) 
		|| (computations[index].state == SUSPENDED)) {
		return;
	}
	SuspendThread(computations[index].thread);
	computations[index].state = SUSPENDED;
}

void computation_resume(int index)
{
	//printf("computation_resume\n");

	if ((index < 0) || (index >= MAX_COMPUTATIONS)) {
		log_err("computation_resume: invalid index %d", index);
		return;
	}
	if (computations[index].state == NO_PROCESS) {
		return;
	}
	if (computations[index].state != SUSPENDED) {
		log_err("computation_resume: index %d: computation not suspended, state %d", 
			index, computations[index].state);
		return;
	}
	ResumeThread(computations[index].thread);
	computations[index].state = RUNNING;
}

