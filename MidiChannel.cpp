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
  attack = 0;
  decay = 0;
  sustain = maxMidiVal;
  release = 0;
  tremoloRange = 0;
  coarseModulation = 0;
  lfoRestart = true;
  detune = DEFAULT_DETUNE;
  detune2 = DEFAULT_DETUNE;
  detuneAbs = DEFAULT_DETUNE;
  detune2Abs = DEFAULT_DETUNE;
  _maxPitch = 0;
  SetBend(0, 0);
  SetPitchBendRange(midiPitchBendRange);
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

void MidiChannel::SetMaxPitch(uint8_t maxPitch) {
  if (maxPitch != _maxPitch) {
    _maxPitch = maxPitch;
    _pitchBender.SetMaxPitch(maxPitch);
  }
}

inline MidiNote *MidiChannel::LookupNote(uint8_t note) {
  if (note > maxMidiPitch) {
    return NULL;
  }
  return _noteMap[note];
}

void MidiChannel::RetuneNotes(OscillatorController *oc) {
  int periodOffset = int(detuneAbs) - int(DEFAULT_DETUNE);
  for (MidiNoteDeque::const_iterator i = _midiNotes.begin(); i != _midiNotes.end(); ++i) {
    MidiNote *midiNote = *i;
    cr_fp_t hz = BendHz(midiNote, midiTuneCents[detune]);
    for (OscillatorDeque::const_iterator o = midiNote->oscillators.begin(); o != midiNote->oscillators.end(); ++o) {
      Oscillator *oscillator = *o;
      if (o == midiNote->oscillators.begin()) {
        // TODO: add another oscillator if detune2 changed from default after note scheduled.
        oc->SetFreqLazy(oscillator, hz, midiNote->velocityScale, periodOffset);
      } else {
        int periodOffset = int(detune2Abs) - int(DEFAULT_DETUNE);
        cr_fp_t hz2 = BendHz(midiNote, midiTuneCents[detune2]);
        oc->SetFreqLazy(oscillator, hz2, midiNote->velocityScale, periodOffset);
      }
    }
  }
}

void MidiChannel::SetBend(int newBend, OscillatorController *oc) {
  if (!_pitchBender.SetBend(newBend)) {
    return;
  }
  RetuneNotes(oc);
}

cr_fp_t MidiChannel::BendHz(MidiNote *midiNote, cr_fp_t detuneCoeff) {
  cr_fp_t hz = pitchToHz[midiNote->pitch] * detuneCoeff;
  return _pitchBender.BendHz(midiNote, hz);
}

void MidiChannel::_AddOscillatorToNote(cr_fp_t hz, MidiNote *midiNote, OscillatorController *oc, int periodOffset) {
  Oscillator *oscillator;
  oscillator = oc->GetFreeOscillator();
  oc->SetFreq(oscillator, hz, midiNote->velocityScale, periodOffset);
  midiNote->oscillators.push_back(oscillator);
}

void MidiChannel::NoteOn(uint8_t note, uint8_t velocity, MidiNote *midiNote, OscillatorController *oc) {
  midiNote->pitch = note;
  midiNote->envelope.Reset(attack, decay, sustain, release);
  midiNote->velocityScale = midiValMap[velocity];
  _AddOscillatorToNote(BendHz(midiNote, midiTuneCents[detune]), midiNote, oc, int(detuneAbs) - int(DEFAULT_DETUNE));
  if (detune2 != DEFAULT_DETUNE || detune2Abs != DEFAULT_DETUNE) {
    _AddOscillatorToNote(BendHz(midiNote, midiTuneCents[detune2]), midiNote, oc, int(detune2Abs) - int(DEFAULT_DETUNE));
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

void MidiChannel::SetPitchBendRange(uint8_t semitones) {
  _pitchBender.SetPitchBendRange(semitones);
}
