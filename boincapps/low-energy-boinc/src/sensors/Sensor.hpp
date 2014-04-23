#ifndef __Sensor_hpp__
#define __Sensor_hpp__

#include <string>
#include "Datapoint.hpp"
#include <fstream>
#include <sstream>

struct Sensor {

        virtual ~Sensor();
        friend std::ostream& operator<<(std::ostream& os, const Sensor& e);
        
        std::string m_name;
        std::string m_description;
        DatapointV m_datapoints;

		
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

typedef std::vector<Sensor*> SensorV;

#endif
