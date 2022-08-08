// Copyright 2019 Josh Bailey (josh@vandervecken.com)

// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#include "config.h"
// cppcheck-suppress missingIncludeSystem
#include <MIDI.h>
#include "types.h"
#include "constants.h"
#include "CRIO.h"
#include "CRMidi.h"
#include "Lfo.h"
#include "OscillatorController.h"
#include "MidiChannel.h"

#ifdef ARDUINO_ARCH_SAM
// cppcheck-suppress missingIncludeSystem
#include <DueTimer.h>
DueTimer masterTimer = Timer.getAvailable();
#define START_ISR(hz, isr) masterTimer.attachInterrupt(isr).setFrequency(hz).start();
#else
// cppcheck-suppress missingIncludeSystem
#include <SAMDTimerInterrupt.h>
SAMDTimer masterTimer(TIMER_TC3);
#define START_ISR(hz, isr) masterTimer.attachInterrupt(hz, isr);
#endif

#define  MIDI_CHANNEL  MIDI_CHANNEL_OMNI
struct ChimeRedSettings : public midi::DefaultSettings {
  // cppcheck-suppress unusedStructMember
  static const bool Use1ByteParsing = true;
  // cppcheck-suppress unusedStructMember
  static const long BaudRate = 31250;
};

#define CR_SERIAL Serial1
#define CR_IO CRIO
#ifdef CR_UI
#undef CR_IO
#define CR_IO CRIOLcd
#else
#ifdef ARDUINO_ARCH_SAM
#undef CR_SERIAL
#define CR_SERIAL Serial2
#endif
#endif

MIDI_CREATE_CUSTOM_INSTANCE(HardwareSerial, CR_SERIAL, MIDI, ChimeRedSettings);
CR_IO crio;
OscillatorController oc;
CRMidi crmidi(&oc, &crio);
void (*isrPtr)(void) = &nextISR;

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

void slipTickISR() {
  oc.Triggered();
  oc.Triggered();
  isrPtr = &nextISR;
}

void nextISR() {
  // This ISR period was used to output pulse less than the ISR period - catch up on next ISR.
  if (crio.handlePulse()) {
    isrPtr = &slipTickISR;
    return;
  }
  if (oc.Triggered()) {
    if (oc.audibleOscillator) {
      cr_fp_t p = crmidi.Modulate(oc.audibleOscillator);
      crio.schedulePulse(p);
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

void masterISR() {
  (*isrPtr)();
}

void enableMidi() {
  START_ISR(masterClockHz, masterISR);
  MIDI.setHandleNoteOn(handleNoteOn);
  MIDI.setHandleNoteOff(handleNoteOff);
  MIDI.setHandleSystemReset(resetAll);
  MIDI.setHandleControlChange(handleControlChange);
  MIDI.setHandleProgramChange(handleProgramChange);
  MIDI.setHandlePitchBend(handlePitchBend);
  // TODO: put serial port in poll only mode.
  MIDI.begin(MIDI_CHANNEL_OMNI);
  // ThruOff must be after begin, because begin enables thru.
  MIDI.turnThruOff();
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
