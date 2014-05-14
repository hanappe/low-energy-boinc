#include <vector>
#include "Sensors.hpp"
#include "Sensor.hpp"
#include "BoincSensors.hpp"
#include "Wattsup.hpp"
#include "CpuLoad.hpp"
#include "LibSensors.hpp"


#ifdef _WIN32
	#include "ArduinoTemp.hpp"
	#include "BoincUserCpuLoad.hpp"
	#include "Ghost.hpp"
#else // Unix
	#include "PStates.hpp"
	#include "ACPI.hpp"
	#include "BoincCpuLoad.hpp"
	#include "UsersCpuLoad.hpp"
	#include "TEMPer.hpp"
#endif

using namespace std;

static const bool debug = false;
static vector<SensorManager*> managers;
static bool initilized = false;

void Sensors::initManagers() {

        if (!initilized) {
                managers.push_back(getBoincSensorsManager());
                managers.push_back(getCpuLoadManager());
                managers.push_back(getWattsupManager());

		#ifdef _WIN32
                managers.push_back(getArduinoTempManager());
                managers.push_back(getBoincUserCpuLoadManager());
                managers.push_back(getGhostManager());
		#else // Unix
                managers.push_back(getACPIManager());
                managers.push_back(getBoincCpuLoadManager());
                //managers.push_back(getLibSensorsManager());
                managers.push_back(getPStatesManager());
                managers.push_back(getUsersCpuLoadManager());
                managers.push_back(getTEMPerManager());
		
		#endif

                initilized = true;
        }
}
void Sensors::update() {
        
        if (initilized) {
                
                size_t nmanagers = managers.size();
                for (size_t i = 0; i < nmanagers; ++i) {
                        if (debug)
                                cout << managers[i]->m_name << ".update_sensors()" << endl;
                        managers[i]->update_sensors();
                }
                
        }
}

void Sensors::releaseManagers() {

        if (initilized) {
                size_t nmanagers = managers.size();
                for (size_t i = 0; i < nmanagers; ++i) {
                        delete managers[i];
                        managers[i] = 0;
                }
                managers.clear();
                initilized = false;
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

