// Copyright 2019 Josh Bailey (josh@vandervecken.com)

// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


#include "MidiChannel.h"

MidiChannel::MidiChannel() {
  bzero(&_noteMap, sizeof(_noteMap));
  Reset();
}

void MidiChannel::ResetCC() {
  _pitchBendRange = 12;
  SetBend(0, NULL, 0);
  attack = 0;
  decay = 0;
  sustain = maxMidiVal;
  release = 0;
  pulserCount = 1;
  pulserSpread = 0;
  tremoloRange = 0;
  coarseModulation = 0;
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

void MidiChannel::SetBend(int newBend, uint8_t maxPitch, OscillatorController *oc) {
  if (bend == newBend) {
    return;
  }
  bend = newBend;
  _pitchBendScale = cr_fp_t(bend) / maxMidiPitchBend;
  _pitchBendDiff = _pitchBendRange;
  if (bend < 0) {
    _pitchBendDiff = -_pitchBendDiff;
    _pitchBendScale = -_pitchBendScale;
  }
  for (MidiNoteDeque::const_iterator i = _midiNotes.begin(); i != _midiNotes.end(); ++i) {
    MidiNote *midiNote = *i;
    midiNote->SetFreqLazy(BendHz(midiNote, maxPitch), pitchToHz[maxPitch], oc);
  }
}

cr_fp_t MidiChannel::BendHz(MidiNote *midiNote, uint8_t maxPitch) {
  cr_fp_t hz = pitchToHz[midiNote->pitch];
  if (bend) {
    int8_t windowPitch = midiNote->pitch;
    if (_pitchBendDiff < 0) {
      windowPitch += _pitchBendDiff;
      if (windowPitch < 0) {
        windowPitch = 0;
      }
    } else {
      if (windowPitch > (maxPitch - _pitchBendDiff)) {
        windowPitch = maxPitch;
      } else {
        windowPitch += _pitchBendDiff;
      }
    }
    cr_fp_t window = _pitchBendScale * (pitchToHz[windowPitch] - hz);
    return hz + window;
  } else {
    return hz;
  }
}

void MidiChannel::NoteOn(uint8_t note, uint8_t velocity, uint8_t maxPitch, MidiNote *midiNote, OscillatorController *oc) {
  midiNote->pitch = note;
  midiNote->envelope.Reset(attack, decay, sustain, release);
  midiNote->velocityScale = midiValMap[velocity];
  cr_fp_t fundamentalHz = BendHz(midiNote, maxPitch);
  cr_fp_t maxHz = pitchToHz[maxPitch];

  if (pulserCount == 2 && pulserSpread > 0) {
    Oscillator *oscillator;
    oscillator = oc->GetFreeOscillator();
    cr_fp_t hzWindow = fundamentalHz / 20;
    oc->SetFreq(oscillator, fundamentalHz + (hzWindow * midiValMap[pulserSpread]), maxHz, midiNote->velocityScale);
    midiNote->oscillators.push_back(oscillator);
    oscillator = oc->GetFreeOscillator();
    oc->SetFreq(oscillator, fundamentalHz - ((hzWindow / 2) * midiValMap[pulserSpread]), maxHz, midiNote->velocityScale);
    midiNote->oscillators.push_back(oscillator);
  } else {
    Oscillator *oscillator = oc->GetFreeOscillator();
    oc->SetFreq(oscillator, fundamentalHz, maxHz, midiNote->velocityScale);
    midiNote->oscillators.push_back(oscillator);
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
