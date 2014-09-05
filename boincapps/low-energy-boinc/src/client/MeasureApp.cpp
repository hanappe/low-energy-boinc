#include "MeasureApp.h"

#include <assert.h>
#include <iostream>
#include <csignal>

#include "boinc_api.h"
#include "filesys.h"
#include "util.h"

#ifdef _WIN32
	#include "win_util.h"
#else // Unix
	#include <unistd.h>
#endif

#include "Sensors.hpp"

using namespace std;

namespace {

bool signaled = false;
void signal_handler(int signum) {
        signaled = true;
}

} // end anonymous namespace

MeasureApp::MeasureApp() :
        App()
{

        // Even in standalone, We need to initialize some Boinc variable
        // to use "boinc_elasped" etc.
        int err = boinc_init();
        if (err) {
                cout << "boinc_init: " << err << endl;
                boinc_finish(err);
        }

        _is_standalone = boinc_is_standalone();

        if (!_is_standalone) {
                std::cerr << "Raspberry app should be standalone!" << std::endl;
        }


        #ifdef _WIN32
        // Nothing
	#else // Unix
        if (debug()) {
                signal(SIGINT, signal_handler);
                signal(SIGHUP, signal_handler);
                signal(SIGQUIT, signal_handler);
                signal(SIGTERM, signal_handler);
                signal(SIGPWR, signal_handler);
        }
        #endif

        initialize();
}


MeasureApp::~MeasureApp() {
}

bool MeasureApp::initialize() {
        if (debug()) {
                std::cout << "MeasureApp::initialize()" << std::endl; 
        } 

        if (!mIsInitialized) {
                Sensors::add_sensor_manager(getBoincSensorsManager());
                Sensors::add_sensor_manager(getCpuLoadManager());
                
	#ifdef _WIN32
                Sensors::add_sensor_manager(getBoincUserCpuLoadManager());
	#else // Unix
                Sensors::add_sensor_manager(getACPIManager());
                Sensors::add_sensor_manager(getBoincCpuLoadManager());
                //Sensors::add_sensor_manager(getLibSensorsManager());
                Sensors::add_sensor_manager(getPStatesManager());
                Sensors::add_sensor_manager(getUsersCpuLoadManager());
        #endif

                mIsInitialized = true;                
                return true;
        }

        return false;
}

void MeasureApp::finalize() {

        if (debug()) {
                std::cout << "MeasureApp::finalize()" << std::endl; 
        } 
        

        if (isInitialized()) {
                // Delete Sensors
                Sensors::releaseManagers();
        }
        
        // Quit Boinc

        boinc_finish(0);

        mIsInitialized = false;
}

void MeasureApp::loop() {

        if (debug()) {
                std::cout << "MeasureApp::loop()" << std::endl; 
        }

        assert(isInitialized());

        int err = 0;

        static const double send_trickle_interval = 45; // seconds
        static const double update_sensors_interval = 0.5; // seconds

        double current_trickle_time = boinc_elapsed_time();

        for (;;) {

                if (signaled) break;
                
                double last_trickle_time = current_trickle_time;
                current_trickle_time = boinc_elapsed_time();
                
                while (current_trickle_time - last_trickle_time < send_trickle_interval) {

                        if (signaled) break;

                        Sensors::update();
                      
                        // Wait "update_sensors_interval" seconds
                        while (true) {
                                if (signaled) break;
                                
                                double delta =  boinc_elapsed_time() - current_trickle_time;;
                                if (delta >= update_sensors_interval) {
                                        break;
                                }
                                
                                boinc_sleep(update_sensors_interval - delta);
                        }


                        current_trickle_time = boinc_elapsed_time();
                }
                
                //std::cout << "Fake send trickle ! with interval:" << delta1 << std::endl;
                
                if (signaled) break;

                ostringstream trickle;
                Sensors::print_datapoints(trickle);
                
                if (trickle.tellp() > 0) {
                        /*
                                std::cout << "trickle content: " << trickle.str() << endl;
                                err = boinc_send_trickle_up((char*) "low-energy-boinc", (char*) trickle.str().data());
                                if (err) {
                                        std::cout << "boinc_send_trickle_up error: " << err << endl;
                                        boinc_finish(err);
                                }
                        */
                } else {
                        //if (debug()) {
                                std::cout << "trickle is empty..." << std::endl;
                                //}
                }
                

        } // end for(;;)
        
        this->finalize();
}

