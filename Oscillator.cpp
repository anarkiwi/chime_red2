// Copyright 2019 Josh Bailey (josh@vandervecken.com)

// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


#include "Oscillator.h"
#include "constants.h"
#include "types.h"
#include "wavetable.h"

// Delta-sigma fractional-period scheduler fixed-point constants. The fractional
// period is Q(cr_hzinv_t::FractionSize) = Q30 (same resolution as hzInv), so one
// whole tick is kFracOneQ30 and half a tick is kFracHalfQ30. The FM bend is
// computed in cr_fp_t (Q15) and promoted to Q30 by a lossless left shift of
// kFracToFp bits. All compile-time constants; no runtime float.
static const uint32_t kFracOneQ30 = uint32_t(1) << cr_hzinv_t::FractionSize;
static const uint32_t kFracHalfQ30 = uint32_t(1) << (cr_hzinv_t::FractionSize - 1);
static const uint32_t kFracMaskQ30 = kFracOneQ30 - 1;
static const int kFracToFp = cr_hzinv_t::FractionSize - cr_fp_t::FractionSize;

Oscillator::Oscillator() : hzInv(0), envelope(0), modEnvelope(0), index(0), _periodOffset(0), _hzPulseUsScale(0), _velocityScale(0), _noisePMin(0), _noiseSpan(0), _fmDepth(0), _fmPhaseStep(0), _fmPhase(0), _periodFracQ30(0), _periodRemQ30(0) {
  Reset();
}

void Oscillator::Reset() {
  audible = false;
  _noisePMin = 0;
  _noiseSpan = 0;
  // Drop any FM left on an oscillator recycled from the free pool; note on
  // re-stamps it via SetFM, so a default (non-FM) voice always has it cleared.
  _fmDepth = 0;
  _fmPhaseStep = 0;
  _fmPhase = 0;
  SetFreq(1, 1, 0, 2, 0);
}

inline void Oscillator::_updateNextClock(cr_tick_t newNextClock) {
  _nextClock = newNextClock;
  if (_nextClock > masterClockMax) {
    _nextClock -= masterClockHz;
  }
}

cr_tick_t Oscillator::TicksUntilTriggered(cr_tick_t masterClock, cr_tick_t clockRemainder) {
  if (_nextClock >= masterClock) {
    return _nextClock - masterClock;
  }
  // account for 0th tick on rollover.
  return _nextClock + (clockRemainder + 1);
}

bool Oscillator::Triggered(cr_tick_t masterClock) {
  return _nextClock == masterClock;
}

void Oscillator::ScheduleNow(cr_tick_t masterClock) {
  // Schedule on next odd numbered clock tick to minimize destructive interference.
  cr_tick_t newNextClock = (masterClock + 1) | 1;
  _updateNextClock(newNextClock);
}

cr_tick_t Oscillator::SetNextTick(cr_tick_t masterClock) {
#ifdef CR_HOST_TEST
  ++testTriggerCount;  // scheduler-cadence characterization test census
#endif
  // Exact target period for this cycle as Q30 ticks. _clockPeriod is the ROUNDED
  // period; un-round it to the truncated integer base so the carry only ever adds
  // the non-negative fraction (never subtracts). For an exact-integer period or
  // pitched noise (_periodFracQ30 == 0) this leaves base == _clockPeriod.
  cr_tick_t base = _clockPeriod;
  if (_periodFracQ30 >= kFracHalfQ30) {
    base -= 1;
  }
  int64_t targetQ30 =
      (static_cast<int64_t>(base) << cr_hzinv_t::FractionSize) + _periodFracQ30;

  // FM carrier: bend the next period by (1 + depth*sin), the modulator walking the
  // sine table once per carrier cycle. Applied in the Q30 fractional domain (not
  // rounded to whole ticks), so even a sub-tick bend survives -- its fraction
  // accumulates in the carry below instead of rounding to zero, which is what used
  // to kill FM at high carrier / low index. depth < fmMaxDepth (<1) so the period
  // can't reach 0. _fmPhaseStep is pre-reduced into [0, maxLfoTable] at config
  // time (MidiChannel::_FmPhaseStep), so the wrap is one subtract -- no ISR loop.
  if (_fmPhaseStep && base < maxClockPeriod) {
    _fmPhase += _fmPhaseStep;
    if (_fmPhase > maxLfoTable) {
      _fmPhase -= (maxLfoTable + 1);
    }
    cr_fp_t depth = _fmDepth;
    if (modEnvelope && !modEnvelope->isNull) {
      depth *= modEnvelope->level;
    }
    cr_fp_t delta = cr_fp_t(static_cast<int>(base)) * depth * SineTable[_fmPhase];
    // Promote the Q15 bend to Q30 (multiply, not shift, since delta may be
    // negative) and add it to the fractional target.
    targetQ30 += static_cast<int64_t>(delta.getInternal()) *
                 (static_cast<int64_t>(1) << kFracToFp);
    if (targetQ30 < static_cast<int64_t>(kFracOneQ30)) {
      targetQ30 = kFracOneQ30;  // clamp to >= 1 tick
    }
  }

  // Delta-sigma carry: emit an integer tick count whose long-term average equals
  // the exact (fractional / FM-modulated) target, so the realized frequency hits
  // the exact pitch instead of the single-clock-rounded one. Each pulse still
  // lands on a whole tick (intPeriod is base or base+1). Commit the accumulated
  // debt only when the pulse is audible; freeze it otherwise so an inaudible span
  // (period >= maxClockPeriod) can't build up phantom debt.
  cr_tick_t intPeriod =
      static_cast<cr_tick_t>(targetQ30 >> cr_hzinv_t::FractionSize);
  uint32_t nextRem =
      _periodRemQ30 + static_cast<uint32_t>(targetQ30 & kFracMaskQ30);
  if (nextRem >= kFracOneQ30) {
    nextRem -= kFracOneQ30;
    ++intPeriod;
  }
  if (intPeriod < 1) {
    intPeriod = 1;
  }
  if (intPeriod < maxClockPeriod) {
    _periodRemQ30 = nextRem;
    _updateNextClock(masterClock + intPeriod);
  } else {
    _updateNextClock(0);
  }
  return intPeriod;
}

