// Copyright 2019 Josh Bailey (josh@vandervecken.com)

// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


#ifndef MIDINOTE_H
#define MIDINOTE_H

#include "Oscillator.h"
#include "OscillatorController.h"
#include "DrumKit.h"

typedef std::deque<Oscillator*> OscillatorDeque;

class MidiNote {
 public:
  MidiNote();
  void ResetOscillators(OscillatorController *oc);
  void Reset();
  void HandleControl();
  // Channel-10 drum schedule: when drumPreset is non-NULL this note is a one-shot
  // percussion voice. AdvanceDrum (run at control rate from HandleControl) glides
  // the oscillator period, gates the noise burst, and idles the envelope at end
  // of life. Non-perc notes leave drumPreset NULL and pay only one NULL test.
  void AdvanceDrum();
  uint8_t pitch;
  cr_fp_t ageMs;
  OscillatorDeque oscillators;
  AdsrEnvelope envelope;
  // Independent envelope shaping the FM modulation index over the note's life
  // (the bell loses brightness faster than loudness). Advances with `envelope`
  // but never governs voice lifetime -- only the amplitude envelope does that.
  AdsrEnvelope modEnvelope;
  cr_fp_t velocityScale;
  // Drum-schedule state, set by MidiChannel at note-on / retrigger. The period
  // endpoints and noise window are pre-resolved to master-clock ticks (via
  // pitchToPeriod[]) so AdvanceDrum needs no table lookups or division by zero.
  const DrumPreset *drumPreset;
  cr_tick_t drumStartPeriod;
  cr_tick_t drumEndPeriod;
  cr_tick_t drumNoisePMin;
  cr_tick_t drumNoiseSpan;
};

#endif
