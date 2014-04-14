#ifndef __Sensor_hpp__
#define __Sensor_hpp__

#include <string>
#include "Datapoint.hpp"

struct Sensor {

        virtual ~Sensor();
		friend std::ostream& operator<<(std::ostream& os, const Sensor& e);

		std::string m_name;
        std::string m_description;
        DatapointV m_datapoints;
};

typedef std::vector<Sensor*> SensorV;

#endif
