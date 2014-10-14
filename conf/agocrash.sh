#!/bin/sh
PROGRAM=$(echo $4 | sed 's|!|/|g')
BASENAME=$(basename "$PROGRAM")
FILENAME="/var/crash/$BASENAME-$1.crash.bz2"
(
	echo "Pid:$1"
	echo "Program:$PROGRAM"
	echo "Core:"
	cat - | uuencode -m -
) | bzip2 > "$FILENAME"
