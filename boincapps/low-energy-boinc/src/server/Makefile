# low-energy-boinc server script
# This should work on Linux 32 bits.  Modify as needed for other platforms.

SHELL = /bin/bash

BOINC_DIR = ../../../../boinc
DEPENDS_DIR = ../../depends

programs += trickle_handler validator assimilator
generated = $(programs) $(objects) $(depends) stderr.txt boinc_finish_called trickle_up.xml

all: $(programs)

# Needs -lcurl for trickle_echo in $(BOINC_DIR)/sched/Makefile
trickle_handler: trickle_handler.cpp
	cp $(BOINC_DIR)/sched/trickle_echo.cpp trickle_echo.cpp.bkp
	cp -a $(DEPENDS_DIR)/json/json* $(BOINC_DIR)/sched/
	cp $< $(BOINC_DIR)/sched/trickle_echo.cpp
	pushd $(BOINC_DIR)/sched; rm -f trickle_echo.o trickle_echo; make -k; popd
	rm -rf $(BOINC_DIR)/sched/json*
	mv trickle_echo.cpp.bkp $(BOINC_DIR)/sched/trickle_echo.cpp
	pushd $(BOINC_DIR)/sched; rm -f trickle_echo.o trickle_echo; make -k; popd

validator: validator.cpp
	cp $(BOINC_DIR)/sched/sample_trivial_validator.cpp sample_trivial_validator.cpp.bkp
	cp $< $(BOINC_DIR)/sched/sample_trivial_validator.cpp
	pushd $(BOINC_DIR)/sched; rm -f sample_trivial_validator.o sample_trivial_validator; make -k; popd
	mv sample_trivial_validator.cpp.bkp $(BOINC_DIR)/sched/sample_trivial_validator.cpp
	pushd $(BOINC_DIR)/sched; rm -f sample_trivial_validator.o sample_trivial_validator; make -k; popd

assimilator: assimilator.cpp
	cp $(BOINC_DIR)/sched/sample_dummy_assimilator.cpp sample_dummy_assimilator.cpp.bkp
	cp $< $(BOINC_DIR)/sched/sample_dummy_assimilator.cpp
	pushd $(BOINC_DIR)/sched; rm -f sample_dummy_assimilator.o sample_dummy_assimilator; make -k; popd
	mv sample_dummy_assimilator.cpp.bkp $(BOINC_DIR)/sched/sample_dummy_assimilator.cpp
	pushd $(BOINC_DIR)/sched; rm -f sample_dummy_assimilator.o sample_dummy_assimilator; make -k; popd

clean:
	rm -f $(generated)

ifneq ($(MAKECMDGOALS),clean)
-include $(depends)
endif



