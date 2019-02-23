#!/bin/bash

MY_PATH="`dirname \"$0\"`"
if [ -f "$MY_PATH/build_number" ]
then
number=`cat $MY_PATH/build_number`
else
number=0
fi

if [ ! -f $MY_PATH/../build_number.h ]
then
# This is needed to build, so make sure it's available
    echo "const int build_number = ""$number;" | tee $MY_PATH/../build_number.h
fi

if [ ! -f "$MY_PATH/ReleaseManager" ]
then
# Script only needs running by Release Managers
	exit;
fi

if [ "$1" == "release" ]
then
	echo "Updating build number"
	let number++
	echo "$number" > $MY_PATH/build_number
	echo "const int build_number = ""$number;" | tee $MY_PATH/../build_number.h
else
	echo "Skipping build number update"
fi
