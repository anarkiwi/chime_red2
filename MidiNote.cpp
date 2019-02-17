// Copyright 2019 Josh Bailey (josh@vandervecken.com)

// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


#include "MidiNote.h"
#include "Oscillator.h"
#include "OscillatorController.h"
#include "constants.h"

MidiNote::MidiNote() {
  Reset();
};

void MidiNote::ResetOscillators(OscillatorController *oc) {
  for (OscillatorDeque::const_iterator o = oscillators.begin(); o != oscillators.end(); ++o) {
    Oscillator *oscillator = *o;
    oc->ReturnFreeOscillator(oscillator);
  }
  Reset();
}

void MidiNote::Reset() {
  pitch = 0;
  ageMs = 0;
  velocityScale = 0;
  oscillators.clear();
  envelope.Reset(0, 0, maxMidiVal, 0);
}

void MidiNote::SetFreqLazy(cr_fp_t hz, cr_fp_t maxHz, OscillatorController *oc) {
  for (OscillatorDeque::const_iterator o = oscillators.begin(); o != oscillators.end(); ++o) {
    Oscillator *oscillator = *o;
    oc->SetFreqLazy(oscillator, hz, maxHz, velocityScale);
  }
}

void MidiNote::HandleControl() {
  ageMs += controlClockTickMs;
  envelope.HandleControl();
}
