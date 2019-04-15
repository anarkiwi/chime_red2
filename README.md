# chime_red2
CHIME RED II

WARNING: Tesla coils can be dangerous. This code is experimental. Don't even try to build this code unless you understand it and the coil you will control with it.


Build and run
=============

* Select Due board, exit IDE
* Edit ~/.arduino15/packages/arduino/hardware/sam/*/platform.txt, to change -Os to -O2 (TODO: test pragma override)
* Re-enter IDE, install libraries ArduinoSTL, DueTimer, FixedPonts, MIDI Library
* Define CR_UI in chime_red2.ino if running on original CHIME RED I hardware
* Change pulseWindowUs and coronaUs in constants.h to suit coil hardare (e.g. set pulseWindowUs to 700 for giant coils; coronaUs sets absolute minimum gate on time)
* Upload to CHIME RED I/II
