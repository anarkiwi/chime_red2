// Copyright 2019 Josh Bailey (josh@vandervecken.com)

// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


#ifndef TYPES_H
#define TYPES_H

// cppcheck-suppress missingIncludeSystem
#include <stdint.h>
// cppcheck-suppress missingIncludeSystem
#include <ArduinoSTL.h>
// cppcheck-suppress missingIncludeSystem
#include <iterator>
// cppcheck-suppress missingIncludeSystem
#include <stack>
// cppcheck-suppress missingIncludeSystem
#include <deque>
// cppcheck-suppress missingIncludeSystem
#include <FixedPoints.h>
// cppcheck-suppress missingIncludeSystem
#include <FixedPointsCommon.h>

typedef uint16_t cr_pulse_t;
typedef uint32_t cr_tick_t;
typedef uint32_t cr_slowtick_t;
typedef SFixed<16, 15> cr_fp_t;

// Inverse frequency (1/Hz). Always <= ~1.06 (note 0 is 1 Hz, plus detune
// headroom), so the 16 integer bits cr_fp_t spends are wasted here -- they buy
// nothing and cost precision. cr_hzinv_t reallocates them: same 32-bit storage
// as cr_fp_t, but 30 fractional bits instead of 15. That drops the 1/f
// truncation error from ~6% (≈100 cents) near 2 kHz to ~2e-6 (<0.01 cents), so
// detuned / pitch-bent / arbitrary frequencies hit the single-clock floor like
// plain notes do. (Plain notes still use the exact pitchToPeriod[] table.)
typedef SFixed<1, 30> cr_hzinv_t;

// The exact master-clock period for an inverse frequency, split into its integer
// and fractional parts. clockHz needs ~16 integer bits, so the product does not
// fit a 32-bit fixed-point type; compute it once in 64-bit integer from hzInv's
// raw Q30 value (no runtime divide). The split is by TRUNCATION (floor), so
// fracPart is always >= 0 -- the oscillator's delta-sigma fractional-period
// scheduler carries fracPart forward across pulses to dither the average onto the
// exact frequency, and an unsigned carry depends on a non-negative remainder.
// fracPart is in units of 1/2^FractionSize ticks, i.e. Q(FractionSize). hzInv >= 0.
struct cr_period_t {
  cr_tick_t intPart;   // floor(period) in whole master ticks
  uint32_t fracPart;   // sub-tick remainder, Q(cr_hzinv_t::FractionSize), [0, 2^FractionSize)
};

inline cr_period_t hzInvToPeriod(cr_tick_t clockHz, cr_hzinv_t hzInv) {
  const int64_t scaled =
      static_cast<int64_t>(clockHz) * static_cast<int64_t>(hzInv.getInternal());
  cr_period_t p;
  p.intPart = static_cast<cr_tick_t>(scaled >> cr_hzinv_t::FractionSize);
  p.fracPart = static_cast<uint32_t>(
      scaled & ((static_cast<int64_t>(1) << cr_hzinv_t::FractionSize) - 1));
  return p;
}

// round(clockHz * hzInv) -- the period rounded to the nearest whole tick. Still
// used where a single integer period is wanted (the pitch tables / tests). The
// fractional scheduler uses hzInvToPeriod above instead.
inline cr_tick_t hzInvToTicks(cr_tick_t clockHz, cr_hzinv_t hzInv) {
  const int64_t scaled =
      static_cast<int64_t>(clockHz) * static_cast<int64_t>(hzInv.getInternal());
  const int64_t half = static_cast<int64_t>(1) << (cr_hzinv_t::FractionSize - 1);
  return static_cast<cr_tick_t>((scaled + half) >> cr_hzinv_t::FractionSize);
}

#endif
