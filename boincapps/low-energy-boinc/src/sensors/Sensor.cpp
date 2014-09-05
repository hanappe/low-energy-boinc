#include "Sensor.hpp"

Sensor::~Sensor() {
}

void Sensor::fileLog(const std::string& msg) {
	std::ofstream f("sensor.txt", std::ios_base::app );
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

std::ostream& operator<<(std::ostream& os, const Sensor& s)
{
	os << s.m_name << ","
		<< s.m_description;
	return os;
}
