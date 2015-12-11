#!/usr/bin/env bash
TMPFILE=$(mktemp -u).xml.in
SOURCEFILE=$(realpath $1)
ln -s ${SOURCEFILE} ${TMPFILE} || exit 1
intltool-merge -x html/po ${TMPFILE} $2 || exit 1

# Remove first line: <?xml version...
# sed -i is not POSIX, and behaves differently on BSD vs GNU
if [ `uname -s` == "FreeBSD" ]; then
	sed -i '' '1d' $2 || exit 1
else
	sed -i '1d' $2 || exit 1
fi

rm ${TMPFILE}

# Restore knockout containerless control flow
# And replace all "and" and "or" respectively by "&&" and "||". Only 3 substitutions are supported !
TMPFILE2=$(realpath $1.tmp)
sed -e '/<ko opts=".*">/{s/\(.*\) and \(.*\)/\1 \&\& \2/gi}' -e '/<ko opts=".*">/{s/\(.*\) and \(.*\)/\1 \&\& \2/gi}' -e '/<ko opts=".*">/{s/\(.*\) and \(.*\)/\1 \&\& \2/gi}' $2 > $TMPFILE2 && mv $TMPFILE2 $2 || exit 1
sed -e '/<ko opts=".*">/{s/\(.*\) or \(.*\)/\1 \|\| \2/gi}' -e '/<ko opts=".*">/{s/\(.*\) or \(.*\)/\1 \|\| \2/gi}' -e '/<ko opts=".*">/{s/\(.*\) or \(.*\)/\1 \|\| \2/gi}' $2 > $TMPFILE2 && mv $TMPFILE2 $2 || exit 1
sed -e "s/<ko opts=\"\(.*\)\">/<\!\-\- ko \1 \-\->/g" $2 > $TMPFILE2 && mv $TMPFILE2 $2 || exit 1
sed -e "s/<\/ko>/<\!\-\- \/ko \-\->/g" $2 > $TMPFILE2 && mv $TMPFILE2 $2 || exit 1
