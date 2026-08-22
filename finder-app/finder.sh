#!/bin/sh
if [ $# -ne 2 ]
then
    exit 1
fi
filesdir=$1
searchstr=$2
if [ ! -d $filesdir ]
then
    exit 1
fi
numfiles=$(find $filesdir -type f | wc -l)
numlines=$(grep -r $searchstr $filesdir | wc -l)

echo "The number of files are $numfiles and the number of matching lines are $numlines"
