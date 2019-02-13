#!/bin/bash

version=`cat ./oscr/version.h | perl -ne 'my @array = /^const\s*int (major|minor|revision)_(version|number) = (\d+;).*/; print $array[2]' | perl -ne 'my @ar=split(";"); print join(".", @ar);'` 
buildno=`cat ./oscr/scripts/build_number`
gitversion=`git rev-parse --short HEAD`
echo "OSCR Source Information"
echo "Current Version: v${version}-${buildno} (git rev ${gitversion})"
echo "Last commit: `git log -1 --format=%cd`"
echo "Use 'git pull' to get up to date"
