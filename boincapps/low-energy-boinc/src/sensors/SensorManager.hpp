#ifndef __SensorManager_hpp__
#define __SensorManager_hpp__

#include "Sensor.hpp"
#include "Error.hpp"

struct SensorManager {
        std::string m_name;

        virtual ~SensorManager();
		// Add SensorManager's sensors to sensors_out
        virtual void add_sensors(SensorV& sensors_out, ErrorV& errors) = 0;
        virtual void update_sensors() = 0;
        
        void fileLog(const std::string& msg) {
                
                std::ofstream f("sensor.log", std::ios_base::app );
                if (f) {
                        std::stringstream s;
                        f << "[";
                        Time::print_format(f, Time::get_current());
                        f << "]";
                        f << "[" << m_name << "] ";
                        f << msg;
                        f << std::endl;
                        f.close();
                }

        }
};

#endif
