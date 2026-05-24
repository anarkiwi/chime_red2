// Copyright 2019 Josh Bailey (josh@vandervecken.com)

// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

// Host-side unit tests for the synth's note-frequency quantization.
//
// These tests are deliberately NOT Arduino-specific: they compile the real
// Oscillator.cpp and the real pitch tables (constants.h / miditables.h) with a
// stock g++ on the CI runner, using tiny stubs for the Arduino-only headers
// (see test/host/stubs/). The fixed-point math in the FixedPoints library is
// pure integer arithmetic, so it is bit-identical on x86 and on the Due/SAMD
// targets -- which is exactly why measuring the error on the host is valid.
//
// The single master clock fires masterISR at masterClockHz; an oscillator can
// only trigger on a whole clock tick, so every realizable note frequency is
// masterClockHz / N for integer N (see Oscillator::SetFreqLazy). For integer N
// the best achievable error is half a tick of period -- the irreducible
// single-master-clock floor (<~30 cents at 2 kHz).
//
// A plain note (default detune, no pitch bend) takes its period straight from
// the precomputed pitchToPeriod[] table, so it hits that floor exactly. Detuned,
// pitch-bent and otherwise arbitrary frequencies go through the period multiply
// instead -- round(masterClockHz * hzInv) -- but hzInv is now cr_hzinv_t
// (SFixed<1,30>, 30 fractional bits) rather than the old cr_fp_t (15 bits), so
// its 1/f truncation error is negligible (<0.01 cents) and that path also lands
// at the single-clock floor. These tests guard both paths against regression.

#include "types.h"
#include "constants.h"
#include "Oscillator.h"

#include <cmath>
#include <cstdio>

