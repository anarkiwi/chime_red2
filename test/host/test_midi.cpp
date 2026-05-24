// Copyright 2019 Josh Bailey (josh@vandervecken.com)

// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

// Host-side MIDI behaviour tests. These compile the real CRMidi + MidiChannel +
// OscillatorController + MidiNote + Lfo + AdsrEnvelope + CRIO (the base, non-LCD
// CRIO; see CR_HOST_TEST in config.h / CRDigitalPin.h) with a stock g++, and
// drive the same handler entry points the Arduino MIDI library calls
// (handleNoteOn/Off, handleControlChange, handlePitchBend). The serial MIDI
// parser and ISR scheduling in the .ino are deliberately out of scope here; this
// checks that a MIDI message produces the right oscillator/voice state.
//
// "Right thing" is observed through the oscillators: a sounding voice is an
// Oscillator with audible == true, whose hzInv equals the requested note's
// inverse frequency (pitchToHzInv[note]) for an un-bent, un-detuned note. The
// CR_HOST_TEST-only OscillatorController::TestOsc() exposes them read-only.
//
// One test (TestEndToEndPinOscillation) goes further and exercises the entire
// device signal chain on the host -- including the master-clock ISR state
// machine (replicated from chime_red2.ino) and CRIO's pulse output toggling the
// (virtual) coil/diag/speaker pins. The host DigitalPin records pin edges (see
// CRDigitalPin.h / CR_HOST_TEST), so a note can be shown to actually make the
// coil pin oscillate at the right frequency, and a note off to stop it.

#include "config.h"
#include "types.h"
#include "constants.h"
#include "pins.h"
#include "CRDigitalPin.h"
#include "OscillatorController.h"
#include "MidiChannel.h"
#include "MidiNote.h"
#include "CRIO.h"
#include "CRMidi.h"
#include "IsrDriver.h"  // shared master-ISR replica (also used by cr_render.cpp)

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

// Base CRIO already suits host tests (percussionEnabled() true, fixedPulse off,
// pollPots() false, maxPitch = maxMidiPitch, no-op coeff/LCD). Subclass only to
// flip percussion on/off for the channel-10 test.
class TestCRIO : public CRIO {
 public:
  bool percEnabled = true;
  bool pollResult = false;  // when true, the control task advances past polling
  bool percussionEnabled() override { return percEnabled; }
  bool pollPots() override { return pollResult; }
};

int AudibleCount(OscillatorController &oc) {
  int n = 0;
  for (uint8_t i = 0; i < oscillatorCount; ++i) {
    if (oc.TestOsc(i)->audible) {
      ++n;
    }
  }
  return n;
}

Oscillator *FirstAudible(OscillatorController &oc) {
  for (uint8_t i = 0; i < oscillatorCount; ++i) {
    if (oc.TestOsc(i)->audible) {
      return oc.TestOsc(i);
    }
  }
  return NULL;
}

// Count sounding voices tuned to a given (un-bent, un-detuned) note.
int AudibleAtNote(OscillatorController &oc, uint8_t note) {
  int n = 0;
  for (uint8_t i = 0; i < oscillatorCount; ++i) {
    Oscillator *o = oc.TestOsc(i);
    if (o->audible && o->hzInv == pitchToHzInv[note]) {
      ++n;
    }
  }
  return n;
}

double Hz(cr_hzinv_t hzInv) { return 1.0 / static_cast<double>(hzInv); }

double ToCents(double realized, double target) {
  return 1200.0 * std::log2(realized / target);
}

// Advance the synth's control task enough whole cycles (counter sweeps 0..3) to
// run envelope updates (case 0) and note expiry (case 1), so a released note's
// oscillator is actually returned to the free pool.
void RunControl(CRMidi &crmidi, int cycles) {
  for (int i = 0; i < cycles; ++i) {
    crmidi.HandleControl();
  }
}

// The master-clock ISR replica (IsrDriver) lives in test/host/IsrDriver.h so the
// SMF->WAV simulator (cr_render.cpp) drives the synth through the identical state
// machine. See that header for the per-Tick() behaviour and latency instrumentation.

// note on allocates one audible oscillator at the note's frequency; note off
// (after the control task runs) releases it back to silence.
void TestNoteOnOff() {
  std::printf("[note] note on sounds the right pitch; note off silences it\n");
  OscillatorController oc;
  TestCRIO crio;
  CRMidi crmidi(&oc, &crio);
  CHECK(AudibleCount(oc) == 0, "expected silence at start, got %d", AudibleCount(oc));

  crmidi.handleNoteOn(1, 60, 100);
  CHECK(AudibleCount(oc) == 1, "note on: expected 1 voice, got %d", AudibleCount(oc));
  Oscillator *o = FirstAudible(oc);
  CHECK(o != NULL && o->hzInv == pitchToHzInv[60],
        "note on: voice tuned to %.2f Hz, expected %.2f Hz",
        o ? Hz(o->hzInv) : 0.0, Hz(pitchToHzInv[60]));
  CHECK(o != NULL && o->envelope != NULL, "note on: voice has no envelope");

  crmidi.handleNoteOff(1, 60);
  RunControl(crmidi, 16);
  CHECK(AudibleCount(oc) == 0, "note off: expected silence, got %d", AudibleCount(oc));
}

