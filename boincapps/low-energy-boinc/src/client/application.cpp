#include "boinc_api.h"
#include "filesys.h"
#include "util.h"
#include "Sensors.hpp"


#include "BoincApp.h"
#include "RaspberryApp.h"
#include "BenchmarkApp.h"

using namespace std;

/*
static void InitializeManagers(const App app) {

        switch (app) {
        case Boinc: { // Linux & Windows
                Sensors::add_sensor_manager(getBoincSensorsManager());
                Sensors::add_sensor_manager(getCpuLoadManager());
                Sensors::add_sensor_manager(getWattsupManager());
                
		#ifdef _WIN32
                Sensors::add_sensor_manager(getArduinoTempManager());
                Sensors::add_sensor_manager(getBoincUserCpuLoadManager());
		#else // Unix
                Sensors::add_sensor_manager(getACPIManager());
                Sensors::add_sensor_manager(getBoincCpuLoadManager());
                Sensors::add_sensor_manager(getLibSensorsManager());
                Sensors::add_sensor_manager(getPStatesManager());
                Sensors::add_sensor_manager(getUsersCpuLoadManager());
                Sensors::add_sensor_manager(getTEMPerManager());
		#endif
       
                break;
        }
        case Standalone: { // Linux & Windows
                Sensors::add_sensor_manager(getCpuLoadManager());
                // Remove if Wattsup is connected to a Raspberry Pi
                Sensors::add_sensor_manager(getWattsupManager());
                
		#ifdef _WIN32
                Sensors::add_sensor_manager(getArduinoTempManager());
		#else // Unix
                Sensors::add_sensor_manager(getACPIManager());
                Sensors::add_sensor_manager(getBoincCpuLoadManager());
                Sensors::add_sensor_manager(getLibSensorsManager());
                Sensors::add_sensor_manager(getPStatesManager());
                Sensors::add_sensor_manager(getUsersCpuLoadManager());
                // Remove if Wattsup is connected to a Raspberry Pi
                Sensors::add_sensor_manager(getTEMPerManager());
		#endif
              
                break;
        }
        case RaspberryPi: { // Linux 
                #ifdef _WIN32
                #else // Unix
                Sensors::add_sensor_manager(getWattsupManager());
                Sensors::add_sensor_manager(getTEMPerManager());
		#endif
              
         break;
        }
        default: {
        
                break;
        }
}

static ReleaseManagers() {

        Sensors::releaseManagers();
}
*/

int main(int, char**) {

        bool standalone = false;
	
        //static const App::Type type = App::Boinc;
        static const App::Type type = App::RaspberryPi;
        //static const App::Type type = App::Benchmark;

        if (type == App::Boinc) {
                BoincApp app;
                app.setDebug(true);
                if (app.isInitialized()) {
                        app.loop();
                }
        }

        if (type == App::RaspberryPi) {
                RaspberryApp app;
                app.setDebug(true);
                if (app.isInitialized()) {
                        app.loop();
                }        
        }        

        if (type == App::Benchmark) {
                BenchmarkApp app;
                app.setDebug(true);
                if (app.isInitialized()) {
                        app.loop();
                }        
        }
        
        
}

#ifdef _WIN32

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPSTR Args, int WinMode) {
        LPSTR command_line;
        char* argv[100];
        int argc;

        command_line = GetCommandLine();
        argc = parse_command_line( command_line, argv );
        return main(argc, argv);
}

#endif
