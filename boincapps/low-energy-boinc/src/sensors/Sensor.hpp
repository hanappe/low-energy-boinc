#ifndef __Sensor_hpp__
#define __Sensor_hpp__

#include <string>
#include "Datapoint.hpp"
#include <fstream>

struct Logger : public std::ostringstream {
	std::ofstream file;
	Logger(const std::string& name) : file("sensor.log", std::ios_base::app ) {
		*this << "[";
		Time::print_format(*this, Time::get_current());
		*this << "]";
		*this << "[" << name << "] ";
	}

	~Logger() {
		writeToFile(str());
	}
			
	void writeToFile(const std::string& msg) {
		if (file) {
			file << msg;
		}
	}
};

struct Sensor {

        virtual ~Sensor();
		friend std::ostream& operator<<(std::ostream& os, const Sensor& e);

		std::string m_name;
        std::string m_description;
        DatapointV m_datapoints;

		

		
		inline Logger FileLog() { return Logger(m_name); }
};

typedef std::vector<Sensor*> SensorV;

#endif
