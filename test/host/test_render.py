#!/usr/bin/env python3
"""Host integration test for the SMF->WAV simulator (test/host/cr_render.cpp).

The hardware integration test (test/test_integration.py) needs a real synth on a
MIDI port. This one needs neither hardware nor a MIDI port: it generates Standard
MIDI Files, renders them with the `cr-render` tool (the real synth code, built
into the Docker image), and asserts on the resulting WAV. It runs as part of the
Docker test step (test/host/coverage.sh) so CI exercises the whole render path.

Stdlib only (struct, wave, subprocess) -- no external deps, matching the rest of
test/host. Exits non-zero on any failure.
"""

import math
import struct
import subprocess
import sys
import tempfile
import wave
import os

from smf_write import PPQ, mtrk, write_smf

CR_RENDER = os.environ.get("CR_RENDER", "cr-render")
failures = 0


def fail(msg):
    global failures
    failures += 1
    print("  FAIL: " + msg)


def render(mid, wav, *opts):
    subprocess.run([CR_RENDER, mid, wav, *opts], check=True, stderr=subprocess.DEVNULL)


def read_wav(path):
    w = wave.open(path, "rb")
    rate, n = w.getframerate(), w.getnframes()
    raw = w.readframes(n)
    w.close()
    return rate, list(struct.unpack("<%dh" % n, raw))


def midi_hz(note):
    return 440.0 * 2 ** ((note - 69) / 12.0)


def pulse_hz(samples, rate):
    """Pulse rate from rising edges (0 -> positive) on the raw unipolar gate."""
    edges = [i for i in range(1, len(samples)) if samples[i - 1] <= 0 < samples[i]]
    if len(edges) < 3:
        return 0.0
    a, b = edges[len(edges) // 4], edges[3 * len(edges) // 4]
    ne = (3 * len(edges) // 4) - (len(edges) // 4)
    return rate * ne / (b - a)


def test_single_note_pitch(tmp):
    """Each rendered note's pulse rate matches its frequency to the clock floor."""
    print("[render] single-note pitch across the playable range")
    for note in (36, 48, 60, 69, 84, 96):
        events = [
            (0, b"\xff\x51\x03" + struct.pack(">I", 500000)[1:]),
            (0, bytes([0x90, note, 100])),
            (PPQ * 2, bytes([0x80, note, 0])),
        ]
        mid, wav = tmp + "/n.mid", tmp + "/n.wav"
        write_smf(mid, events)
        render(mid, wav, "--raw", "--no-normalize", "--tail", "0.2")
        rate, s = read_wav(wav)
        hz = pulse_hz(s, rate)
        tgt = midi_hz(note)
        cents = 1200 * math.log2(hz / tgt) if hz > 0 else 999
        bound = 5.0 if tgt <= 250.0 else 30.0
        print(
            "  note %3d  target %8.2f Hz  rendered %8.2f Hz  (%+6.1f cents)"
            % (note, tgt, hz, cents)
        )
        if abs(cents) > bound:
            fail("note %d off by %.1f cents (bound %.1f)" % (note, cents, bound))


def test_polyphony_and_perc(tmp):
    """A chord + channel-10 percussion renders audible, finite, correctly-sized."""
    print("[render] chord + percussion is non-silent and correctly timed")
    ev: list = [(0, b"\xff\x51\x03" + struct.pack(">I", 500000)[1:])]
    for n in (48, 52, 55):  # held triad on ch1
        ev.append((0, bytes([0x90, n, 90])))
    ev.append((PPQ * 2, bytes([0x80, 48, 0])))
    ev += [(0, bytes([0x80, 52, 0])), (0, bytes([0x80, 55, 0]))]
    # ch10 GM 38 = snare (a mapped drum; unmapped channel-10 notes are silent).
    ev += [(0, bytes([0x99, 38, 120])), (PPQ // 2, bytes([0x89, 38, 0]))]
    mid, wav = tmp + "/c.mid", tmp + "/c.wav"
    write_smf(mid, ev)
    render(mid, wav, "--tail", "1.0")
    rate, s = read_wav(wav)
    if not s:
        fail("chord render produced no samples")
        return
    peak = max(abs(x) for x in s)
    nonzero = sum(1 for x in s if x != 0)
    dur = len(s) / rate
    print(
        "  duration %.2f s, %d samples @ %d Hz, peak %d, %.1f%% non-zero"
        % (dur, len(s), rate, peak, 100.0 * nonzero / len(s))
    )
    if peak == 0:
        fail("chord render is silent (peak 0)")
    # triad held 2 beats @120bpm (1.0s); ch10 hit ends 0.25s later (last event
    # 1.25s) + 1.0s tail = ~2.25s.
    if not (2.0 < dur < 2.6):
        fail("chord render duration %.2f s out of expected range" % dur)


def test_format1_multitrack(tmp):
    """Format-1 multi-track + a mid-file tempo change renders without error."""
    print("[render] format-1 multi-track with tempo change")
    cond = mtrk(
        [
            (0, b"\xff\x51\x03" + struct.pack(">I", 500000)[1:]),
            (PPQ * 2, b"\xff\x51\x03" + struct.pack(">I", 1000000)[1:]),
        ]
    )
    notes = mtrk(
        [
            (0, bytes([0x90, 60, 100])),
            (PPQ, bytes([60, 0])),  # running status
            (0, bytes([0x90, 64, 100])),
            (PPQ * 2, bytes([0x80, 64, 0])),
        ]
    )
    mid, wav = tmp + "/f1.mid", tmp + "/f1.wav"
    write_smf(mid, None, fmt=1, ntrks=2, tracks=[cond, notes])
    render(mid, wav, "--tail", "0.5")
    rate, s = read_wav(wav)
    # 2 beats @120bpm (1.0s) then 1 beat @60bpm to the last note-off at tick 1440
    # (1.0s) -> last event 2.0s, + 0.5s tail = ~2.5s. A wrong tempo map would shift
    # this (e.g. ignoring the change gives ~1.5s).
    dur = len(s) / rate
    print("  duration %.2f s (expected ~2.5 s)" % dur)
    if not (2.2 < dur < 2.8):
        fail("format-1 tempo-mapped duration %.2f s out of expected range" % dur)


def main():
    print("chime_red2 host render (SMF->WAV simulator) integration test")
    with tempfile.TemporaryDirectory() as tmp:
        test_single_note_pitch(tmp)
        test_polyphony_and_perc(tmp)
        test_format1_multitrack(tmp)
    print(
        "\n%s (%d failure%s)"
        % ("PASS" if failures == 0 else "FAIL", failures, "" if failures == 1 else "s")
    )
    return 0 if failures == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