// MIDI note-on with velocity 0 must behave as note off.
void TestVelocityZeroIsNoteOff() {
  std::printf("[velocity0] note on velocity 0 == note off\n");
  OscillatorController oc;
  TestCRIO crio;
  CRMidi crmidi(&oc, &crio);
  crmidi.handleNoteOn(1, 64, 100);
  CHECK(AudibleCount(oc) == 1, "setup: expected 1 voice, got %d", AudibleCount(oc));
  crmidi.handleNoteOn(1, 64, 0);  // velocity 0
  RunControl(crmidi, 16);
  std::printf("  after velocity-0 note on + control: %d voice(s) sounding\n", AudibleCount(oc));
  CHECK(AudibleCount(oc) == 0, "velocity 0 did not silence the note (%d voices)",
        AudibleCount(oc));
}

// pitch bend retunes the sounding voice; full up-bend reaches the note one bend
// range higher; returning the wheel to centre restores the original pitch.
void TestPitchBend() {
  std::printf("[bend] pitch bend retunes the sounding voice and restores on centre\n");
  OscillatorController oc;
  TestCRIO crio;
  CRMidi crmidi(&oc, &crio);
  const uint8_t note = 69;  // A4, 440 Hz
  crmidi.handleNoteOn(1, note, 100);
  cr_hzinv_t base = FirstAudible(oc)->hzInv;
  CHECK(base == pitchToHzInv[note], "unbent voice not at base pitch");

  crmidi.handlePitchBend(1, 8191);  // full up bend
  Oscillator *o = FirstAudible(oc);
  CHECK(o->hzInv < base, "up-bend should raise pitch (lower hzInv): %.2f -> %.2f Hz",
        Hz(base), Hz(o->hzInv));
  const uint8_t target = note + midiPitchBendRange;  // +12 semis
  double err = std::fabs(Hz(o->hzInv) - Hz(pitchToHzInv[target])) / Hz(pitchToHzInv[target]);
  std::printf("  full up-bend: %.2f Hz (target note %u = %.2f Hz, %.2f%% off)\n",
              Hz(o->hzInv), target, Hz(pitchToHzInv[target]), 100.0 * err);
  CHECK(err < 0.01, "full up-bend %.2f Hz far from target %.2f Hz", Hz(o->hzInv),
        Hz(pitchToHzInv[target]));

  crmidi.handlePitchBend(1, 0);  // wheel back to centre
  CHECK(FirstAudible(oc)->hzInv == base, "pitch did not restore on centre bend");
}

// a detune control change (CC 94, cents) retunes sounding voices.
void TestDetuneCC() {
  std::printf("[cc] detune CC retunes the sounding voice\n");
  OscillatorController oc;
  TestCRIO crio;
  CRMidi crmidi(&oc, &crio);
  crmidi.handleNoteOn(1, 60, 100);
  cr_hzinv_t base = FirstAudible(oc)->hzInv;

  crmidi.handleControlChange(1, 94, 70);  // detune +6 cents-ish
  Oscillator *o = FirstAudible(oc);
  cr_hzinv_t expected = pitchToHzInv[60] * static_cast<cr_hzinv_t>(midiTuneCents[70]);
  CHECK(o->hzInv != base, "detune CC did not change pitch");
  CHECK(o->hzInv == expected, "detuned voice %.4f Hz != expected %.4f Hz",
        Hz(o->hzInv), Hz(expected));
}

// a 2nd-oscillator detune (CC 95) makes each note sound two voices.
void TestDetune2TwoOscillators() {
  std::printf("[cc] 2nd-oscillator detune (CC 95) sounds two voices per note\n");
  OscillatorController oc;
  TestCRIO crio;
  CRMidi crmidi(&oc, &crio);
  crmidi.handleControlChange(1, 95, 70);  // set detune2 before the note on
  crmidi.handleNoteOn(1, 60, 100);
  CHECK(AudibleCount(oc) == 2, "detune2: expected 2 voices for one note, got %d",
        AudibleCount(oc));
}