namespace {

int g_failures = 0;

#define CHECK(cond, ...)                                                   \
  do {                                                                     \
    if (!(cond)) {                                                         \
      ++g_failures;                                                        \
      std::printf("  FAIL (%s:%d): ", __FILE__, __LINE__);                 \
      std::printf(__VA_ARGS__);                                            \
      std::printf("\n");                                                   \
    }                                                                      \
  } while (0)

// Upper limit of the range we characterise, per the task.
const double kMaxHz = 2000.0;

// Regression bounds (worst-case absolute pitch error), with generous margin.
//
// Plain MIDI notes now route through pitchToPeriod[], so their only error is the
// irreducible single-master-clock rounding floor: ~3 cents below 250 Hz and
// ~23 cents at the top of the range. These bounds guard that path stays optimal.
const double kNoteCentsBoundLow = 5.0;    // exact MIDI notes, target <= 250 Hz
const double kNoteCentsBound2k = 30.0;    // exact MIDI notes, target <= 2 kHz
// Arbitrary (e.g. pitch-bent) frequencies now also reach the single-clock floor
// thanks to the 30-bit cr_hzinv_t (was ~130 cents with 15-bit hzInv). If this
// regresses past the floor, hzInv precision has been lost again.
const double kSweepCentsBound2k = 40.0;   // arbitrary frequencies, <= 2 kHz

double ToCents(double realized, double target) {
  return 1200.0 * std::log2(realized / target);
}

// Drive the real Oscillator and read back the integer clock period (in master
// ticks) it chose for a requested hzInv. SetNextTick returns _clockPeriod.
cr_tick_t PeriodForHzInv(Oscillator *osc, cr_hzinv_t hzInv, uint8_t pitch = 0) {
  osc->SetFreqLazy(hzInv, pitch, /*velocityScale=*/0, /*periodOffset=*/0);
  return osc->SetNextTick(0);
}

// Independent (double-precision) re-implementation of the period the code is
// documented to compute: round(masterClockHz * hzInv), clamped to at least one
// tick. Cross-checks the oscillator's integer hzInvToTicks() path.
cr_tick_t ModelPeriod(cr_hzinv_t hzInv) {
  double n = std::round(static_cast<double>(masterClockHz) * static_cast<double>(hzInv));
  if (n < 1.0) {
    n = 1.0;
  }
  return static_cast<cr_tick_t>(n);
}

double RealizedHz(cr_tick_t period) {
  return static_cast<double>(masterClockHz) / static_cast<double>(period);
}

// The realizable frequency grid is masterClockHz / N. Rounding the period to
// the nearest tick bounds the error at half a tick of period -- the best a
// single master clock can do at this rate, before the extra hzInv truncation
// error. Used to isolate how much of the measured error is the truncation.
double PeriodRoundingBoundCents(cr_tick_t period) {
  return 1200.0 * std::log2(1.0 + 0.5 / static_cast<double>(period));
}

// Assert the real code computes the period it is documented to, across a wide
// range of inputs. This is the precise behavioural invariant of the current
// quantizer; the accuracy tests below measure its musical consequences.
void TestPeriodFormula() {
  std::printf("[period formula] osc period == round(masterClockHz * hzInv)\n");
  Oscillator osc;
  osc.SetMaxPitch(maxMidiPitch);
  for (double f = 20.0; f <= kMaxHz + 1e-9; f += 0.5) {
    cr_hzinv_t hzInv = cr_hzinv_t(1.0 / f);
    cr_tick_t got = PeriodForHzInv(&osc, hzInv);
    cr_tick_t want = ModelPeriod(hzInv);
    CHECK(got == want, "f=%.2f Hz: period %lu != model %lu", f,
          (unsigned long)got, (unsigned long)want);
    CHECK(got >= 1, "f=%.2f Hz: period %lu < 1", f, (unsigned long)got);
  }
}

// Guard the precomputed period table. pitchToPeriod[] is generated for a fixed
// masterClockHz; if masterClockHz is changed without regenerating it, every note
// silently detunes. Assert each entry is still round(masterClockHz / pitchToHz),
// and that the oscillator's plain-note path actually returns it.
void TestPeriodTableExact() {
  std::printf("[period table] pitchToPeriod == round(masterClockHz / pitchToHz), and is used\n");
  // Table correctness for every note: guards against masterClockHz being changed
  // without regenerating the literal table (which would silently detune all notes).
  for (int note = 0; note <= absMaxMidiPitch; ++note) {
    double f = static_cast<double>(pitchToHz[note]);
    long want = std::lround(static_cast<double>(masterClockHz) / f);
    if (want < 1) {
      want = 1;
    }
    CHECK(pitchToPeriod[note] == static_cast<cr_tick_t>(want),
          "pitchToPeriod[%d]=%lu != round(masterClockHz/%.4f)=%ld -- regenerate table",
          note, (unsigned long)pitchToPeriod[note], f, want);
  }
  // The oscillator must actually take the precomputed-period path for every
  // playable note. (Above maxMidiPitch the 15-bit hzInv can no longer resolve
  // adjacent semitones -- 1/f collides between neighbours -- so the multiply path
  // there is ambiguous; that is exactly why the range is capped and why the period
  // table, not the hzInv multiply, is the source of truth.)
  Oscillator osc;
  osc.SetMaxPitch(maxMidiPitch);
  for (int note = 0; note <= maxMidiPitch; ++note) {
    cr_tick_t period = PeriodForHzInv(&osc, pitchToHzInv[note], note);
    CHECK(period == pitchToPeriod[note],
          "note %d: oscillator period %lu != table %lu (plain-note path not taken)",
          note, (unsigned long)period, (unsigned long)pitchToPeriod[note]);
  }
}

// Worst-case uncertainty for the notes the synth actually plays (default detune,
// no pitch bend): drive the oscillator exactly as MidiChannel does -- the real
// pitchToHzInv[] table entry together with the note's pitch, so the precomputed
// pitchToPeriod[] path is exercised -- and compare the realized frequency to the
// table's intended frequency pitchToHz[].
void TestNoteAccuracyTo2k() {
  std::printf("[per-note] realized vs intended frequency, MIDI notes up to %.0f Hz\n",
              kMaxHz);
  std::printf("  note  targetHz  realizedHz   errHz   errCents\n");
  Oscillator osc;
  osc.SetMaxPitch(maxMidiPitch);
  double worst_cents = 0.0, worst_hz = 0.0;
  int worst_note = 0;
  for (int note = 1; note <= absMaxMidiPitch; ++note) {
    double target = static_cast<double>(pitchToHz[note]);
    if (target > kMaxHz) {
      break;
    }
    cr_tick_t period = PeriodForHzInv(&osc, pitchToHzInv[note], note);
    double realized = RealizedHz(period);
    double err_hz = realized - target;
    double err_cents = ToCents(realized, target);
    std::printf("  %3d  %8.3f  %9.3f  %+6.2f   %+7.2f\n", note, target,
                realized, err_hz, err_cents);
    double bound = target <= 250.0 ? kNoteCentsBoundLow : kNoteCentsBound2k;
    CHECK(std::fabs(err_cents) <= bound,
          "note %d (%.2f Hz): |%.2f cents| exceeds bound %.1f", note, target,
          err_cents, bound);
    if (std::fabs(err_cents) > std::fabs(worst_cents)) {
      worst_cents = err_cents;
      worst_note = note;
    }
    if (std::fabs(err_hz) > std::fabs(worst_hz)) {
      worst_hz = err_hz;
    }
  }
  std::printf("  --> worst note error up to %.0f Hz: %+.2f cents (%+.3f Hz) at note %d\n",
              kMaxHz, worst_cents, worst_hz, worst_note);
}

// Worst-case uncertainty for an arbitrary requested frequency (e.g. a
// pitch-bent note): sweep finely and feed cr_hzinv_t(1/f), characterising the
// resolution of the whole hzInv -> period quantizer.
void TestSweepResolutionTo2k() {
  std::printf("[sweep] worst-case uncertainty for arbitrary frequencies up to %.0f Hz\n",
              kMaxHz);
  Oscillator osc;
  osc.SetMaxPitch(maxMidiPitch);
  double worst_cents = 0.0, worst_hz = 0.0, at_hz = 0.0;
  double worst_vs_floor = 0.0;  // how far past the single-clock rounding floor.
  for (double f = 20.0; f <= kMaxHz + 1e-9; f += 0.1) {
    cr_tick_t period = PeriodForHzInv(&osc, cr_hzinv_t(1.0 / f));
    double realized = RealizedHz(period);
    double err_cents = ToCents(realized, f);
    if (std::fabs(err_cents) > std::fabs(worst_cents)) {
      worst_cents = err_cents;
      worst_hz = realized - f;
      at_hz = f;
    }
    double over = std::fabs(err_cents) - PeriodRoundingBoundCents(period);
    if (over > worst_vs_floor) {
      worst_vs_floor = over;
    }
  }
  std::printf("  --> worst error: %+.2f cents (%+.3f Hz) at %.1f Hz\n",
              worst_cents, worst_hz, at_hz);
  std::printf("  --> worst excess over period-rounding floor (hzInv truncation): %.2f cents\n",
              worst_vs_floor);
  CHECK(std::fabs(worst_cents) <= kSweepCentsBound2k,
        "sweep worst %.2f cents exceeds bound %.1f", worst_cents,
        kSweepCentsBound2k);
  // Sanity: the quantizer really is lossy at this rate (guards against the test
  // silently measuring nothing).
  CHECK(std::fabs(worst_cents) > 1.0,
        "sweep worst %.2f cents implausibly small -- test not exercising quantizer",
        worst_cents);
}

// Lock the synth's worst-case pitch error across its whole operating envelope,
// so any change that detunes the instrument fails here. The envelope is: every
// playable MIDI note (default detune/no bend -> the pitchToPeriod[] path) and
// every arbitrary frequency a detune or pitch bend can request, up to the top
// note times full +cents detune (the hzInv -> period multiply path). The only
// irreducible error is single-master-clock period rounding -- largest at the
// shortest period, i.e. the highest pitch. Today the quantizer hits exactly that
// floor on both paths, so the cap is that floor (plus a tiny allowance for
// cr_hzinv_t's 1/f truncation on the arbitrary path). A failure means accuracy
// regressed past the hardware floor: hzInv precision was lost, or pitchToPeriod[]
// desynced from masterClockHz.
void TestWorstCaseAccuracyCap() {
  std::printf("[cap] worst-case pitch error across the operating envelope\n");
  Oscillator osc;
  osc.SetMaxPitch(maxMidiPitch);

  double worst = 0.0, worstHz = 0.0;
  const char *worstWhat = "";
  cr_tick_t minPeriod = ~static_cast<cr_tick_t>(0);

  // (a) Every playable plain note: the precomputed pitchToPeriod[] path.
  for (int note = 1; note <= maxMidiPitch; ++note) {
    cr_tick_t period = PeriodForHzInv(&osc, pitchToHzInv[note], note);
    if (period < minPeriod) {
      minPeriod = period;
    }
    double err = std::fabs(ToCents(RealizedHz(period), static_cast<double>(pitchToHz[note])));
    if (err > worst) {
      worst = err;
      worstHz = static_cast<double>(pitchToHz[note]);
      worstWhat = "note";
    }
  }

  // (b) Arbitrary (detuned / pitch-bent) frequencies, up to the top note times
  // full +cents detune -- the highest frequency normal controls can request.
  const double topHz = static_cast<double>(pitchToHz[maxMidiPitch]) *
                       static_cast<double>(midiTuneCents[maxMidiVal]);
  double worstExcess = 0.0;
  for (double f = 20.0; f <= topHz + 1e-9; f += 0.1) {
    cr_tick_t period = PeriodForHzInv(&osc, cr_hzinv_t(1.0 / f));
    if (period < minPeriod) {
      minPeriod = period;
    }
    double err = std::fabs(ToCents(RealizedHz(period), f));
    if (err > worst) {
      worst = err;
      worstHz = f;
      worstWhat = "freq";
    }
    double excess = err - PeriodRoundingBoundCents(period);
    if (excess > worstExcess) {
      worstExcess = excess;
    }
  }

  const double floorCap = PeriodRoundingBoundCents(minPeriod);
  // cr_hzinv_t truncation on the arbitrary path is documented < 0.01 cents; allow
  // a generous 1 cent so the cap tracks the physical floor, not a magic number.
  const double kHzInvTruncationCents = 1.0;
  const double cap = floorCap + kHzInvTruncationCents;
  std::printf("  operating range: up to %.1f Hz (note %u + full cents detune)\n",
              topHz, maxMidiPitch);
  std::printf("  worst-case error: %.2f cents (at %.1f Hz, %s path)\n",
              worst, worstHz, worstWhat);
  std::printf("  single-clock floor at top (period %lu ticks): %.2f cents; "
              "worst excess over floor: %.4f cents\n",
              (unsigned long)minPeriod, floorCap, worstExcess);
  std::printf("  --> cap = %.2f cents (floor %.2f + %.2f truncation allowance)\n",
              cap, floorCap, kHzInvTruncationCents);

  CHECK(worst <= cap,
        "worst-case pitch error %.2f cents exceeds cap %.2f -- accuracy regressed "
        "past the single-master-clock floor",
        worst, cap);
  // Guard the test is actually exercising the lossy quantizer (not measuring nothing).
  CHECK(worst > 5.0,
        "worst-case error %.2f cents implausibly small -- test not exercising the range",
        worst);
}

// Pitch bend now interpolates in inverse-Hz space (PitchBender::BendHz). This
// mirrors that math against the real tables + oscillator and asserts a bent note
// is sane: endpoints land on the note and its bend target (to the single-clock
// floor), and intermediate bends rise monotonically and stay strictly between --
// the previous forward-Hz subtraction sent bent notes to ~1 Hz / 52631 Hz.
// (The dimensional correctness itself is enforced by the type system: hzInv is
// cr_hzinv_t and pitchToHz is cr_fp_t, so forwardHz - hzInv no longer compiles.)
void TestBendInverseHz() {
  std::printf("[bend] inverse-Hz pitch-bend interpolation is monotonic and bounded\n");
  const uint8_t note = 69;                       // A4, 440 Hz
  const uint8_t target = note + midiPitchBendRange;  // bend up 12 semitones -> A5
  const double noteHz = static_cast<double>(pitchToHz[note]);
  const double targetHz = static_cast<double>(pitchToHz[target]);
  const cr_hzinv_t noteInv = pitchToHzInv[note];
  const cr_hzinv_t targetInv = pitchToHzInv[target];
  std::printf("  note %u (%.2f Hz) bending up to note %u (%.2f Hz)\n",
              note, noteHz, target, targetHz);
  // Allow the single-clock floor (up to half a tick) around the window edges.
  const double margin = std::pow(2.0, kSweepCentsBound2k / 1200.0);
  double prev = 0.0;
  for (int16_t bend : {0, 2048, 4096, 6144, 8192}) {  // 0 .. full up-bend
    cr_fp_t scale = (bend == 0) ? cr_fp_t(0) : midiPbProp[bend];
    cr_hzinv_t bentInv = noteInv;
    if (scale > cr_fp_t(0)) {
      bentInv = noteInv + static_cast<cr_hzinv_t>(scale) * (targetInv - noteInv);
    }
    double realized = RealizedHz(hzInvToTicks(masterClockHz, bentInv));
    std::printf("  bend %5d (scale %.5f) -> %8.2f Hz  (%+7.2f cents from note)\n",
                bend, static_cast<double>(scale), realized, ToCents(realized, noteHz));
    // Sane and bounded: never the old garbage, always within the bend window.
    CHECK(realized >= noteHz / margin && realized <= targetHz * margin,
          "bend %d realized %.2f Hz outside [%.2f, %.2f] -- bend math broken",
          bend, realized, noteHz, targetHz);
    CHECK(realized > prev, "bend %d not monotonic (%.2f <= %.2f)", bend, realized, prev);
    prev = realized;
  }
  // Endpoints: zero bend == the note, full bend == the target, to the floor.
  double zeroErr = ToCents(RealizedHz(hzInvToTicks(masterClockHz, noteInv)), noteHz);
  double fullErr = ToCents(RealizedHz(hzInvToTicks(masterClockHz, targetInv)), targetHz);
  CHECK(std::fabs(zeroErr) <= kSweepCentsBound2k, "zero-bend off by %.2f cents", zeroErr);
  CHECK(std::fabs(fullErr) <= kSweepCentsBound2k, "full-bend off by %.2f cents", fullErr);
}

// Timing side of the single master clock: the trigger time of every pulse is
// quantized to a master tick, so the period (and any pulse-width handling that
// counts ticks) has this granularity and up to half a tick of jitter.
void TestTimingGranularity() {
  std::printf("[timing] master clock granularity\n");
  double tick_us = 1e6 / static_cast<double>(masterClockHz);
  double jitter_us = 0.5 * tick_us;
  std::printf("  masterClockHz = %lu, master tick = %.4f us, max trigger jitter = +/-%.4f us\n",
              (unsigned long)masterClockHz, tick_us, jitter_us);
  std::printf("  masterClockPeriodUs (integer, used by pulse-width state machine) = %lu\n",
              (unsigned long)masterClockPeriodUs);
  // masterClockPeriodUs is integer-truncated (1e6/52631 = 19.0002 -> 19).
  CHECK(masterClockPeriodUs == (cr_tick_t)(1e6 / masterClockHz),
        "masterClockPeriodUs %lu unexpected", (unsigned long)masterClockPeriodUs);
  CHECK(tick_us > 0.0 && jitter_us > 0.0, "non-positive timing granularity");
}

}  // namespace

int main() {
  std::printf("chime_red2 host frequency-uncertainty tests (master clock %lu Hz)\n\n",
              (unsigned long)masterClockHz);
  TestPeriodFormula();
  TestPeriodTableExact();
  TestNoteAccuracyTo2k();
  TestSweepResolutionTo2k();
  TestWorstCaseAccuracyCap();
  TestBendInverseHz();
  TestTimingGranularity();
  std::printf("\n%s (%d failure%s)\n", g_failures == 0 ? "PASS" : "FAIL",
              g_failures, g_failures == 1 ? "" : "s");
  return g_failures == 0 ? 0 : 1;
}
