#!/bin/bash

# Usage:
#  align_symmetrize source.txt target.txt output.moses method [eflomal options]
# Where method is one of the symmetrization methods from atools (the -c
# argument).

if [ -z $4 ] ; then
    echo "Error: symmetrization argument missing!"
    echo "You might want to try 'grow-diag-final-and' (or 'none')"
    exit 1
fi

SYMMETRIZATION=$4
OPTIONS="--verbose"

DIR=`dirname $0`/..
if [ "$SYMMETRIZATION" == "none" ]; then
    python3 $DIR/align.py $OPTIONS -s "$1" -t "$2" -f "$3" "${@:5}"
else
    FWD=`mktemp`
    BWD=`mktemp`
    python3 $DIR/align.py $OPTIONS --overwrite -s "$1" -t "$2" \
        -f "$FWD" -r "$BWD" "${@:5}"
    atools -c $SYMMETRIZATION -i "$FWD" -j "$BWD" >"$3"
    rm -f "$FWD" "$BWD"
fi