// distinct notes allocate distinct voices; the voice pool (oscillatorCount) caps
// polyphony rather than misbehaving.
void TestPolyphony() {
  std::printf("[poly] distinct notes allocate distinct voices, pool caps polyphony\n");
  OscillatorController oc;
  TestCRIO crio;
  CRMidi crmidi(&oc, &crio);
  const uint8_t notes[] = {60, 64, 67, 72};
  for (uint8_t n : notes) {
    crmidi.handleNoteOn(1, n, 100);
  }
  CHECK(AudibleCount(oc) == 4, "expected 4 voices, got %d", AudibleCount(oc));
  for (uint8_t n : notes) {
    CHECK(AudibleAtNote(oc, n) == 1, "note %u not sounding exactly once", n);
  }

  // Exhaust the pool: many more distinct notes than oscillatorCount voices.
  for (uint8_t n = 40; n < 40 + 2 * oscillatorCount; ++n) {
    crmidi.handleNoteOn(1, n, 100);
  }
  CHECK(AudibleCount(oc) <= oscillatorCount,
        "polyphony exceeded pool: %d > %d", AudibleCount(oc), oscillatorCount);
  std::printf("  voices after over-subscription: %d (pool %d)\n", AudibleCount(oc),
              oscillatorCount);
}

// notes above the configured max pitch, and messages on disabled channels, are
// ignored rather than sounding or crashing.
void TestOutOfRange() {
  std::printf("[range] out-of-range note / disabled channel are ignored\n");
  OscillatorController oc;
  TestCRIO crio;
  CRMidi crmidi(&oc, &crio);
  crmidi.handleNoteOn(1, absMaxMidiPitch, 100);  // > maxPitch (96)
  CHECK(AudibleCount(oc) == 0, "note above maxPitch should be ignored, got %d voices",
        AudibleCount(oc));
  crmidi.handleNoteOn(9, 60, 100);  // channel 9: not enabled (max 8, perc is 10)
  CHECK(AudibleCount(oc) == 0, "note on disabled channel 9 should be ignored, got %d",
        AudibleCount(oc));
  crmidi.handleNoteOn(1, 60, 100);  // valid
  CHECK(AudibleCount(oc) == 1, "valid note should sound, got %d", AudibleCount(oc));
}

// CC 123 (All Notes Off) silences every voice on the channel immediately.
void TestAllNotesOff() {
  std::printf("[cc] All Notes Off (CC 123) silences the channel\n");
  OscillatorController oc;
  TestCRIO crio;
  CRMidi crmidi(&oc, &crio);
  crmidi.handleNoteOn(1, 60, 100);
  crmidi.handleNoteOn(1, 64, 100);
  CHECK(AudibleCount(oc) == 2, "setup: expected 2 voices, got %d", AudibleCount(oc));
  crmidi.handleControlChange(1, 123, 0);
  CHECK(AudibleCount(oc) == 0, "All Notes Off left %d voices", AudibleCount(oc));
}

// the percussion channel (10) sounds only when CRIO reports it enabled.
void TestPercussionChannel() {
  std::printf("[perc] channel 10 honours percussionEnabled()\n");
  OscillatorController oc;
  TestCRIO crio;
  CRMidi crmidi(&oc, &crio);

  crio.percEnabled = false;
  crmidi.handleNoteOn(10, 60, 100);
  CHECK(AudibleCount(oc) == 0, "percussion disabled: note should be ignored, got %d",
        AudibleCount(oc));

  crio.percEnabled = true;
  const uint8_t note = 60;
  crmidi.handleNoteOn(10, note, 100);
  CHECK(AudibleCount(oc) >= 1, "percussion enabled: note should sound, got %d",
        AudibleCount(oc));

  // Channel 10 has its own pitched-noise path (no shared vibrato LFO, no bend
  // retune): note on stamps the voice with a master-clock period window one bend
  // range either side of the note, so the voice starts at the note's own period.
  Oscillator *p = FirstAudible(oc);
  CHECK(p != NULL && p->TestClockPeriod() == pitchToPeriod[note],
        "percussion: voice should start at note period %u, got %u",
        pitchToPeriod[note], p ? p->TestClockPeriod() : 0);

  // Modulate() still yields a sane pulse, and moves the voice to a random period
  // inside the window [period(note+range), period(note-range)]. The host's
  // deterministic random() returns the low bound, so the period lands exactly on
  // the window minimum -- the highest pitch, period(note+range).
  const uint8_t hi = note + midiPitchBendRange;  // +12 semitones
  const uint8_t lo = note - midiPitchBendRange;   // -12 semitones
  CHECK(p != NULL && crmidi.Modulate(p) > cr_fp_t(0),
        "percussion: Modulate should return a positive pulse");
  CHECK(p != NULL && p->TestClockPeriod() == pitchToPeriod[hi],
        "percussion noise: period %u != window min %u (note + %u semis)",
        p ? p->TestClockPeriod() : 0, pitchToPeriod[hi], midiPitchBendRange);
  CHECK(p != NULL && p->TestClockPeriod() >= pitchToPeriod[hi] &&
            p->TestClockPeriod() <= pitchToPeriod[lo],
        "percussion noise: period %u outside window [%u, %u]",
        p ? p->TestClockPeriod() : 0, pitchToPeriod[hi], pitchToPeriod[lo]);
}

