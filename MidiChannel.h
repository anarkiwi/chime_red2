// Copyright 2019 Josh Bailey (josh@vandervecken.com)

// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


#ifndef MIDICHANNEL_H
#define MIDICHANNEL_H

#include "Oscillator.h"
#include "OscillatorController.h"
#include "MidiNote.h"

typedef std::vector<MidiNote*> MidiNoteDeque;

class PitchBender {
 public:
  PitchBender() {
    Reset();
  };
  void Reset() {
    _bend = 0;
    _pitchBendScale = 0;
    _pitchBendDiff = 0;
    SetMaxPitch(0);
    SetPitchBendRange(midiPitchBendRange);
  };
  void SetMaxPitch(uint8_t maxPitch) {
    _maxPitch = maxPitch;
  }
  void SetPitchBendRange(uint8_t newPitchBendRange) {
    _pitchBendRange = newPitchBendRange;
  }
  bool SetBend(int16_t newBend) {
    if (newBend == _bend) {
      return false;
    }
    _bend = newBend;
    if (_bend == 0) {
      _pitchBendScale = 0;
      _pitchBendDiff = 0;
    } else {
      _pitchBendScale = midiPbProp[abs(_bend)];
      if (_bend > 0) {
        _pitchBendDiff = _pitchBendRange;
      } else {
        _pitchBendDiff = -_pitchBendRange;
      }
    }
    return true;
  }
  cr_hzinv_t BendHz(MidiNote *midiNote, cr_hzinv_t hzInv) {
    if (_pitchBendScale > 0) {
      int16_t windowPitch = midiNote->pitch + _pitchBendDiff;
      if (windowPitch < 0) {
        windowPitch = 0;
      } else if (windowPitch > _maxPitch) {
        windowPitch = _maxPitch;
      }
      // Interpolate toward the bend target in inverse-Hz space. This previously
      // used pitchToHz (forward Hz), but the bend value became inverse-Hz in the
      // avoid-division change, so forwardHz - invHz was dimensionally wrong and
      // sent any bent note to garbage frequencies. Inverse-Hz interpolation is
      // consistent and division-free.
      // (origin/main's "Fix pitchbend" made the same pitchToHz->pitchToHzInv
      // correction but kept cr_fp_t; this branch supersedes it with cr_hzinv_t's
      // 30-bit inverse-Hz precision.)
      cr_hzinv_t window = static_cast<cr_hzinv_t>(_pitchBendScale) * (pitchToHzInv[(uint8_t)windowPitch] - hzInv);
      hzInv += window;
    }
    return hzInv;
  }
  // Master-clock period window for pitched noise: [pMin, pMin+span] spans the
  // bend range either side of the played note (clamped like BendHz), read
  // straight from the integer pitchToPeriod[] table. Computed once per note on
  // so the per-pulse noise update is just one random pick inside the window --
  // no inverse-Hz math, no division. Higher pitch -> smaller period, so pMin is
  // at note+range and the wider period (pMax) is at note-range.
  void NoiseBounds(uint8_t pitch, cr_tick_t &pMin, cr_tick_t &span) const {
    int lo = int(pitch) - int(_pitchBendRange);
    if (lo < 0) {
      lo = 0;
    }
    int hi = int(pitch) + int(_pitchBendRange);
    if (hi > _maxPitch) {
      hi = _maxPitch;
    }
    pMin = pitchToPeriod[hi];
    span = pitchToPeriod[lo] - pMin;
  }
 private:
  int16_t _bend;
  uint8_t _pitchBendRange;
  cr_fp_t _pitchBendScale;
  int8_t _pitchBendDiff;
  uint8_t _maxPitch;
};

class MidiChannel {
  public:
    MidiChannel();
    void Reset();
    void ResetCC();
    void IdleAllNotes();
    void SetMaxPitch(uint8_t maxPitch);
    MidiNote *LookupNote(uint8_t note);
    void SetBend(int newBend, OscillatorController *oc);
    cr_hzinv_t BendHz(MidiNote *midiNote, cr_fp_t detuneCoeff);
    void NoteOn(uint8_t note, uint8_t velocity, MidiNote *midiNote, OscillatorController *oc);
    void ReleaseNote(uint8_t note);
    bool ResetNote(uint8_t note);
    MidiNote *NotesExpire(OscillatorController *oc);
    void HandleControl();
    void RetuneNotes(OscillatorController *oc);
    void SetPitchBendRange(uint8_t semitones);
    uint8_t attack;
    uint8_t decay;
    uint8_t sustain;
    uint8_t release;
    uint8_t detune;
    uint8_t detune2;
    uint8_t detuneAbs;
    uint8_t detune2Abs;
    uint8_t tremoloRange;
    uint8_t coarseModulation;
    uint8_t volume;
    bool lfoRestart;
    // Structural (set once for the percussion channel), not a resettable CC:
    // when true, note on stamps each voice's oscillator with a noise period
    // window so Modulate() drives it as pitched noise instead of vibrato.
    bool noiseModulated;
   private:
    void _AddOscillatorToNote(cr_hzinv_t hz, MidiNote *midiNote, OscillatorController *oc, int periodOffset);
    MidiNoteDeque _midiNotes;
    std::stack<MidiNote*> _idleNotes;
    MidiNote *_noteMap[maxMidiPitch+1];
    PitchBender _pitchBender = PitchBender();
    uint8_t _maxPitch;
};

#endif
