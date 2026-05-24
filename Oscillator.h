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
  cr_tick_t TicksUntilTriggered(cr_tick_t masterClock, cr_tick_t clockRemainder);
  bool Triggered(cr_tick_t masterClock);
  bool SetFreq(cr_hzinv_t newHzInv, uint8_t newPitch, cr_fp_t newVelocityScale, cr_tick_t masterClock, int periodOffset);
  bool SetFreqLazy(cr_hzinv_t newHzInv, uint8_t newPitch, cr_fp_t newVelocityScale, int periodOffset);
  void SetMaxPitch(uint8_t maxPitch);
  cr_tick_t SetNextTick(cr_tick_t masterClock);
  void ScheduleNow(cr_tick_t masterClock);
  void Reset();
  // Channel-10 pitched noise. SetNoiseRange stamps the period window [pMin,
  // pMin+span] (precomputed from pitchToPeriod[] at note on); ApplyNoisePeriod
  // drops a fresh random period inside it. The mutation is lazy (it only sets
  // _clockPeriod, like SetFreqLazy) -- it takes effect at the oscillator's next
  // SetNextTick with no reschedule, which is the cheapest possible update.
  void SetNoiseRange(cr_tick_t pMin, cr_tick_t span) { _noisePMin = pMin; _noiseSpan = span; }
  cr_tick_t noiseSpan() const { return _noiseSpan; }
  void ApplyNoisePeriod(cr_tick_t offset) {
    cr_tick_t period = _noisePMin + offset;
    _clockPeriod = period ? period : 1;
  }
  cr_hzinv_t hzInv;
  cr_fp_t pulseUsScale;
  bool audible;
  AdsrEnvelope *envelope;
  uint8_t index;
#ifdef CR_HOST_TEST
  // Read-only period introspection for host MIDI tests (test/host/test_midi.cpp).
  // Its only caller is the test, which build.sh's repo-root cppcheck does not
  // see, so suppress the resulting unused-function report.
  // cppcheck-suppress unusedFunction
  cr_tick_t TestClockPeriod() const { return _clockPeriod; }
  // Per-oscillator trigger census for the scheduler characterization test
  // (test_midi.cpp TestSchedulerCadence). SetNextTick() -- called exactly when
  // this oscillator triggers and is rescheduled in OscillatorController::
  // _Reschedule() -- bumps it, so the test can pin each voice's trigger cadence
  // (= masterClockHz / period) independent of the scheduling algorithm. That is
  // the behavioural safety net for a future _Reschedule rewrite (e.g. scanning
  // only active voices, or a next-trigger heap).
  unsigned long testTriggerCount = 0;
#endif
 private:
  void _updateNextClock(cr_tick_t offset);
  void _computeHzPulseUsScale(uint8_t newPitch);
  int _periodOffset;
  cr_fp_t _hzPulseUsScale;
  cr_fp_t _velocityScale;
  cr_tick_t _clockPeriod;
  cr_tick_t _nextClock;
  cr_fp_t _maxHz;
  cr_fp_t _maxHzInv;
  cr_tick_t _noisePMin;
  cr_tick_t _noiseSpan;
};

#endif
