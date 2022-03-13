#!/bin/bash

platform=$1
fbqn=$2

sudo apt-get update
curl -fsSL https://downloads.arduino.cc/arduino-1.8.19-linux64.tar.xz | tar Jxf -
mkdir ~/bin
ln -s ~/arduino*/arduino ~/bin/arduino
curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | BINDIR=~/bin sh
mkdir -p ~/.arduino15/packages
PATH=~/bin:$PATH arduino-cli config init
PATH=~/bin:$PATH arduino-cli core update-index
PATH=~/bin:$PATH arduino-cli core install $platform
PATH=~/bin:$PATH arduino-cli lib install ArduinoSTL
PATH=~/bin:$PATH arduino-cli lib install DueTimer
PATH=~/bin:$PATH arduino-cli lib install FixedPoints
PATH=~/bin:$PATH arduino-cli lib install "MIDI Library"
curl -fsSL https://github.com/danmar/cppcheck/archive/refs/tags/2.5.tar.gz | tar zxvf -
cd cppcheck*
mkdir build
cd build
cmake ..
cmake --build .
sudo make install
cd ../..
PATH=~/bin:$PATH arduino-cli compile --fqbn $fqbn chime_red2.ino
cppcheck --enable=all --language=c++ --error-exitcode=1 --xml --inline-suppr *.cpp *.h *.ino
