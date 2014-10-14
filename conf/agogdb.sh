#!/bin/bash
FILENAME="$1"
TMPFILE=$(mktemp)
PROGRAM=$(bzgrep '^Program:' "$FILENAME" | cut -d ":" -f 2)
bzcat "$FILENAME" | uudecode > "$TMPFILE"
echo $PROGRAM
echo "thr a a bt" | gdb -q "$PROGRAM" "$TMPFILE" 

