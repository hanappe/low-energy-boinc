#ifndef __Sensor_hpp__
#define __Sensor_hpp__

#include <string>
#include "Datapoint.hpp"

struct Sensor {
        std::string m_name;
        std::string m_description;
        DatapointV m_datapoints;
        virtual ~Sensor();
};

typedef std::vector<Sensor*> SensorV;

#endif
