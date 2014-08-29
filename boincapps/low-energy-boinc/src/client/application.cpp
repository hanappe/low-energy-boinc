#include <iostream>
#include <fstream>
#include <sstream>
#include <csignal>
#include <unistd.h>
#include "boinc_api.h"
#include "filesys.h"
#include "util.h"
#include "sensors/Sensors.hpp"

using namespace std;

static bool signaled = false;

static void signal_handler(int signum) {
        signaled = true;
}

static long get_milestone() {
	string str;
	int err = boinc_resolve_filename_s("milestone", str);
	if (err) {
		cerr << "boinc_resolve_filename_s: milestone: " << err << endl;
		boinc_finish(err);
	}

	ifstream ifs(str.data());
	if (!ifs.is_open()) {
		return 0;
	}

	long milestone = 0;
	ifs >> milestone;
	ifs.close();

	return milestone;
}

static void save_milestone(long milestone) {
        ofstream ofs("milestone_tmp");
        if (!ofs.is_open()) {
                cerr << "ofstream: can't open: milestone_tmp" << endl;
                boinc_finish(-1);
        }

        ofs << milestone << endl;
	ofs.close();

	string str;
	int err = boinc_resolve_filename_s("milestone", str);
	if (err) {
		cerr << "boinc_resolve_filename_s: milestone: " << err << endl;
		boinc_finish(err);
	}

	err = boinc_rename("milestone_tmp", str.c_str());
	if (err) {
		cerr << "boinc_rename: milestone_tmp milestone: " << err << endl;
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
                signal(SIGINT, signal_handler);
                signal(SIGHUP, signal_handler);
                signal(SIGQUIT, signal_handler);
                signal(SIGTERM, signal_handler);
                signal(SIGPWR, signal_handler);
        }

        string in_s;
        err = boinc_resolve_filename_s("in", in_s);
        if (err) {
                cerr << "boinc_resolve_filename_s: in: " << err << endl;
                boinc_finish(err);
        }

        long milestones = 0;
	double seconds = 0;
        ifstream in(in_s.data());
        if (!in.is_open()) {
                cerr << "ifstream: can't open: " << in_s << endl;
                //boinc_finish(-1);
                milestones = 4;
                seconds = 30;
        } else {
                in >> milestones >> seconds;
                in.close();
        }

        if (milestones <= 0) {
                cerr << "milestones: " << milestones << endl;
                boinc_finish(-1);
        }

        if (seconds <= 10) {
                cerr << "seconds: " << seconds << endl;
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

        for (long i = start_milestone; i < milestones; i++) {

		save_milestone(i);

                while ((seconds * start_milestone) + (t1 - t0) < (seconds * (i + 1))) {
                        if (signaled) break;
                        boinc_fraction_done(((seconds * start_milestone) + (t1 - t0)) / (seconds * milestones));
                        Sensors::update();

                        while (true) {
                                if (signaled) break;
                                double t2 = boinc_elapsed_time();
                                if (t2 - t1 >= 0.5) break;
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
			}
			else {
				err = boinc_send_trickle_up((char*) "low-energy-boinc", (char*) trickle.str().data());
				if (err) {
					cerr << "boinc_send_trickle_up error: " << err << endl;
					boinc_finish(err);
				}
			}
                }
        }

	ofstream out(out_s.data());
	if (!out.is_open()) {
		cerr << "ofstream: can't open: " << out_s << endl;
		boinc_finish(-1);
	}

	out << milestones << " " << seconds << endl;
	out.close();

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
