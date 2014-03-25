#include "PStates.hpp"

using namespace std;

#ifdef _WIN32

struct PStatesManager : SensorManager {

        PStatesManager() {
                m_name = "PStatesManager";
        }

        void add_sensors(SensorV& sensors, ErrorV& errors) {
        }

        void update_sensors() {
        }
};

#else // !_WIN32

#include <iomanip>
#include <fstream>
#include <sstream>
#include <vector>
#include <unistd.h>

static const string ERRORS[] = {
#define ERROR_NO_ERROR 0
        "no error",
#define ERROR_BAD_NUMBER_OF_CPUS 1
        "bad number of cpus",
#define ERROR_CPUFREQ_STATS_UNREADABLE 2
        "cpufreq stats unreadable",
#define ERROR_HARDWARE_CHANGES 3
        "hardware changes",
};

struct PState : Sensor {

        int m_cpu;
        int m_frequency_index;
        int m_frequency;
        time_t m_steps_time;
        long m_steps;
        bool m_collect;

        PState(int cpu, int frequency_index, int frequency) {
                m_cpu = cpu;
                m_frequency_index = frequency_index;
                m_frequency = frequency;
                m_steps_time = 0;
                m_steps = -1;
                m_collect = false;

                ostringstream ss;
                ss << "cpu" << cpu << "pstate" << frequency_index;
                m_name = ss.str();

                ss.str("");
                ss << "frequency " << frequency;
                m_description = ss.str();
        }
};

struct PStatesManager : SensorManager {

        int m_error;
        bool m_error_reported;
        long m_ncpus;
        vector<PState*> m_sensors;
        int m_update_period;
        time_t m_update_time;

        PStatesManager() {
                m_name = "PStatesManager";
                m_error = 0;
                m_error_reported = false;
                m_ncpus = sysconf(_SC_NPROCESSORS_ONLN);
                if (m_ncpus < 0) {
                        m_error = ERROR_BAD_NUMBER_OF_CPUS;
                }
                m_update_period = 15;
                m_update_time = 0;
        }

        ~PStatesManager() {
                size_t nsensors = m_sensors.size();
                for (size_t i = 0; i < nsensors; i++) {
                        delete m_sensors[i];
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

                size_t nsensors = m_sensors.size();
                for (size_t i = 0; i < nsensors; i++) {
                        PState* pstate = m_sensors[i];
                        if (pstate->m_collect) {
                                sensors.push_back(pstate);
                        } else {
                                pstate->m_datapoints.clear();
                        }
                }
        }

        void update_sensors() {
                if (m_error) return;

                time_t t = Datapoint::get_current_time();
                if (t < m_update_time + m_update_period) return;
                m_update_time = t;
                long rounded_time = (m_update_time / m_update_period) * m_update_period;

                bool build = (m_sensors.size() == 0);
                size_t pstate_index = 0;

                for (long cpu = 0; cpu < m_ncpus; cpu++) {
                        ostringstream ss;
                        ss << "/sys/devices/system/cpu/cpu" << cpu
                           << "/cpufreq/stats/time_in_state";
                        string filename = ss.str();

                        ifstream time_in_state(filename.data());
                        if (!time_in_state.is_open()) {
                                m_error = ERROR_CPUFREQ_STATS_UNREADABLE;
                                return;
                        }

                        t = Datapoint::get_current_time();

                        for (int frequency_index = 0; true; frequency_index++) {
                                long frequency = -1, steps = -1;
                                time_in_state >> frequency >> steps;

                                if (frequency == -1 || steps == -1) {
                                        if (build && frequency_index > 1) {
                                                // Rename last pstate.
                                                ostringstream ss;
                                                ss << "cpu" << cpu << "pstateN";
                                                PState* pN = m_sensors.back();
                                                pN->m_name = ss.str();
                                                pN->m_collect = true; // Tag pN
                                        }

                                        break;
                                }

                                PState* pstate = 0;

                                if (build) {
                                        pstate = new PState(cpu, frequency_index, frequency);
                                        m_sensors.push_back(pstate);
                                        if (frequency_index == 0) pstate->m_collect = true; // Tag p0
                                }
                                else {
                                        pstate = m_sensors[pstate_index++];

                                        if (pstate->m_cpu != cpu) {
                                                m_error = ERROR_HARDWARE_CHANGES;
                                                return;
                                        }

                                        if (pstate->m_frequency_index != frequency_index) {
                                                m_error = ERROR_HARDWARE_CHANGES;
                                                return;
                                        }

                                        if (pstate->m_frequency != frequency) {
                                                m_error = ERROR_HARDWARE_CHANGES;
                                                return;
                                        }
                                }

                                if (pstate->m_steps == -1) {
                                        pstate->m_steps_time = t;
                                        pstate->m_steps = steps;
                                        continue;
                                }

                                double value = ((double) steps - pstate->m_steps) / (t - pstate->m_steps_time);
								value *= 0.01;
                                pstate->m_datapoints.push_back(Datapoint(rounded_time, value));
                                pstate->m_steps_time = t;
                                pstate->m_steps = steps;
                        }
                }
        }
};

#endif // _WIN32

static PStatesManager manager;

SensorManager* getPStatesManager() {
        return &manager;
}

