#include "Sensor.hpp"


Sensor::~Sensor() {
}

std::ostream& operator<<(std::ostream& os, const Sensor& s)
{
	os << s.m_name << ","
		<< s.m_description;
	return os;
}