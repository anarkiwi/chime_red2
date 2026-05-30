// Copyright 2019 Josh Bailey (josh@vandervecken.com)

// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


#ifndef CONSTANTS_H
#define CONSTANTS_H

const cr_fp_t minBreakoutUs = 16;
const cr_fp_t maxBreakoutUs = 48;
const cr_fp_t pulseWindowUs = 120;
const uint8_t pulseGuardTicks = 1;

const uint8_t oscillatorCount = 16;
const cr_tick_t masterClockHz = 52631;
// Compile-time constant (1e6/52631 = 19.0002 -> 19), folded at build time, not a
// runtime FP op; exempt from the soft-float gate (test/host/softfloat_guard.sh).
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-conversion"
const cr_tick_t masterClockPeriodUs = 1e6 / masterClockHz;
#pragma GCC diagnostic pop
const cr_tick_t masterClockMax = masterClockHz - 1;
const cr_tick_t maxClockPeriod = 8192;

const cr_slowtick_t controlClockRelHz = 10;
const cr_slowtick_t controlClockHz = masterClockHz / controlClockRelHz;
const cr_slowtick_t controlClockMax = controlClockRelHz - 1;
// Compile-time constant: this double->float narrowing is folded by the compiler,
// not a runtime floating-point op (which would be software-emulated on the Due's
// FPU-less Cortex-M3). Exempt just this line from the soft-float regression gate
// (test/host/softfloat_guard.sh). NB controlClockTickMs is still a `float`, so the
// `cr_fp_t += controlClockTickMs` at runtime (MidiNote/AdsrEnvelope HandleControl)
// does a float->fixed conversion each control tick -- a flagged, not-yet-taken
// optimization is to make this a cr_fp_t constant.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdouble-promotion"
#pragma GCC diagnostic ignored "-Wfloat-conversion"
const float controlClockTickMs = 1e3 / float(controlClockHz);
#pragma GCC diagnostic pop
// Fixed-point sibling of controlClockTickMs for the runtime per-control-tick age
// accumulation (AdsrEnvelope / MidiNote HandleControl). Converting the float once,
// here, makes those `cr_fp_t += ...` updates pure fixed-point adds instead of a
// float->fixed conversion every control tick -- which is software-emulated on the
// FPU-less Cortex-M3 (Arduino Due). controlClockTickMs stays float for the
// compile-time AdsrCurveLevelStep[] table initialiser.
const cr_fp_t controlClockTickMsFp = cr_fp_t(controlClockTickMs);

const cr_slowtick_t lfoClockRelHz = 50;
const cr_slowtick_t lfoClockHz = masterClockHz / lfoClockRelHz;
const cr_slowtick_t lfoClockMax = lfoClockRelHz - 1;
const cr_tick_t lfoTickMax = lfoClockHz - 1;
const uint16_t maxLfoTable = 1e3 - 1;
const cr_fp_t lfoMaxHz = 10;

const uint8_t stdMidiChannels = 16;
const uint8_t maxMidiChannel = 8;
const uint8_t midiChannelStorage = maxMidiChannel + 1; // +1 reserved for ch 10 automatically
const cr_fp_t maxMidiPitchBend = 8192;
const uint8_t absMaxMidiPitch = 127;
const uint8_t maxMidiPitch = 96;
const uint8_t maxMidiVal = 127;
const uint8_t midiPitchBendRange = 12;

// FM-bell feature: peak period-swing fraction at full modulation index. Kept
// below 1 so the period-domain modulation in Oscillator::SetNextTick can never
// drive the period to <= 0. Compile-time constant (folded), so this is not a
// runtime float op; exempt this one line from the soft-float gate like the other
// scalar/table consts above (test/host/softfloat_guard.sh).
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdouble-promotion"
#pragma GCC diagnostic ignored "-Wfloat-conversion"
const cr_fp_t fmMaxDepth = cr_fp_t(0.9);
#pragma GCC diagnostic pop

const uint8_t lfoCount = 3;

// MIDI beat clock is 24 pulses per quarter note (PPQN). For tempo-synced LFOs a
// CC selects a musical division from this table: the value indexes the number of
// clock pulses in one full LFO cycle. Index 0 == free-run (the SetHz/Tick path).
// All entries fold to integers at compile time (no runtime FP -- soft-float gate
// safe). See CRMidi handleControlChange (the sync CCs) and Lfo::SetClockSync.
const uint16_t midiClockPpqn = 24;
const uint16_t lfoSyncClocks[] = {
    0,                       // 0: free-run
    midiClockPpqn * 4,       // 1: whole note    (1/1)   = 96
    midiClockPpqn * 2,       // 2: half          (1/2)   = 48
    (midiClockPpqn * 3) / 2, // 3: dotted quarter (1/4.) = 36
    midiClockPpqn,           // 4: quarter       (1/4)   = 24
    (midiClockPpqn * 2) / 3, // 5: quarter triplet (1/4T) = 16
    midiClockPpqn / 2,       // 6: eighth        (1/8)   = 12
    midiClockPpqn / 4,       // 7: sixteenth     (1/16)  = 6
};
const uint8_t lfoSyncMax = (sizeof(lfoSyncClocks) / sizeof(lfoSyncClocks[0])) - 1;

#include "miditables.h"

#endif
