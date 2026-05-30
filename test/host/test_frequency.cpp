// Copyright 2019 Josh Bailey (josh@vandervecken.com)

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

// Host-side unit tests for the synth's note-frequency accuracy.
//
// These tests compile the real Oscillator.cpp and the real pitch tables
// (constants.h / miditables.h) with a stock g++ on the CI runner, using tiny
// stubs for the Arduino-only headers (see test/host/stubs/). The fixed-point
// math is pure integer arithmetic, bit-identical on x86 and on the Due/SAMD
// targets -- which is why measuring on the host is valid.
//
// An oscillator can only fire a coil pulse on a whole master-clock tick, so any
// SINGLE period is an integer masterClockHz / N. Naively that caps accuracy at
// the single-master-clock rounding floor (~33 cents at 2 kHz, where N is
// small). The oscillator beats that with a delta-sigma fractional-period
// scheduler (Oscillator::SetNextTick): it keeps the exact fractional period and
// carries the rounding remainder forward, so the integer periods DITHER (e.g.
// 25, 26, 25, 25, 26 ...) and the long-term AVERAGE frequency is exact while
// every pulse still lands on a clean tick. So these tests assert on the AVERAGE
// over many pulses (near-exact), plus the per-pulse bound (within one tick),
// plus that FM modulation now survives at high carrier / low index (it used to
// round to zero).

#include "Oscillator.h"
#include "constants.h"
#include "types.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

