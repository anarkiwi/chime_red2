// Copyright 2019 Josh Bailey (josh@vandervecken.com)

// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.




#include "Oscillator.h"
#include "constants.h"
#include "types.h"


Oscillator::Oscillator() {
  index = 0;
  Reset();
}

void Oscillator::Reset() {
  audible = false;
  SetFreq(2, 1, 0, 2);
}

inline cr_tick_t Oscillator::ClockRemainder(cr_tick_t masterClock) {
  return masterClockMax - masterClock;
}

cr_tick_t Oscillator::TicksUntilTriggered(cr_tick_t masterClock) {
  if (masterClock > _nextClock) {
    return _nextClock + ClockRemainder(masterClock);
  }
  return _nextClock - masterClock;
}

bool Oscillator::Triggered(cr_tick_t masterClock) {
  return TicksUntilTriggered(masterClock) == 0;
}

void Oscillator::ScheduleNext(cr_tick_t masterClock) {
  _nextClock = masterClock + 1;
  if (_nextClock > masterClockMax) {
    _nextClock = 0;
  }
}

void Oscillator::SetNextTick(cr_tick_t masterClock) {
  cr_tick_t clockRemainder = ClockRemainder(masterClock);
  if (clockRemainder < _clockPeriod) {
    _nextClock = (_clockPeriod - clockRemainder) - 1;
  } else {
    _nextClock += _clockPeriod;
  }
}

void Oscillator::SetFreqLazy(cr_fp_t newHz, cr_fp_t maxHz, cr_fp_t velocityScale) {
  if (hz != newHz) {
    hz = newHz;
    cr_fp_t clockPeriod_fp = roundFixed(cr_fp_t(masterClockHz) / hz);
    _clockPeriod = clockPeriod_fp.getInteger();
    // TODO: avoid division
    pulseUsScale = cr_fp_t(1.0) - (hz / maxHz);
    pulseUsScale *= pulseUsScale;
    pulseUsScale *= velocityScale;
  }
}

void Oscillator::SetFreq(cr_fp_t newHz, cr_fp_t maxHz, cr_fp_t velocityScale, cr_tick_t masterClock) {
  SetFreqLazy(newHz, maxHz, velocityScale);
  ScheduleNext(masterClock);
}
