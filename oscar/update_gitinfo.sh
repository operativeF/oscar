#!/bin/bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd $DIR

GIT_BRANCH=`git rev-parse --abbrev-ref HEAD`
GIT_REVISION=`git rev-parse --short HEAD`
$(git diff-index --quiet HEAD --)
if [ $? -ne 0 ]; then
    GIT_REVISION="$GIT_REVISION+"  # uncommitted changes
fi

[ -z "$GIT_BRANCH" ] &&	GIT_BRANCH="Unknown";
[ -z "$GIT_REVISION" ] && GIT_REVISION="Unknown";

echo // This is an auto generated file > $DIR/git_info.h.new
echo const QString GIT_BRANCH=\"$GIT_BRANCH\"\; >> $DIR/git_info.h.new
echo const QString GIT_REVISION=\"$GIT_REVISION\"\; >> $DIR/git_info.h.new

if diff $DIR/git_info.h $DIR/git_info.h.new &> /dev/null; then
    rm $DIR/git_info.h.new
else
    echo Updating $DIR/git_info.h
    mv $DIR/git_info.h.new $DIR/git_info.h
fi