// Exercise the remaining control-change handlers, the program-change handler,
// and the disabled-channel guards. These are mostly routing/no-crash checks; the
// load-bearing assertions are that a program change does an All-Notes-Off and
// that messages on a disabled channel are ignored.
void TestControlChangeHandling() {
  std::printf("[cc] remaining CCs, program change, and disabled-channel guards\n");
  OscillatorController oc;
  TestCRIO crio;
  CRMidi crmidi(&oc, &crio);
  crmidi.handleNoteOn(1, 60, 100);
  CHECK(AudibleCount(oc) == 1, "setup: expected 1 voice, got %d", AudibleCount(oc));

  crmidi.handleControlChange(1, 121, 0);   // Reset All Controllers (+ retune)
  crmidi.handleControlChange(1, 90, 70);   // 2nd-osc detune in clock periods
  crmidi.handleControlChange(1, 89, 70);   // detune in clock periods
  crmidi.handleControlChange(1, 87, 0);    // LFO free-run (restart off)
  crmidi.handleControlChange(1, 31, 5);    // pitch bend range = 5 semitones
  crmidi.handleControlChange(1, 30, 0);    // Reset CC (Ableton-friendly alias)
  crmidi.handleControlChange(1, 27, 100);  // configurable LFO speed
  crmidi.handleControlChange(1, 50, 0);    // unhandled CC -> default branch
  CHECK(AudibleCount(oc) == 1, "control changes should not drop the note, got %d",
        AudibleCount(oc));

  // A disabled channel (9: above maxMidiChannel, not percussion) is ignored by
  // every handler.
  crmidi.handleControlChange(9, 7, 100);
  crmidi.handleNoteOff(9, 60);
  crmidi.handlePitchBend(9, 1000);
  CHECK(AudibleCount(oc) == 1, "disabled-channel messages changed state, got %d",
        AudibleCount(oc));

  // Program change resets the channel: an implicit All-Notes-Off.
  crmidi.handleProgramChange(1, 0);
  CHECK(AudibleCount(oc) == 0, "program change should silence the channel, got %d",
        AudibleCount(oc));
  crmidi.handleProgramChange(9, 0);  // disabled channel: ignored, no crash

  // With pots reporting changes, the control task runs its full cycle including
  // the coefficient-update step.
  crio.pollResult = true;
  RunControl(crmidi, 8);
}

// An ADSR envelope set via control changes runs through its phases: a note with
// non-default attack/decay/sustain/release starts silent, ramps up in attack,
// then settles at the sustain level; note off releases it back to silence.
// (Observed via the public Oscillator::envelope the voice points at.)
void TestEnvelopeAdsr() {
  std::printf("[env] ADSR envelope ramps through attack/decay/sustain/release\n");
  OscillatorController oc;
  TestCRIO crio;
  CRMidi crmidi(&oc, &crio);
  // Short attack/decay so the phases complete in a few control cycles; sustain
  // below max so decay actually moves (CC 73 attack, 75 decay, 24 sustain, 72 rel).
  crmidi.handleControlChange(1, 73, 1);
  crmidi.handleControlChange(1, 75, 1);
  crmidi.handleControlChange(1, 24, 64);
  crmidi.handleControlChange(1, 72, 1);
  crmidi.handleNoteOn(1, 60, 100);

  Oscillator *o = FirstAudible(oc);
  CHECK(o != NULL && o->envelope != NULL && !o->envelope->isNull,
        "env: voice should have a non-null shaped envelope");
  double attackStart = o ? static_cast<double>(o->envelope->level) : -1.0;
  CHECK(attackStart == 0.0, "env: attack should start at level 0, got %.3f",
        attackStart);

  RunControl(crmidi, 12);  // a few envelope advances into the attack ramp
  double rising = static_cast<double>(FirstAudible(oc)->envelope->level);
  CHECK(rising > attackStart, "env: level should rise during attack (%.3f -> %.3f)",
        attackStart, rising);

  RunControl(crmidi, 200);  // well past attack+decay -> sustain
  double sustain = static_cast<double>(FirstAudible(oc)->envelope->level);
  std::printf("  attack start %.3f, rising %.3f, sustain %.3f (target ~0.50)\n",
              attackStart, rising, sustain);
  CHECK(sustain > 0.0 && sustain < 1.0, "env: sustain level %.3f not in (0,1)", sustain);
  CHECK(AudibleCount(oc) == 1, "env: held note should still sound");

  crmidi.handleNoteOff(1, 60);
  RunControl(crmidi, 40);  // release -> idle -> expire
  CHECK(AudibleCount(oc) == 0, "env: released note should fall silent, got %d",
        AudibleCount(oc));

  // A zero-attack, non-zero-decay envelope starts straight in decay at full
  // level and decays toward sustain.
  crmidi.handleControlChange(1, 73, 0);   // attack 0
  crmidi.handleControlChange(1, 75, 1);   // decay > 0
  crmidi.handleNoteOn(1, 62, 100);
  Oscillator *d = FirstAudible(oc);
  CHECK(d != NULL && d->envelope->level == cr_fp_t(1.0),
        "env: zero-attack note should start decay at full level, got %.3f",
        d ? static_cast<double>(d->envelope->level) : -1.0);
  RunControl(crmidi, 200);
  CHECK(static_cast<double>(FirstAudible(oc)->envelope->level) < 1.0,
        "env: decay should drop below full level");
}

