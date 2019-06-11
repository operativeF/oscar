#!/bin/bash
#

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd $DIR

sed -i s/PlaceholderText/Text/ mainwindow.ui
#
