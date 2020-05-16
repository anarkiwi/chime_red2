// Copyright 2019 Josh Bailey (josh@vandervecken.com)

// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


//#pragma GCC diagnostic error "-Wall"
//#pragma GCC diagnostic error "-Werror"

// TODO: Have to hack ~/.arduino15/packages/arduino/hardware/sam/*/platform.txt, to change -Os to -O2,
// rather than following GCC pragma.
// find .arduino15/packages/arduino/hardware/sam -name platform.txt -exec perl -pi -e 's/Os/O2/g' {} \;
// #pragma GCC optimize ("-O2")
// #pragma GCC push_options

// Define when running on CR original hardware.
// #define CR_UI 1

// cppcheck-suppress missingIncludeSystem
#include <DueTimer.h>
// cppcheck-suppress missingIncludeSystem
#include <MIDI.h>
#include "types.h"
#include "constants.h"
#include "CRIO.h"
#include "CRMidi.h"
#include "Lfo.h"
#include "OscillatorController.h"
#include "MidiChannel.h"

DueTimer masterTimer = Timer.getAvailable();

#define  MIDI_CHANNEL  MIDI_CHANNEL_OMNI
struct ChimeRedSettings : public midi::DefaultSettings {
  // cppcheck-suppress unusedStructMember
  static const bool Use1ByteParsing = true;
  static const long BaudRate = 31250;
};

#ifdef CR_UI
MIDI_CREATE_CUSTOM_INSTANCE(HardwareSerial, Serial1, MIDI, ChimeRedSettings);
CRIOLcd crio;
#else
MIDI_CREATE_CUSTOM_INSTANCE(HardwareSerial, Serial2, MIDI, ChimeRedSettings);
CRIO crio;
#endif


OscillatorController oc;
CRMidi crmidi(&oc, &crio);

inline void handleNoteOn(byte channel, byte note, byte velocity) {
  crmidi.handleNoteOn(channel, note, velocity);
}

inline void handleNoteOff(byte channel, byte note, byte velocity) {
  crmidi.handleNoteOff(channel, note);
}

inline void handleControlChange(byte channel, byte number, byte value) {
  crmidi.handleControlChange(channel, number, value);
}

inline void handleProgramChange(byte channel, byte number) {
  crmidi.handleProgramChange(channel, number);
}

inline void handlePitchBend(byte channel, int bend) {
  crmidi.handlePitchBend(channel, bend);
}

inline void resetAll() {
  crmidi.ResetAll();
}

void masterISR() {
  bool remainderPulse = crio.handlePulse();
  if (remainderPulse) {
    crio.slipTick = true;
    return;
  }
  Oscillator *audibleOscillator = NULL;
  oc.Tick();
  if (crio.slipTick) {
    oc.Triggered(&audibleOscillator);
    oc.Tick();
    oc.Triggered(&audibleOscillator);
    crio.slipTick = false;
    return;
  }
  if (oc.Triggered(&audibleOscillator)) {
    if (audibleOscillator) {
      cr_fp_t p = crmidi.Modulate(audibleOscillator);
      crio.schedulePulse(p);
      crio.startPulse();
    }
    return;
  }
  if (oc.controlTriggered) {
    if (crmidi.HandleControl()) {
      MIDI.read();
      oc.controlTriggered = false;
    }
  }
}

void enableMidi() {
  masterTimer.attachInterrupt(masterISR).setFrequency(masterClockHz).start();
  MIDI.setHandleNoteOn(handleNoteOn);
  MIDI.setHandleNoteOff(handleNoteOff);
  MIDI.setHandleSystemReset(resetAll);
  MIDI.setHandleControlChange(handleControlChange);
  MIDI.setHandleProgramChange(handleProgramChange);
  MIDI.setHandlePitchBend(handlePitchBend);
  MIDI.turnThruOff();
  // TODO: put serial port in poll only mode.
  MIDI.begin(MIDI_CHANNEL_OMNI);
}

// cppcheck-suppress unusedFunction
void setup() {
  resetAll();
  enableMidi();
}

// cppcheck-suppress unusedFunction
void loop() {
  crio.updateLcd(); // interruptable and slow.
}
