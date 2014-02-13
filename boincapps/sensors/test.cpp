#include <iostream>
#include <csignal>
#include <sstream>
#include <unistd.h>
#include "Sensors.hpp"
#include "PStates.hpp"
#include "BoincSensors.hpp"
#include "Wattsup.hpp"
#include "TEMPer.hpp"
#include "BoincSensors.hpp"
#include "CpuLoad.hpp"
#include "BoincCpuLoad.hpp"
#include "UsersCpuLoad.hpp"
#include "LibSensors.hpp"
#include "ACPI.hpp"

using namespace std;

static bool debug = false;
static bool signaled = false;

void signal_handler(int signum) {
        signaled = true;
}

int main(int argc, char** argv) {

        signal(SIGINT, signal_handler);
        signal(SIGHUP, signal_handler);
        signal(SIGQUIT, signal_handler);
        signal(SIGTERM, signal_handler);
        signal(SIGPWR, signal_handler);

        Sensors::add_sensor_manager(getWattsupManager());
        Sensors::add_sensor_manager(getTEMPerManager());
        Sensors::add_sensor_manager(getPStatesManager());
        Sensors::add_sensor_manager(getBoincSensorsManager());
        Sensors::add_sensor_manager(getCpuLoadManager());
        Sensors::add_sensor_manager(getBoincCpuLoadManager());
        Sensors::add_sensor_manager(getUsersCpuLoadManager());
        Sensors::add_sensor_manager(getLibSensorsManager());
        Sensors::add_sensor_manager(getACPIManager());

        time_t t0 = Datapoint::get_current_time();

        while (true) {
                if (signaled) {
                        cout << endl;
                        return 0;
                }

                usleep(250000);

                Sensors::update();

                time_t t1 = Datapoint::get_current_time();
                if (t1 - t0 < 10) continue;
                t0 = t1;

                ostringstream trickle;
                Sensors::print_datapoints(trickle);
                if (trickle.tellp() > 0) cout << trickle.str() << endl;

                if (debug) cout << '.';
        }

        return 0;
}

