#include "UsersCpuLoad.hpp"

#include <iomanip>
#include <fstream>
#include <sstream>
#include <vector>
#include <unordered_map>

using namespace std;

static const string ERRORS[] = {
#define ERROR_NO_ERROR 0
        "no error",
#define ERROR_PROC_UNREADABLE 1
        "/proc unreadable",
#define ERROR_BAD_NUMBER_OF_CPUS 2
        "bad number of cpus",
#define ERROR_BAD_CLK_TCK 3
        "bad clock tick",
};

#ifdef _WIN32

#include "Wmi.h"

struct UsersCpuLoadSensor : Sensor {
        UsersCpuLoadSensor() {
                m_name = "userscpuload";
                m_description = "CPU load of logged user"; // Assuming there is just one user logged
        }
};

struct UsersCpuLoadManager : SensorManager {

		int m_error;
        bool m_error_reported;
        

        UsersCpuLoadSensor m_sensor;
        int m_update_period;
        time_t m_update_time;
        time_t m_record_time;
        //long m_ncpus;
        //long m_clk_tck;
        //uid_t m_boinc_user_id;
        //PidToStat m_stat_history;


        UsersCpuLoadManager() : SensorManager() {
                m_name = "BoincCpuLoadManager";
                m_error = 0;
                m_error_reported = false;
                m_update_period = 5;
                m_update_time = 0;
                m_record_time = 0;
        }

		

        void add_sensors(SensorV& sensors, ErrorV& errors) {
			if (m_error) {
				if (!m_error_reported) {
						errors.push_back(Error(__FILE__, m_error, ERRORS[m_error]));
						m_error_reported = true;
				}
				return;
            }

            update_sensors();
            sensors.push_back(&m_sensor);
		}

        void update_sensors() {
				if (m_error) return;

                time_t t = Datapoint::get_current_time();
                if (t < m_update_time + m_update_period) return;
                m_update_time = t;
                long rounded_t = (t / m_update_period) * m_update_period;

				const long long user_cpu_load = Wmi::GetInstance()->getUserCpuLoad();
				std::cout << "USER CPU LOAD: " << user_cpu_load << std::endl;

                if (m_record_time > 0) {
                        double boinc_cpu_load_ratio = (user_cpu_load / 100.0f);
                        m_sensor.m_datapoints.push_back(Datapoint(rounded_t, boinc_cpu_load_ratio));
                }

                m_record_time = t;
        }
};

#else // !_WIN32


#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <pwd.h>
#include <unistd.h>
#include <utmpx.h>
#include "ProcessStat.hpp"



typedef unordered_map<int, ProcessStat> PidToStat;
typedef unordered_map<int, bool> UserToLogged;

struct UsersCpuLoadSensor : Sensor {
        UsersCpuLoadSensor() {
                m_name = "userscpuload";
                m_description = "CPU load of logged users' processes";
        }
};

struct Directory {

        string m_path;
        DIR* m_dir;

        Directory(const string& path) {
                m_path = path;
                m_dir = 0;
                open();
        }

        ~Directory() {
                close();
        }

        void open() {
                m_dir = opendir(m_path.data());
        }

        void close() {
                if (m_dir != 0) {
                        closedir(m_dir);
                        m_dir = 0;
                }
        }

        bool is_open() {
                return m_dir != 0;
        }

        void get_contents(vector<string>& filenames) {
                if (!is_open()) return;
                while (true) {
                        struct dirent* entry = readdir(m_dir);
                        if (entry == 0) break;
                        filenames.push_back(entry->d_name);
                }
        }
};

struct UsersCpuLoadManager : SensorManager {

        int m_error;
        bool m_error_reported;
        UsersCpuLoadSensor m_sensor;
        int m_update_period;
        time_t m_update_time;
        time_t m_record_time;
        long m_ncpus;
        long m_clk_tck;
        PidToStat m_stat_history;
        UserToLogged m_user_logged;

