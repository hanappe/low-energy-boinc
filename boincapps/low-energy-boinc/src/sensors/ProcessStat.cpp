#include "ProcessStat.hpp"

bool ProcessStat::read(std::ifstream& s) {
        s >> pid;

        while (true) {
                int c = s.get();
                if (c == EOF) return false;
                if (c == '(') break;
        }

        comm += '(';
        while (true) {
                int c = s.get();
                if (c == EOF) return false;
                comm += c;
                if (c == ')') break;
        }

        s >> state >> ppid >> pgrp >> session
          >> tty_nr >> tpgid >> flags >> minflt
          >> cminflt >> majflt >> cmajflt >> utime
          >> stime >> cutime >> cstime >> priority
          >> nice >> num_threads >> itrealvalue >> starttime
          >> vsize >> rss >> rsslim >> startcode
          >> endcode >> startstack >> kstkesp >> kstkeip
          >> signal >> blocked >> sigignore >> sigcatch
          >> wchan >> nswap >> cnswap >> exit_signal
          >> processor >> rt_priority >> policy >> delayacct_blkio_ticks
          >> guest_time >> cguest_time;

        return !(s.rdstate() & std::ifstream::failbit);
}

void ProcessStat::print_to(std::ostream& s) {
        s << pid << ' ' << comm << ' ' << state << ' ' << ppid << ' '
          << pgrp << ' ' << session << ' ' << tty_nr << ' ' << tpgid << ' '
          << flags << ' ' << minflt << ' ' << cminflt << ' ' << majflt << ' '
          << cmajflt << ' ' << utime << ' ' << stime << ' ' << cutime << ' '
          << cstime << ' ' << priority << ' ' << nice << ' ' << num_threads << ' '
          << itrealvalue << ' ' << starttime << ' ' << vsize << ' ' << rss << ' '
          << rsslim << ' ' << startcode << ' ' << endcode << ' ' << startstack << ' '
          << kstkesp << ' ' << kstkeip << ' ' << signal << ' ' << blocked << ' '
          << sigignore << ' ' << sigcatch << ' ' << wchan << ' ' << nswap << ' '
          << cnswap << ' ' << exit_signal << ' ' << processor << ' ' << rt_priority << ' '
          << policy << ' ' << delayacct_blkio_ticks << ' ' << guest_time << ' ' << cguest_time;
}

