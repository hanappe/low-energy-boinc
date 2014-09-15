
// TODO
// - thread priorities
// - mutex to protect log? other mutexes?

#include <windows.h>
#include <tchar.h>
#include <stdio.h>
#include "computation.h"
#include "counter.h"
#include "log.h"
#include "measurements.h"
#include "winsocket.h"

enum {
	EXP_IDLE,
	EXP_CPU_100,
	EXP_CPU_80,
	EXP_CPU_60,
	EXP_CPU_40,
	EXP_CPU_20,
	EXP_END
};

int count_cpus()
{
	return 4;
}



static HANDLE throttling_thread = NULL;
static int throttling_continue = 1;
static int _cpu_usage = 100;

void throttling_set_cpu_usage(int cpu_usage)
{
	_cpu_usage = cpu_usage;
}

int throttling_get_cpu_usage()
{
	return _cpu_usage;
}

static DWORD throttling_proc(LPVOID param)
{
#define THROTTLING_PERIOD  5000

	while (throttling_continue) {
		int msec_run = THROTTLING_PERIOD * throttling_get_cpu_usage() / 100;
		int msec_sleep = THROTTLING_PERIOD - msec_run;

		if (msec_sleep > 0) {
			for (int i = 0; i < count_cpus(); i++)
				computation_suspend(i);

			//printf("Sleep %d msec\n", msec_sleep);
			Sleep(msec_sleep);
		}

		if (!throttling_continue) 
			break;

		if (msec_run > 0) {
			for (int i = 0; i < count_cpus(); i++)
				computation_resume(i);

			//printf("Run %d msec\n", msec_run);
			Sleep(msec_run);
		}
	}
	return 0;
}

int throttling_start()
{
	DWORD thread_id;

	throttling_thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) throttling_proc, NULL, 0, &thread_id);

	if (throttling_thread == NULL)
		return -1;

	return 0;
}

int throttling_stop()
{
	for (int i = 0; i < count_cpus(); i++)
		computation_resume(i);
	throttling_continue = 0;
    TerminateThread(throttling_thread, 0);
    CloseHandle(throttling_thread);
	throttling_thread = NULL;
	return 0;
}

static HANDLE experiment_thread = NULL;
static int experiment_continue = 1;
static int experiment_state = EXP_IDLE;

void experiment_next_state()
{
	experiment_state++;
	for (int i = 0; i < count_cpus(); i++) {
		computation_terminate(i);
	}
}

