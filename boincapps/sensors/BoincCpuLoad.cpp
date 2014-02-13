#include "BoincCpuLoad.hpp"

#ifdef _WIN32

struct BoincCpuLoadManager : SensorManager {

        BoincCpuLoadManager() {
                m_name = "BoincCpuLoadManager";
        }

        void add_sensors(SensorV& sensors) {
        }

        void update_sensors() {
        }
};

#else // !_WIN32

#include <iomanip>
#include <fstream>
#include <sstream>
#include <vector>
#include <unordered_map>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <pwd.h>
#include <unistd.h>
#include "ProcessStat.hpp"
#include "gui_rpc_client.h"

using namespace std;

static const string ERRORS[] = {
#define ERROR_NO_ERROR 0
        "no error",
#define ERROR_PROC_UNREADABLE 1
        "/proc unreadble",
#define ERROR_BOINC_NOT_RUNNING 2
        "BOINC not running",
#define ERROR_NO_BOINC_USER 3
        "no boinc user",
#define ERROR_BAD_CLK_TCK 4
        "bad clock tick",
#define ERROR_BAD_NUMBER_OF_CPUS 5
        "bad number of cpus",
};

typedef unordered_map<int, ProcessStat> PidToStat;

struct BoincCpuLoadSensor : Sensor {
        BoincCpuLoadSensor() {
                m_name = "boinccpuload";
                m_description = "CPU load of all BOINC processes";
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

struct BoincCpuLoadManager : SensorManager {

        int m_error;
        bool m_error_reported;
        BoincCpuLoadSensor m_sensor;
        int m_update_period;
        time_t m_update_time;
        time_t m_record_time;
        long m_ncpus;
        long m_clk_tck;
        uid_t m_boinc_user_id;
        PidToStat m_stat_history;

        BoincCpuLoadManager() {
                m_name = "BoincCpuLoadManager";
                m_error = 0;
                m_error_reported = false;
                m_update_period = 5;
                m_update_time = 0;
                m_record_time = 0;
                m_boinc_user_id = -1;

                RPC_CLIENT rpc;
                if (rpc.init("localhost") != 0) {
                        m_error = ERROR_BOINC_NOT_RUNNING;
                        return;
                }

                struct passwd* pwd = getpwnam("boinc");
                if (pwd == 0) {
                        m_error = ERROR_NO_BOINC_USER;
                        return;
                }

                m_boinc_user_id = pwd->pw_uid;

                m_ncpus = sysconf(_SC_NPROCESSORS_ONLN);
                if (m_ncpus < 0) {
                        m_error = ERROR_BAD_NUMBER_OF_CPUS;
                }

                m_clk_tck = sysconf(_SC_CLK_TCK);
                if (m_clk_tck < 0) {
                        m_error = ERROR_BAD_CLK_TCK;
                        return;
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

                string proc = "/proc";
                Directory dir(proc);
                if (!dir.is_open()) {
                        m_error = ERROR_PROC_UNREADABLE;
                        return;
                }

                vector<string> filenames;
                dir.get_contents(filenames);
                dir.close();

                PidToStat m_stat_history_ = m_stat_history;
                m_stat_history.clear();

                double boinc_utime = 0;
                double boinc_stime = 0;

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

                        if (buf.st_uid != m_boinc_user_id) continue;

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
                                boinc_utime += (stat.utime - stat_.utime);
                                boinc_stime += (stat.stime - stat_.stime);
                        }

                        m_stat_history[stat.pid] = stat;
                }

                if (m_record_time > 0) {
                        double boinc_time = (boinc_utime + boinc_stime) / ((double) m_clk_tck * (t - m_record_time) * m_ncpus);
                        m_sensor.m_datapoints.push_back(Datapoint(rounded_t, boinc_time));
                }

                m_record_time = t;
        }
};

#endif // _WIN32

static BoincCpuLoadManager manager;

SensorManager* getBoincCpuLoadManager() {
        return &manager;
}

