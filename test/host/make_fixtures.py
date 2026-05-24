#!/usr/bin/env python3
"""Generate the FM-bell MIDI fixtures under test/host/fixtures/.

These are tracked binary SMFs the renderer and test_fm.py consume, so nobody
hand-edits SMF bytes -- change a parameter here and regenerate:

    python3 test/host/make_fixtures.py            # rewrite fixtures/
    python3 test/host/make_fixtures.py --check     # CI drift guard (no writes)

Output is deterministic (events built in a fixed order), so regenerating produces
no git diff unless a parameter actually changed. The --check mode regenerates to a
temp dir and diffs against the tracked fixtures, exiting non-zero on any mismatch;
coverage.sh runs it so a stale fixture fails the build.

Stdlib-only, shares the SMF writer with the rest of test/host (smf_write).
"""

import os
import sys
import tempfile

from smf_write import PPQ, cc, note_off, note_on, program_change, tempo, write_smf

TEMPO = 500000  # us/qn -> 120 bpm, so a quarter (PPQ ticks) is 0.5 s
FIXTURES_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fixtures")

# CC map (mirrors CRMidi::handleControlChange).
CC_AMP_ATTACK, CC_AMP_DECAY, CC_AMP_SUSTAIN, CC_AMP_RELEASE = 73, 75, 24, 72
CC_FM_RATIO_FINE, CC_FM_RATIO, CC_FM_INDEX = 19, 20, 21
CC_MOD_ATTACK, CC_MOD_DECAY, CC_MOD_SUSTAIN, CC_MOD_RELEASE = 102, 103, 104, 105

# Program-change presets (mirrors CRMidi::handleProgramChange).
PC_BELL, PC_BASS_PLUCK, PC_BASS_GROWL, PC_BASS_FAT = 14, 38, 39, 40


def beats(count):
    return int(round(count * PPQ))


def fm_setup(
    ch=1,
    ratio10=14,
    ratio_fine=0,
    index=100,
    amp_attack=0,
    amp_decay=124,
    amp_sustain=0,
    amp_release=40,
    mod_attack=0,
    mod_decay=110,
    mod_sustain=0,
    mod_release=0,
):
    """Tick-0 CC block configuring a channel for FM (bell defaults; override the
    envelopes/ratio for bass). CC19 (fine ratio) is emitted only when non-zero, so
    the bell fixtures' bytes are unaffected by adding the fine control."""
    events = [
        (0, tempo(TEMPO)),
        (0, cc(ch, CC_AMP_ATTACK, amp_attack)),
        (0, cc(ch, CC_AMP_DECAY, amp_decay)),
        (0, cc(ch, CC_AMP_SUSTAIN, amp_sustain)),
        (0, cc(ch, CC_AMP_RELEASE, amp_release)),
    ]
    if ratio_fine:
        events.append((0, cc(ch, CC_FM_RATIO_FINE, ratio_fine)))
    events += [
        (0, cc(ch, CC_FM_RATIO, ratio10)),
        (0, cc(ch, CC_FM_INDEX, index)),
        (0, cc(ch, CC_MOD_ATTACK, mod_attack)),
        (0, cc(ch, CC_MOD_DECAY, mod_decay)),
        (0, cc(ch, CC_MOD_SUSTAIN, mod_sustain)),
        (0, cc(ch, CC_MOD_RELEASE, mod_release)),
    ]
    return events


def preset(number, ch=1):
    """Tick-0 program-change selecting a built-in preset (CRMidi handles it)."""
    return [(0, tempo(TEMPO)), (0, program_change(ch, number))]


def strike(setup, note=72, hold_beats=16, vel=115):
    events = list(setup)
    events.append((0, note_on(1, note, vel)))
    events.append((beats(hold_beats), note_off(1, note)))
    return events


def melody(setup, notes, gap_beats=2.0, vel=112):
    # SMF event times are *deltas* (cumulative), so track the running absolute
    # tick and emit (next_abs - last). Each strike is released 8 ticks before the
    # next so the bell rings into it. (A plain beats(k*gap) would be an absolute
    # time misused as a delta -- gaps would balloon and the file run far long.)
    events = list(setup)  # setup events are all at tick 0
    last = 0
    for k, note in enumerate(notes):
        on = beats(k * gap_beats)
        events.append((on - last, note_on(1, note, vel)))
        off = on + beats(gap_beats) - 8
        events.append((off - on, note_off(1, note)))
        last = off
    return events


def bass(setup):
    return strike(setup, note=36, hold_beats=3, vel=118)


def fixtures():
    """Name -> event list. Listenable demos + controls for test_fm.py."""
    bell = fm_setup()
    return {
        # Bell demos.
        "fm_bell_c5": strike(bell, note=72),
        "fm_bell_c4": strike(bell, note=60),
        "fm_bell_melody": melody(
            fm_setup(amp_decay=96, mod_decay=96, amp_release=72),
            notes=(60, 55, 52, 55, 60, 64, 60),
        ),
        # Bass demos (C2). Fat = near-unity ratio via the fine adjust (needs CC19);
        # pluck = bright FM attack, clean body; growl = sustained modulation.
        "fm_bass_fat": bass(
            fm_setup(
                ratio10=10,
                ratio_fine=8,
                index=70,
                amp_decay=40,
                amp_sustain=95,
                mod_decay=30,
                mod_sustain=40,
                mod_release=30,
            )
        ),
        "fm_bass_pluck": bass(
            fm_setup(
                ratio10=12,
                index=115,
                amp_decay=55,
                amp_sustain=60,
                amp_release=30,
                mod_decay=30,
                mod_sustain=0,
                mod_release=20,
            )
        ),
        "fm_bass_growl": bass(
            fm_setup(
                ratio10=12,
                index=110,
                amp_decay=60,
                amp_sustain=85,
                mod_decay=90,
                mod_sustain=70,
                mod_release=30,
            )
        ),
        # Program-change preset path (PC 40 = fat bass): exercises handleProgramChange.
        "fm_bass_preset": bass(preset(PC_BASS_FAT)),
        # Test controls.
        "fm_off_c5": strike(fm_setup(ratio10=0, index=0), note=72, hold_beats=8),
        "fm_decay_c5": strike(fm_setup(mod_decay=70), note=72, hold_beats=8),
        "fm_constant_c5": strike(
            fm_setup(mod_decay=0, mod_sustain=127), note=72, hold_beats=8
        ),
    }


def write_all(dest):
    os.makedirs(dest, exist_ok=True)
    paths = {}
    for name, events in fixtures().items():
        path = os.path.join(dest, name + ".mid")
        write_smf(path, events)
        paths[name] = path
    return paths


def check():
    """Regenerate to a temp dir and diff against the tracked fixtures."""
    drift = []
    with tempfile.TemporaryDirectory() as tmp:
        fresh = write_all(tmp)
        for name, fresh_path in fresh.items():
            tracked = os.path.join(FIXTURES_DIR, name + ".mid")
            with open(fresh_path, "rb") as f:
                want = f.read()
            have = None
            if os.path.exists(tracked):
                with open(tracked, "rb") as f:
                    have = f.read()
            if have != want:
                drift.append(name)
    if drift:
        print("FAIL: fixtures out of date (run make_fixtures.py): " + ", ".join(drift))
        return 1
    print("fixtures up to date (%d)" % len(fresh))
    return 0


def main():
    if "--check" in sys.argv[1:]:
        return check()
    paths = write_all(FIXTURES_DIR)
    for name in sorted(paths):
        print("wrote", os.path.relpath(paths[name]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
