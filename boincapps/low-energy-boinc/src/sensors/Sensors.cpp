#include "Sensors.hpp"

#include <vector>

using namespace std;

static const bool debug = false;
static vector<SensorManager*> managers;

void Sensors::update() {
        size_t nmanagers = managers.size();
        for (size_t i = 0; i < nmanagers; ++i) {
                if (debug)
                        cout << managers[i]->m_name << ".update_sensors()" << endl;
                managers[i]->update_sensors();
        }
}

void Sensors::releaseManagers() {

        size_t nmanagers = managers.size();
        for (size_t i = 0; i < nmanagers; ++i) {
                delete managers[i];
                managers[i] = 0;
        }
        managers.clear();
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
        
        // Each SensorManager
        for (std::vector<SensorManager*>::iterator it = managers.begin(); it != managers.end(); ++it) {
                (*it)->add_sensors(sensors, errors);
        }
        
        
        // Each Sensor
        size_t total_datapoints = 0;
        for (SensorV::iterator it = sensors.begin(); it != sensors.end(); ++it) {
                total_datapoints = (*it)->m_datapoints.size();
        }
        
        
        if (total_datapoints > 0) {
                s << "<sensors>" << endl;
                
                size_t index = 0;
                for (SensorV::const_iterator it = sensors.begin(); it != sensors.end(); ++it) {
                        const Sensor* sensor = (*it);
                        if (sensor->m_datapoints.empty()) {
                                continue;
                        }

                        s << index++ << "," << (*sensor) << endl;
                }
                
                s << "</sensors>" << endl
                  << "<datapoints>" << endl;
                
                index = 0;
                for (SensorV::iterator it = sensors.begin(); it != sensors.end(); ++it) {
                        Sensor* sensor = (*it);
                        DatapointV& datapoints = sensor->m_datapoints;
                        
                        if (datapoints.empty()) {
                                continue;
                        }
			
                        for (DatapointV::iterator it = datapoints.begin(); it != datapoints.end(); ++it) {
                                s << index << "," << (*it) << endl; // Print Datapoint to stream
                        }
                        
                        datapoints.clear();
                        index++;
                }
                
                s << "</datapoints>" << endl;
        }
        
        if (!errors.empty()) {
                s << "<errors>" << endl;
                
                for (ErrorV::const_iterator it = errors.begin(); it != errors.end(); ++it) {
                        s << (*it) << endl; // Print Error to stream
                }
                
                s << "</errors>" << endl;
        }
}