// Advancing the master clock (OscillatorController::Triggered, as the ISR does)
// schedules the sounding voice and ticks the LFOs. After a note on, driving the
// clock should mark the note's oscillator as the audible one.
void TestSchedulerAndLfoTick() {
  std::printf("[sched] master-clock advance schedules the voice and ticks LFOs\n");
  OscillatorController oc;
  TestCRIO crio;
  CRMidi crmidi(&oc, &crio);
  crmidi.handleNoteOn(1, 69, 100);  // A4, period ~120 master ticks
  crmidi.handleControlChange(1, 76, 127);  // fast vibrato LFO so its table wraps

  bool sawAudible = false;
  // Enough ticks to fire the ~120-tick note many times and wrap the LFO table.
  for (int i = 0; i < 60000; ++i) {
    oc.Triggered();
    if (oc.audibleOscillator != NULL && oc.audibleOscillator->audible) {
      sawAudible = true;
    }
  }
  CHECK(sawAudible, "scheduler never selected the sounding voice as audible");
}

// The modulation chain runs without misbehaving: with tremolo + vibrato + volume
// + a shaped envelope, Modulate() returns a sane pulse width, exercising the LFO
// level lookups and AM/FM modulation.
void TestModulation() {
  std::printf("[mod] tremolo/vibrato/volume modulation produces a sane pulse\n");
  OscillatorController oc;
  TestCRIO crio;
  CRMidi crmidi(&oc, &crio);
  // Shaped (non-null) envelope that starts at the sustain level, so the envelope
  // branch of the modulation chain multiplies by a non-zero level.
  crmidi.handleControlChange(1, 24, 64);   // sustain
  crmidi.handleControlChange(1, 72, 10);   // release
  crmidi.handleNoteOn(1, 60, 100);

  crmidi.handleControlChange(1, 92, 64);   // tremolo depth  -> AM modulation
  crmidi.handleControlChange(1, 1, 64);    // vibrato depth  -> FM modulation
  crmidi.handleControlChange(1, 7, 100);   // channel volume (< max)
  crmidi.handleControlChange(1, 85, 1);    // tremolo LFO shape: down saw
  crmidi.handleControlChange(1, 86, 2);    // vibrato LFO shape: up saw
  crmidi.handleControlChange(1, 22, 100);  // tremolo LFO speed (>= 1 Hz)
  crmidi.handleControlChange(1, 76, 1);    // vibrato LFO speed (< 1 Hz path)

  Oscillator *o = FirstAudible(oc);
  CHECK(o != NULL, "mod: expected a sounding voice");
  cr_fp_t pulse = crmidi.Modulate(o);
  std::printf("  modulated pulse = %.2f us (window %.0f us)\n",
              static_cast<double>(pulse), static_cast<double>(pulseWindowUs));
  CHECK(pulse > cr_fp_t(0) && pulse <= pulseWindowUs,
        "mod: pulse %.2f us outside (0, %.0f]", static_cast<double>(pulse),
        static_cast<double>(pulseWindowUs));
}

