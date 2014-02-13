#ifndef __ProcessStat_hpp__
#define __ProcessStat_hpp__

#include <iostream>
#include <fstream>
#include <string>

struct ProcessStat {
        // Only on Linux >= 2.6.24
        int pid;
        std::string comm;
        char state;
        int ppid;
        int pgrp;
        int session;
        int tty_nr;
        int tpgid;
        unsigned long flags;
        unsigned long minflt;
        unsigned long cminflt;
        unsigned long majflt;
        unsigned long cmajflt;
        unsigned long utime;
        unsigned long stime;
        long cutime;
        long cstime;
        long priority;
        long nice;
        long num_threads;
        long itrealvalue;
        unsigned long long starttime;
        unsigned long vsize;
        long rss;
        unsigned long rsslim;
        unsigned long startcode;
        unsigned long endcode;
        unsigned long startstack;
        unsigned long kstkesp;
        unsigned long kstkeip;
        unsigned long signal;
        unsigned long blocked;
        unsigned long sigignore;
        unsigned long sigcatch;
        unsigned long wchan;
        unsigned long nswap;
        unsigned long cnswap;
        int exit_signal;
        int processor;
        unsigned int rt_priority;
        unsigned int policy;
        unsigned long long delayacct_blkio_ticks;
        unsigned long guest_time;
        long cguest_time;

        bool read(std::ifstream& s);
        void print_to(std::ostream& s);
};

#endif
