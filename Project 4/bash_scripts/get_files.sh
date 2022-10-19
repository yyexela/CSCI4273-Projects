#!/bin/bash

# Check number of command line arguments
if [ ! $# -eq 1 ]
then
	# First command line argument must be a directory to check
	exit 1
fi

# List all files in DFS directory
for filedir in "$1"/*
do
	# Extract all filenames stored
	if [ -d $filedir ]
	then
		filename=$(echo "$filedir" | cut -d'/' -f2)
		echo -n "$filename "
		for part in "$filedir"/*
		do
			# Extract all part numbers stored
			partno=$(echo "$part" | cut -d'/' -f3)
			echo -n "$partno"
		done
		echo -ne " $1"'\n'
	fi
done
