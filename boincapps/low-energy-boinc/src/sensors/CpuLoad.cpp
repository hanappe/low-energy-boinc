#include "CpuLoad.hpp"

#include <fstream>
#include <sstream>
#include <vector>

#ifdef _WIN32
	#include "Wmi.h"
#else //UNIX
	#include <iomanip>
	#include <unistd.h>
#endif

using namespace std;

static const bool debug = false;

static const string ERRORS[] = {
#define ERROR_NO_ERROR 0
        "no error",
#define ERROR_BAD_NUMBER_OF_CPUS 1
        "bad number of cpus",
#define ERROR_BAD_CLK_TCK 2
        "bad clock tick",
#define ERROR_PROC_STAT_UNREADABLE 3
        "proc stat unreadble",
#define ERROR_PROC_STAT_CONTENTS 4
        "bad proc stat contents",
};


#ifdef _WIN32


struct CpuStat {
        string cpu;
        long long load_percentage;

        bool read(istream& s) {
				s >> cpu
                >> load_percentage;
				return !(s.rdstate() & ifstream::failbit);
        }

        void print_to(ostream& s) {
                s << cpu << load_percentage;
        }
};

struct CpuLoadSensor : Sensor {
		CpuLoadSensor() {
                m_name = "cpuload";
                m_description = "Total CPU load";
        }	
};

struct CpuFrequencySensor : Sensor {
		CpuFrequencySensor() {
                m_name = "cpufrequency";
                m_description = "Cpu Frequency";
		}
};

// Send the cpu load in range [0.0f - 1.0f]

struct CpuLoadManager : SensorManager {

		int m_error;
        bool m_error_reported;
        long m_ncpus;
        long m_clk_tck;
        CpuLoadSensor m_machine;
        CpuFrequencySensor m_frequency;
		int m_update_period;
        time_t m_update_time;
        time_t m_record_time;

        CpuLoadManager() {
                m_name = "CpuLoadManager";
                m_error = 0;
                m_error_reported = false;
				

				m_ncpus = Wmi::GetInstance()->getNumCore();
				if (m_ncpus <= 0) {
                        m_error = ERROR_BAD_NUMBER_OF_CPUS;
                }
				
				m_clk_tck = Wmi::GetInstance()->getMaxClockSpeed();
				if (m_clk_tck <= 0) {
                        m_error = ERROR_BAD_CLK_TCK;
                }
				
				if (debug) {
					std::cout << "CpuLoadManager() NumberOfCores: " << m_ncpus << " MaxClockSpeed: " << m_clk_tck << std::endl; 
				}

                m_update_period = 5;
                m_update_time = 0;
                m_record_time = 0;
        }

		void add_sensors(SensorV& sensors, ErrorV& errors) {
                update_sensors();

                if (m_error) {
                        if (!m_error_reported) {
                                errors.push_back(Error(__FILE__, m_error, ERRORS[m_error]));
                                m_error_reported = true;
                        }
                        return;
                }

                sensors.push_back(&m_machine);
				sensors.push_back(&m_frequency);
        }

        void update_sensors() {
				
                if (m_error) return;

                time_t t = Datapoint::get_current_time();
				
                if (t < m_update_time + m_update_period) return;
                m_update_time = t;
                time_t rounded_t = (t / m_update_period) * m_update_period;
				
                if (m_record_time > 0) {

						long long cpu_load; 
						long long cpu_frequency;

						Wmi::GetInstance()->getCpuInfo(cpu_load, cpu_frequency);
						
						double cpu_load_ratio = 0;
						if (cpu_load > 0) {
							cpu_load_ratio = (static_cast<double>(cpu_load) / 100.0);
						}

                        m_machine.m_datapoints.push_back(Datapoint(rounded_t, cpu_load_ratio));
						m_frequency.m_datapoints.push_back(Datapoint(rounded_t, static_cast<double>(cpu_frequency)));
						
						if (debug) {
							std::cout << "CpuLoad: " << cpu_load << " " << cpu_load_ratio << std::endl;
							std::cout << "CpuFrequency: " << cpu_frequency << " " << std::endl;
						}
				}
				m_record_time = t;
        }
};

#else // !_WIN32



struct CpuStat {
        string cpu;
        long long user;
        long long nice;
        long long system;
        long long idle;
        long long iowait;
        long long irq;
        long long softirq;

        bool read(istream& s) {
                // Only on Linux >= 2.6. See 'man 5 proc'.
                s >> cpu
                  >> user
                  >> nice
                  >> system
                  >> idle
                  >> iowait
                  >> irq
                  >> softirq;

                return !(s.rdstate() & ifstream::failbit);
        }

        void print_to(ostream& s) {
                s << cpu << ' '
                  << user << ' '
                  << nice << ' '
                  << system << ' '
                  << idle;
        }
};

struct CpuLoadSensor : Sensor {
        CpuLoadSensor() {
                m_name = "cpuload";
                m_description = "Total CPU load";
        }
};

struct CpuLoadManager : SensorManager {

        int m_error;
        bool m_error_reported;
        long m_ncpus;
        long m_clk_tck;
        CpuLoadSensor m_machine;
        int m_update_period;
        time_t m_update_time;
        time_t m_record_time;
        CpuStat m_stat;

        CpuLoadManager() {
                m_name = "CpuLoadManager";
                m_error = 0;
                m_error_reported = false;

                m_ncpus = sysconf(_SC_NPROCESSORS_ONLN);
                if (m_ncpus < 0) {
                        m_error = ERROR_BAD_NUMBER_OF_CPUS;
                }

                m_clk_tck = sysconf(_SC_CLK_TCK);
                if (m_clk_tck < 0) {
                        m_error = ERROR_BAD_CLK_TCK;
                }

                m_update_period = 5;
                m_update_time = 0;
                m_record_time = 0;
        }

        void add_sensors(SensorV& sensors, ErrorV& errors) {
                update_sensors();

                if (m_error) {
                        if (!m_error_reported) {
                                errors.push_back(Error(__FILE__, m_error, ERRORS[m_error]));
                                m_error_reported = true;
                        }
                        return;
                }

                sensors.push_back(&m_machine);
        }

        void update_sensors() {
                if (m_error) return;

                time_t t = Datapoint::get_current_time();
                if (t < m_update_time + m_update_period) return;
                m_update_time = t;
                long rounded_t = (t / m_update_period) * m_update_period;

                ifstream proc_stat("/proc/stat");
                if (!proc_stat.is_open()) {
                        m_error = ERROR_PROC_STAT_UNREADABLE;
                        return;
                }

                string line;
                getline(proc_stat, line);
                if (proc_stat.rdstate() & ifstream::failbit) {
                        m_error = ERROR_PROC_STAT_CONTENTS;
                        return;
                }

                CpuStat stat;
                istringstream line_(line);
                if (!stat.read(line_)) {
                        m_error = ERROR_PROC_STAT_CONTENTS;
                        return;
                }

                if (m_record_time > 0) {
                        double idle = ((double) (stat.idle - m_stat.idle)
                                       + (stat.iowait - m_stat.iowait))
                                / ((double) m_clk_tck * (t - m_record_time) * m_ncpus);
                        if (idle > 1.0) idle = 1.0;
                        m_machine.m_datapoints.push_back(Datapoint(rounded_t, 1.0 - idle));
                }

                m_record_time = t;
                m_stat = stat;
        }
};

#endif // end ifdef _WIN32

static CpuLoadManager * manager;

SensorManager* getCpuLoadManager() {

		if (!manager) {
				manager = new CpuLoadManager;
		}

        return manager;
}

