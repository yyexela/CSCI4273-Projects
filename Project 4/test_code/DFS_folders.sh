#!/bin/sh
# This script makes sure there is an empty ./cache directory
# Note: this will not necessarily work on Windows because this uses
#       Linux command syntax

for i in {1..4}
do
    dir="./DIR$i"
    # Create directory if it doesn't exist
    if [ ! -d $dir ]
    then
        mkdir $dir
    fi
done
