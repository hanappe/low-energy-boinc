#include "BoincUserCpuLoad.hpp"

#include <iomanip>
#include <fstream>
#include <sstream>
#include <vector>
#include <unordered_map>

using namespace std;

static const bool debug = false;

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

#ifdef _WIN32

#include "Wmi.h"

struct BoincCpuLoadSensor : Sensor {
        BoincCpuLoadSensor() {
                m_name = "boinccpuload";
                m_description = "CPU load of all BOINC processes";
        }
};

struct UserCpuLoadSensor : Sensor {
        UserCpuLoadSensor() {
                m_name = "usercpuload";
                m_description = "CPU load of all USER processes";
        }
};

struct BoincUserCpuLoadManager : SensorManager {

		int m_error;
        bool m_error_reported;

        BoincCpuLoadSensor m_boinc_sensor;
		UserCpuLoadSensor m_user_sensor;
        int m_update_period;
        time_t m_update_time;
        time_t m_record_time;

        BoincUserCpuLoadManager() : SensorManager() {
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
				sensors.push_back(&m_boinc_sensor);
				sensors.push_back(&m_user_sensor);
		}

        void update_sensors() {
				if (m_error) return;

                time_t t = Datapoint::get_current_time();
                if (t < m_update_time + m_update_period) return;
                m_update_time = t;
                long rounded_t = (t / m_update_period) * m_update_period;
				
				long long boinc_cpu_load;
				long long user_cpu_load;

				Wmi::GetInstance()->getBoincAndUserCpuLoad(boinc_cpu_load, user_cpu_load);

                if (m_record_time > 0) {
						double boinc_cpu_load_ratio = (boinc_cpu_load / 100.0);
						double user_cpu_load_ratio = (user_cpu_load / 100.0);
						m_boinc_sensor.m_datapoints.push_back(Datapoint(rounded_t, boinc_cpu_load_ratio));
						m_user_sensor.m_datapoints.push_back(Datapoint(rounded_t, user_cpu_load_ratio));
						if (debug) {
								std::cout << "BoincCpuLoad: " << boinc_cpu_load_ratio << std::endl;
								std::cout << "UserCpuLoad: " << user_cpu_load_ratio << std::endl;
						}
				}

                m_record_time = t;
        }
};

#endif // _WIN32

static BoincUserCpuLoadManager manager;

SensorManager* getBoincUserCpuLoadManager() {
        return &manager;
}

