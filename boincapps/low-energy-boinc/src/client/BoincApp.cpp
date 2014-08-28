#include "BoincApp.h"

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


long get_milestone() {

	string str;
	int err = boinc_resolve_filename_s("milestone", str);
	if (err) {
		std::cerr << "boinc_resolve_filename_s: milestone: " << err << endl;
		boinc_finish(err);
	}

	

	FILE * ifile = boinc_fopen(str.data(), "r");
	if (!ifile) {
		std::cerr << "get_milestone:can't open file: " << str << std::endl;
		return 0;
	}

	long milestone = 0;
	fscanf(ifile, "%d", &milestone);
	fclose(ifile);

	return milestone;
}

void save_milestone(long milestone) {

        FILE * ofile = boinc_fopen("milestone_tmp", "w");
        
        if (!ofile) {
                std::cerr << "FILE: can't open: milestone_tmp" << endl;
                boinc_finish(-1);
        }
        fprintf (ofile, "%d\n", milestone);
        fclose(ofile);
        
        string str;
        int err = boinc_resolve_filename_s("milestone", str);
        if (err) {
                std::cerr << "boinc_resolve_filename_s: milestone: " << err << endl;
                boinc_finish(err);
        }
        
        err = boinc_rename("milestone_tmp", str.c_str());
        if (err) {
                std::cerr << "boinc_rename: milestone_tmp milestone: " << err << endl;
                boinc_finish(err);
        }
}

} // end anonymous namespace