// End to end: a note on makes the coil output pin physically oscillate at the
// note's pitch, and a note off stops it. This drives the entire device signal
// chain on the host -- MIDI note -> MidiChannel/MidiNote -> OscillatorController
// scheduling -> the master-clock ISR state machine (IsrDriver, replicated from
// chime_red2.ino) -> CRIO's pulse output toggling the (virtual) coil/diag/
// speaker pins -- and measures the coil pin's pulse rate against the requested
// frequency. The recording host DigitalPin (CRDigitalPin.h, CR_HOST_TEST) makes
// the pin waveform observable; everything upstream of the pin is the real synth.
void TestEndToEndPinOscillation() {
  std::printf("[e2e] note on -> coil pin oscillates at pitch -> note off -> pin idle\n");
  std::printf("       note  targetHz    pinHz   errCents   period ticks (min/avg/max)\n");

  // A spread of notes across the playable range (low bass up to ~1 kHz). Each
  // must drive the coil pin at its frequency to within the single-master-clock
  // floor -- the same bound test_frequency holds the scheduler to.
  const uint8_t notes[] = {36, 48, 60, 69, 84};
  for (uint8_t note : notes) {
    OscillatorController oc;
    TestCRIO crio;
    CRMidi crmidi(&oc, &crio);
    IsrDriver driver(oc, crmidi, crio);

    auto resetPins = [&]() {
      crHostPin(coilOutPin).Reset();
      crHostPin(diagOutPin).Reset();
      crHostPin(speakerOutPin).Reset();
    };

    CrPinRecord &coil = crHostPin(coilOutPin);
    resetPins();

    // Note on, then let the scheduler settle (skip the initial schedule
    // transient) before starting a clean measurement window.
    crmidi.handleNoteOn(1, note, 100);
    for (int i = 0; i < 4000; ++i) {
      driver.Tick();
    }
    resetPins();

    // Run until the pin has emitted many pulses (or a generous safety cap), so
    // the averaged period is well resolved.
    const unsigned long targetEdges = 200;
    const unsigned long capTicks = 2000000;
    const unsigned long startTicks = driver.masterTicks();
    while (coil.risingEdges < targetEdges &&
           (driver.masterTicks() - startTicks) < capTicks) {
      driver.Tick();
    }
    CHECK(coil.risingEdges >= targetEdges,
          "note %u: coil pin emitted only %lu pulses (expected >= %lu) -- not oscillating",
          note, coil.risingEdges, targetEdges);
    if (coil.risingEdges < 2) {
      continue;  // nothing measurable; the CHECK above already failed.
    }

    // Average pulse-to-pulse period in master ticks -> realized pin frequency.
    const double avgPeriod = static_cast<double>(coil.lastRisingTick - coil.firstRisingTick) /
                             static_cast<double>(coil.risingEdges - 1);
    const double pinHz = static_cast<double>(masterClockHz) / avgPeriod;
    const double targetHz = Hz(pitchToHzInv[note]);
    const double schedHz = static_cast<double>(masterClockHz) /
                           static_cast<double>(pitchToPeriod[note]);
    const double errCents = ToCents(pinHz, targetHz);
    std::printf("       %3u  %8.2f  %8.2f   %+7.2f   %lu / %.2f / %lu\n",
                note, targetHz, pinHz, errCents, coil.minGapTicks, avgPeriod,
                coil.maxGapTicks);

    // (1) The pin oscillates at the period the oscillator actually scheduled
    // (pitchToPeriod[note]); averaged over hundreds of pulses the slip jitter
    // cancels, so the measured rate should be near-exact.
    CHECK(std::fabs(ToCents(pinHz, schedHz)) <= 5.0,
          "note %u: pin %.2f Hz != scheduled %.2f Hz (period %lu ticks)", note,
          pinHz, schedHz, static_cast<unsigned long>(pitchToPeriod[note]));

    // (2) End-to-end pitch accuracy vs the intended note frequency, held to the
    // single-clock floor (<=5 cents below 250 Hz, <=30 cents up to 2 kHz) -- the
    // same bounds test_frequency uses for the per-note path.
    const double bound = targetHz <= 250.0 ? 5.0 : 30.0;
    CHECK(std::fabs(errCents) <= bound,
          "note %u (%.2f Hz): pin off by %.2f cents (bound %.1f)", note, targetHz,
          errCents, bound);

    // The three output pins are driven together (pulseOn/pulseOff write coil,
    // diag and speaker), so they must show identical pulse counts.
    CHECK(crHostPin(diagOutPin).risingEdges == coil.risingEdges &&
              crHostPin(speakerOutPin).risingEdges == coil.risingEdges,
          "note %u: output pins disagree (coil %lu, diag %lu, speaker %lu)", note,
          coil.risingEdges, crHostPin(diagOutPin).risingEdges,
          crHostPin(speakerOutPin).risingEdges);

    // Note off: the pin must stop oscillating. Drive the ISR machine until the
    // voice is released (envelope -> idle -> oscillator expired back to the
    // pool), then confirm no further pulses are emitted.
    crmidi.handleNoteOff(1, note);
    int settle = 0;
    while (AudibleCount(oc) > 0 && settle < 200000) {
      driver.Tick();
      ++settle;
    }
    CHECK(AudibleCount(oc) == 0, "note %u: voice still sounding after note off", note);

    coil.Reset();
    for (int i = 0; i < 20000; ++i) {
      driver.Tick();
    }
    CHECK(coil.risingEdges == 0,
          "note %u: coil pin still pulsing after note off (%lu pulses)", note,
          coil.risingEdges);
  }
}

