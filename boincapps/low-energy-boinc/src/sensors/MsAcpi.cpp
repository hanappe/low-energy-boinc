#include "MsAcpi.hpp"

#include <iostream>
#include <cstring>
#include <sstream>

#include "Wmi.h"

#ifdef _WIN32
 
using namespace std;

// To get Fan & Temperature, doesn't work on all PC, depends of drivers and motherboards (and seems to have so error with 64-bits architecture)

struct MsAcpiSensor : Sensor {

		 MsAcpiSensor() {
				cout <<  "MsAcpiSensor" << endl;

				// Temperature: check MSAcpi_ThermalZoneTemperature (WMI)
				// Fan: check Win32_Fan (WMI)
				
                m_description = "MsAcpiSensor (Fan & Temperature, doesn't work on all PC, depends of for now)";
        }

        ~MsAcpiSensor(){
			/*
				if (m_wmi) {
					delete m_wmi;
					m_wmi = 0;
				}
			*/
		}

        virtual void update(time_t t) {
				cout <<  "MsAcpiSensor::update" << endl;
				///Wmi::GetInstance()->getMsAcpi();
				LONG temp;
				HRESULT hr = Wmi::GetCpuTemperature(&temp);
				printf("hr=0x%08x temp=%i\n", hr, temp);
				//m_wmi->Wmi::printAllProcess();
		}
};

/*

struct BatterySensor : ACPISensor {

        int m_battery_index;

        BatterySensor(int battery_index) {
                m_battery_index = battery_index;
                stringstream ss;
                ss << "battery" << m_battery_index;
                m_name = ss.str();
                m_description = "Battery";
        }

        void update(time_t t) {
                int err = read_acpi_batt(m_battery_index);
                if (err) return;
                m_datapoints.push_back(Datapoint(t, batteries[m_battery_index].percentage));
        }

};

struct ACAdapterSensor : ACPISensor {

        global_t* m_global;

        ACAdapterSensor(global_t* global) {
                m_global = global;
                m_name = "ac-adapter";
                m_description = "AC adapter";
        }

        void update(time_t t) {
                read_acpi_acstate(m_global);
                m_datapoints.push_back(Datapoint(t, m_global->adapt.ac_state));
        }

};
*/

static const string ERRORS[] = {
#define ERROR_NO_ERROR 0
        "no error",
#define ERROR_NO_ACPI_SUPPORT 1
        "no ACPI support",
};

struct MsAcpiManager : SensorManager {

        int m_error;
        bool m_error_reported;
        int m_update_period;
        time_t m_time;
        //vector<ACPISensor*> m_sensors;

		MsAcpiSensor * m_sensor;

        //global_t m_acpi_global;
		

        MsAcpiManager() {
				cout <<  "MsAcpiManager" << endl;
                m_name = "MsAcpiManager";
                m_error = 0;
                m_error_reported = false;
                m_update_period = 5;
                m_time = 0;

				m_sensor = new MsAcpiSensor();

				/*
                int err = check_acpi_support();
                if (err) {
                        m_error = ERROR_NO_ACPI_SUPPORT;
                        return;
                }

                memset(&m_acpi_global, 0, sizeof(m_acpi_global));

                err = init_acpi_batt(&m_acpi_global);
                if (!err)
                        for (int i = 0; i < MAX_ITEMS && i < m_acpi_global.batt_count; i++)
                                m_sensors.push_back(new BatterySensor(i));

                err = init_acpi_acadapt(&m_acpi_global);
                if (!err)
                        m_sensors.push_back(new ACAdapterSensor(&m_acpi_global));
				*/
        }

        ~MsAcpiManager() {
                if (m_error) return;

                //size_t nsensors = m_sensors.size();
                //for (size_t i = 0; i < nsensors; i++) {
                //        delete m_sensors[i];
                //}

				if (m_sensor) {
					delete m_sensor;
					m_sensor = 0;
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
				

				/*
                size_t nsensors = m_sensors.size();
                for (size_t i = 0; i < nsensors; i++) {
                        sensors.push_back(m_sensors[i]);
                }
				*/

				sensors.push_back(m_sensor);
        }

        void update_sensors() {
                if (m_error) return;

                time_t t = Datapoint::get_current_time();
                if (t < m_time + m_update_period) return;
                m_time = t;

                long rounded_t = (t / m_update_period) * m_update_period;
				
				// Kev
                //size_t nsensors = m_sensors.size();
                //for (size_t i = 0; i < nsensors; i++) {
                //        m_sensors[i]->update(rounded_t);
                //}
				m_sensor->update(rounded_t);
        }
};

#endif // end ifdef _WIN32

static MsAcpiManager manager;

SensorManager* getMsAcpiManager() {
        return &manager;
}



