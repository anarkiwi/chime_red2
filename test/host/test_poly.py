#!/usr/bin/env python3
"""Host integration test: polyphony + velocity dynamics through the real synth.

Builds a multi-track SMF in memory (like test_render.py -- no tracked fixture),
renders it with `cr-render`, and asserts on the raw coil-gate WAV. Two things are
checked, both auto-verifiable from the gate signal alone:

  1. POLYPHONY -- the synth sounds chords on multiple channels at once, it is NOT
     mono-priority. Two C-major triads (six voices) struck together on channels 1
     and 2 produce a coil edge rate far above the highest single voice (all a
     "play only the highest note" engine could manage). The coil is a single
     1-bit gate, so the voices' pulse trains are time-interleaved onto it; the
     realised rate sits below the ideal sum of the six because pulses landing on
     the same master tick coalesce (more so at higher pitches/voice counts). So we
     assert: well above the highest single voice, and not exceeding the ideal sum.

  2. VELOCITY -- note-on velocity scales the pulse WIDTH (Oscillator::pulseUsScale
     = hzPulseUsScale * velocityScale). At a FIXED pitch (so the edge rate, set by
     pitch, is constant) the gate DUTY must rise monotonically with velocity,
     while the edge rate stays put. Three velocity levels are struck on one held
     pitch; the two simultaneous triads above are also at different velocities.

Empirically grounded (probe, master clock 52631 Hz): the two triads (C4 E4 G4 +
C5 E5 G5) -> ~1715 Hz edge rate (highest-only would be 784 Hz, ideal six-note sum
2950 Hz); C5 at vel 25/75/125 -> duty 0.0081 / 0.0244 / 0.0406 (~1:3:5), edge rate
522 Hz throughout.

Runs from coverage.sh (guarded on cr-render existing), exits non-zero on failure.
Also writes the rendered WAV to $CR_POLY_WAV if set, for listening.
"""

import math
import os
import struct
import subprocess
import sys
import tempfile
import wave

from smf_write import PPQ, mtrk, note_off, note_on, tempo, write_smf

CR_RENDER = os.environ.get("CR_RENDER", "cr-render")
TEMPO = 500000  # us/qn -> 120 bpm, so a quarter note (PPQ ticks) is 0.5 s
failures = 0


def fail(msg):
    global failures
    failures += 1
    print("  FAIL: " + msg)


def midi_hz(note):
    return 440.0 * 2 ** ((note - 69) / 12.0)


def ticks(tsec):
    """Absolute SMF ticks for a wall-clock time (at TEMPO)."""
    return int(round(tsec / (TEMPO / 1e6) * PPQ))


def read_wav(path):
    w = wave.open(path, "rb")
    rate, n = w.getframerate(), w.getnframes()
    raw = w.readframes(n)
    w.close()
    return rate, list(struct.unpack("<%dh" % n, raw))


def edge_rate(samples, rate, t0, t1):
    """Rising-edge rate (Hz) of the unipolar gate in [t0, t1] s."""
    lo, hi = int(t0 * rate), int(t1 * rate)
    seg = samples[lo:hi]
    edges = sum(1 for i in range(1, len(seg)) if seg[i - 1] <= 0 < seg[i])
    return edges / (t1 - t0)


def duty(samples, rate, t0, t1):
    """Mean gate level (= coil on-time fraction) in [t0, t1] s."""
    lo, hi = int(t0 * rate), int(t1 * rate)
    seg = samples[lo:hi]
    return sum(seg) / len(seg) / 32767.0 if seg else 0.0


def _events_to_track(timed_events):
    """Turn a list of (abs_tick, event_bytes) into an MTrk (delta-encoded)."""
    timed_events = sorted(timed_events, key=lambda te: te[0])
    out = []
    last = 0
    for abs_tick, ev in timed_events:
        out.append((abs_tick - last, ev))
        last = abs_tick
    return mtrk(out)


# Two C-major triads, one per channel, struck together -- six simultaneous
# voices across two channels, each channel at a different velocity. This is the
# "chords on multiple channels at different velocity levels" case.
CH1_TRIAD = [60, 64, 67]  # C4 E4 G4
CH2_TRIAD = [72, 76, 79]  # C5 E5 G5
CHORD_PITCHES = CH1_TRIAD + CH2_TRIAD
# Highest single voice (a mono-priority engine's ceiling) vs. the ideal sum of
# all six interleaved pulse trains.
CHORD_HIGHEST_HZ = midi_hz(max(CHORD_PITCHES))
CHORD_SUM_HZ = sum(midi_hz(n) for n in CHORD_PITCHES)

# Velocity ramp: one fixed pitch (so edge rate is constant) struck at rising
# velocities in non-overlapping windows.
VEL_PITCH = 72  # C5
VEL_STEPS = ((25, 2.0), (75, 3.0), (125, 4.0))  # (velocity, onset seconds)


