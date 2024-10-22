#!/bin/sh

if [ $# -lt 2 ]
then
    exit 1
fi

filesdir=$1
searchstr=$2

if [ ! -d $filesdir ]
then
    exit 1
fi

file_count=$(find $filesdir -maxdepth 1 -type f | wc -l)
matching_lines=$(grep -R $searchstr $filesdir | wc -l)

echo "The number of files are $file_count and the number of matching lines are $matching_lines"