BoincApp::BoincApp() :
        App(),
        _out_s(),
        _milestones(-1),
        _seconds(-1),
        _t0(-1),
        _t1(-1),
        _start_milestone(-1)
{
        int err = boinc_init();
        if (err) {
                cout << "boinc_init: " << err << endl;
                boinc_finish(err);
        }

        _is_standalone = boinc_is_standalone();

        if (_is_standalone) {
                std::cerr << "boinc app is standalone!" << std::endl;
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


        string in_s;
        err = boinc_resolve_filename_s("in", in_s);
        if (err) {
                cerr << "boinc_resolve_filename_s: in: " << err << endl;
                if (debug()) std::cout << "boinc_resolve_filename_s: in: " << err << endl;
                boinc_finish(err);
        }

        _milestones = 0;
        _seconds = 0;
        
        FILE * fin = boinc_fopen(in_s.c_str(), "r");
        if (!fin) {
                std::cerr << "ifstream: can't open: " << in_s << endl;
		
                //boinc_finish(-1);
                _milestones = 4;
                _seconds = 30;
        } else {
                fscanf (fin, "%d", &_milestones);
                fscanf (fin, "%lf", &_seconds);
                fclose (fin);
        }
		
        if (_milestones <= 0) {
                std::cerr << "milestones: " << _milestones << endl;
                boinc_finish(-1);
        }

        if (_seconds <= 10) {
                std::cerr << "seconds: " << _seconds << endl;
                boinc_finish(-1);
        }

        //string out_s;
        err = boinc_resolve_filename_s("out", _out_s);
        if (err) {
                cerr << "boinc_resolve_filename_s: out: " << err << endl;
                boinc_finish(err);
        }

        // Boinc documentation: "to get elapsed time since start of episode"
        _t0 = boinc_elapsed_time(); 
        _t1 = _t0;
        _start_milestone = get_milestone();

        const double fraction_done_at_start = (_seconds * _start_milestone) / (_seconds * _milestones);
        // Boinc documentation: "the fraction_done argument is an estimate of the workunit fraction complete (from 0 to 1)"
        boinc_fraction_done(fraction_done_at_start);

        if (initialize()) {
                mIsInitialized = true;
        }
}


BoincApp::~BoincApp() {

}

bool BoincApp::initialize() {
        if (debug()) {
                std::cout << "BoincApp::initialize()" << std::endl; 
        } 

        if (!mIsInitialized) {
                Sensors::add_sensor_manager(getBoincSensorsManager());
                Sensors::add_sensor_manager(getCpuLoadManager());
                Sensors::add_sensor_manager(getWattsupManager());
                
	#ifdef _WIN32
                Sensors::add_sensor_manager(getArduinoTempManager());
                Sensors::add_sensor_manager(getBoincUserCpuLoadManager());
	#else // Unix
                Sensors::add_sensor_manager(getACPIManager());
                Sensors::add_sensor_manager(getBoincCpuLoadManager());
                //Sensors::add_sensor_manager(getLibSensorsManager());
                Sensors::add_sensor_manager(getPStatesManager());
                Sensors::add_sensor_manager(getUsersCpuLoadManager());
                Sensors::add_sensor_manager(getTEMPerManager());
        #endif
        
                return true;
        }

        return false;
}

void BoincApp::finalize() {

        if (debug()) {
                std::cout << "BoincApp::finalize()" << std::endl; 
        } 
        

        if (isInitialized()) {
                // Delete Sensors
                Sensors::releaseManagers();
        }
        // Save milestones

        FILE * fout = boinc_fopen(_out_s.c_str(), "w");
        
        if (!fout) {
                std::cerr << "ofstream out: can't open: " << _out_s << endl;
                boinc_finish(-1);
        }

        fprintf (fout, "%d %f\n", _milestones, _seconds);
        fclose(fout);
        
        // Quit Boinc

        boinc_fraction_done(1);
        boinc_finish(0);

        mIsInitialized = false;
}

void BoincApp::loop() {

        if (debug()) {
                std::cout << "BoincApp::loop()" << std::endl; 
        }

        assert(isInitialized());
        assert(!_out_s.empty());
        //assert(_is_standalone != -1);
        assert(_milestones != -1);
        assert(_seconds != -1);

        assert(_t0 != -1);
        assert(_t1 != -1);
        assert(_start_milestone != -1);


        int err = 0;

        for (long i = _start_milestone; i < _milestones; i++) {
                
                if (debug()) {
                        std::cout << "i: " << i << std::endl;
                        std::cout << "start_milestone: " << _start_milestone << " milestones: " << _milestones << std::endl;	
                }
                
                save_milestone(i);
                
                while ( ((_seconds * _start_milestone) + (_t1 - _t0)) < (_seconds * (i + 1))) {
                        if (signaled) break;
                        
                        const double fraction_done = ((_seconds * _start_milestone) + (_t1 - _t0))  / (_seconds * _milestones);
                        //const double fraction_done_bis = start_milestone + ( (t1 - t0) / (seconds * milestones));
                        
                        boinc_fraction_done(fraction_done);
                        
                        if (debug()) {
                                std::cout << "fraction done: " << fraction_done << " (t1 - t0): " << (_t1 - _t0) << " t1: " << _t1 << " t0: " << _t0 << std::endl;
                                //std::cout << "fraction done bis: " << fraction_done_bis << std::endl;
                        }
                        
                        Sensors::update();

                        while (true) {
                                if (signaled) break;

                                double t2 = boinc_elapsed_time();
                                if (t2 - _t1 >= 0.5) {
                                        break;
                                }
                                boinc_sleep(0.5 - (t2 - _t1));
                        }
                        
                        _t1 = boinc_elapsed_time();
                }
                
                if (signaled) break;
                
                ostringstream trickle;
                Sensors::print_datapoints(trickle);
                

                if (trickle.tellp() > 0) {
                        if (debug()) {
                                std::cout << "Debug: trickle content: " << trickle.str() << endl;
                        } else {
                                std::cout << "trickle content: " << trickle.str() << endl;
                                err = boinc_send_trickle_up((char*) "low-energy-boinc", (char*) trickle.str().data());
                                if (err) {
                                        std::cout << "boinc_send_trickle_up error: " << err << endl;
                                        boinc_finish(err);
                                }
                        }
                } else {
                        //if (debug()) {
                                std::cout << "trickle is empty..." << std::endl;
                                //}
                }
        }
        
        this->finalize();
}

