#ifndef __Sensors_hpp__
#define __Sensors_hpp__

#include <iostream>

//#include "SensorManager.hpp"

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


class SensorManager;

struct Sensors {

        
        static void initManagers();
        static void update();
        static void releaseManagers();
        
        static void print_datapoints(std::ostream& s);
        static void add_sensor_manager(SensorManager* m);
};

#endif
