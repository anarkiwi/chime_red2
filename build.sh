#!/bin/bash

set -e

platform=$1
fqbn=$2
urls=https://raw.githubusercontent.com/sparkfun/Arduino_Boards/master/IDE_Board_Manager/package_sparkfun_index.json

sudo apt-get update && sudo apt-get install wget unzip
wget -O/tmp/ide.zip https://downloads.arduino.cc/arduino-ide/arduino-ide_2.3.2_Linux_64bit.zip
unzip /tmp/ide.zip
find arduino-ide* -name python3 -exec ln -sf $(which python3) {} \;
mkdir ~/bin
ln -s ~/arduino*/arduino-ide ~/bin/arduino
PATH=~/bin:$PATH curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | BINDIR=~/bin sh
mkdir -p ~/.arduino15/packages
PATH=~/bin:$PATH arduino-cli config init
PATH=~/bin:$PATH arduino-cli core update-index --additional-urls $urls
PATH=~/bin:$PATH arduino-cli core install $platform --additional-urls $urls
PATH=~/bin:$PATH arduino-cli lib install ArduinoSTL
PATH=~/bin:$PATH arduino-cli lib install DueTimer
PATH=~/bin:$PATH arduino-cli lib install FixedPoints
PATH=~/bin:$PATH arduino-cli lib install "MIDI Library"
PATH=~/bin:$PATH arduino-cli lib install SAMD_TimerInterrupt
PATH=~/bin:$PATH arduino-cli compile --fqbn $fqbn chime_red2.ino
curl -fsSL https://github.com/danmar/cppcheck/archive/refs/tags/2.8.tar.gz | tar zxvf -
cd cppcheck*
mkdir build
cd build
cmake ..
cmake --build .
sudo make install
cd ../..
cppcheck --enable=all --language=c++ --error-exitcode=1 --xml --inline-suppr *.cpp *.h *.ino
