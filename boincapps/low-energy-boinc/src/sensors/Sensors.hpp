#ifndef __Sensors_hpp__
#define __Sensors_hpp__

#include <iostream>
#include "Sensors.hpp"
#include "SensorManager.hpp"

struct Sensors {

		static void initManagers();
		static void update();
		static void releaseManagers();

		static void print_datapoints(std::ostream& s);
		static void add_sensor_manager(SensorManager* m);
};

#endif
