#include "RaspberryApp.h"

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

#include "standalone_trickle_up.h"

using namespace std;

namespace {

bool signaled = false;
void signal_handler(int signum) {
        signaled = true;
}

} // end anonymous namespace

RaspberryApp::RaspberryApp() :
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

        if (initialize()) {
                mIsInitialized = true;
        }
}


RaspberryApp::~RaspberryApp() {
}

bool RaspberryApp::initialize() {
        if (debug()) {
                std::cout << "RaspberryApp::initialize()" << std::endl; 
        } 

        if (!mIsInitialized) {
                Sensors::add_sensor_manager(getBoincSensorsManager());
                Sensors::add_sensor_manager(getCpuLoadManager());
                Sensors::add_sensor_manager(getWattsupManager());
                
	#ifdef _WIN32
                Sensors::add_sensor_manager(getArduinoTempManager());
                Sensors::add_sensor_manager(getBoincUserCpuLoadManager());
	#else // Unix
                Sensors::add_sensor_manager(getPStatesManager());
                Sensors::add_sensor_manager(getUsersCpuLoadManager());
                Sensors::add_sensor_manager(getTEMPerManager());
        #endif
        
                return true;
        }

        return false;
}

void RaspberryApp::finalize() {

        if (debug()) {
                std::cout << "RaspberryApp::finalize()" << std::endl; 
        } 
        

        if (isInitialized()) {
                // Delete Sensors
                Sensors::releaseManagers();
        }
        
        // Quit Boinc

        boinc_finish(0);

        mIsInitialized = false;
}



void RaspberryApp::loop() {


// TEMP
//  TODO Remove this
        ostringstream oss_xml;
        oss_xml << "<sensors>\n"
            << "0,cpuload,Total CPU load\n"
            << "1,battery0,Battery\n"
            << "2,battery1,Battery\n"
            << "</sensors>\n"
            << "<datapoints>\n"
            << "0,2014-08-21T14:24:44,null\n"
            << "0,2014-08-21T14:24:45,0.132\n"
            << "0,2014-08-21T14:24:50,0.0235\n"
            << "0,2014-08-21T14:24:55,0.025\n"
            << "0,2014-08-21T14:25:00,0.0235\n"
            << "0,2014-08-21T14:25:05,0.0215\n"
            << "0,2014-08-21T14:25:10,0.0275\n"
            << "1,2014-08-21T14:24:39,null\n"
            << "1,2014-08-21T14:24:40,0\n"
            << "1,2014-08-21T14:24:45,0\n"
            << "1,2014-08-21T14:24:50,0\n"
            << "1,2014-08-21T14:24:55,0\n"
            << "1,2014-08-21T14:25:00,0\n"
            << "1,2014-08-21T14:25:05,0\n"
            << "1,2014-08-21T14:25:10,0\n"
            << "2,2014-08-21T14:24:39,null\n"
            << "2,2014-08-21T14:24:40,0\n"
            << "2,2014-08-21T14:24:45,0\n"
            << "2,2014-08-21T14:24:50,0\n"
            << "2,2014-08-21T14:24:55,0\n"
            << "2,2014-08-21T14:25:00,0\n"
            << "2,2014-08-21T14:25:05,0\n"
            << "2,2014-08-21T14:25:10,0\n"
            << "</datapoints>\n"
            << "<errors>\n"
            << "../sensors/BoincSensors.cpp,1,BOINC not running\n"
            << "../sensors/BoincCpuLoad.cpp,2,BOINC not running\n"
            << "</errors>\n";

        // TEMP

        //send_trickle_up((const char*) "low-energy-boinc", oss_xml.str().data());



        //return;


        if (debug()) {
                std::cout << "RaspberryApp::loop()" << std::endl; 
        }

        assert(isInitialized());

        int err = 0;

        static const double send_trickle_interval = 45; // seconds
        static const double update_sensors_interval = 0.5; // seconds

        double current_trickle_time = boinc_elapsed_time();

        for (;;) {

                if (signaled) break;
                
                
                //double delta1 = ;
                //trickle_time_end - trickle_time_begin;
                //trickle_interval_begin - _t0;


                double last_trickle_time = current_trickle_time;
                current_trickle_time = boinc_elapsed_time();
                
                if (debug()) {
                        std::cout << "current_trickle_time" << current_trickle_time << std::endl; 
                        //std::cout << "delta1: " << delta1 << std::endl; 
                }

                //return;

                while (current_trickle_time - last_trickle_time < send_trickle_interval) {

                        if (signaled) break;


                        if (debug()) {
                                std::cout << "RaspberryApp::loop() Sensors update()" << std::endl; 
                        } 

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

                        //boinc_sleep(0.1);
                        current_trickle_time = boinc_elapsed_time();
                }
                
                //std::cout << "Fake send trickle ! with interval:" << delta1 << std::endl;
                
                if (signaled) break;

                ostringstream trickle;
                Sensors::print_datapoints(trickle);
                
                if (trickle.tellp() > 0) {

                                std::cout << "trickle content: " << trickle.str() << endl;
                                //err = boinc_send_trickle_up((char*) "low-energy-boinc", (char*) trickle.str().data());
                                //err = send_trickle_up((char*) "low-energy-boinc", oss_xml.str().data());
                                //err = send_trickle_up((char*) "low-energy-boinc", (char*) trickle.str().data());
                                if (err) {
                                        std::cout << "send_trickle_up error: " << err << endl;
                                        boinc_finish(err);
                                }
                                
                }

        } // end for(;;)
        
        this->finalize();
}

