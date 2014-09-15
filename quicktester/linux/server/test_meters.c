#define _BSD_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include "log.h"
#include "meter.h"

static int signaled = 0;

void signal_handler(int signum)
{
        signaled = 1;
}

int main(int argc, char** argv)
{
        signal(SIGINT, signal_handler);
        signal(SIGHUP, signal_handler);
        signal(SIGQUIT, signal_handler);
        signal(SIGTERM, signal_handler);
        signal(SIGPWR, signal_handler);

        printf("Initializing...\n");

        if (meters_init() != 0) {
                log_err("main: failed to initialise the meters");
                exit(1);
        }

        int nmeters = meters_count();
        printf("Found %d meter%s.\n", nmeters, nmeters == 1 ? "" : "s");

        meters_start();

        while (1) {
                usleep(1100000);

                if (signaled) {
                        printf("\n");
                        break;
                }

                for (int i = 0; i < nmeters; i++) {
                        meter_t* m = meters_get(i);
                        float energy = meter_get_energy(m, NULL, NULL);
                        printf("meter %d: %gJ\n", i, energy);
                }
        }

        printf("Finalizing...\n");

        meters_stop();
        if (meters_fini() != 0) {
                log_err("main: failed to finalize the meters");
                exit(1);
        }

        return 0;
}

