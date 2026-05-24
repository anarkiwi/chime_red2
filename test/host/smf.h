// Minimal Standard MIDI File (SMF / .mid) reader for the host simulator
// (test/host/cr_render.cpp). Header-only, stdlib-only -- no external MIDI
// dependency -- so it stays in the same "compile the real synth with stock g++"
// spirit as the rest of test/host, and links straight into the renderer.
//
// Scope: formats 0 and 1 (multi-track merged), PPQN and SMPTE time division,
// running status, tempo meta events (for PPQN timing). It decodes only the
// channel-voice messages the synth acts on -- note on/off, control change,
// pitch bend, program change -- and yields them flattened and sorted by
// absolute time in seconds. Everything else (aftertouch, sysex, other metas) is
// parsed for correct stream positioning but dropped.
#ifndef CR_HOST_SMF_H
#define CR_HOST_SMF_H

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace cr_smf {

struct Event {
  double seconds = 0.0;
  enum Kind { kNoteOn, kNoteOff, kCC, kPitchBend, kProgram } kind = kNoteOn;
  uint8_t channel = 1;  // MIDI channel 1..16 (the synth's convention)
  uint8_t d1 = 0;       // note / cc number / program; bend LSB folded into bend14
  uint8_t d2 = 0;       // velocity / cc value
  int bend14 = 0;       // pitch bend, centred: [-8192, 8191] (kPitchBend only)
};

struct File {
  std::vector<Event> events;  // sorted by seconds (non-decreasing)
  double endSeconds = 0.0;    // time of the last event
};

namespace detail {

// Big-endian fixed-width integer reads with bounds checks.
inline bool ReadU16(const std::vector<uint8_t> &b, size_t &p, uint16_t &out) {
  if (p + 2 > b.size()) return false;
  out = (uint16_t(b[p]) << 8) | b[p + 1];
  p += 2;
  return true;
}
inline bool ReadU32(const std::vector<uint8_t> &b, size_t &p, uint32_t &out) {
  if (p + 4 > b.size()) return false;
  out = (uint32_t(b[p]) << 24) | (uint32_t(b[p + 1]) << 16) |
        (uint32_t(b[p + 2]) << 8) | b[p + 3];
  p += 4;
  return true;
}
// Variable-length quantity (7 bits/byte, MSB = continuation).
inline bool ReadVlq(const std::vector<uint8_t> &b, size_t &p, uint32_t &out) {
  out = 0;
  for (int i = 0; i < 4; ++i) {
    if (p >= b.size()) return false;
    uint8_t c = b[p++];
    out = (out << 7) | (c & 0x7f);
    if (!(c & 0x80)) return true;
  }
  return false;  // malformed: >4 continuation bytes
}

// One raw channel/tempo event at an absolute tick, before tempo->seconds mapping.
struct Raw {
  uint64_t tick;
  uint8_t status;  // full status byte (0x80..0xEF), or 0xFF for a tempo marker
  uint8_t d1, d2;
  uint32_t tempo;  // microseconds per quarter note (tempo markers only)
};

}  // namespace detail

// Parse the file at `path`. On success fills `out` and returns true; on any
// structural error returns false with a human-readable reason in `err`.
inline bool Load(const char *path, File &out, std::string &err) {
  FILE *f = std::fopen(path, "rb");
  if (!f) {
    err = "cannot open file";
    return false;
  }
  std::vector<uint8_t> b;
  {
    std::fseek(f, 0, SEEK_END);
    long n = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (n > 0) {
      b.resize(static_cast<size_t>(n));
      if (std::fread(b.data(), 1, b.size(), f) != b.size()) {
        std::fclose(f);
        err = "short read";
        return false;
      }
    }
    std::fclose(f);
  }

  using detail::ReadU16;
  using detail::ReadU32;
  using detail::ReadVlq;
  using detail::Raw;

  size_t p = 0;
  if (b.size() < 14 || b[0] != 'M' || b[1] != 'T' || b[2] != 'h' || b[3] != 'd') {
    err = "missing MThd header";
    return false;
  }
  p = 4;
  uint32_t hdrLen = 0;
  uint16_t format = 0, ntrks = 0, division = 0;
  if (!ReadU32(b, p, hdrLen) || !ReadU16(b, p, format) ||
      !ReadU16(b, p, ntrks) || !ReadU16(b, p, division)) {
    err = "truncated MThd";
    return false;
  }
  p = 4 + 4 + hdrLen;  // skip any extra header bytes per the declared length

  // SMPTE division has a constant seconds/tick; PPQN depends on the tempo map.
  const bool smpte = (division & 0x8000) != 0;
  double smpteSecPerTick = 0.0;
  uint16_t ppqn = 0;
  if (smpte) {
    const int fps = -static_cast<int8_t>(division >> 8);  // 24/25/29/30
    const int tpf = division & 0xff;
    if (fps <= 0 || tpf <= 0) {
      err = "bad SMPTE division";
      return false;
    }
    smpteSecPerTick = 1.0 / (static_cast<double>(fps) * tpf);
  } else {
    ppqn = division & 0x7fff;
    if (ppqn == 0) {
      err = "zero PPQN division";
      return false;
    }
  }

  std::vector<Raw> raw;
  for (uint16_t t = 0; t < ntrks; ++t) {
    if (p + 8 > b.size() || b[p] != 'M' || b[p + 1] != 'T' || b[p + 2] != 'r' ||
        b[p + 3] != 'k') {
      err = "missing MTrk chunk";
      return false;
    }
    p += 4;
    uint32_t trkLen = 0;
    if (!ReadU32(b, p, trkLen)) {
      err = "truncated MTrk length";
      return false;
    }
    const size_t trkEnd = p + trkLen;
    if (trkEnd > b.size()) {
      err = "MTrk overruns file";
      return false;
    }
    uint64_t tick = 0;
    uint8_t runningStatus = 0;
    while (p < trkEnd) {
      uint32_t delta = 0;
      if (!ReadVlq(b, p, delta)) {
        err = "bad delta-time";
        return false;
      }
      tick += delta;
      if (p >= trkEnd) {
        err = "event past track end";
        return false;
      }
      uint8_t status = b[p];
      if (status & 0x80) {
        ++p;
        if (status < 0xf0) runningStatus = status;  // channel voice: latches
      } else {
        status = runningStatus;  // running status: reuse previous, byte is data
        if (!(status & 0x80)) {
          err = "running status with no prior status";
          return false;
        }
      }

      if (status == 0xff) {  // meta event
        if (p >= trkEnd) { err = "truncated meta"; return false; }
        uint8_t type = b[p++];
        uint32_t len = 0;
        if (!ReadVlq(b, p, len) || p + len > trkEnd) {
          err = "bad meta length";
          return false;
        }
        if (type == 0x51 && len == 3) {  // set tempo (us per quarter note)
          uint32_t us = (uint32_t(b[p]) << 16) | (uint32_t(b[p + 1]) << 8) | b[p + 2];
          raw.push_back(Raw{tick, 0xff, 0, 0, us});
        }
        p += len;  // type 0x2F (end of track) and all others: skip the payload
      } else if (status == 0xf0 || status == 0xf7) {  // sysex / escape
        uint32_t len = 0;
        if (!ReadVlq(b, p, len) || p + len > trkEnd) {
          err = "bad sysex length";
          return false;
        }
        p += len;
      } else {
        const uint8_t hi = status & 0xf0;
        const uint8_t chan = (status & 0x0f) + 1;  // -> synth's 1..16
        const bool twoData = (hi != 0xc0 && hi != 0xd0);
        if (p >= trkEnd || (twoData && p + 1 >= trkEnd)) {
          err = "truncated channel message";
          return false;
        }
        uint8_t d1 = b[p++];
        uint8_t d2 = twoData ? b[p++] : 0;
        if (hi == 0x80 || hi == 0x90 || hi == 0xb0 || hi == 0xc0 || hi == 0xe0) {
          raw.push_back(Raw{tick, uint8_t(hi | (chan - 1)), d1, d2, 0});
        }
        // 0xA0 (poly aftertouch) and 0xD0 (channel pressure): positioned, dropped.
      }
    }
    p = trkEnd;  // tolerate trailing bytes inside the declared track length
  }

  // Stable sort by tick so same-tick events keep file/track order (tempo events
  // in the conductor track land before notes that share their tick).
  std::stable_sort(raw.begin(), raw.end(),
                   [](const Raw &a, const Raw &c) { return a.tick < c.tick; });

  // Walk in tick order, mapping ticks -> seconds through the tempo map.
  double anchorSec = 0.0;
  uint64_t anchorTick = 0;
  double usPerQuarter = 500000.0;  // 120 BPM default until the first tempo meta
  for (const Raw &r : raw) {
    double sec;
    if (smpte) {
      sec = static_cast<double>(r.tick) * smpteSecPerTick;
    } else {
      sec = anchorSec + (static_cast<double>(r.tick - anchorTick) *
                         usPerQuarter / static_cast<double>(ppqn) / 1e6);
    }
    if (r.status == 0xff) {  // tempo change: re-anchor (ignored under SMPTE)
      if (!smpte) {
        anchorSec = sec;
        anchorTick = r.tick;
        usPerQuarter = static_cast<double>(r.tempo);
      }
      continue;
    }
    Event e;
    e.seconds = sec;
    e.channel = (r.status & 0x0f) + 1;
    const uint8_t hi = r.status & 0xf0;
    if (hi == 0x90 && r.d2 > 0) {
      e.kind = Event::kNoteOn;
      e.d1 = r.d1;
      e.d2 = r.d2;
    } else if (hi == 0x90 || hi == 0x80) {  // note off, incl. note-on velocity 0
      e.kind = Event::kNoteOff;
      e.d1 = r.d1;
      e.d2 = r.d2;
    } else if (hi == 0xb0) {
      e.kind = Event::kCC;
      e.d1 = r.d1;
      e.d2 = r.d2;
    } else if (hi == 0xc0) {
      e.kind = Event::kProgram;
      e.d1 = r.d1;
    } else if (hi == 0xe0) {
      e.kind = Event::kPitchBend;
      e.bend14 = ((int(r.d2) << 7) | int(r.d1)) - 8192;  // centre at 0
    } else {
      continue;
    }
    out.events.push_back(e);
  }
  if (!out.events.empty()) {
    out.endSeconds = out.events.back().seconds;
  }
  return true;
}

}  // namespace cr_smf

#endif  // CR_HOST_SMF_H