namespace {

int g_failures = 0;

#define CHECK(cond, ...)                                                       \
  do {                                                                         \
    if (!(cond)) {                                                             \
      ++g_failures;                                                            \
      std::printf("  FAIL (%s:%d): ", __FILE__, __LINE__);                     \
      std::printf(__VA_ARGS__);                                                \
      std::printf("\n");                                                       \
    }                                                                          \
  } while (0)

// Upper limit of the characterised range: the synth's highest playable MIDI
// note (maxMidiPitch) and its frequency. Derived from the code's own constant
// so the test tracks the playable range if maxMidiPitch changes (today note 96
// ~2093 Hz).
const uint8_t kMaxNote = maxMidiPitch;
const double kMaxHz = static_cast<double>(pitchToHz[maxMidiPitch]);

// With the delta-sigma scheduler the AVERAGE realized frequency hits the exact
// pitch -- the single-clock rounding floor (was ~33 cents at 2 kHz) is gone
// from the mean. The residual is hzInv truncation (<0.01 cents) plus the
// finite-N averaging tail, so a couple of cents is a generous regression guard.
// A failure means the carry was lost (back to the rounding floor) or hzInv
// precision regressed.
const double kAvgCentsBound = 2.0;
const int kAvgPulses = 4096; // pulses to average; residual ~ 1/N tick

double ToCents(double realized, double target) {
  return 1200.0 * std::log2(realized / target);
}

double RealizedHz(double period) {
  return static_cast<double>(masterClockHz) / period;
}

// The exact (fractional) period the scheduler targets for an inverse frequency:
// masterClockHz * hzInv, in master ticks. The dithered integer periods average
// to this.
double ExactPeriod(cr_hzinv_t hzInv) {
  return static_cast<double>(masterClockHz) * static_cast<double>(hzInv);
}

// Drive the real oscillator for n pulses and return the per-pulse integer
// periods it schedules. Advances a master clock by each period (exercising the
// wrap) -- though the returned sequence depends only on the carry state, not
// the clock.
std::vector<cr_tick_t> CollectPeriods(Oscillator &osc, cr_hzinv_t hzInv,
                                      uint8_t pitch, int n) {
  osc.SetFreqLazy(hzInv, pitch, /*velocityScale=*/0, /*periodOffset=*/0);
  std::vector<cr_tick_t> out;
  out.reserve(n);
  cr_tick_t clk = 0;
  for (int i = 0; i < n; ++i) {
    cr_tick_t p = osc.SetNextTick(clk);
    out.push_back(p);
    clk += p;
    while (clk > masterClockMax) {
      clk -= masterClockHz;
    }
  }
  return out;
}

double Mean(const std::vector<cr_tick_t> &v) {
  double s = 0.0;
  for (cr_tick_t p : v) {
    s += static_cast<double>(p);
  }
  return s / static_cast<double>(v.size());
}

double AvgPeriod(Oscillator &osc, cr_hzinv_t hzInv, uint8_t pitch = 0) {
  return Mean(CollectPeriods(osc, hzInv, pitch, kAvgPulses));
}

// Guard the precomputed period table: pitchToPeriod[] is generated for a fixed
// masterClockHz; if masterClockHz changes without regenerating it, every note
// silently detunes. (Build-time table guard -- independent of the scheduler.)
void TestPeriodTable() {
  std::printf("[period table] pitchToPeriod == round(masterClockHz / "
              "pitchToHz)\n");
  for (int note = 0; note <= absMaxMidiPitch; ++note) {
    double f = static_cast<double>(pitchToHz[note]);
    long want = std::lround(static_cast<double>(masterClockHz) / f);
    if (want < 1) {
      want = 1;
    }
    CHECK(pitchToPeriod[note] == static_cast<cr_tick_t>(want),
          "pitchToPeriod[%d]=%lu != round(masterClockHz/%.4f)=%ld -- "
          "regenerate table",
          note, (unsigned long)pitchToPeriod[note], f, want);
  }
}

// The dithered per-pulse periods average to the EXACT fractional period
// masterClockHz * hzInv -- not its rounded integer. This is the core invariant
// of the delta-sigma scheduler; the accuracy tests below are its musical
// reading.
void TestPeriodAverageExact() {
  std::printf("[period avg] dithered period averages to exact masterClockHz * "
              "hzInv\n");
  Oscillator osc;
  osc.SetMaxPitch(maxMidiPitch);
  double worst = 0.0;
  for (double f = 20.0; f <= kMaxHz + 1e-9; f += 0.5) {
    cr_hzinv_t hzInv = cr_hzinv_t(1.0 / f);
    double avg = AvgPeriod(osc, hzInv);
    double exact = ExactPeriod(hzInv);
    double err = std::fabs(avg - exact);
    if (err > worst) {
      worst = err;
    }
    CHECK(err < 0.05,
          "f=%.2f Hz: average period %.5f != exact %.5f (err %.5f ticks)", f,
          avg, exact, err);
  }
  std::printf("  worst average-period error: %.6f ticks (over %d pulses)\n",
              worst, kAvgPulses);
}

// Worst-case uncertainty for the notes the synth plays (default detune, no
// pitch bend): the averaged realized frequency vs the table's intended pitch.
void TestNoteAccuracy() {
  std::printf("[per-note] averaged realized vs intended frequency, every "
              "playable MIDI note (1..%u, up to %.1f Hz)\n",
              kMaxNote, kMaxHz);
  std::printf("  note  targetHz  realizedHz   errCents\n");
  Oscillator osc;
  osc.SetMaxPitch(maxMidiPitch);
  double worst_cents = 0.0;
  int worst_note = 0;
  for (int note = 1; note <= kMaxNote; ++note) {
    double target = static_cast<double>(pitchToHz[note]);
    double realized = RealizedHz(AvgPeriod(osc, pitchToHzInv[note], note));
    double err_cents = ToCents(realized, target);
    std::printf("  %3d  %8.3f  %9.3f  %+7.3f\n", note, target, realized,
                err_cents);
    CHECK(std::fabs(err_cents) <= kAvgCentsBound,
          "note %d (%.2f Hz): averaged |%.3f cents| exceeds bound %.1f", note,
          target, err_cents, kAvgCentsBound);
    if (std::fabs(err_cents) > std::fabs(worst_cents)) {
      worst_cents = err_cents;
      worst_note = note;
    }
  }
  std::printf("  --> worst averaged note error: %+.3f cents at note %d "
              "(was ~33 cents at the single-clock floor)\n",
              worst_cents, worst_note);
}

// Worst-case uncertainty for an arbitrary requested frequency (e.g. a
// pitch-bent note): sweep finely and feed cr_hzinv_t(1/f). The average now hits
// the exact f.
void TestSweepResolution() {
  std::printf("[sweep] averaged uncertainty for arbitrary frequencies up to "
              "%.1f Hz\n",
              kMaxHz);
  Oscillator osc;
  osc.SetMaxPitch(maxMidiPitch);
  double worst_cents = 0.0, at_hz = 0.0;
  for (double f = 20.0; f <= kMaxHz + 1e-9; f += 0.1) {
    double realized = RealizedHz(AvgPeriod(osc, cr_hzinv_t(1.0 / f)));
    double err_cents = ToCents(realized, f);
    if (std::fabs(err_cents) > std::fabs(worst_cents)) {
      worst_cents = err_cents;
      at_hz = f;
    }
  }
  std::printf("  --> worst averaged error: %+.3f cents at %.1f Hz\n",
              worst_cents, at_hz);
  CHECK(std::fabs(worst_cents) <= kAvgCentsBound,
        "sweep worst averaged %.3f cents exceeds bound %.1f", worst_cents,
        kAvgCentsBound);
  // Sanity: dithering really is happening at the top of the range (the period
  // is non-integer there) -- guards against the test silently measuring a
  // degenerate exact-integer case.
  auto periods = CollectPeriods(osc, cr_hzinv_t(1.0 / (kMaxHz - 3.0)), 0, 256);
  cr_tick_t mn = periods[0], mx = periods[0];
  for (cr_tick_t p : periods) {
    mn = std::min(mn, p);
    mx = std::max(mx, p);
  }
  CHECK(mx != mn,
        "expected the period to dither near %.1f Hz, but it was a "
        "constant %lu ticks",
        kMaxHz, (unsigned long)mn);
}

// Lock the worst-case AVERAGED pitch error across the whole operating envelope:
// every playable note (default detune) and every arbitrary frequency a detune
// or pitch bend can request, up to the top note times full +cents detune. The
// delta-sigma average eliminates the single-clock floor; the cap guards that.
void TestWorstCaseAccuracyCap() {
  std::printf("[cap] worst-case AVERAGED pitch error across the operating "
              "envelope\n");
  Oscillator osc;
  osc.SetMaxPitch(maxMidiPitch);
  double worst = 0.0, worstHz = 0.0;
  const char *worstWhat = "";

  for (int note = 1; note <= maxMidiPitch; ++note) {
    double err =
        std::fabs(ToCents(RealizedHz(AvgPeriod(osc, pitchToHzInv[note], note)),
                          static_cast<double>(pitchToHz[note])));
    if (err > worst) {
      worst = err;
      worstHz = static_cast<double>(pitchToHz[note]);
      worstWhat = "note";
    }
  }
  const double topHz = static_cast<double>(pitchToHz[maxMidiPitch]) *
                       static_cast<double>(midiTuneCents[maxMidiVal]);
  for (double f = 20.0; f <= topHz + 1e-9; f += 0.1) {
    double err =
        std::fabs(ToCents(RealizedHz(AvgPeriod(osc, cr_hzinv_t(1.0 / f))), f));
    if (err > worst) {
      worst = err;
      worstHz = f;
      worstWhat = "freq";
    }
  }
  std::printf("  operating range: up to %.1f Hz; worst averaged error: %.3f "
              "cents (at %.1f Hz, %s path)\n",
              topHz, worst, worstHz, worstWhat);
  std::printf("  --> cap = %.2f cents (was the ~33-cent single-clock floor)\n",
              kAvgCentsBound);
  CHECK(worst <= kAvgCentsBound,
        "worst averaged pitch error %.3f cents exceeds cap %.2f -- the "
        "fractional-period carry was lost (back to the single-clock floor)",
        worst, kAvgCentsBound);
}

// Every individual scheduled period stays within one tick of the exact period:
// the coil fires on these, so no single pulse is wildly off even though the
// sequence dithers. This is the per-pulse complement of the average-accuracy
// tests -- it pins the jitter at exactly the single-clock floor while the MEAN
// is exact.
void TestPerPulseBounded() {
  std::printf("[per-pulse] each scheduled period is within one tick of the "
              "exact period\n");
  Oscillator osc;
  osc.SetMaxPitch(maxMidiPitch);
  const int notes[] = {36, 60, 84, static_cast<int>(maxMidiPitch)};
  for (int note : notes) {
    std::vector<cr_tick_t> periods =
        CollectPeriods(osc, pitchToHzInv[note], note, 512);
    cr_tick_t mn = periods[0], mx = periods[0];
    for (cr_tick_t p : periods) {
      mn = std::min(mn, p);
      mx = std::max(mx, p);
    }
    double exact = ExactPeriod(pitchToHzInv[note]);
    CHECK(mx - mn <= 1, "note %d: per-pulse spread %lu..%lu exceeds 1 tick",
          note, (unsigned long)mn, (unsigned long)mx);
    for (cr_tick_t p : periods) {
      CHECK(std::fabs(static_cast<double>(p) - exact) < 1.0 + 1e-9,
            "note %d: period %lu more than one tick from exact %.3f", note,
            (unsigned long)p, exact);
    }
    // Where the exact period is non-integer, the sequence must actually dither.
    if (std::fabs(exact - std::round(exact)) > 0.05) {
      CHECK(
          mx != mn,
          "note %d: exact period %.3f is non-integer but the schedule did not "
          "dither (carry inactive?)",
          note, exact);
    }
    std::printf("  note %3d: exact %.3f ticks, scheduled in {%lu, %lu}\n", note,
                exact, (unsigned long)mn, (unsigned long)mx);
  }
}

// FM modulation now survives at high carrier / low index. The old code added
// round(period * depth * sin) -- a whole-tick bend -- so a sub-tick bend
// rounded to zero and the modulation vanished. The fractional FM bend feeds the
// same delta-sigma carry, so even a <1-tick swing dithers through and the
// period tracks the modulator. Verified by binning periods on the modulator
// phase: the average period is longer on the positive-sine half than the
// negative-sine half (correct FM direction), and the overall average stays on
// the carrier (no DC detune).
void TestFmAverageModulation() {
  std::printf("[fm] sub-tick FM modulation survives at high carrier / low "
              "index (was rounded to zero)\n");
  Oscillator osc;
  osc.SetMaxPitch(maxMidiPitch);
  const uint8_t note = 84; // ~1046 Hz, period ~50 ticks
  const double basePeriod = ExactPeriod(pitchToHzInv[note]);
  // Index small enough that the peak bend is < half a tick -- the old round()
  // would yield zero modulation for every phase.
  const double peakBendTicks = 0.4;
  const cr_fp_t depth = cr_fp_t(peakBendTicks / basePeriod);
  const uint16_t phaseStep = 400; // ratio ~1.4 (round(1.4*1000) % 1000)
  CHECK(peakBendTicks < 0.5, "test setup: peak bend must be sub-tick to prove "
                             "the old code rounded it to zero");

  osc.SetFreqLazy(pitchToHzInv[note], note, 0, 0);
  osc.SetFM(depth, phaseStep);

  const int kCycles = 5000; // multiple of the 5-cycle modulator period
  const double kPi = 3.14159265358979323846;
  double sumPos = 0.0, sumNeg = 0.0, sumAll = 0.0;
  int nPos = 0, nNeg = 0;
  int phase = 0;
  cr_tick_t clk = 0;
  for (int i = 0; i < kCycles; ++i) {
    cr_tick_t p = osc.SetNextTick(clk);
    clk += p;
    while (clk > masterClockMax) {
      clk -= masterClockHz;
    }
    phase = (phase + phaseStep) % (maxLfoTable + 1); // mirror _fmPhase walk
    double s = std::sin(2.0 * kPi * phase / (maxLfoTable + 1));
    sumAll += static_cast<double>(p);
    if (s > 0.3) {
      sumPos += static_cast<double>(p);
      ++nPos;
    } else if (s < -0.3) {
      sumNeg += static_cast<double>(p);
      ++nNeg;
    }
  }
  double avgPos = sumPos / nPos;
  double avgNeg = sumNeg / nNeg;
  double avgAll = sumAll / kCycles;
  std::printf("  carrier ~%.0f Hz, base period %.3f ticks, peak bend %.2f tick "
              "(sub-tick)\n",
              RealizedHz(basePeriod), basePeriod, peakBendTicks);
  std::printf("  avg period: positive-sine half %.4f, negative-sine half %.4f, "
              "overall %.4f\n",
              avgPos, avgNeg, avgAll);
  // Modulation present and in the correct direction: longer period (lower
  // pitch) when the modulator is positive. With the old whole-tick round this
  // difference would be exactly zero.
  CHECK(
      avgPos - avgNeg > 0.1,
      "FM did not modulate the period (pos %.4f - neg %.4f = %.4f) -- sub-tick "
      "bend was lost",
      avgPos, avgNeg, avgPos - avgNeg);
  // No spurious DC detune: averaged over whole modulator cycles, the carrier
  // sits on its base period.
  CHECK(std::fabs(avgAll - basePeriod) < 0.2,
        "FM introduced a DC pitch shift: overall avg %.4f vs base %.4f", avgAll,
        basePeriod);
}

// Pitch bend interpolates in inverse-Hz space (PitchBender::BendHz): a bent
// note is sane -- endpoints land on the note and its bend target, intermediate
// bends rise monotonically and stay strictly between. (The dimensional
// correctness is enforced by the type system: hzInv is cr_hzinv_t, pitchToHz is
// cr_fp_t.)
void TestBendInverseHz() {
  std::printf(
      "[bend] inverse-Hz pitch-bend interpolation is monotonic and bounded\n");
  const uint8_t note = 69; // A4, 440 Hz
  const uint8_t target =
      note + midiPitchBendRange; // bend up 12 semitones -> A5
  const double noteHz = static_cast<double>(pitchToHz[note]);
  const double targetHz = static_cast<double>(pitchToHz[target]);
  const cr_hzinv_t noteInv = pitchToHzInv[note];
  const cr_hzinv_t targetInv = pitchToHzInv[target];
  std::printf("  note %u (%.2f Hz) bending up to note %u (%.2f Hz)\n", note,
              noteHz, target, targetHz);
  Oscillator osc;
  osc.SetMaxPitch(maxMidiPitch);
  // Observe the averaged (dithered) pitch, so a bend that lands on a note is
  // exact -- the endpoints sit on A4 / A5 to within kAvgCentsBound, not the
  // single-clock-rounded ~5 cents.
  const double margin = std::pow(2.0, kAvgCentsBound / 1200.0);
  double prev = 0.0;
  for (int16_t bend : {0, 2048, 4096, 6144, 8192}) { // 0 .. full up-bend
    cr_fp_t scale = (bend == 0) ? cr_fp_t(0) : midiPbProp[bend];
    cr_hzinv_t bentInv = noteInv;
    if (scale > cr_fp_t(0)) {
      bentInv =
          noteInv + static_cast<cr_hzinv_t>(scale) * (targetInv - noteInv);
    }
    double realized = RealizedHz(AvgPeriod(osc, bentInv));
    std::printf(
        "  bend %5d (scale %.5f) -> %8.2f Hz  (%+7.2f cents from note)\n", bend,
        static_cast<double>(scale), realized, ToCents(realized, noteHz));
    CHECK(realized >= noteHz / margin && realized <= targetHz * margin,
          "bend %d realized %.2f Hz outside [%.2f, %.2f] -- bend math broken",
          bend, realized, noteHz, targetHz);
    CHECK(realized > prev, "bend %d not monotonic (%.2f <= %.2f)", bend,
          realized, prev);
    prev = realized;
  }
}

// Timing side of the single master clock: the trigger time of every pulse is
// quantized to a master tick, so the period has this granularity and up to half
// a tick of jitter (which the delta-sigma scheduler turns from a fixed bias
// into a mean-zero dither).
void TestTimingGranularity() {
  std::printf("[timing] master clock granularity\n");
  double tick_us = 1e6 / static_cast<double>(masterClockHz);
  double jitter_us = 0.5 * tick_us;
  std::printf("  masterClockHz = %lu, master tick = %.4f us, max trigger "
              "jitter = +/-%.4f us\n",
              (unsigned long)masterClockHz, tick_us, jitter_us);
  std::printf("  masterClockPeriodUs (integer, used by pulse-width state "
              "machine) = %lu\n",
              (unsigned long)masterClockPeriodUs);
  CHECK(masterClockPeriodUs == (cr_tick_t)(1e6 / masterClockHz),
        "masterClockPeriodUs %lu unexpected",
        (unsigned long)masterClockPeriodUs);
  CHECK(tick_us > 0.0 && jitter_us > 0.0, "non-positive timing granularity");
}

} // namespace

int main() {
  std::printf(
      "chime_red2 host frequency-accuracy tests (master clock %lu Hz)\n\n",
      (unsigned long)masterClockHz);
  TestPeriodTable();
  TestPeriodAverageExact();
  TestNoteAccuracy();
  TestSweepResolution();
  TestWorstCaseAccuracyCap();
  TestPerPulseBounded();
  TestFmAverageModulation();
  TestBendInverseHz();
  TestTimingGranularity();
  std::printf("\n%s (%d failure%s)\n", g_failures == 0 ? "PASS" : "FAIL",
              g_failures, g_failures == 1 ? "" : "s");
  return g_failures == 0 ? 0 : 1;
}
