"""Shared Standard MIDI File writer for the host fixture/test generators.

Stdlib-only (struct), matching the rest of test/host. Used by make_fixtures.py
(fixture generation), test_fm.py and test_render.py so SMF authoring lives in one
place -- the same "one copy to keep in sync" approach as IsrDriver.h on the C++
side. PPQ is shared so fixture/test timings agree.
"""

import struct

PPQ = 480


def vlq(n):
    """Encode an integer as a MIDI variable-length quantity."""
    out = bytearray([n & 0x7F])
    n >>= 7
    while n:
        out.append((n & 0x7F) | 0x80)
        n >>= 7
    return bytes(reversed(out))


def mtrk(events):
    """Build an MTrk chunk from (delta_ticks, event_bytes) pairs."""
    data = bytearray()
    for delta, ev in events:
        data += vlq(delta) + ev
    data += vlq(0) + b"\xff\x2f\x00"  # end of track
    return b"MTrk" + struct.pack(">I", len(data)) + bytes(data)


def write_smf(path, events, fmt=0, ntrks=1, tracks=None):
    """Write a complete SMF. Pass `events` for the common single-track case, or
    pre-built `tracks` (with fmt/ntrks) for multi-track files."""
    if tracks is None:
        tracks = [mtrk(events)]
    body = b"".join(tracks)
    mid = b"MThd" + struct.pack(">IHHH", 6, fmt, ntrks, PPQ) + body
    with open(path, "wb") as f:
        f.write(mid)


def tempo(usec_per_qn):
    """A set-tempo meta event (microseconds per quarter note)."""
    return b"\xff\x51\x03" + struct.pack(">I", usec_per_qn)[1:]


def cc(channel, number, value):
    """A control-change event on a 1-based channel."""
    return bytes([0xB0 | (channel - 1), number & 0x7F, value & 0x7F])


def note_on(channel, note, velocity):
    """A note-on event on a 1-based channel."""
    return bytes([0x90 | (channel - 1), note & 0x7F, velocity & 0x7F])


def note_off(channel, note):
    """A note-off event on a 1-based channel."""
    return bytes([0x80 | (channel - 1), note & 0x7F, 0])


def program_change(channel, number):
    """A program-change event on a 1-based channel (selects a preset)."""
    return bytes([0xC0 | (channel - 1), number & 0x7F])
