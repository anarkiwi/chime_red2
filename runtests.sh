#!/bin/bash
cppcheck --enable=style,performance,information,portability,warning --language=c++ --error-exitcode=1 --xml --inline-suppr *.cpp *.h *.ino && \
  arduino-cli compile --fqbn arduino:sam:arduino_due_x chime_red2.ino
