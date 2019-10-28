// Copyright 2019 Josh Bailey (josh@vandervecken.com)

// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


#ifndef OSCILLATOR_H
#define OSCILLATOR_H

#include "types.h"
#include "AdsrEnvelope.h"


class Oscillator {
 public:
  Oscillator();
  cr_tick_t TicksUntilTriggered(cr_tick_t masterClock);
  bool Triggered(cr_tick_t masterClock);
  void SetFreq(cr_fp_t newHz, cr_fp_t maxHz, cr_fp_t newVelocityScale, cr_tick_t masterClock);
  void SetFreqLazy(cr_fp_t newHz, cr_fp_t maxHz, cr_fp_t newVelocityScale);
  void SetNextTick(cr_tick_t masterClock);
  void ScheduleNext(cr_tick_t masterClock);
  void Reset();
  cr_fp_t hz;
  cr_fp_t pulseUsScale;
  bool audible;
  AdsrEnvelope *envelope;
  uint8_t index;
 private:
  cr_fp_t _hzPulseUsScale;
  cr_fp_t _velocityScale;
  cr_tick_t ClockRemainder(cr_tick_t masterClock);
  cr_tick_t _clockPeriod;
  cr_tick_t _nextClock;
};

#endif
