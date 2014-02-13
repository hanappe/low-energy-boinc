#!/bin/sh

usage() {
    echo "usage: $0 <boinc directory, defaults to ./boinc>"
    exit -1
}

BOINCDIR=$1
if [ "${BOINCDIR}" == "" ]
then
    BOINCDIR=./boinc
fi

CONFIGURE_BOINC="no"
UPDATE_BOINC="no"
CLEANUP_BOINC="no"
COMPILE_BOINC="no"

if [ ! -e ${BOINCDIR} ]; then
    git clone git://boinc.berkeley.edu/boinc-v2.git ${BOINCDIR}
    CONFIGURE_BOINC="yes"
    UPDATE_BOINC="no"
    COMPILE_BOINC="yes"
else
    UPDATE_BOINC="yes"
fi

pushd ${BOINCDIR}

if [ "${UPDATE_BOINC}" == "yes" ]; then
    git pull origin
    COMPILE_BOINC="yes"
fi

if [ "${CONFIGURE_BOINC}" == "yes" ]; then
    ./_autosetup
    ./configure -C --disable-client --disable-manager
    COMPILE_BOINC="yes"
fi

if [ "${CLEANUP_BOINC}" == "yes" ]; then
    make -k clean
fi

if [ "${COMPILE_BOINC}" == "yes" ]; then
    make
fi

popd
