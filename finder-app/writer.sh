#!/usr/bin/env sh

if [ $# -lt 2 ]
then
    echo "writer:
         writer.sh file text"
    exit 1
fi

WRITEFILE=$1
WRITESTR=$2

if ! ((  $(touch $WRITEFILE) )); then
    pathdir=$( echo "${WRITEFILE%/*}" )
    echo "Creating dir $pathdir!"
    mkdir -p $pathdir
    touch $WRITEFILE
fi

echo $WRITESTR > $WRITEFILE
