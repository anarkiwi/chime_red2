// Copyright 2019 Josh Bailey (josh@vandervecken.com)

// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


#ifndef MIDINOTE_H
#define MIDINOTE_H

#include "Oscillator.h"
#include "OscillatorController.h"

typedef std::deque<Oscillator*> OscillatorDeque;

class MidiNote {
 public:
  MidiNote();
  void ResetOscillators(OscillatorController *oc);
  void Reset();
  void SetFreqLazy(cr_fp_t hz, cr_fp_t hz2, cr_fp_t maxHz, OscillatorController *oc);
  void HandleControl();
  uint8_t pitch;
  cr_fp_t ageMs;
  OscillatorDeque oscillators;
  AdsrEnvelope envelope;
  cr_fp_t velocityScale;
};

#endif
