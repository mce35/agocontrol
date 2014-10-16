#!/bin/sh
#
# writes a bzip compressed file with program name, and a uuencoded core dump
# use a kernel core_pattern like this: |/var/crash/agocrash.sh %p %s %c %E
#
BASEDIR=/var/crash
QUEUEDIR=${BASEDIR}/queue
PROGRAM=$(echo $4 | sed 's|!|/|g')
BASENAME=$(basename "$PROGRAM")
COREFILENAME="${BASEDIR}/$1.core"
QUEUFILENAME="${QUEUEDIR}/$1"

cat - > ${COREFILENAME}

(
	echo "Pid:$1"
	echo "Program:$PROGRAM"
)  > "$QUEUEFILENAME"
