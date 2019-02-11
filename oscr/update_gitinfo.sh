#!/bin/bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd $DIR
echo Updating $DIR/git_info.h

GIT_BRANCH=`git -C $DIR rev-parse --abbrev-ref HEAD`
GIT_REVISION=`git -C $DIR rev-parse --short HEAD`

[ -z "$GIT_BRANCH" ] &&	GIT_BRANCH="Unknown";
[ -z "$GIT_REVISION" ] && GIT_REVISION="Unknown";

echo // This is an auto generated file > $DIR/git_info.h
echo const QString GIT_BRANCH=\"$GIT_BRANCH\"\; >> $DIR/git_info.h
echo const QString GIT_REVISION=\"$GIT_REVISION\"\; >> $DIR/git_info.h
