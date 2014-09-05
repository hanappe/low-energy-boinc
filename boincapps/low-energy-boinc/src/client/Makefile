# low-energy-boinc client application 
# This should work on Linux 32 bits.  Modify as needed for other platforms.

SHELL = /bin/bash

CC = gcc
CFLAGS = -g -Wall -std=c99

CXX = g++
CXXFLAGS = -g -Wall -std=c++0x

BOINC_DIR = ../../../../boinc

CXXFLAGS += -I$(BOINC_DIR) \
	-I$(BOINC_DIR)/api \
	-I$(BOINC_DIR)/lib \
	-I./../sensors \
	-I./../../depends

LDFLAGS = -L$(BOINC_DIR)/api \
	-L$(BOINC_DIR)/lib \
	-L.

sensors_sources = $(addprefix ../sensors/, Sensors.cpp \
	ACPI.cpp \
	BoincCpuLoad.cpp \
	BoincSensors.cpp \
	CpuLoad.cpp \
	Datapoint.cpp \
	Error.cpp \
        LibSensors.cpp \
	ProcessStat.cpp \
	PStates.cpp \
	Sensor.cpp \
	SensorManager.cpp \
	TEMPer.cpp \
	Wattsup.cpp \
	UsersCpuLoad.cpp)

sensors_objects = ${sensors_sources:%.cpp=%.o}
sensors_staticlibs = libstdc++.a libusb.a libsensors.a libacpi.a
sensors_libs = -lboinc_api -lboinc -lpthread

sa_trickle_sources = ../../depends/json/json.cpp standalone_trickle_up.cpp
sa_trickle_objects = ${sa_trickle_sources:%.cpp=%.o}
sa_trickle_libs = -lcurl


application_sources = $(sa_trickle_sources) App.cpp BoincApp.cpp RaspberryApp.cpp BenchmarkApp.cpp application.cpp
application_objects = ${application_sources:%.cpp=%.o}
application_libs = $(sa_trickle_libs)

# standalone trickle up


standalone_sources = standalone.cpp
standalone_objects = ${standalone_sources:%.cpp=%.o}


#all: $(depends) $(programs)
all: application standalone

%.a:
	ln -s `$(CXX) -print-file-name=$@`

#application: $(application_objs) $(application_static_libs)
#	$(CXX) $(LDFLAGS) -o $@ $^ $(application_ldflags)

application:  $(sensors_objects) $(sensors_staticlibs) $(application_libs) $(application_objects)
	$(CXX) $(LDFLAGS) -o $@ $^ $(sensors_libs)

#standalone:  $(sensors_objects) $(sensors_staticlibs) $(sa_trickle_objects) $(standalone_objects)
#	$(CXX) $(LDFLAGS) -o $@ $^ $(sensors_libs) $(sa_trickle_libs)

%.o: %.c
	g++ -c $(CXXFLAGS) $^ -o $@

%.o: %.cpp
	g++ -c $(CXXFLAGS) $^ -o $@

clean:
	rm -f $(sensors_objects) $(application_objects)  stderr.txt


ifneq ($(MAKECMDGOALS),clean)
-include $(depends)
endif
