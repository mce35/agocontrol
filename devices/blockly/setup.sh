#!/bin/sh

set -x
set -e

[ -z "$WRK" ] && echo "You must set WRK and AGO environment vars!" && exit 1
[ -z "$AGO" ] && echo "You must set WRK and AGO environment vars!" && exit 1
[ ! -d "$WRK" ] && echo "You must set WRK and AGO environment vars!" && exit 1
[ ! -d "$AGO" ] && echo "You must set WRK and AGO environment vars!" && exit 1

mkdir -p $WRK
cd $WRK
if [ ! -d blockly ]; then
	git clone https://github.com/google/blockly.git
fi
if [ ! -d closure-library ]; then
	git clone https://github.com/google/closure-library.git
fi

cd blockly

ln -fs $AGO/devices/blockly/blocks/agocontrol.js blocks/agocontrol.js
ln -fs $AGO/devices/blockly/blocks/common.js blocks/common.js
ln -fs $AGO/devices/blockly/blocks/datetime.js blocks/datetime.js
ln -fs $AGO/devices/blockly/generators/lua.js generators/lua.js
[ ! -d generators/lua ] && ln -fs $AGO/devices/blockly/generators/lua/ generators/lua

# This may fail...
for f in $AGO/devices/blockly/patches/*.patch; do
	# Ignore errors
	patch --forward --strip 1 < $f || true
done


echo "Environment should now be ready for building. Please go to $WRK and execute: python build.py"
