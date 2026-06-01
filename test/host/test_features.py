#!/usr/bin/env python3
"""Host integration test: synth features through the real render path.

Builds a multi-track SMF in memory (like test_render.py -- no tracked fixture),
renders it with `cr-render`, and asserts on the raw coil-gate WAV. Four feature
areas are checked, all auto-verifiable from the gate signal alone (edge rate =
pitch, duty = amplitude):

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

  3. ENVELOPE -- the ADSR shapes the duty over a note: a slow attack ramps up then
     holds; a decay falls to a held sustain (below the full-sustain plateau); and
     a sustain-0 note rings down exponentially (near-constant window-to-window
     duty ratio) to silence and frees itself.

  4. LFOs -- vibrato (CC1 depth, CC76 Hz) wobbles the PITCH, so the edge rate
     swings; tremolo (CC92 depth, CC22 Hz) wobbles the AMPLITUDE, so the duty
     swings while the edge rate stays at the steady-note floor.

Empirically grounded (probe, master clock 52631 Hz): the two triads (C4 E4 G4 +
C5 E5 G5) -> ~1715 Hz edge rate (highest-only would be 784 Hz, ideal six-note sum
2950 Hz); C5 at vel 25/75/125 -> duty 0.0081 / 0.0244 / 0.0406 (~1:3:5); sustain-0
ring duty ratio ~0.20 per window; vibrato edge-rate swing ~27% vs ~4% steady;
tremolo duty swing ~117% with edge rate flat at ~4%.

Runs from coverage.sh (guarded on cr-render existing), exits non-zero on failure.
Also writes the rendered WAV to $CR_FEATURES_WAV if set, for listening.
"""

import math
import os
import struct
import subprocess
import sys
import tempfile
import wave

from smf_write import PPQ, cc, mtrk, note_off, note_on, tempo, write_smf

CR_RENDER = os.environ.get("CR_RENDER", "cr-render")
TEMPO = 500000  # us/qn -> 120 bpm, so a quarter note (PPQ ticks) is 0.5 s
# ADSR control-change numbers (mirror CRMidi::handleControlChange).
CC_ATTACK, CC_DECAY, CC_SUSTAIN, CC_RELEASE = 73, 75, 24, 72
# Vibrato (pitch LFO) and tremolo (amplitude LFO): per-channel depth + global Hz.
CC_VIB_RANGE, CC_VIB_HZ = 1, 76
CC_TREM_RANGE, CC_TREM_HZ = 92, 22
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

# Envelope phases, all on ch 3 / one fixed pitch / full velocity, so the gate
# DUTY tracks the amplitude envelope alone. Each sets its own ADSR then strikes.
ENV_PITCH = 72  # C5
ENV_ATTACK_ON = 6.0  # slow attack (CC50 ~146ms), full sustain, held -> ramp+hold
ENV_DECAY_ON = 8.0  # instant attack, decay CC40, sustain 64, held -> decay+hold
ENV_RING_ON = 10.0  # instant attack, decay CC70, sustain 0, NO note-off -> rings
# down exponentially and frees itself (the struck-bell case + the sustain-0 fix).

# LFO phases on ch 4 / one held pitch, steady amplitude (no decay), so the only
# variation over the note is the LFO. Vibrato (CC1 depth, CC76 ~5 Hz) wobbles the
# PITCH -> the pulse edge rate swings. Tremolo (CC92 depth, CC22 ~5 Hz) wobbles
# the pulse WIDTH -> the gate duty swings while the edge rate stays put.
LFO_PITCH = 72  # C5
LFO_HZ_CC = 64  # ~5 Hz (0-127 maps to 0-10 Hz)
VIB_ON = 12.5  # vibrato note onset (after the envelope phases have freed)
TREM_ON = 15.0  # tremolo note onset
LFO_HOLD = 2.0  # hold each ~2 s so several LFO cycles land in the window


