#!/usr/bin/env python3
"""Host integration test for the FM-bell feature.

Renders the tracked FM fixtures (test/host/fixtures/, built by make_fixtures.py)
with the real synth (`cr-render`) and asserts on the coil gate, stdlib-only:

  1. FM modulates the carrier period -- the spread (coefficient of variation) of
     the pulse-to-pulse intervals is far larger with FM on than off, while the
     mean pulse rate still sits on the played pitch.
  2. The modulation-index envelope decays brightness -- for a fixture with a mod
     decay the interval spread collapses from the strike to later in the note,
     while a constant-index fixture keeps spreading.

Runs from coverage.sh (guarded on cr-render existing), like test_render.py. Exits
non-zero on any failure.
"""

import math
import os
import struct
import subprocess
import sys
import tempfile
import wave

CR_RENDER = os.environ.get("CR_RENDER", "cr-render")
FIXTURES_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fixtures")
failures = 0


def fail(msg):
    global failures
    failures += 1
    print("  FAIL: " + msg)


def midi_hz(note):
    return 440.0 * 2 ** ((note - 69) / 12.0)


def read_wav(path):
    w = wave.open(path, "rb")
    rate, n = w.getframerate(), w.getnframes()
    raw = w.readframes(n)
    w.close()
    return rate, list(struct.unpack("<%dh" % n, raw))


def render_fixture(name, tmp):
    """Render a tracked fixture to a raw unipolar-gate WAV and load it."""
    mid = os.path.join(FIXTURES_DIR, name + ".mid")
    wav = os.path.join(tmp, name + ".wav")
    subprocess.run(
        [CR_RENDER, mid, wav, "--raw", "--no-normalize", "--tail", "0.2"],
        check=True,
        stderr=subprocess.DEVNULL,
    )
    return read_wav(wav)


def cov(values):
    """Coefficient of variation (std / mean) -- 0 for a perfectly steady series."""
    if len(values) < 2:
        return 0.0
    mean = sum(values) / len(values)
    if mean == 0:
        return 0.0
    var = sum((v - mean) ** 2 for v in values) / len(values)
    return math.sqrt(var) / mean


def interval_stats(samples, rate, t0, t1):
    """CoV and mean rate of rising-edge intervals in the [t0, t1] s window."""
    lo, hi = int(t0 * rate), int(t1 * rate)
    seg = samples[lo:hi]
    edges = [i for i in range(1, len(seg)) if seg[i - 1] <= 0 < seg[i]]
    if len(edges) < 4:
        return 0.0, 0.0
    intervals = [edges[i] - edges[i - 1] for i in range(1, len(edges))]
    mean_interval = sum(intervals) / len(intervals)
    return cov(intervals), (rate / mean_interval if mean_interval else 0.0)


def test_fm_modulates_period(tmp):
    print("[fm] FM modulates the carrier period (vs FM off)")
    rate_off, s_off = render_fixture("fm_off_c5", tmp)
    rate_on, s_on = render_fixture("fm_decay_c5", tmp)
    cov_off, hz_off = interval_stats(s_off, rate_off, 0.1, 0.6)
    cov_on, hz_on = interval_stats(s_on, rate_on, 0.1, 0.6)
    tgt = midi_hz(72)
    print(
        "  FM off: pulse %.1f Hz (target %.1f), interval CoV %.4f"
        % (hz_off, tgt, cov_off)
    )
    print("  FM on : pulse %.1f Hz, interval CoV %.4f" % (hz_on, cov_on))
    if hz_off <= 0 or abs(1200 * math.log2(hz_off / tgt)) > 30:
        fail("FM-off pulse rate %.1f Hz off target %.1f" % (hz_off, tgt))
    if not (cov_on > 3 * cov_off and cov_on > 0.05):
        fail(
            "FM did not modulate the period (CoV on %.4f vs off %.4f)"
            % (cov_on, cov_off)
        )


def test_mod_envelope_decays_brightness(tmp):
    print("[fm] modulation-index envelope decays brightness over the note")
    rate_d, s_d = render_fixture("fm_decay_c5", tmp)
    rate_c, s_c = render_fixture("fm_constant_c5", tmp)
    early_d, _ = interval_stats(s_d, rate_d, 0.1, 0.6)
    late_d, _ = interval_stats(s_d, rate_d, 2.5, 3.0)
    early_c, _ = interval_stats(s_c, rate_c, 0.1, 0.6)
    late_c, _ = interval_stats(s_c, rate_c, 2.5, 3.0)
    print("  decaying index: interval CoV early %.4f -> late %.4f" % (early_d, late_d))
    print("  constant index: interval CoV early %.4f -> late %.4f" % (early_c, late_c))
    if not (early_d > 0.05 and late_d < 0.5 * early_d):
        fail(
            "mod envelope did not decay brightness (early %.4f late %.4f)"
            % (early_d, late_d)
        )
    if not (late_c > 0.5 * early_c):
        fail(
            "constant index unexpectedly decayed (early %.4f late %.4f)"
            % (early_c, late_c)
        )


def test_fm_bass_and_preset(tmp):
    print("[fm] fine-ratio bass + program-change preset path")
    rate_f, s_f = render_fixture("fm_bass_fat", tmp)
    rate_p, s_p = render_fixture("fm_bass_preset", tmp)
    cov_f, hz_f = interval_stats(s_f, rate_f, 0.15, 0.5)
    cov_p, _ = interval_stats(s_p, rate_p, 0.15, 0.5)
    print("  fat bass (CC, ratio 1.08): CoV %.4f at %.0f Hz" % (cov_f, hz_f))
    print("  fat bass (Program Change 40): CoV %.4f" % cov_p)
    if not cov_f > 0.05:
        fail("fine-ratio fat bass did not modulate (CoV %.4f)" % cov_f)
    if not cov_p > 0.05:
        fail("program-change preset produced no FM (CoV %.4f)" % cov_p)
    if abs(cov_p - cov_f) > 0.03:
        fail("preset voice %.4f != CC-driven voice %.4f" % (cov_p, cov_f))


def main():
    print("chime_red2 host FM-bell integration test")
    with tempfile.TemporaryDirectory() as tmp:
        test_fm_modulates_period(tmp)
        test_mod_envelope_decays_brightness(tmp)
        test_fm_bass_and_preset(tmp)
    print(
        "\n%s (%d failure%s)"
        % ("PASS" if failures == 0 else "FAIL", failures, "" if failures == 1 else "s")
    )
    return 0 if failures == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
