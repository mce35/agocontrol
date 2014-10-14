#!/bin/bash
DIR=/var/crash
while RES=$(inotifywait -e create $DIR); do
	FILENAME=${RES#?*CREATE }
	echo $FILENAME
	TMPFILE=$(mktemp)
	PROGRAM=$(bzgrep '^Program:' "$FILENAME" | cut -d ":" -f 2)
	bzcat "$FILENAME" | uudecode > "$TMPFILE"
	echo $PROGRAM
	echo "thr a a bt" | gdb -q "$PROGRAM" "$TMPFILE"
	rm "$TMPFILE"
done