void Oscillator::SetMaxPitch(uint8_t maxPitch) {
  _maxHz = pitchToHz[maxPitch];
  // _maxHzInv only scales pulse width (_computeHzPulseUsScale), which does not
  // need the extra precision, so keep it as cr_fp_t.
  _maxHzInv = static_cast<cr_fp_t>(pitchToHzInv[maxPitch]);
}

void Oscillator::_computeHzPulseUsScale(uint8_t newPitch) {
  if (newPitch) {
    // _maxHzInv is the reciprocal of the max frequency, so this is a multiply,
    // not a divide -- pitchToHz[newPitch] / maxHz expressed division-free.
    _hzPulseUsScale = 1.0 - (pitchToHz[newPitch] * _maxHzInv);
    if (_hzPulseUsScale <= 0) {
      _hzPulseUsScale = 0;
    } else if (_hzPulseUsScale > 1.0) {
      _hzPulseUsScale = 1.0;
    }
  } else {
    _hzPulseUsScale = 1.0;
  }
}

bool Oscillator::SetFreqLazy(cr_hzinv_t newHzInv, uint8_t newPitch, cr_fp_t newVelocityScale, int newPeriodOffset) {
  bool hzChange = hzInv != newHzInv || _periodOffset != newPeriodOffset;
  bool velocityChange = hzChange || (_velocityScale != newVelocityScale);

  if (hzChange) {
    hzInv = newHzInv;
    _periodOffset = newPeriodOffset;
    // Exact fractional period for the delta-sigma scheduler. The old plain-note
    // fast path (pitchToPeriod[newPitch]) is dropped: with the 30-bit cr_hzinv_t
    // its rounded integer is bit-identical for plain notes, but it discarded the
    // sub-tick fraction the carry needs to dither the average onto the exact
    // pitch. _clockPeriod stays the ROUNDED period (ch10 noise bounds /
    // TestClockPeriod / the scheduler-cadence test depend on it); SetNextTick
    // un-rounds it via _periodFracQ30.
    // _clockPeriod is the ROUNDED period (round-to-nearest tick) via the canonical
    // helper; _periodFracQ30 is the sub-tick remainder from the same product. The
    // SetNextTick carry un-rounds _clockPeriod with that fraction, so the two stay
    // consistent (rounded = floor + (frac >= half)). Detune is whole ticks.
    long rounded = static_cast<long>(hzInvToTicks(masterClockHz, hzInv)) + _periodOffset;
    uint32_t fracQ30 = hzInvToPeriod(masterClockHz, hzInv).fracPart;
    if (rounded < 1) {
      rounded = 1;
      fracQ30 = 0;
    }
    _clockPeriod = static_cast<cr_tick_t>(rounded);
    _periodFracQ30 = fracQ30;
    // Seed the carry at half a tick so a short/percussive note's first cycle is
    // not systematically rounded the same way (faster convergence; the one-time
    // seed does not affect the long-term average).
    _periodRemQ30 = fracQ30 >> 1;
    _computeHzPulseUsScale(newPitch);
  }
  if (velocityChange) {
    _velocityScale = newVelocityScale;
    pulseUsScale = _hzPulseUsScale * _velocityScale;
  }
  return hzChange || velocityChange;
}

bool Oscillator::SetFreq(cr_hzinv_t newHzInv, uint8_t newPitch, cr_fp_t newVelocityScale, cr_tick_t masterClock, int newPeriodOffset) {
  if (SetFreqLazy(newHzInv, newPitch, newVelocityScale, newPeriodOffset)) {
    ScheduleNow(masterClock);
    return true;
  }
  return false;
}