def build_smf(path):
    """Two-track SMF exercising both behaviours.

    Phase A (0.0-1.6 s): ch 1 plays a C4/E4/G4 triad at velocity 110 while ch 2
    plays a C5/E5/G5 triad at velocity 50 -- six voices on two channels at two
    velocity levels, all sounding at once (the polyphony case).
    Phase B (2.0-4.7 s): ch 2 strikes one fixed pitch (C5) three times at rising
    velocity (25, 75, 125) in separate ~0.7 s windows (the velocity case).
    """
    a = [(0, tempo(TEMPO))]
    for n in CH1_TRIAD:
        a.append((0, note_on(1, n, 110)))
    off = ticks(1.6)
    for n in CH1_TRIAD:
        a.append((off, note_off(1, n)))

    b = []
    for n in CH2_TRIAD:
        b.append((0, note_on(2, n, 50)))
    off = ticks(1.6)
    for n in CH2_TRIAD:
        b.append((off, note_off(2, n)))
    for vel, tsec in VEL_STEPS:
        b.append((ticks(tsec), note_on(2, VEL_PITCH, vel)))
        b.append((ticks(tsec + 0.7), note_off(2, VEL_PITCH)))

    write_smf(
        path,
        None,
        fmt=1,
        ntrks=2,
        tracks=[_events_to_track(a), _events_to_track(b)],
    )


def test_polyphony(samples, rate):
    print("[poly] two simultaneous triads on two channels sound all six voices")
    measured = edge_rate(samples, rate, 0.3, 1.4)
    highest = CHORD_HIGHEST_HZ  # all a mono-priority engine could produce
    ideal_sum = CHORD_SUM_HZ
    print(
        "  edge rate %.1f Hz (highest-note-only %.1f, ideal 6-note sum %.1f)"
        % (measured, highest, ideal_sum)
    )
    # Polyphony shows up as an edge rate far above any single voice: a mono engine
    # could produce at most the highest note's rate, so >1.5x that proves multiple
    # voices interleave onto the coil. The upper bound is the ideal sum of all six
    # rates; the realised rate sits below it because pulses that land on the same
    # 1/52631 s master tick coalesce (more voices / higher pitches -> more
    # collisions), so we only require it not to EXCEED the ideal sum.
    if not measured > 1.5 * highest:
        fail(
            "chord edge rate %.1f Hz not above 1.5x the highest voice %.1f Hz "
            "-- looks mono, not polyphonic" % (measured, highest)
        )
    if not measured <= 1.02 * ideal_sum:
        fail(
            "chord edge rate %.1f Hz exceeds the ideal six-note sum %.1f Hz "
            "(impossible -- measurement or rate bug)" % (measured, ideal_sum)
        )


def test_velocity(samples, rate):
    print("[poly] note-on velocity scales gate duty (pulse width), not pitch")
    wins = [(25, 2.1, 2.6), (75, 3.1, 3.6), (125, 4.1, 4.6)]
    duties, rates = [], []
    pitch_hz = midi_hz(72)  # C5 -- fixed, so edge rate must stay constant
    for vel, t0, t1 in wins:
        d = duty(samples, rate, t0, t1)
        r = edge_rate(samples, rate, t0, t1)
        duties.append((vel, d))
        rates.append((vel, r))
        print("  vel %3d: duty %.5f, edge rate %.1f Hz" % (vel, d, r))

    # Duty strictly increases with velocity.
    for (v0, d0), (v1, d1) in zip(duties, duties[1:]):
        if not d1 > d0 * 1.2:
            fail(
                "duty did not rise enough from vel %d (%.5f) to vel %d (%.5f)"
                % (v0, d0, v1, d1)
            )
    # All three at the same pitch -> edge rate within 5%% of C5 and of each other.
    for vel, r in rates:
        if r <= 0 or abs(1200 * math.log2(r / pitch_hz)) > 90:  # ~5% = ~85 cents
            fail(
                "vel %d edge rate %.1f Hz strayed from C5 %.1f Hz" % (vel, r, pitch_hz)
            )


def main():
    print("chime_red2 host polyphony + velocity integration test")
    keep = os.environ.get("CR_POLY_WAV")  # optional: persist the WAV for listening
    with tempfile.TemporaryDirectory() as tmp:
        mid = os.path.join(tmp, "poly.mid")
        raw_wav = os.path.join(tmp, "poly_raw.wav")
        build_smf(mid)
        # Raw unipolar gate for measurement (edge rate / duty are gate properties).
        subprocess.run(
            [CR_RENDER, mid, raw_wav, "--raw", "--no-normalize", "--tail", "0.3"],
            check=True,
            stderr=subprocess.DEVNULL,
        )
        rate, samples = read_wav(raw_wav)
        test_polyphony(samples, rate)
        test_velocity(samples, rate)
        if keep:
            # A normalised, DC-blocked render is the nice-to-listen-to version.
            subprocess.run(
                [CR_RENDER, mid, keep, "--tail", "0.5", "--rate", "44100"],
                check=True,
                stderr=subprocess.DEVNULL,
            )
            print("  wrote listenable WAV to " + keep)
    print(
        "\n%s (%d failure%s)"
        % ("PASS" if failures == 0 else "FAIL", failures, "" if failures == 1 else "s")
    )
    return 0 if failures == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
