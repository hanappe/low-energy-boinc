# low-energy-boinc test
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

SENSORS_PATH = ../sensors

sensors_srcs = $(addprefix ../sensors/, /Sensors.cpp \
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
	UsersCpuLoad.cpp \
	Wattsup.cpp)

test_srcs = test.cpp $(sensors_srcs)
test_deps = ${test_srcs:%.cpp=%.d}
test_objs = ${test_srcs:%.cpp=%.o}
test_static_libs =
test_loadlibes = -lusb -lsensors -lacpi -lboinc_api -lboinc -lpthread
depends += $(test_deps)
objects += $(test_objs) $(test_static_libs)
programs += test

generated = $(programs) $(objects) $(depends)

all: $(depends) $(programs)

%.a:
	ln -s `$(CXX) -print-file-name=$@`

test: $(test_objs) $(test_static_libs)
	$(CXX) $(LDFLAGS) -o $@ $^ $(test_loadlibes)

%.d: %.c
	$(CC) -o $@ $(CFLAGS) $< -MM -MT '$@ $*.o'

%.d: %.cpp
	$(CXX) -o $@ $(CXXFLAGS) $< -MM -MT '$@ $*.o'

clean:
	rm -f $(generated)

ifneq ($(MAKECMDGOALS),clean)
-include $(depends)
endif

