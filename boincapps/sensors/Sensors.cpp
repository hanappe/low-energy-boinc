#include <vector>
#include "Sensors.hpp"
#include "Sensor.hpp"
#include "PStates.hpp"
#include "BoincSensors.hpp"
#include "Wattsup.hpp"
#include "TEMPer.hpp"
#include "BoincSensors.hpp"
#include "CpuLoad.hpp"
#include "BoincCpuLoad.hpp"
#include "UsersCpuLoad.hpp"
#include "LibSensors.hpp"
#include "ACPI.hpp"

using namespace std;

static const bool debug = false;
static vector<SensorManager*> managers;

void Sensors::update() {
        if (managers.size() == 0) {
                managers.push_back(getWattsupManager());
                managers.push_back(getTEMPerManager());
                managers.push_back(getPStatesManager());
                managers.push_back(getBoincSensorsManager());
                managers.push_back(getBoincCpuLoadManager());
                managers.push_back(getUsersCpuLoadManager());
                managers.push_back(getCpuLoadManager());
                managers.push_back(getLibSensorsManager());
                managers.push_back(getACPIManager());
        }

        size_t nmanagers = managers.size();
        for (size_t i = 0; i < nmanagers; i++) {
                if (debug) cout << managers[i]->m_name << ".update_sensors()" << endl;
                managers[i]->update_sensors();
        }
}

void Sensors::add_sensor_manager(SensorManager* m) {
        size_t nmanagers = managers.size();
        for (size_t i = 0; i < nmanagers; i++)
                if (managers[i] == m) return;
        managers.push_back(m);
}

void Sensors::print_datapoints(ostream& s) {
        SensorV sensors;
        ErrorV errors;
        size_t nmanagers = managers.size();
        for (size_t i = 0; i < nmanagers; i++) {
                managers[i]->add_sensors(sensors, errors);
        }

        size_t nsensors = sensors.size();
        size_t ndatapoints = 0;
        for (size_t i = 0; i < nsensors; i++) {
                Sensor* sensor = sensors[i];
                ndatapoints += sensor->m_datapoints.size();
        }

        if (ndatapoints > 0) {
                s << "<sensors>" << endl;

                size_t index = 0;
                for (size_t i = 0; i < nsensors; i++) {
                        Sensor* sensor = sensors[i];
                        if (sensor->m_datapoints.size() == 0) {
                                continue;
                        }

                        s << index++ << ","
                          << sensor->m_name << ","
                          << sensor->m_description << endl;
                }

                s << "</sensors>" << endl
                  << "<datapoints>" << endl;

                index = 0;

                for (size_t i = 0; i < nsensors; i++) {
                        Sensor* sensor = sensors[i];
                        size_t ndatapoints_ = sensor->m_datapoints.size();
                        if (ndatapoints_ == 0) {
                                continue;
                        }

                        for (size_t j = 0; j < ndatapoints_; j++) {
                                s << index << ",";
                                sensor->m_datapoints[j].print_to(s);
                                s << endl;
                        }

                        sensor->m_datapoints.clear();
                        index++;
                }

                s << "</datapoints>" << endl;
        }

        size_t nerrors = errors.size();

        if (nerrors > 0) {
                s << "<errors>" << endl;

                for (size_t i = 0; i < nerrors; i++) {
                        const Error& error = errors[i];
                        s << error.m_module << ","
                          << error.m_code << ","
                          << error.m_text << endl;
                }

                s << "</errors>" << endl;
        }
}

