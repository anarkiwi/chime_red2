#!/bin/bash

set -e

profile=$1

sudo apt-get update && sudo apt-get install wget unzip
wget -O/tmp/ide.zip https://downloads.arduino.cc/arduino-ide/arduino-ide_2.3.2_Linux_64bit.zip
unzip /tmp/ide.zip
find arduino-ide* -name python3 -exec ln -sf $(which python3) {} \;
mkdir ~/bin
ln -s ~/arduino*/arduino-ide ~/bin/arduino
PATH=~/bin:$PATH curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | BINDIR=~/bin sh
mkdir -p ~/.arduino15/packages
PATH=~/bin:$PATH arduino-cli config init
# Build via the sketch.yaml build profile. This auto-installs the pinned board
# core and libraries -- and reads the SparkFun board-index URL straight from
# sketch.yaml -- into an isolated per-sketch dir, so CI exercises the exact
# one-command "arduino-cli compile --profile" path documented in the README.
PATH=~/bin:$PATH arduino-cli compile --profile $profile chime_red2.ino
# cppcheck runs once in the host-test Docker image (test/host/Dockerfile), not
# here -- that keeps a single pinned cppcheck version and avoids each board job
# rebuilding cppcheck from source.
