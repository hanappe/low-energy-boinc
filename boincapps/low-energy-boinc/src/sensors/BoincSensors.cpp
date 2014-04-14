#include "BoincSensors.hpp"

#include <vector>
#include <unordered_map>
#include "boinc_api.h"
#include "gui_rpc_client.h"

using namespace std;



struct ResultSensor : Sensor {
        string m_project_url;
        string m_result_name;
        bool m_updated;

        ResultSensor(RESULT* result) {
                // NOTE: do not keep the result pointer. It will
                // become obsolete after a while.
                m_project_url = result->project_url;
                m_result_name = result->name;
                m_updated = false;

                m_name = toOSDName(m_project_url + m_result_name);
                m_description = "workunit";
        }

        void update(time_t t, RESULT* result) {
                if (m_updated) return;
                if (result->fraction_done > 0) m_datapoints.push_back(Datapoint(t, result->fraction_done));
                m_updated = true;
        }

        static string toOSDName(const string& s) {
                string result;
                size_t start_i = s.find("http://") != 0 ? 0 : 7;
                for (size_t i = start_i; i < s.length(); i++) {
                        result += isalnum(s[i]) ? s[i] : '_';
                }
                return result;
        }

};

typedef unordered_map<string, ResultSensor*> StringToSensor;

static const string ERRORS[] = {
#define ERROR_NO_ERROR
        "no error",
#define ERROR_BOINC_NOT_RUNNING 1
        "BOINC not running",
#define ERROR_NO_BOINC_STATE 2
        "no BOINC state",
};

struct BoincSensorsManager : SensorManager {

        int m_error;
        bool m_error_reported;
        RPC_CLIENT m_boinc_rpc;
        int m_update_period;
        time_t m_update_time;
        vector<ResultSensor*> m_sensors;
        StringToSensor m_stringToSensor;

        BoincSensorsManager() {
                m_name = "BoincSensorsManager";
                m_error = 0;
                m_error_reported = false;
                m_update_period = 60;
                m_update_time = 0;

                if (m_boinc_rpc.init("localhost") != 0) {
                        m_error = ERROR_BOINC_NOT_RUNNING;
                        return;
                }
        }

        ~BoincSensorsManager() {
                for (size_t i = 0; i < m_sensors.size(); i++)
                        delete m_sensors[i];
        }

        void add_sensors(SensorV& sensors_out, ErrorV& errors) {

                if (m_error) {
                        if (!m_error_reported) {
                                errors.push_back(Error(__FILE__, m_error, ERRORS[m_error]));
                                m_error_reported = true;
                        }
                        m_error = 0;
                } else {
					for (size_t i = 0; i < m_sensors.size(); i++)
							sensors_out.push_back(m_sensors[i]);
				}
        }

        void update_sensors() {
                if (m_error) return;

                time_t t = Datapoint::get_current_time();
                if (t < m_update_time + m_update_period) return;
                m_update_time = t;
                long rounded_t = (t / m_update_period) * m_update_period;

                CC_STATE cc_state;
                if (m_boinc_rpc.get_state(cc_state) != 0) {
                        m_error = ERROR_NO_BOINC_STATE;
                        return;
                }

                // Manage known sensors.
                for (size_t i = 0; i < m_sensors.size(); i++) {
                        ResultSensor* sensor = m_sensors[i];
                        sensor->m_updated = false; // clear updated state
                        RESULT* result = cc_state.lookup_result(sensor->m_project_url.data(), sensor->m_result_name.data());
                        if (result != 0) sensor->update(rounded_t, result);
                }

                // Check for new results.
                for (size_t i = 0; i < cc_state.results.size(); i++) {
                        RESULT* result = cc_state.results[i];
                        if (result == 0) continue;

                        ResultSensor key(result);
                        StringToSensor::const_iterator it = m_stringToSensor.find(key.m_name);
                        ResultSensor* sensor = 0;

                        if (it == m_stringToSensor.end()) {
                                // TODO: we don't need to track results for lowenergyboinc.
                                sensor = new ResultSensor(result);
                                m_stringToSensor[sensor->m_name] = sensor;
                                m_sensors.push_back(sensor);
                        }
                        else {
                                sensor = it->second;
                        }

                        sensor->update(rounded_t, result);
                }

                // TODO: check status of the result. Register it only if it is *running*.

                // Update sensor vector.
                vector<ResultSensor*> newSensors;
                for (size_t i = 0; i < m_sensors.size(); i++) {
                        ResultSensor* sensor = m_sensors[i];
                        if (!sensor->m_updated) {
                                // Result has disappeared. Delete sensor.
                                m_stringToSensor.erase(sensor->m_name);
                                delete sensor;
                                continue;
                        }

                        newSensors.push_back(sensor);
                }

                m_sensors = newSensors;
        }
};

/*
#endif // _WIN32
*/
static BoincSensorsManager manager;

SensorManager* getBoincSensorsManager() {
        return &manager;
}

