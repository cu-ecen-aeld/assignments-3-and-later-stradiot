#!/bin/sh

if [ $# -lt 2 ]
then
    exit 1
fi

writefile=$1
writestr=$2

echo "Installing file $writefile"
install -D /dev/null $writefile

if [ ! $? -eq 0 ]
then
    echo "File could not be created"
    exit 1
fi

echo $writestr > $writefile
