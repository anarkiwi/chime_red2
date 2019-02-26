// Copyright 2019 Josh Bailey (josh@vandervecken.com)

// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


//#pragma GCC diagnostic error "-Wall"
//#pragma GCC diagnostic error "-Werror"

// TODO: Have to hack ~/.arduino15/packages/arduino/hardware/sam/1.6.11/platform.txt, to change -Os to -O2,
// rather than following GCC pragma.
// #pragma GCC optimize ("-O2")
// #pragma GCC push_options

// Define when running on full hardware.
// #define CR_UI 1

#include <DueTimer.h>
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
  static const bool Use1ByteParsing = true;
};
MIDI_CREATE_CUSTOM_INSTANCE(HardwareSerial, Serial1, MIDI, ChimeRedSettings);


CRIO crio;
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

inline void handlePitchBend(byte channel, int bend) {
  crmidi.handlePitchBend(channel, bend);
}

inline void resetAll() {
  crmidi.ResetAll();
}

inline cr_fp_t amModulate(cr_fp_t p, cr_fp_t depth, Lfo *lfo) {
  p /= 2;
  p += (p * depth * lfo->Level());
  return p;
}

void masterISR() {
  crio.handlePulse();
  oc.Tick();
  Oscillator *audibleOscillator = NULL;
  if (oc.Triggered(&audibleOscillator)) {
    if (audibleOscillator) {
      cr_fp_t p = crio.pw;
      if (!crio.fixedPulseEnabled()) {
        MidiChannel *midiChannel = crmidi.getOscillatorChannel(audibleOscillator);
        p *= audibleOscillator->pulseUsScale;
        if (midiChannel->tremoloRange) {
          p = amModulate(p, midiValMap[midiChannel->tremoloRange], oc.tremoloLfo);
        }
        p *= audibleOscillator->envelope->level;
        p += coronaUs;
      }
      crio.schedulePulse(p);
    }
  } else {
    if (crio.scheduled) {
      crio.startPulse();
    } else if (oc.controlTriggered) {
      if (crmidi.HandleControl()) {
        MIDI.read();
        oc.controlTriggered = false;
      }
    }
  }
}

void enableMidi() {
  masterTimer.attachInterrupt(masterISR).setFrequency(masterClockHz).start();
  MIDI.setHandleNoteOn(handleNoteOn);
  MIDI.setHandleNoteOff(handleNoteOff);
  MIDI.setHandleSystemReset(resetAll);
  MIDI.setHandleStop(resetAll);
  MIDI.setHandleControlChange(handleControlChange);
  MIDI.setHandlePitchBend(handlePitchBend);
  MIDI.begin(MIDI_CHANNEL_OMNI);
}

void setup() {
  resetAll();
  enableMidi();
}

void loop() {
  crio.updateLcd(); // interruptable and slow.
}
