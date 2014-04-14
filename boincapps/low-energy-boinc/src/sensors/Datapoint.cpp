#include <iomanip>

#ifdef _WIN32
#include <float.h>
//#include <cmath>
//#include <math.h>

//std::nan("string") is equivalent to std::strtod("NAN(string)", (char**)NULL).
//http://en.cppreference.com/w/c/numeric/math/nan

#define NAN std::strtod("NAN(string)", (char**)NULL)
#else
#include <cmath>
#define NAN nan("")
#endif // ifdef _WIN32

#include <sstream>
#include "Datapoint.hpp"

//using namespace std;

#ifdef _WIN32

#define isnan _isnan

static struct tm *
localtime_r (const time_t *timer, struct tm *result)
{
	localtime_s(result, timer);
	return result;
}

#endif // _WIN32

time_t Datapoint::get_current_time() {
        return time(NULL);
}

Datapoint::Datapoint(double value) {
        this->m_time = get_current_time();
        this->m_value = value;
}

Datapoint::Datapoint(time_t time, double value) {
        this->m_time = time;
        this->m_value = value;
}

std::ostream& operator<<(std::ostream& stream, const Datapoint& dp) {
	struct tm tm;
        localtime_r(&dp.m_time, &tm);

        stream << std::setw(4) << std::setfill('0') << (1900 + tm.tm_year)
           << '-' << std::setw(2) << std::setfill('0') << tm.tm_mon + 1
           << '-' << std::setw(2) << std::setfill('0') << tm.tm_mday
           << 'T' << std::setw(2) << std::setfill('0') << tm.tm_hour
           << ':' << std::setw(2) << std::setfill('0') << tm.tm_min
           << ':' << std::setw(2) << std::setfill('0') << tm.tm_sec
           << ',';

        if (isnan(dp.m_value)) {
                stream << "null";
                return stream;
        }

        long value = (long) dp.m_value;
        if (dp.m_value == (double) value) {
                // Integral value
                stream << value;
                return stream;
        }

        stream << dp.m_value;
		return stream;
}


DatapointV::DatapointV() {
        m_firstDatapoint = true;
}

void DatapointV::push_back(const Datapoint& datapoint) {
        if (m_firstDatapoint) {
                // Add a NaN at the beginning of a new data series.
                //vector<Datapoint>::push_back(Datapoint(datapoint.m_time - 1, nan("") ));
                std::vector<Datapoint>::push_back(Datapoint(datapoint.m_time - 1, NAN));
                m_firstDatapoint = false;
        }

        std::vector<Datapoint>::push_back(datapoint);
}

