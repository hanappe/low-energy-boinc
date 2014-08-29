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
//#define localtime localtime_s

static struct tm *
localtime_r (const time_t *timer, struct tm *result)
{
  struct tm *local_result;
  local_result = localtime (timer);

  if (local_result == NULL || result == NULL)
    return NULL;

  std::memcpy (result, local_result, sizeof (result));
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

void Datapoint::print_to(std::ostream& st) const {
        struct tm tm;
        localtime_r(&m_time, &tm);

        st << std::setw(4) << std::setfill('0') << (1900 + tm.tm_year)
           << '-' << std::setw(2) << std::setfill('0') << tm.tm_mon + 1
           << '-' << std::setw(2) << std::setfill('0') << tm.tm_mday
           << 'T' << std::setw(2) << std::setfill('0') << tm.tm_hour
           << ':' << std::setw(2) << std::setfill('0') << tm.tm_min
           << ':' << std::setw(2) << std::setfill('0') << tm.tm_sec
           << ',';

        if (isnan(m_value)) {
                st << "null";
                return;
        }

        long value = (long) m_value;
        if (m_value == (double) value) {
                // Integral value
                st << value;
                return;
        }

        st << m_value;
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

        vector<Datapoint>::push_back(datapoint);
}