// Worst-case latency from a MIDI message arriving to the synth acting on it,
// measured in master-clock ISRs. Two stages:
//
//  (1) Parse latency. The .ino only calls MIDI.read() when the control task
//      (CRMidi::HandleControl) completes a cycle, and the control task only
//      advances on ISRs where no oscillator is due and no pulse is being output.
//      So sounding voices steal ISRs from it and spread the MIDI.read() points
//      apart. A message arriving just after a read waits until the next one --
//      that worst-case spacing is measured here, idle and under polyphony.
//
//  (2) Act latency. Once handleNoteOn runs, the voice is scheduled (ScheduleNow)
//      and the coil pin first pulses a few ISRs later. Measured on a fresh voice
//      (the shared coil pin can't attribute a pulse to one voice under load).
//
// Reported (not a tight pass/fail, except sanity ceilings): worst total =
// parse + act. This characterises responsiveness and guards against a change
// that lets the control task be starved of MIDI service.
void TestMessageLatency() {
  std::printf("[latency] worst-case MIDI message processing latency\n");
  const double usPerTick = 1e6 / static_cast<double>(masterClockHz);

  // Max ISRs between MIDI.read() points, for `voices` sustained notes starting at
  // `note`, measured over `ticks` ISRs after the schedule settles.
  auto readGap = [](uint8_t voices, uint8_t note, unsigned long ticks) -> unsigned long {
    OscillatorController oc;
    TestCRIO crio;
    CRMidi crmidi(&oc, &crio);
    IsrDriver driver(oc, crmidi, crio);
    for (uint8_t v = 0; v < voices; ++v) {
      crmidi.handleNoteOn(1, note + v, 100);  // distinct notes -> distinct voices
    }
    for (int i = 0; i < 4000; ++i) {
      driver.Tick();  // let scheduling reach steady state
    }
    driver.ResetLatency();
    for (unsigned long i = 0; i < ticks; ++i) {
      driver.Tick();
    }
    return driver.maxReadGap();
  };

  const unsigned long kRun = 200000;  // ~3.8 s of synth time
  const unsigned long idleGap = readGap(0, 0, kRun);
  const unsigned long chordGap = readGap(16, 60, kRun);              // dense mid chord
  const unsigned long topGap = readGap(16, maxMidiPitch - 15, kRun);  // 16 high voices

  // Act latency: handleNoteOn -> first coil pulse, on a fresh voice.
  unsigned long actTicks = 0;
  {
    OscillatorController oc;
    TestCRIO crio;
    CRMidi crmidi(&oc, &crio);
    IsrDriver driver(oc, crmidi, crio);
    for (int i = 0; i < 200; ++i) {
      driver.Tick();
    }
    crHostPin(coilOutPin).Reset();
    const unsigned long t0 = driver.masterTicks();
    crmidi.handleNoteOn(1, 60, 100);
    const unsigned long cap = t0 + 100000;
    while (crHostPin(coilOutPin).risingEdges == 0 && driver.masterTicks() < cap) {
      driver.Tick();
    }
    CHECK(crHostPin(coilOutPin).risingEdges > 0, "act latency: fresh voice never pulsed");
    if (crHostPin(coilOutPin).risingEdges > 0) {
      actTicks = crHostPin(coilOutPin).firstRisingTick - t0;
    }
    crHostPin(coilOutPin).Reset();
  }

  const unsigned long worst = chordGap + actTicks;
  std::printf("  parse latency, ISRs between MIDI.read(): idle %lu, 16-voice chord %lu, "
              "16 high voices %lu\n", idleGap, chordGap, topGap);
  std::printf("  act latency, note-on -> first coil pulse: %lu ISRs\n", actTicks);
  std::printf("  --> worst-case message latency (16-voice chord): %lu ISRs = %.1f us = %.3f ms\n",
              worst, worst * usPerTick, worst * usPerTick / 1000.0);
  std::printf("  --> adversarial 16x high-note load, parse latency: %lu ISRs = %.1f us\n",
              topGap, topGap * usPerTick);

  // Sanity ceilings: generous, meant to catch a control task that stops servicing
  // MIDI, not normal cadence. Idle tracks the control-cycle period; loaded stays
  // bounded (no starvation).
  // Measured today (deterministic, bit-identical across platforms): idle 11,
  // chord 23, high 29, act 2 ISRs. Ceilings sit a few x above to lock against a
  // real starvation regression while tolerating minor control-cadence tweaks.
  CHECK(idleGap > 0 && idleGap <= 32,
        "idle MIDI.read spacing %lu ISRs unexpected (control cadence broken?)", idleGap);
  CHECK(chordGap <= 128,
        "16-voice MIDI.read spacing %lu ISRs too high -- control task starved", chordGap);
  CHECK(topGap <= 256,
        "adversarial MIDI.read spacing %lu ISRs too high -- control task starved", topGap);
  CHECK(actTicks > 0 && actTicks <= 32, "act latency %lu ISRs unexpected", actTicks);
}

