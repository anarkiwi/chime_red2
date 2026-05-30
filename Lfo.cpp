// Copyright 2019 Josh Bailey (josh@vandervecken.com)

// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


#include "Lfo.h"

Lfo::Lfo() {
  Reset();
  Restart();
  SetHz(1.0);
}

void Lfo::SetTable(uint8_t value) {
  switch (value) {
    case 2:
      _table = (cr_fp_t*)UpSawTable;
      break;
    case 1:
      _table = (cr_fp_t*)DownSawTable;
      break;
    case 0:
    default:
      _table = (cr_fp_t*)SineTable;
      break;
  }
}

void Lfo::Reset() {
  SetTable(0);
  _clocksPerCycle = 0;
  _clockStepWhole = 0;
  _clockStepFracNum = 0;
  _clockFracAcc = 0;
}

void Lfo::Restart() {
  _tablePos = 0;
  _ticks = 0;
  _clockFracAcc = 0;
}

void Lfo::SetClockSync(uint16_t clocksPerCycle) {
  _clocksPerCycle = clocksPerCycle;
  if (clocksPerCycle) {
    // Spread the (maxLfoTable + 1)-entry table across one cycle of clock pulses.
    // Integer split into whole + remainder so ClockStep stays division-free.
    _clockStepWhole = (maxLfoTable + 1) / clocksPerCycle;
    _clockStepFracNum = (maxLfoTable + 1) % clocksPerCycle;
  } else {
    _clockStepWhole = 0;
    _clockStepFracNum = 0;
  }
  Restart();
}

void Lfo::ClockStep() {
  if (!_clocksPerCycle) {
    return;  // free-running LFO: advanced by Tick(), not the MIDI clock.
  }
  _tablePos += _clockStepWhole;
  _clockFracAcc += _clockStepFracNum;
  if (_clockFracAcc >= _clocksPerCycle) {
    _clockFracAcc -= _clocksPerCycle;
    ++_tablePos;
  }
  // One pulse advances the table by at most (maxLfoTable+1)/min-division + 1, so
  // this wraps at most once -- bounded, ISR-safe work.
  while (_tablePos > maxLfoTable) {
    _tablePos -= maxLfoTable + 1;
  }
}

void Lfo::SetHz(cr_fp_t hz) {
  _hz = hz;
  if (hz >= 1) {
    _tablePosPerTick = roundFixed(_hz).getInteger();
    _tickStep = 1;
  } else {
    _tablePosPerTick = 1;
    _tickStep = roundFixed(cr_fp_t(1.0) / _hz).getInteger();
  }
  _ticks = 0;
}

void Lfo::Tick() {
  if (_clocksPerCycle) {
    return;  // clock-synced: phase advances on MIDI clock (ClockStep), not here.
  }
  if (++_ticks == _tickStep) {
    _tablePos += _tablePosPerTick;
    if (_tablePos > maxLfoTable) {
      _tablePos -= maxLfoTable + 1;
    }
    _ticks = 0;
  }
}

cr_fp_t Lfo::Level() {
  return _table[_tablePos];
}
