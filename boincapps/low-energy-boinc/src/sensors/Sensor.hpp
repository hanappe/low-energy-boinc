#ifndef __Sensor_hpp__
#define __Sensor_hpp__

#include <string>
#include "Datapoint.hpp"
#include <fstream>
#include <sstream>

struct Sensor {

        std::string m_name;
        std::string m_description;
        DatapointV m_datapoints;
        
        virtual ~Sensor();
	
        void fileLog(const std::string& msg);
	
        friend std::ostream& operator<<(std::ostream& os, const Sensor& e);
};

typedef std::vector<Sensor*> SensorV;

#endif