// Scheduling work per master ISR must stay bounded: the master clock fires every
// ~19 us, and the dominant per-ISR cost is OscillatorController::_Reschedule()
// scanning the oscillator pool. This locks that cost at O(oscillatorCount) and,
// crucially, independent of how many voices are sounding -- so a change that makes
// the scan nested, repeated, or polyphony-dependent (any of which could push the
// ISR past its budget on the Due) fails here. Counts come from the CR_HOST_TEST
// instrumentation in OscillatorController (testRescheduleCalls / Visits).
void TestRescheduleWorkBounded() {
  std::printf("[work] master-ISR scheduling work is O(oscillatorCount), polyphony-independent\n");

  // Run `voices` sustained notes from `baseNote` for `ticks` ISRs (after the
  // schedule settles) and report: visits-per-reschedule (must be exactly
  // oscillatorCount) and the worst oscillator-visits in any single ISR firing.
  struct Result { unsigned long perReschedule; unsigned long worstPerTick; unsigned long calls; };
  auto run = [](uint8_t voices, uint8_t baseNote, unsigned long ticks) -> Result {
    OscillatorController oc;
    TestCRIO crio;
    CRMidi crmidi(&oc, &crio);
    IsrDriver driver(oc, crmidi, crio);
    for (uint8_t v = 0; v < voices; ++v) {
      crmidi.handleNoteOn(1, baseNote + v, 100);  // distinct notes -> distinct voices
    }
    for (int i = 0; i < 4000; ++i) {
      driver.Tick();  // reach steady state before measuring
    }
    oc.testRescheduleCalls = 0;
    oc.testRescheduleVisits = 0;
    unsigned long worstPerTick = 0;
    for (unsigned long i = 0; i < ticks; ++i) {
      const unsigned long before = oc.testRescheduleVisits;
      driver.Tick();
      const unsigned long delta = oc.testRescheduleVisits - before;
      if (delta > worstPerTick) {
        worstPerTick = delta;
      }
    }
    const unsigned long perResched =
        oc.testRescheduleCalls ? oc.testRescheduleVisits / oc.testRescheduleCalls : 0;
    return {perResched, worstPerTick, oc.testRescheduleCalls};
  };

  const unsigned long kRun = 200000;
  const Result idle = run(0, 0, kRun);
  const Result chord = run(16, 60, kRun);
  const Result high = run(16, maxMidiPitch - 15, kRun);
  std::printf("  visits per reschedule: idle %lu, 16-voice %lu, 16 high %lu (oscillatorCount = %u)\n",
              idle.perReschedule, chord.perReschedule, high.perReschedule, oscillatorCount);
  std::printf("  worst oscillator-visits in one master ISR: idle %lu, 16-voice %lu, 16 high %lu\n",
              idle.worstPerTick, chord.worstPerTick, high.worstPerTick);

  // Each reschedule scans exactly the whole pool, regardless of load. A nested or
  // polyphony-scaled scan would break this immediately.
  CHECK(idle.perReschedule == oscillatorCount && chord.perReschedule == oscillatorCount &&
            high.perReschedule == oscillatorCount,
        "reschedule no longer O(oscillatorCount): %lu/%lu/%lu visits per call (expected %u)",
        idle.perReschedule, chord.perReschedule, high.perReschedule, oscillatorCount);

  // A single ISR reschedules at most twice (a slip tick does two catch-up
  // triggers), so per-ISR scheduling work is capped at 2*oscillatorCount, not
  // growing with polyphony.
  const unsigned long cap = 2 * oscillatorCount;
  CHECK(idle.worstPerTick <= cap && chord.worstPerTick <= cap && high.worstPerTick <= cap,
        "per-ISR scheduling work exceeded %lu (idle %lu, chord %lu, high %lu) -- ISR budget risk",
        cap, idle.worstPerTick, chord.worstPerTick, high.worstPerTick);

  // Guard the test is actually exercising the scheduler (not measuring nothing).
  CHECK(chord.calls > 0 && chord.worstPerTick > 0,
        "scheduler not exercised under load (calls %lu, worst %lu)", chord.calls,
        chord.worstPerTick);
}

}  // namespace

int main() {
  std::printf("chime_red2 host MIDI tests (master clock %lu Hz, max pitch %u)\n\n",
              (unsigned long)masterClockHz, maxMidiPitch);
  TestNoteOnOff();
  TestVelocityZeroIsNoteOff();
  TestPitchBend();
  TestDetuneCC();
  TestDetune2TwoOscillators();
  TestPolyphony();
  TestOutOfRange();
  TestAllNotesOff();
  TestPercussionChannel();
  TestEnvelopeAdsr();
  TestSchedulerAndLfoTick();
  TestModulation();
  TestControlChangeHandling();
  TestEndToEndPinOscillation();
  TestMessageLatency();
  TestRescheduleWorkBounded();
  std::printf("\n%s (%d failure%s)\n", g_failures == 0 ? "PASS" : "FAIL",
              g_failures, g_failures == 1 ? "" : "s");
  return g_failures == 0 ? 0 : 1;
}
