#include <iomanip>
#include <cmath>
#include <sstream>
#include "Datapoint.hpp"

using namespace std;

#ifdef _WIN32

#define isnan _isnan

static struct tm *
localtime_r (const time_t *timer, struct tm *result)
{
  struct tm *local_result;
  local_result = localtime (timer);

  if (local_result == NULL || result == NULL)
    return NULL;

  memcpy (result, local_result, sizeof (result));
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

void Datapoint::print_to(ostream& st) const {
        struct tm tm;
        localtime_r(&m_time, &tm);

        st << setw(4) << setfill('0') << (1900 + tm.tm_year)
           << '-' << setw(2) << setfill('0') << tm.tm_mon + 1
           << '-' << setw(2) << setfill('0') << tm.tm_mday
           << 'T' << setw(2) << setfill('0') << tm.tm_hour
           << ':' << setw(2) << setfill('0') << tm.tm_min
           << ':' << setw(2) << setfill('0') << tm.tm_sec
           << ',';

        if (::isnan(m_value)) {
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
                vector<Datapoint>::push_back(Datapoint(datapoint.m_time - 1, ::nan("")));
                m_firstDatapoint = false;
        }

        vector<Datapoint>::push_back(datapoint);
}

