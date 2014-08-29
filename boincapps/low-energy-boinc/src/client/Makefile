# This should work on Linux 32 bits.  Modify as needed for other platforms.

SHELL = /bin/bash

CC = gcc
CFLAGS = -g -Wall -std=c99

CXX = g++
CXXFLAGS = -g -Wall -std=c++0x

BOINC_DIR = ../../../boinc

CXXFLAGS += -I$(BOINC_DIR) \
 -I$(BOINC_DIR)/api \
 -I$(BOINC_DIR)/lib

LDFLAGS = -L$(BOINC_DIR)/api \
 -L$(BOINC_DIR)/lib \
 -L.

sensors_srcs = $(addprefix sensors/, Sensors.cpp Sensor.cpp Error.cpp	\
    Datapoint.cpp SensorManager.cpp ProcessStat.cpp PStates.cpp		\
    Wattsup.cpp TEMPer.cpp BoincSensors.cpp CpuLoad.cpp			\
    BoincCpuLoad.cpp UsersCpuLoad.cpp LibSensors.cpp ACPI.cpp)
application_srcs = application.cpp $(sensors_srcs)
application_deps = ${application_srcs:%.cpp=%.d}
application_objs = ${application_srcs:%.cpp=%.o}
application_static_libs = libstdc++.a libusb.a libsensors.a libacpi.a
application_ldflags = -lboinc_api -lboinc -lpthread
depends += $(application_deps)
objects += $(application_objs) $(application_static_libs)
programs += application

programs += trickle_handler validator assimilator
generated = $(programs) $(objects) $(depends) stderr.txt boinc_finish_called trickle_up.xml

all: $(depends) $(programs)

%.a:
	ln -s `$(CXX) -print-file-name=$@`

application: $(application_objs) $(application_static_libs)
	$(CXX) $(LDFLAGS) -o $@ $^ $(application_ldflags)

# Needs -lcurl for trickle_echo in $(BOINC_DIR)/sched/Makefile
trickle_handler: trickle_handler.cpp
	cp $(BOINC_DIR)/sched/trickle_echo.cpp trickle_echo.cpp.bkp
	cp -a json/json* $(BOINC_DIR)/sched/
	cp $< $(BOINC_DIR)/sched/trickle_echo.cpp
	pushd $(BOINC_DIR)/sched; rm -f trickle_echo.o trickle_echo; make -k; popd
	rm -rf $(BOINC_DIR)/sched/json*
	mv trickle_echo.cpp.bkp $(BOINC_DIR)/sched/trickle_echo.cpp
	mv $(BOINC_DIR)/sched/trickle_echo $@
	pushd $(BOINC_DIR)/sched; rm -f trickle_echo.o trickle_echo; make -k; popd

validator: validator.cpp
	cp $(BOINC_DIR)/sched/sample_trivial_validator.cpp sample_trivial_validator.cpp.bkp
	cp $< $(BOINC_DIR)/sched/sample_trivial_validator.cpp
	pushd $(BOINC_DIR)/sched; rm -f sample_trivial_validator.o sample_trivial_validator; make -k; popd
	mv sample_trivial_validator.cpp.bkp $(BOINC_DIR)/sched/sample_trivial_validator.cpp
	mv $(BOINC_DIR)/sched/sample_trivial_validator $@
	pushd $(BOINC_DIR)/sched; rm -f sample_trivial_validator.o sample_trivial_validator; make -k; popd

assimilator: assimilator.cpp
	cp $(BOINC_DIR)/sched/sample_dummy_assimilator.cpp sample_dummy_assimilator.cpp.bkp
	cp $< $(BOINC_DIR)/sched/sample_dummy_assimilator.cpp
	pushd $(BOINC_DIR)/sched; rm -f sample_dummy_assimilator.o sample_dummy_assimilator; make -k; popd
	mv sample_dummy_assimilator.cpp.bkp $(BOINC_DIR)/sched/sample_dummy_assimilator.cpp
	mv $(BOINC_DIR)/sched/sample_dummy_assimilator $@
	pushd $(BOINC_DIR)/sched; rm -f sample_dummy_assimilator.o sample_dummy_assimilator; make -k; popd

%.d: %.c
	$(CC) -o $@ $(CFLAGS) $< -MM -MT '$@ $*.o'

%.d: %.cpp
	$(CXX) -o $@ $(CXXFLAGS) $< -MM -MT '$@ $*.o'

clean:
	rm -f $(generated)

ifneq ($(MAKECMDGOALS),clean)
-include $(depends)
endif



