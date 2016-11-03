#!/bin/bash

# Usage:
#  align_symmetrize source.txt target.txt output.moses method [eflomal options]
# Where method is one of the symmetrization methods from atools (the -c
# argument).

if [ -z $4 ] ; then
    echo "Error: symmetrization argument missing!"
    echo "You might want to try grow-diag-final-and"
    exit 1
fi
SYMMETRIZATION=$4

#SYMMETRIZATION=grow-diag-final-and
#if [ ! -z $4 ] ; then
#    SYMMETRIZATION=$4
#fi

DIR=`dirname $0`/..
FWD=`mktemp`
BWD=`mktemp`
python3 $DIR/align.py --verbose --overwrite -s "$1" -t "$2" -o "$FWD" "${@:5}" &
python3 $DIR/align.py --verbose --overwrite -r -s "$1" -t "$2" -o "$BWD" "${@:5}" &

wait
atools -c $SYMMETRIZATION -i "$FWD" -j "$BWD" >"$3"
rm "$FWD" "$BWD"

