#ifndef __Datapoint_hpp__
#define __Datapoint_hpp__

#include <ctime>
#include <iostream>
#include <vector>

namespace Time {
	void print_format(std::ostream& os, const time_t& t);
	time_t  get_current();
}

struct Datapoint {

        static time_t  get_current_time();

        Datapoint(double value);
        Datapoint(time_t time, double value);
        //void print_to(std::ostream& st) const;

        time_t m_time;
        double m_value;

		friend std::ostream& operator<<(std::ostream& os, const Datapoint& d);	
};

struct DatapointV : std::vector<Datapoint> {
        DatapointV();
        void push_back(const Datapoint& datapoint);
        bool m_firstDatapoint;
};

#endif