def build_smf(path):
    """Three-track SMF exercising polyphony, velocity, and the ADSR envelope.

    Phase A (0.0-1.6 s): ch 1 plays a C4/E4/G4 triad at velocity 110 while ch 2
    plays a C5/E5/G5 triad at velocity 50 -- six voices on two channels at two
    velocity levels, all sounding at once (the polyphony case).
    Phase B (2.0-4.7 s): ch 2 strikes one fixed pitch (C5) three times at rising
    velocity (25, 75, 125) in separate ~0.7 s windows (the velocity case).
    Phase C (6.0-~11 s, ch 3): one pitch run through the envelope phases --
    a slow attack ramp, a decay to a held sustain, and a sustain-0 exponential
    ring-down with no note-off (must fade to silence and free itself).
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

    c = []

    def adsr(tsec, atk, dec, sus, rel):
        for num, val in (
            (CC_ATTACK, atk),
            (CC_DECAY, dec),
            (CC_SUSTAIN, sus),
            (CC_RELEASE, rel),
        ):
            c.append((ticks(tsec), cc(3, num, val)))

    # Attack ramp: slow attack, full sustain; held 1.2 s then released.
    adsr(ENV_ATTACK_ON, 50, 0, 127, 20)
    c.append((ticks(ENV_ATTACK_ON), note_on(3, ENV_PITCH, 127)))
    c.append((ticks(ENV_ATTACK_ON + 1.2), note_off(3, ENV_PITCH)))
    # Decay to a held sustain: instant attack, decay CC40, sustain 64; held 1.2 s.
    adsr(ENV_DECAY_ON, 0, 40, 64, 20)
    c.append((ticks(ENV_DECAY_ON), note_on(3, ENV_PITCH, 127)))
    c.append((ticks(ENV_DECAY_ON + 1.2), note_off(3, ENV_PITCH)))
    # Sustain-0 exponential ring-down, NO note-off: must fall to silence + free.
    adsr(ENV_RING_ON, 0, 70, 0, 20)
    c.append((ticks(ENV_RING_ON), note_on(3, ENV_PITCH, 127)))

    # Track D (ch 4): steady-amplitude held notes with vibrato then tremolo.
    d = []

    def steady_adsr(tsec):
        # Instant attack, no decay, full sustain -> a flat amplitude the LFO rides.
        for num, val in (
            (CC_ATTACK, 0),
            (CC_DECAY, 0),
            (CC_SUSTAIN, 127),
            (CC_RELEASE, 20),
        ):
            d.append((ticks(tsec), cc(4, num, val)))

    # Vibrato: pitch LFO on, tremolo off; held LFO_HOLD s.
    steady_adsr(VIB_ON)
    d.append((ticks(VIB_ON), cc(4, CC_VIB_RANGE, 100)))
    d.append((ticks(VIB_ON), cc(4, CC_VIB_HZ, LFO_HZ_CC)))
    d.append((ticks(VIB_ON), cc(4, CC_TREM_RANGE, 0)))
    d.append((ticks(VIB_ON), note_on(4, LFO_PITCH, 127)))
    d.append((ticks(VIB_ON + LFO_HOLD), note_off(4, LFO_PITCH)))
    # Tremolo: amplitude LFO on, vibrato off; held LFO_HOLD s.
    steady_adsr(TREM_ON)
    d.append((ticks(TREM_ON), cc(4, CC_VIB_RANGE, 0)))
    d.append((ticks(TREM_ON), cc(4, CC_TREM_RANGE, 100)))
    d.append((ticks(TREM_ON), cc(4, CC_TREM_HZ, LFO_HZ_CC)))
    d.append((ticks(TREM_ON), note_on(4, LFO_PITCH, 127)))
    d.append((ticks(TREM_ON + LFO_HOLD), note_off(4, LFO_PITCH)))

    write_smf(
        path,
        None,
        fmt=1,
        ntrks=4,
        tracks=[
            _events_to_track(a),
            _events_to_track(b),
            _events_to_track(c),
            _events_to_track(d),
        ],
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


def test_envelope(samples, rate):
    print("[poly] ADSR envelope shapes the gate duty over the note")
    # 1) Attack ramps up then holds (full sustain). Strike window is lower than
    # the plateau; the plateau is steady.
    atk_strike = duty(samples, rate, ENV_ATTACK_ON + 0.0, ENV_ATTACK_ON + 0.05)
    atk_peak = duty(samples, rate, ENV_ATTACK_ON + 0.15, ENV_ATTACK_ON + 0.20)
    atk_hold = duty(samples, rate, ENV_ATTACK_ON + 0.6, ENV_ATTACK_ON + 1.0)
    print(
        "  attack: strike %.5f -> peak %.5f, hold %.5f"
        % (atk_strike, atk_peak, atk_hold)
    )
    if not atk_peak > 2.0 * atk_strike:
        fail("attack did not ramp up (strike %.5f, peak %.5f)" % (atk_strike, atk_peak))
    if not abs(atk_hold - atk_peak) < 0.2 * atk_peak:
        fail("attack plateau not held (peak %.5f, hold %.5f)" % (atk_peak, atk_hold))

    # 2) Decay falls from the strike to a held sustain ~ (64/127) of full level.
    dec_strike = duty(samples, rate, ENV_DECAY_ON + 0.0, ENV_DECAY_ON + 0.05)
    dec_hold = duty(samples, rate, ENV_DECAY_ON + 0.7, ENV_DECAY_ON + 1.1)
    print("  decay:  strike %.5f -> sustain-hold %.5f" % (dec_strike, dec_hold))
    if not dec_hold < 0.8 * dec_strike:
        fail(
            "decay did not fall to sustain (strike %.5f, hold %.5f)"
            % (dec_strike, dec_hold)
        )
    if not dec_hold > 0.1 * dec_strike:
        fail(
            "sustain-64 collapsed to silence (strike %.5f, hold %.5f)"
            % (dec_strike, dec_hold)
        )
    # The sustain (64) hold should sit below the full-sustain (127) attack plateau.
    if not dec_hold < atk_hold:
        fail(
            "sustain-64 hold %.5f not below full-sustain hold %.5f"
            % (dec_hold, atk_hold)
        )

    # 3) Sustain-0, no note-off: exponential ring-down to silence that frees the
    # voice. Duty must decrease across successive windows and reach ~0, with the
    # decline being exponential (near-constant window-to-window RATIO, not a
    # constant linear step).
    win = [
        duty(samples, rate, ENV_RING_ON + a, ENV_RING_ON + a + 0.2)
        for a in (0.05, 0.35, 0.65, 0.95)
    ]
    tail = duty(samples, rate, ENV_RING_ON + 1.8, ENV_RING_ON + 2.3)
    print("  ring:   %s -> tail %.5f" % (" ".join("%.5f" % d for d in win), tail))
    if not all(win[i] > win[i + 1] for i in range(len(win) - 1)):
        fail("sustain-0 ring did not decrease monotonically: %s" % win)
    if not tail < 0.02 * win[0]:
        fail(
            "sustain-0 note did not ring down to silence (start %.5f, tail %.5f)"
            % (win[0], tail)
        )
    # Exponential: consecutive ratios stay similar (linear decay would make the
    # ratios climb toward zero as the step becomes a larger fraction of the level).
    ratios = [win[i + 1] / win[i] for i in range(len(win) - 1) if win[i] > 0]
    print("    window ratios %s" % (" ".join("%.2f" % r for r in ratios)))
    if len(ratios) >= 2 and not max(ratios) - min(ratios) < 0.25:
        fail("ring-down not exponential (window ratios vary too much: %s)" % ratios)


def _lfo_spans(samples, rate, t0, t1, wms=50):
    """Per-window edge rate and duty across [t0, t1]; return their relative
    swings (peak-to-peak / mean) -- how much each dimension the LFO moves."""
    step = int(rate * wms / 1000)
    lo, hi = int(t0 * rate), int(t1 * rate)
    rates, duties = [], []
    for a in range(lo, hi - step, step):
        seg = samples[a : a + step]
        edges = sum(1 for i in range(1, len(seg)) if seg[i - 1] <= 0 < seg[i])
        rates.append(edges / (wms / 1000.0))
        duties.append(sum(seg) / len(seg) / 32767.0)
    rmean = sum(rates) / len(rates)
    dmean = sum(duties) / len(duties)
    rswing = (max(rates) - min(rates)) / rmean if rmean else 0.0
    dswing = (max(duties) - min(duties)) / dmean if dmean else 0.0
    return rswing, dswing, rmean


def test_lfo(samples, rate):
    print("[poly] vibrato modulates pitch; tremolo modulates amplitude")
    # Baseline: a steady note's own jitter floor for both dimensions. Reuse the
    # full-sustain attack hold window (ch 3), which has no LFO.
    base_r, base_d, _ = _lfo_spans(
        samples, rate, ENV_ATTACK_ON + 0.6, ENV_ATTACK_ON + 1.0
    )
    print(
        "  baseline (no LFO): edge-rate swing %.0f%%, duty swing %.0f%%"
        % (100 * base_r, 100 * base_d)
    )

    # Vibrato: pitch wobble -> the pulse edge rate swings well above the floor.
    vib_r, vib_d, vib_mean = _lfo_spans(
        samples, rate, VIB_ON + 0.2, VIB_ON + LFO_HOLD - 0.1
    )
    print(
        "  vibrato: edge-rate swing %.0f%% (mean %.0f Hz), duty swing %.0f%%"
        % (100 * vib_r, vib_mean, 100 * vib_d)
    )
    if not vib_r > 0.10:
        fail("vibrato did not swing the pitch (edge-rate swing %.0f%%)" % (100 * vib_r))
    if not vib_r > 3 * base_r:
        fail(
            "vibrato edge-rate swing %.0f%% not clearly above the no-LFO floor %.0f%%"
            % (100 * vib_r, 100 * base_r)
        )

    # Tremolo: amplitude wobble -> the duty swings hugely while the edge rate
    # (pitch) stays at the steady-note floor.
    trem_r, trem_d, _ = _lfo_spans(
        samples, rate, TREM_ON + 0.2, TREM_ON + LFO_HOLD - 0.1
    )
    print(
        "  tremolo: duty swing %.0f%%, edge-rate swing %.0f%%"
        % (100 * trem_d, 100 * trem_r)
    )
    if not trem_d > 0.30:
        fail("tremolo did not swing the amplitude (duty swing %.0f%%)" % (100 * trem_d))
    if not trem_d > 3 * base_d:
        fail(
            "tremolo duty swing %.0f%% not clearly above the no-LFO floor %.0f%%"
            % (100 * trem_d, 100 * base_d)
        )
    if not trem_r < 0.10:
        fail(
            "tremolo perturbed the pitch (edge-rate swing %.0f%% should stay near "
            "the no-LFO floor)" % (100 * trem_r)
        )


def main():
    print("chime_red2 host feature integration test (polyphony/velocity/envelope/LFOs)")
    keep = os.environ.get("CR_FEATURES_WAV")  # optional: persist the WAV for listening
    with tempfile.TemporaryDirectory() as tmp:
        mid = os.path.join(tmp, "poly.mid")
        raw_wav = os.path.join(tmp, "poly_raw.wav")
        build_smf(mid)
        # Raw unipolar gate for measurement (edge rate / duty are gate properties).
        # The tail must outlast the sustain-0 ring-down (last note-on at 10 s, no
        # note-off) so the test can see it fall to silence.
        subprocess.run(
            [CR_RENDER, mid, raw_wav, "--raw", "--no-normalize", "--tail", "3"],
            check=True,
            stderr=subprocess.DEVNULL,
        )
        rate, samples = read_wav(raw_wav)
        test_polyphony(samples, rate)
        test_velocity(samples, rate)
        test_envelope(samples, rate)
        test_lfo(samples, rate)
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
