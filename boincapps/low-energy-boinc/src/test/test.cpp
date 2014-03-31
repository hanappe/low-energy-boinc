#include <iostream>
#include <csignal>
#include <sstream>

#include "Sensors.hpp"
#include "BoincSensors.hpp"
#include "Wattsup.hpp"
#include "TEMPer.hpp"
#include "BoincSensors.hpp"
#include "CpuLoad.hpp"
#include "LibSensors.hpp"

#ifdef _WIN32
	#include <windows.h>

	#include "ArduinoTemp.hpp"
	#include "MsAcpi.hpp"
	#include "BoincUserCpuLoad.hpp"

#else // Unix
	#include <unistd.h>

	#include "PStates.hpp"
	#include "ACPI.hpp"
	#include "BoincCpuLoad.hpp"
        #include "UsersCpuLoad.hpp"
//#include "ArduinoTemp.hpp"
#endif


using namespace std;

static bool debug = false;
static bool signaled = false;

void signal_handler(int signum) {
        signaled = true;
}

void my_sleep(unsigned long milliseconds) {
#ifdef _WIN32
      Sleep(milliseconds);
#else
      usleep(milliseconds*1000);
#endif
}

int main(int argc, char** argv) {

        //Sensors::add_sensor_manager(getWattsupManager());
        //Sensors::add_sensor_manager(getLibSensorsManager());
        //Sensors::add_sensor_manager(getCpuLoadManager());
        
#ifdef _WIN32
        //Sensors::add_sensor_manager(getBoincUserCpuLoadManager());
        Sensors::add_sensor_manager(getArduinoTempManager());
        //Sensors::add_sensor_manager(getMsAcpiManager());
#else // Unix
        signal(SIGINT, signal_handler);
        signal(SIGHUP, signal_handler);
        signal(SIGQUIT, signal_handler);
        signal(SIGTERM, signal_handler);
        signal(SIGPWR, signal_handler);
        //Sensors::add_sensor_manager(getArduinoTempManager());
	
        //Sensors::add_sensor_manager(getTEMPerManager());
        //Sensors::add_sensor_manager(getPStatesManager());
        //Sensors::add_sensor_manager(getBoincSensorsManager());
        
        //Sensors::add_sensor_manager(getBoincCpuLoadManager());
        //Sensors::add_sensor_manager(getUsersCpuLoadManager());
        //Sensors::add_sensor_manager(getACPIManager());
        
#endif
        
        
        time_t t0 = Datapoint::get_current_time();
        
        while (true) {
                if (signaled) {
                        cout << endl;
                        return 0;
                }
                
                my_sleep(250);
                
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
