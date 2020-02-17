// Copyright 2019 Josh Bailey (josh@vandervecken.com)

// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


#include "Oscillator.h"
#include "constants.h"
#include "types.h"


Oscillator::Oscillator() {
  index = 0;
  hz = 0;
  _periodOffset = 0;
  _velocityScale = 0;
  _hzPulseUsScale = 0;
  envelope = NULL;
  Reset();
}

void Oscillator::Reset() {
  audible = false;
  SetFreq(2, 1, 0, 2, 0);
}

inline void Oscillator::_updateNextClock(cr_tick_t newNextClock) {
  _nextClock = newNextClock;
  if (_nextClock > masterClockMax) {
    _nextClock -= masterClockMax;
  }
}

cr_tick_t Oscillator::TicksUntilTriggered(cr_tick_t masterClock, cr_tick_t clockRemainder) {
  if (masterClock > _nextClock) {
    return _nextClock + clockRemainder;
  }
  return _nextClock - masterClock;
}

bool Oscillator::Triggered(cr_tick_t masterClock) {
  return _nextClock == masterClock;
}

void Oscillator::ScheduleNext(cr_tick_t masterClock) {
  _updateNextClock(((masterClock / _clockPeriod) + 1) * _clockPeriod);
}

void Oscillator::ScheduleNow(cr_tick_t masterClock) {
  _updateNextClock(masterClock + 1);
}

cr_tick_t Oscillator::SetNextTick(cr_tick_t masterClock) {
  _updateNextClock(masterClock + _clockPeriod);
  return _clockPeriod;
}

void Oscillator::_computeHzPulseUsScale(cr_fp_t maxHz) {
  if (hz > 1.0) {
    // TODO: avoid division
    _hzPulseUsScale = 1.0 - (hz / maxHz);
    if (_hzPulseUsScale <= 0) {
      _hzPulseUsScale = 0;
    } else if (_hzPulseUsScale > 1.0) {
      _hzPulseUsScale = 1.0;
    }
  } else {
    _hzPulseUsScale = 1.0;
  }
}

bool Oscillator::SetFreqLazy(cr_fp_t newHz, cr_fp_t maxHz, cr_fp_t newVelocityScale, int newPeriodOffset) {
  bool hzChange = hz != newHz || _periodOffset != newPeriodOffset;
  bool velocityChange = hzChange || (_velocityScale != newVelocityScale);

  if (hzChange) {
    hz = newHz;
    _periodOffset = newPeriodOffset;
    cr_fp_t clockPeriod_fp = roundFixed(cr_fp_t(masterClockHz) / hz);
    _clockPeriod = clockPeriod_fp.getInteger();
    if (_clockPeriod + _periodOffset > 0) {
      _clockPeriod = _clockPeriod + _periodOffset;
    } else {
      _clockPeriod = 1;
    }
    _computeHzPulseUsScale(maxHz);
  }
  if (velocityChange) {
    _velocityScale = newVelocityScale;
    pulseUsScale = _hzPulseUsScale * _velocityScale;
  }
  return hzChange || velocityChange;
}

bool Oscillator::SetFreq(cr_fp_t newHz, cr_fp_t maxHz, cr_fp_t newVelocityScale, cr_tick_t masterClock, int newPeriodOffset) {
  if (SetFreqLazy(newHz, maxHz, newVelocityScale, newPeriodOffset)) {
    if (hz > 1.0) {
      ScheduleNext(masterClock);
    } else {
      ScheduleNow(masterClock);
    }
  }
}