static DWORD experiment_proc(LPVOID param)
{
	int compute = 1;
	char buffer[256];
#if 0
	int size = 4000;
	int idle_sleep = 1 * 60 * 1000; 
#else
	int size = 1000;
	int idle_sleep = 10 * 1000; 
#endif

	log_info("Experiment started");

	while (experiment_state < EXP_END) {

		if (experiment_continue == 0) {
			log_info("Experiment finished prematurely"); 
			printf("\n\nExperiment finished prematurely\n\n");
			return 0;
		}

		switch (experiment_state) {
			case EXP_IDLE: throttling_set_cpu_usage(100); compute = 0; break;
			case EXP_CPU_100: throttling_set_cpu_usage(100); compute = 1; break;
			case EXP_CPU_80: throttling_set_cpu_usage(80); compute = 1; break;
			case EXP_CPU_60: throttling_set_cpu_usage(60); compute = 1; break;
			case EXP_CPU_40: throttling_set_cpu_usage(40); compute = 1; break;
			case EXP_CPU_20: throttling_set_cpu_usage(20); compute = 1; break;
		}

		for (int test = 0; test < 3; test++) {

			double start = counter_get();
			double kflops = 0.0;

			// start

			if (compute == 0) {

				if (winsocket_send("start idle\n") != -1) {
					log_err("experiment_proc: failed to send START message");
				}

				Sleep(idle_sleep);

				if (winsocket_send("stop\n") != -1) {
					log_err("experiment_proc: failed to send START message");
				}

			} else {

				computation_data_t** d = (computation_data_t**) malloc(count_cpus() * sizeof(computation_data_t*));
				if (d == NULL) {
					log_err("experiment_proc: out of memory");
					return 1;
				}

				_snprintf(buffer, sizeof(buffer)-1, "start size-%d_cpu-%d_run-%d\n", size, throttling_get_cpu_usage(), test);
				buffer[sizeof(buffer)-1] = 0;

				if (winsocket_send(buffer) != -1) {
					log_err("experiment_proc: failed to send START message");
				}

				for (int i = 0; i < count_cpus(); i++) {

					_snprintf(buffer, sizeof(buffer)-1, "slot-%d_size-%d_cpu-%d_run-%d.txt", i, size, throttling_get_cpu_usage(), test);
					buffer[sizeof(buffer)-1] = 0;

					FILE* out = fopen(buffer, "w");
					if (out == NULL) {
						log_err("experiment_proc: failed to open file '%s'", buffer);
						return 1;
					}

					d[i] = new_computation_data(size, out);
					if (d[i] == NULL) {
						log_err("experiment_proc: out of memory");
						return 1;
					}

					if (computation_start(i, d[i]) != 0)
						return 1;
				}
				for (int i = 0; i < count_cpus(); i++) {
					computation_wait(i);
				}
				kflops = 0.0;
				for (int i = 0; i < count_cpus(); i++) {
					kflops += d[i]->kflops;
					delete_computation_data(d[i]);
				}

				free(d);

				_snprintf(buffer, sizeof(buffer)-1, "set kflops %f\n", kflops);
				buffer[sizeof(buffer)-1] = 0;
				winsocket_send(buffer);

				if (winsocket_send("stop\n") != -1) {
					log_err("experiment_proc: failed to send START message");
				}

			}

			if (compute) {
				log_info("CPU usage %d, Test %d, Time %.3f seconds, KFLOPS %0.2f", 
					throttling_get_cpu_usage(), test, counter_get() - start, kflops);
			} else {
				log_info("Idle, Test %d, Time %.3f seconds", 
					test, counter_get() - start);
			}
		}

		experiment_state++;
	}

	log_info("Experiment finished"); 
	printf("\n\nExperiment finished\n\n");
	return 0;
}

int experiment_start()
{
	DWORD thread_id;

	experiment_thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) experiment_proc, NULL, 0, &thread_id);

	if (experiment_thread == NULL)
		return -1;

	return 0;
}

int experiment_stop()
{
	experiment_continue = 0;
	for (int i = 0; i < count_cpus(); i++) {
		computation_terminate(i);
	}
    WaitForSingleObject(experiment_thread, INFINITE);
    CloseHandle(experiment_thread);
	experiment_thread = NULL;
	return 0;
}


int _tmain(int argc, _TCHAR* argv[])
{
	if (log_set_file("log.txt") != 0)
		return 1;

	if (winsocket_init() != 0) {
		//return 1;
	}

	if (counter_init() != 0)
		return 1;

	if (computation_init() != 0)
		return 1;

	if (measurements_start() != 0)
			return 1;

	if (throttling_start() != 0)
		return 1;

	if (experiment_start() != 0)
		return 1;

	char buf[80];

	while (experiment_state < EXP_END) {
        printf("Press [enter] to skip test, [q] to quit:  ");
        fgets(buf, 79, stdin);
		if (buf[0]=='q' || buf[0]=='Q') {
            log_info("Requested quit\n");
            printf("Requested quit\n");
			experiment_stop();
			break;
		}
		if (buf[0]=='\0' || buf[0]=='\n' || buf[0]=='\r') {
            log_info("Requested skip\n");
            printf("Requested skip\n");
			experiment_next_state();
		}
	}

	throttling_stop();

	measurements_stop();

	winsocket_end();

	printf("Press [enter] to close the console");
    fgets(buf, 79, stdin);

	return 0;
}

