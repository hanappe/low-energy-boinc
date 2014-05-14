#include <iostream>
#include <fstream>
#include <sstream>
#include <csignal>

#include "boinc_api.h"
#include "filesys.h"
#include "util.h"
#include "Sensors.hpp"

#ifdef _WIN32
	#include "win_util.h"
#else // Unix
	#include <unistd.h>
#endif

using namespace std;

static const bool debug = true;
static bool signaled = false;

static void signal_handler(int signum) {
        signaled = true;
}

static long get_milestone() {

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

static void save_milestone(long milestone) {

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



int main(int, char**) {

        int err = boinc_init();
        if (err) {
                cerr << "boinc_init: " << err << endl;
                boinc_finish(err);
        }

        int standalone = boinc_is_standalone();
        if (standalone) {
                std::cout << "boinc app is standalone!" << std::endl;
        } else {
                std::cout << "not standalone ! \o/" << std::endl;
        }



	#ifdef _WIN32
	#else // Unix
        if (standalone) {
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
                if (debug) std::cout << "boinc_resolve_filename_s: in: " << err << endl;
                boinc_finish(err);
        }

        long milestones = 0;
        double seconds = 0;
        
        FILE * fin = boinc_fopen(in_s.c_str(), "r");
        if (!fin) {
                std::cerr << "ifstream: can't open: " << in_s << endl;
		
                //boinc_finish(-1);
                milestones = 4;
                seconds = 30;
        } else {
                fscanf (fin, "%d", &milestones);
                fscanf (fin, "%lf", &seconds);
                fclose (fin);
        }
		
        if (milestones <= 0) {
                std::cerr << "milestones: " << milestones << endl;
                boinc_finish(-1);
        }

        if (seconds <= 10) {
                std::cerr << "seconds: " << seconds << endl;
                boinc_finish(-1);
        }

        string out_s;
        err = boinc_resolve_filename_s("out", out_s);
        if (err) {
                cerr << "boinc_resolve_filename_s: out: " << err << endl;
                boinc_finish(err);
        }

        double t0 = boinc_elapsed_time();
        double t1 = t0;
        long start_milestone = get_milestone();
        boinc_fraction_done((seconds * start_milestone) / (seconds * milestones));
        Sensors::initManagers();

        for (long i = start_milestone; i < milestones; i++) {
                if (debug) std::cout << "in for loop milestone" << std::endl;
                
                save_milestone(i);
                
                while ((seconds * start_milestone) + (t1 - t0) < (seconds * (i + 1))) {
                        if (signaled) break;

                        const double fraction_done = (seconds * start_milestone) + (t1 - t0) / (seconds * milestones);
                        boinc_fraction_done(fraction_done);
                        
                        if (debug) std::cout << "fraction done: " << fraction_done << " milestones: " << (t1 - t0) << " t1: " << t1 << " t0: " << t0 << std::endl;
                        Sensors::update();

                        while (true) {
                                if (signaled) {
                                        break;
                                }
                                double t2 = boinc_elapsed_time();
                                if (t2 - t1 >= 0.5) {
                                        break;
                                }
                                boinc_sleep(0.5 - (t2 - t1));
                        }
                        
                        t1 = boinc_elapsed_time();
                }
                
                if (signaled) break;
                
                ostringstream trickle;
                Sensors::print_datapoints(trickle);
                
                if (trickle.tellp() > 0) {
                        if (standalone) {
                                cout << trickle.str() << endl;
                        } else {
                                err = boinc_send_trickle_up((char*) "low-energy-boinc", (char*) trickle.str().data());
                                if (err) {
                                        std::cerr << "boinc_send_trickle_up error: " << err << endl;
                                        boinc_finish(err);
                                }
                        }
                }
        }

        Sensors::releaseManagers();
        
        /*
          
        // Temp dead code, often use to perfom some tests
        
        const int mymilestones = 6;
        const int mytime = 20;// 2 * 60; //in sec
        const double mysleeptime = mytime / double(mymilestones);
        const double fractioninc = 100.0 / double(mymilestones);
        for (int i = 0; i < mymilestones-1; i++) {
        
        boinc_sleep(mysleeptime);
        const double fraction_done = (fractioninc * (i+1)) / 100.0;
        //std::cout << "test: " << boinc_fraction_done(fraction_done) << std::endl;
        boinc_fraction_done(fraction_done);
        if (debug) std::cout << "fraction done: " << fraction_done << " i: " << i << " mysleeptime: " << mysleeptime << " fractioninc: " << fractioninc << std::endl;
        
        }*/
        
        FILE * fout = boinc_fopen(out_s.c_str(), "w");
        
        if (!fout) {
                std::cerr << "ofstream out: can't open: " << out_s << endl;
                if (debug) std::cout << "ofstream out: can't open: " << out_s << endl;
                boinc_finish(-1);
        }

        fprintf (fout, "%d %f\n", milestones, seconds);
        fclose(fout);
        
        boinc_fraction_done(1);
        boinc_finish(0);
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
