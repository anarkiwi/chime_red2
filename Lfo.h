// Copyright 2019 Josh Bailey (josh@vandervecken.com)

// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


#ifndef LFO_H
#define LFO_H

#include "types.h"
#include "constants.h"
#include "wavetable.h"


class Lfo {
 public:
  Lfo();
  void Reset();
  void Restart();
  void SetTable(uint8_t value);
  void Tick();
  void SetHz(cr_fp_t hz);
  // Tempo-sync: bind one full LFO cycle to a fixed number of incoming MIDI clock
  // pulses (24 PPQN). 0 returns the LFO to free-running (the SetHz/Tick path).
  // Called at control rate from a CC handler -- the one division here is fine.
  void SetClockSync(uint16_t clocksPerCycle);
  // Advance the LFO phase by exactly one MIDI clock pulse. Integer-only and
  // allocation-free: this runs in the master-ISR context (CRMidi::handleClock,
  // reached from MIDI.read()), so it must never touch soft-float or divide.
  void ClockStep();
  cr_fp_t Level();
#ifdef CR_HOST_TEST
  // Read-only phase introspection for the host clock-sync test
  // (test/host/test_midi.cpp TestClockSyncLfo), not used by any scanned TU.
  // cppcheck-suppress unusedFunction
  uint16_t TestTablePos() const { return _tablePos; }
  // cppcheck-suppress unusedFunction
  bool TestClockSynced() const { return _clocksPerCycle != 0; }
#endif
 private:
  cr_fp_t _hz;
  uint16_t _ticks;
  uint16_t _tickStep;
  uint16_t _tablePosPerTick;
  uint16_t _tablePos;
  cr_fp_t *_table;
  // Clock-sync state (0 == free-run). The per-clock phase advance is held as a
  // whole + fractional (Bresenham) table-position step so one cycle spans
  // exactly _clocksPerCycle pulses with no drift, using only integer adds.
  uint16_t _clocksPerCycle;
  uint16_t _clockStepWhole;
  uint16_t _clockStepFracNum;
  uint16_t _clockFracAcc;
};

#endif
