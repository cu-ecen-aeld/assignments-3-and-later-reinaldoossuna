#!/usr/bin/env sh

if [ $# -lt 2 ]
then
    echo "finder:
         finder dir searchstr"
    exit 1
fi

if [ ! -d $1 ]
then
    echo "finder:
         finder dir searchstr"
    echo "first arg MUST be a directory"
    exit 1
fi

SEARCHSTR=$2
FILESDIR=$1

nfiles=$( find ${FILESDIR}  -type f | wc -l)
total_match=$( grep -r ${SEARCHSTR} ${FILESDIR} | wc -l)

echo "The number of files are $nfiles and the number of matching lines are $total_match"
