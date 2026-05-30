// Copyright 2019 Josh Bailey (josh@vandervecken.com)

// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


// Code-defined GM-style drum kit for MIDI channel 10.
//
// A channel-10 note-on no longer plays generic pitched noise; instead a small
// set of GM drum notes each select a DrumPreset -- a built-in "schedule" that,
// advanced at control rate by MidiNote::AdvanceDrum (off the master ISR, integer
// only), emulates a drum by combining features the synth already has:
//   * a pitch DROP (oscillator period glides startPitch -> endPitch) -- kick, tom
//   * a NOISE burst (the existing pitched-noise window, gated on for noiseMs) --
//     snare, hats, clap
//   * an amplitude ADSR (the per-note envelope) -- the percussive shape, and the
//     one-shot voice lifetime (the voice frees itself at durationMs).
//
// All fields are integers, folded at compile time -- no runtime float, so the
// soft-float gate (test/host/softfloat_guard.sh) is satisfied and this stays a
// header-only addition (no new TU to register there). Pitches must be >= ~12 so
// the period stays < maxClockPeriod (8192) and the voice actually pulses; note 0
// (1 Hz) is sub-audio and must not be used as a drum pitch.

#ifndef DRUMKIT_H
#define DRUMKIT_H

#include "types.h"

struct DrumPreset {
  // Pitch glide (pitch DOWN means period UP -- a lower pitch is a longer period).
  uint8_t startPitch;    // onset pitch
  uint8_t endPitch;      // landing pitch after the glide
  uint16_t pitchDropMs;  // glide duration; 0 = static pitch (no glide)
  // Noise burst (reuses the pitched-noise window + CRMidi::Modulate's perc branch).
  uint16_t noiseMs;        // burst length from note-on; 0 = tonal only
  uint8_t noisePitch;      // noise window centre pitch (snare/hat: high = bright)
  uint8_t noiseSpanSemis;  // noise window half-width in semitones
  // Amplitude ADSR (0..127 -> AdsrEnvelope curve). Percussive: fast attack, a
  // decay, sustain 0 so it dies away. durationMs is the one-shot lifetime: past
  // it AdvanceDrum idles the envelope so the voice is freed without a note-off
  // (a sustain-0 envelope parks silently in its sustain phase otherwise).
  uint8_t attack;
  uint8_t decay;
  uint8_t sustain;
  uint8_t release;
  uint16_t durationMs;
};

// Preset indices (order matches drumPresets[] below).
enum {
  DRUM_KICK = 0,
  DRUM_SNARE,
  DRUM_CLOSED_HAT,
  DRUM_OPEN_HAT,
  DRUM_LOW_TOM,
  DRUM_CLAP,
  DRUM_COUNT
};

// The Core 6 kit. Values are starting points tuned by ear; the constraints that
// matter for correctness are: every pitch >= ~12 (period < 8192), decay > 0 with
// sustain 0 (so it dies), and durationMs past the audible decay (so the voice is
// reclaimed). See MidiNote::AdvanceDrum for how these are played.
const DrumPreset drumPresets[DRUM_COUNT] = {
    // startPitch, endPitch, pitchDropMs, noiseMs, noisePitch, noiseSpanSemis,
    //   attack, decay, sustain, release, durationMs
    {45, 21, 45, 0, 0, 0, 0, 42, 0, 8, 120},     // DRUM_KICK: fast pitch drop, tonal
    {60, 50, 20, 90, 84, 6, 0, 46, 0, 14, 150},  // DRUM_SNARE: noise burst + short tonal drop
    {92, 92, 0, 40, 92, 4, 0, 26, 0, 6, 45},     // DRUM_CLOSED_HAT: brief bright noise, fast decay
    {90, 90, 0, 320, 90, 4, 0, 60, 0, 20, 330},  // DRUM_OPEN_HAT: long bright noise
    {50, 38, 110, 0, 0, 0, 0, 55, 0, 16, 260},   // DRUM_LOW_TOM: slow pitch drop, tonal
    {80, 80, 0, 90, 80, 8, 0, 40, 0, 12, 100},   // DRUM_CLAP: noise burst, medium decay
};

// GM percussion note -> preset, or NULL for an unmapped note (channel-10 notes
// outside this map are silent). The Core 6 GM assignments:
//   36 Bass Drum 1, 38 Acoustic Snare, 42 Closed Hi-Hat, 46 Open Hi-Hat,
//   45 Low Tom, 39 Hand Clap.
inline const DrumPreset *LookupDrumPreset(uint8_t note) {
  switch (note) {
    case 36:
      return &drumPresets[DRUM_KICK];
    case 38:
      return &drumPresets[DRUM_SNARE];
    case 42:
      return &drumPresets[DRUM_CLOSED_HAT];
    case 46:
      return &drumPresets[DRUM_OPEN_HAT];
    case 45:
      return &drumPresets[DRUM_LOW_TOM];
    case 39:
      return &drumPresets[DRUM_CLAP];
    default:
      return NULL;
  }
}

#endif
