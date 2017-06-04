#!/bin/sh
set -x
set -e

[ -z "$WRK" ] && echo "You must set WRK and AGO environment vars!" && exit 1
[ -z "$AGO" ] && echo "You must set WRK and AGO environment vars!" && exit 1
[ ! -d "$WRK" ] && echo "You must set WRK and AGO environment vars!" && exit 1
[ ! -d "$AGO" ] && echo "You must set WRK and AGO environment vars!" && exit 1

[ ! -d "$WRK/blockly" ] && echo "You must run ./setup.sh first" && exit 1

cd $WRK/closure-library
CLOSURE_REV=$(git rev-parse HEAD)

cd $WRK/blockly
BLOCKLY_REV=$(git rev-parse HEAD)

REV_INFO="// Built at `date` from blockly $BLOCKLY_REV, closure $CLOSURE_REV"

for f in blockly_compressed.js blocks_compressed.js lua_compressed.js; do
	TARGET=$AGO/core/rpc/html/configuration/blockly/blockly/$f
	echo $REV_INFO > $TARGET
	cat $f >> $TARGET
done


echo "All files copyed to Blockly component in web dir."
