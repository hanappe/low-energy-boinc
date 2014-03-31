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
	-I./../sensors

LDFLAGS = -L$(BOINC_DIR)/api \
	-L$(BOINC_DIR)/lib \
	-L.

sensors_srcs = $(addprefix ../sensors/, Sensors.cpp \
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

application_srcs = application.cpp $(sensors_srcs)
application_deps = ${application_srcs:%.cpp=%.d}
application_objs = ${application_srcs:%.cpp=%.o}
application_static_libs = libstdc++.a libusb.a libsensors.a libacpi.a
application_ldflags = -lboinc_api -lboinc -lpthread
depends += $(application_deps)
objects += $(application_objs) $(application_static_libs)
programs += application

generated = $(programs) $(objects) $(depends) stderr.txt

all: $(depends) $(programs)

%.a:
	ln -s `$(CXX) -print-file-name=$@`

application: $(application_objs) $(application_static_libs)
	$(CXX) $(LDFLAGS) -o $@ $^ $(application_ldflags)

%.d: %.c
	$(CC) -o $@ $(CFLAGS) $< -MM -MT '$@ $*.o'

%.d: %.cpp
	$(CXX) -o $@ $(CXXFLAGS) $< -MM -MT '$@ $*.o'

clean:
	rm -f $(generated)

ifneq ($(MAKECMDGOALS),clean)
-include $(depends)
endif
