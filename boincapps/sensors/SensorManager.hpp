#ifndef __SensorManager_hpp__
#define __SensorManager_hpp__

#include "Sensor.hpp"
#include "Error.hpp"

struct SensorManager {
        std::string m_name;

        virtual ~SensorManager();
        virtual void add_sensors(SensorV& sensors, ErrorV& errors) = 0;
        virtual void update_sensors() = 0;
};

#endif
