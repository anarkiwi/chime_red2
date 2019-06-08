// Copyright 2019 Josh Bailey (josh@vandervecken.com)

// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


#include "MidiChannel.h"

#define DEFAULT_DETUNE  64

MidiChannel::MidiChannel() {
  bzero(&_noteMap, sizeof(_noteMap));
  Reset();
}

void MidiChannel::ResetCC() {
  bend = 0;
  _pitchBendRange = 12;
  attack = 0;
  decay = 0;
  sustain = maxMidiVal;
  release = 0;
  tremoloRange = 0;
  coarseModulation = 0;
  lfoRestart = true;
  detune = DEFAULT_DETUNE;
  detune2 = DEFAULT_DETUNE;
  SetBend(0, NULL, 0);
}

void MidiChannel::Reset() {
  _midiNotes.clear();
  while (!_idleNotes.empty()) {
    _idleNotes.pop();
  }
  ResetCC();
}

void MidiChannel::IdleAllNotes() {
  for (MidiNoteDeque::const_iterator i = _midiNotes.begin(); i != _midiNotes.end(); ++i) {
    MidiNote *midiNote = *i;
    midiNote->envelope.Idle();
  }
  HandleControl();
}

inline MidiNote *MidiChannel::LookupNote(uint8_t note) {
  return _noteMap[note];
}

void MidiChannel::RetuneNotes(uint8_t maxPitch, OscillatorController *oc) {
  cr_fp_t maxHz = pitchToHz[maxPitch];
  for (MidiNoteDeque::const_iterator i = _midiNotes.begin(); i != _midiNotes.end(); ++i) {
    MidiNote *midiNote = *i;
    cr_fp_t hz = BendHz(midiNote, maxPitch, midiTuneCents[detune]);
    cr_fp_t hz2 = BendHz(midiNote, maxPitch, midiTuneCents[detune2]);
    // TODO: add another oscillator if detune2 changed from default after note scheduled.
    midiNote->SetFreqLazy(hz, hz2, maxHz, oc);
  }
}

void MidiChannel::SetBend(int newBend, uint8_t maxPitch, OscillatorController *oc) {
  if (bend == newBend) {
    return;
  }
  bend = newBend;
  if (bend == 0) {
    _pitchBendScale = 0;
    _pitchBendDiff = 0;
  } else {
    _pitchBendScale = cr_fp_t(abs(bend)) / maxMidiPitchBend;
    if (bend > 0) {
      _pitchBendDiff = _pitchBendRange;
    } else {
      _pitchBendDiff = -_pitchBendRange;
    }
  }
  RetuneNotes(maxPitch, oc);
}

cr_fp_t MidiChannel::BendHz(MidiNote *midiNote, uint8_t maxPitch, cr_fp_t detuneCoeff) {
  cr_fp_t hz = pitchToHz[midiNote->pitch] * detuneCoeff;
  if (_pitchBendScale > 0) {
    int16_t windowPitch = midiNote->pitch + _pitchBendDiff;
    if (windowPitch < 0) {
      windowPitch = 0;
    } else if (windowPitch > maxPitch) {
      windowPitch = maxPitch;
    }
    cr_fp_t window = _pitchBendScale * (pitchToHz[(uint8_t)windowPitch] - hz);
    hz += window;
  }
  return hz;
}

void MidiChannel::_AddOscillatorToNote(cr_fp_t hz, cr_fp_t maxHz, MidiNote *midiNote, OscillatorController *oc) {
  Oscillator *oscillator;
  oscillator = oc->GetFreeOscillator();
  oc->SetFreq(oscillator, hz, maxHz, midiNote->velocityScale);
  midiNote->oscillators.push_back(oscillator);
}

void MidiChannel::NoteOn(uint8_t note, uint8_t velocity, uint8_t maxPitch, MidiNote *midiNote, OscillatorController *oc) {
  midiNote->pitch = note;
  midiNote->envelope.Reset(attack, decay, sustain, release);
  midiNote->velocityScale = midiValMap[velocity];
  cr_fp_t maxHz = pitchToHz[maxPitch];
  _AddOscillatorToNote(BendHz(midiNote, maxPitch, midiTuneCents[detune]), maxHz, midiNote, oc);
  if (detune2 != DEFAULT_DETUNE) {
    _AddOscillatorToNote(BendHz(midiNote, maxPitch, midiTuneCents[detune2]), maxHz, midiNote, oc);
  }
  for (OscillatorDeque::const_iterator o = midiNote->oscillators.begin(); o != midiNote->oscillators.end(); ++o) {
    Oscillator *oscillator = *o;
    oscillator->audible = true;
    oscillator->envelope = &(midiNote->envelope);
  }
  _midiNotes.push_back(midiNote);
  _noteMap[note] = midiNote;
}

void MidiChannel::ReleaseNote(uint8_t note) {
  MidiNote *midiNote = LookupNote(note);
  if (midiNote) {
    midiNote->envelope.Release();
  }
}

bool MidiChannel::ResetNote(uint8_t note) {
  MidiNote *midiNote = LookupNote(note);
  if (midiNote) {
    midiNote->pitch = note;
    midiNote->envelope.Reset(attack, decay, sustain, release);
    return true;
  }
  return false;
}

MidiNote *MidiChannel::NotesExpire(OscillatorController *oc) {
  while (!_idleNotes.empty()) {
    MidiNote *midiNote = _idleNotes.top();
    _idleNotes.pop();
    _noteMap[midiNote->pitch] = NULL;
    midiNote->ResetOscillators(oc);
    return midiNote;
  }
  return NULL;
}

void MidiChannel::HandleControl() {
  uint8_t noteCount = _midiNotes.size();
  for (uint8_t n = 0; n < noteCount; ++n) {
    MidiNote *midiNote = _midiNotes.back();
    _midiNotes.pop_back();
    midiNote->HandleControl();
    if (midiNote->envelope.IsIdle()) {
      _idleNotes.push(midiNote);
    } else {
      _midiNotes.push_front(midiNote);
    }
  }
}
