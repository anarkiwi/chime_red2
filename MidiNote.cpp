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
  modEnvelope.Reset(0, 0, maxMidiVal, 0);
  drumPreset = NULL;
  drumStartPeriod = 0;
  drumEndPeriod = 0;
  drumNoisePMin = 0;
  drumNoiseSpan = 0;
}

void MidiNote::HandleControl() {
  ageMs += controlClockTickMsFp;
  envelope.HandleControl();
  modEnvelope.HandleControl();
  if (drumPreset) {
    AdvanceDrum();
  }
}

void MidiNote::AdvanceDrum() {
  // Integer-only (no runtime float -- soft-float gate): the per-tick pitch glide
  // and noise gate are driven off this note's existing ageMs clock.
  const int32_t elapsed = ageMs.getInteger();
  if (elapsed >= int32_t(drumPreset->durationMs)) {
    // One-shot done: idle the amplitude envelope so MidiChannel::HandleControl
    // retires the note this cycle and frees the oscillator. A sustain-0 drum
    // envelope parks silently in its sustain phase otherwise and never frees.
    envelope.Idle();
    return;
  }
  cr_tick_t period;
  const uint16_t dropMs = drumPreset->pitchDropMs;
  if (dropMs == 0 || elapsed >= int32_t(dropMs)) {
    period = drumEndPeriod;
  } else {
    // Linear period interpolation start -> end (signed; pitch down => period up).
    const int32_t span = int32_t(drumEndPeriod) - int32_t(drumStartPeriod);
    period = cr_tick_t(int32_t(drumStartPeriod) + (span * elapsed) / int32_t(dropMs));
  }
  const bool noiseOn = drumPreset->noiseMs && elapsed < int32_t(drumPreset->noiseMs);
  for (OscillatorDeque::const_iterator o = oscillators.begin(); o != oscillators.end(); ++o) {
    Oscillator *oscillator = *o;
    oscillator->SetClockPeriodLazy(period);
    // While the noise window is set, Modulate() re-randomises the period each
    // pulse (the burst); clearing it hands period authority back to the glide.
    oscillator->SetNoiseRange(noiseOn ? drumNoisePMin : 0, noiseOn ? drumNoiseSpan : 0);
  }
}
