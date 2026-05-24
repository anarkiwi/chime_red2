// Copyright 2019 Josh Bailey (josh@vandervecken.com)

// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


#include "Oscillator.h"
#include "constants.h"
#include "types.h"
#include "wavetable.h"


Oscillator::Oscillator() : hzInv(0), envelope(0), modEnvelope(0), index(0), _periodOffset(0), _hzPulseUsScale(0), _velocityScale(0), _noisePMin(0), _noiseSpan(0), _fmDepth(0), _fmPhaseStep(0), _fmPhase(0) {
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
  cr_tick_t period = _clockPeriod;
  // FM carrier: bend the next period by (1 + depth*sin), with the modulator
  // walking the sine table once per carrier cycle. Gated on an in-range base
  // period so cr_fp_t(period) cannot overflow SFixed<16,15>, and so a default
  // voice (phaseStep == 0) keeps the exact original behaviour -- the frequency
  // tests and scheduler-cadence guard exercise that path unchanged. Period-domain
  // (division-free); depth stays < 1 (fmMaxDepth) so the period can't reach 0.
  // _fmPhaseStep is pre-reduced into [0, maxLfoTable] at config time
  // (MidiChannel::_FmPhaseStep), so the table wrap is a single subtract -- no
  // parameter-dependent loop in the ISR.
  if (_fmPhaseStep && period < maxClockPeriod) {
    _fmPhase += _fmPhaseStep;
    if (_fmPhase > maxLfoTable) {
      _fmPhase -= (maxLfoTable + 1);
    }
    cr_fp_t depth = _fmDepth;
    if (modEnvelope && !modEnvelope->isNull) {
      depth *= modEnvelope->level;
    }
    cr_fp_t delta = cr_fp_t(static_cast<int>(period)) * depth * SineTable[_fmPhase];
    int modPeriod = static_cast<int>(period) + roundFixed(delta).getInteger();
    period = modPeriod > 0 ? static_cast<cr_tick_t>(modPeriod) : 1;
  }
  if (period < maxClockPeriod) {
    _updateNextClock(masterClock + period);
  } else {
    _updateNextClock(0);
  }
  return period;
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
    // A plain note (default detune, no pitch bend) arrives as exactly the table
    // entry, so use its precomputed optimal period and skip the lossy
    // masterClockHz * (1/f) fixed-point multiply (1/f truncated to 15 fractional
    // bits costs up to ~100 cents near 2 kHz). Detuned/bent notes and arbitrary
    // frequencies (hzInv != table) fall back to the multiply as before. (Within
    // the playable range each note has a distinct hzInv; above ~note 98 adjacent
    // semitones share an hzInv, so this dispatch would need to key on pitch too.)
    if (newPitch <= absMaxMidiPitch && newHzInv == pitchToHzInv[newPitch]) {
      _clockPeriod = pitchToPeriod[newPitch];
    } else {
      _clockPeriod = hzInvToTicks(masterClockHz, hzInv);
    }
    if (_clockPeriod + _periodOffset > 0) {
      _clockPeriod = _clockPeriod + _periodOffset;
    } else {
      _clockPeriod = 1;
    }
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