        UsersCpuLoadManager() {
                m_name = "UsersCpuLoadManager";
                m_error = 0;
                m_error_reported = false;
                m_update_period = 5;
                m_update_time = 0;
                m_record_time = 0;

                m_ncpus = sysconf(_SC_NPROCESSORS_ONLN);
                if (m_ncpus < 0) {
                        m_error = ERROR_BAD_NUMBER_OF_CPUS;
                }

                m_clk_tck = sysconf(_SC_CLK_TCK);
                if (m_clk_tck < 0) {
                        m_error = ERROR_BAD_CLK_TCK;
                }
        }

        void add_sensors(SensorV& sensors, ErrorV& errors) {
                if (m_error) {
                        if (!m_error_reported) {
                                errors.push_back(Error(__FILE__, m_error, ERRORS[m_error]));
                                m_error_reported = true;
                        }
                        return;
                }

                update_sensors();
                sensors.push_back(&m_sensor);
        }

        void update_sensors() {
                if (m_error) return;

                time_t t = Datapoint::get_current_time();
                if (t < m_update_time + m_update_period) return;
                m_update_time = t;
                long rounded_t = (t / m_update_period) * m_update_period;

                // FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME FIXME
                // Short processes (< m_update_period seconds) will be completely ignored by this approach.

		// IDEA: if parent process accumulate the cpuload of their children, we may only need to consider
		// user processes which are the root of user process trees.

                {
                        m_user_logged.clear();
                        setutxent(); // rewind utmp read
                        while (true) {
                                struct utmpx* entry = getutxent();
                                if (entry == 0) break;
                                if (entry->ut_type != USER_PROCESS) continue;

                                struct passwd* pwd = getpwnam(entry->ut_user);
                                if (pwd == 0) continue;
                                m_user_logged[pwd->pw_uid] = true;
                        }
                }

                double users_utime = 0;
                double users_stime = 0;

                PidToStat m_stat_history_ = m_stat_history;
                m_stat_history.clear();

                if (m_user_logged.size() > 0) {
                        string proc = "/proc";
                        Directory dir(proc);
                        if (!dir.is_open()) {
                                m_error = ERROR_PROC_UNREADABLE;
                                return;
                        }

                        vector<string> filenames;
                        dir.get_contents(filenames);
                        dir.close();

                        for (size_t i = 0; i < filenames.size(); i++) {
                                string& filename = filenames[i];

                                bool is_integer = true;
                                for(string::const_iterator it = filename.begin();
                                    is_integer && it != filename.end(); it++)
                                        is_integer &= isdigit(*it);

                                if (!is_integer) continue;

                                ostringstream path;
                                path << proc << "/" << filename;

                                struct stat buf;
                                int res = lstat(path.str().data(), &buf);
                                if (res != 0) continue;

                                UserToLogged::const_iterator it0 = m_user_logged.find(buf.st_uid);
                                if (it0 == m_user_logged.end()) continue;

                                path << "/stat";

                                ifstream process_stat(path.str().data());
                                if (!process_stat.is_open()) {
                                        continue;
                                }

                                ProcessStat stat;
                                if (!stat.read(process_stat)) {
                                        continue;
                                }

                                PidToStat::const_iterator it = m_stat_history_.find(stat.pid);
                                if (it != m_stat_history_.end()) {
                                        ProcessStat stat_ = it->second;
                                        users_utime += (stat.utime - stat_.utime);
                                        users_stime += (stat.stime - stat_.stime);
                                }

                                m_stat_history[stat.pid] = stat;
                        }
                }

                if (m_record_time > 0) {
                        double users_time = (users_utime + users_stime) / ((double) m_clk_tck * (t - m_record_time) * m_ncpus);
                        m_sensor.m_datapoints.push_back(Datapoint(rounded_t, users_time));
                }

                m_record_time = t;
        }
};

#endif // _WIN32

static UsersCpuLoadManager * manager = 0;

SensorManager* getUsersCpuLoadManager() {

        if (!manager) {
                manager = new UsersCpuLoadManager;
        }
        return manager;
}

