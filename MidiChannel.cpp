// Copyright 2019 Josh Bailey (josh@vandervecken.com)

// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


#include "MidiChannel.h"

#define DEFAULT_DETUNE  64


MidiChannel::MidiChannel() {
  bzero(&_noteMap, sizeof(_noteMap));
  noiseModulated = false;
  Reset();
}

void MidiChannel::ResetCC() {
  attack = 0;
  decay = 0;
  sustain = maxMidiVal;
  release = 0;
  tremoloRange = 0;
  coarseModulation = 0;
  fmRatio = 0;
  fmRatioFine = 0;
  fmIndex = 0;
  modAttack = 0;
  modDecay = 0;
  modSustain = maxMidiVal;  // default mod envelope is null => constant index
  modRelease = 0;
  lfoRestart = true;
  detune = DEFAULT_DETUNE;
  detune2 = DEFAULT_DETUNE;
  detuneAbs = DEFAULT_DETUNE;
  detune2Abs = DEFAULT_DETUNE;
  _maxPitch = 0;
  volume = 127;
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
    cr_hzinv_t hz = BendHz(midiNote, midiTuneCents[detune]);
    for (OscillatorDeque::const_iterator o = midiNote->oscillators.begin(); o != midiNote->oscillators.end(); ++o) {
      Oscillator *oscillator = *o;
      if (o == midiNote->oscillators.begin()) {
        // TODO: add another oscillator if detune2 changed from default after note scheduled.
        oc->SetFreqLazy(oscillator, hz, midiNote->pitch, midiNote->velocityScale, periodOffset);
      } else {
        int periodOffset2 = int(detune2Abs) - int(DEFAULT_DETUNE);
        cr_hzinv_t hz2 = BendHz(midiNote, midiTuneCents[detune2]);
        oc->SetFreqLazy(oscillator, hz2, midiNote->pitch, midiNote->velocityScale, periodOffset2);
      }
    }
    _StampFM(midiNote);
  }
}

void MidiChannel::SetBend(int newBend, OscillatorController *oc) {
  if (!_pitchBender.SetBend(newBend)) {
    return;
  }
  RetuneNotes(oc);
}

cr_hzinv_t MidiChannel::BendHz(MidiNote *midiNote, cr_fp_t detuneCoeff) {
  cr_hzinv_t hz = pitchToHzInv[midiNote->pitch] * static_cast<cr_hzinv_t>(detuneCoeff);
  return _pitchBender.BendHz(midiNote, hz);
}

void MidiChannel::_AddOscillatorToNote(cr_hzinv_t hz, MidiNote *midiNote, OscillatorController *oc, int periodOffset) {
  Oscillator *oscillator;
  oscillator = oc->GetFreeOscillator();
  oc->SetFreq(oscillator, hz, midiNote->pitch, midiNote->velocityScale, periodOffset);
  midiNote->oscillators.push_back(oscillator);
}

void MidiChannel::_StampFM(MidiNote *midiNote) {
  // Translate the channel's FM CCs to per-cycle modulation state and stamp it on
  // every oscillator of the note, pointing each at this note's modulation-index
  // envelope. Control-rate (note on / retune); idempotent so re-stamping a held
  // note on a CC or pitch-bend change is safe.
  const cr_fp_t depth = _FmDepth();
  const uint16_t step = _FmPhaseStep();
  for (OscillatorDeque::const_iterator o = midiNote->oscillators.begin(); o != midiNote->oscillators.end(); ++o) {
    Oscillator *oscillator = *o;
    oscillator->modEnvelope = &(midiNote->modEnvelope);
    oscillator->SetFM(depth, step);
  }
}

void MidiChannel::_InitDrum(MidiNote *midiNote, const DrumPreset *preset) {
  // Load the preset's schedule onto the note and pre-resolve the pitch-glide
  // endpoints and noise window to master-clock ticks (integer pitchToPeriod[]
  // reads, like PitchBender::NoiseBounds) so AdvanceDrum stays division-free.
  midiNote->drumPreset = preset;
  midiNote->ageMs = 0;
  midiNote->envelope.Reset(preset->attack, preset->decay, preset->sustain, preset->release);
  midiNote->drumStartPeriod = pitchToPeriod[preset->startPitch];
  midiNote->drumEndPeriod = pitchToPeriod[preset->endPitch];
  uint8_t hi = preset->noisePitch + preset->noiseSpanSemis;
  if (hi > maxMidiPitch) {
    hi = maxMidiPitch;
  }
  int lo = int(preset->noisePitch) - int(preset->noiseSpanSemis);
  if (lo < 0) {
    lo = 0;
  }
  // Higher pitch -> smaller period, so pMin is at the high edge; span is positive.
  midiNote->drumNoisePMin = pitchToPeriod[hi];
  midiNote->drumNoiseSpan = pitchToPeriod[(uint8_t)lo] - pitchToPeriod[hi];
}

void MidiChannel::_AddDrumOscillator(uint8_t drumPitch, MidiNote *midiNote, OscillatorController *oc) {
  Oscillator *oscillator = oc->GetFreeOscillator();
  if (oscillator == NULL) {
    return;  // pool exhausted: drop the hit gracefully (no NULL deref).
  }
  oc->SetFreq(oscillator, pitchToHzInv[drumPitch], drumPitch, midiNote->velocityScale, 0);
  midiNote->oscillators.push_back(oscillator);
}

void MidiChannel::_DrumNoteOn(uint8_t note, uint8_t velocity, MidiNote *midiNote, OscillatorController *oc) {
  const DrumPreset *preset = LookupDrumPreset(note);
  // preset is non-NULL in practice: CRMidi::handleNoteOn silences unmapped
  // channel-10 notes before a voice is ever allocated. Guard anyway.
  if (preset == NULL) {
    return;
  }
  midiNote->pitch = note;
  midiNote->velocityScale = midiValMap[velocity];
  _InitDrum(midiNote, preset);
  _AddDrumOscillator(preset->startPitch, midiNote, oc);
  const bool noiseOn = preset->noiseMs > 0;  // noise drums start their burst now.
  for (OscillatorDeque::const_iterator o = midiNote->oscillators.begin(); o != midiNote->oscillators.end(); ++o) {
    Oscillator *oscillator = *o;
    oscillator->audible = true;
    oscillator->envelope = &(midiNote->envelope);
    oscillator->SetNoiseRange(noiseOn ? midiNote->drumNoisePMin : 0,
                              noiseOn ? midiNote->drumNoiseSpan : 0);
  }
  _midiNotes.push_back(midiNote);
  _noteMap[note] = midiNote;
}

void MidiChannel::_RestartDrum(MidiNote *midiNote, const DrumPreset *preset) {
  // Re-trigger a still-ringing drum on its existing oscillator(s): restart the
  // schedule (envelope + cached endpoints + ageMs) and re-seed the start period.
  _InitDrum(midiNote, preset);
  const bool noiseOn = preset->noiseMs > 0;
  for (OscillatorDeque::const_iterator o = midiNote->oscillators.begin(); o != midiNote->oscillators.end(); ++o) {
    Oscillator *oscillator = *o;
    oscillator->audible = true;  // it may have decayed to silence but not yet freed.
    oscillator->envelope = &(midiNote->envelope);
    oscillator->SetClockPeriodLazy(midiNote->drumStartPeriod);
    oscillator->SetNoiseRange(noiseOn ? midiNote->drumNoisePMin : 0,
                              noiseOn ? midiNote->drumNoiseSpan : 0);
  }
}

void MidiChannel::NoteOn(uint8_t note, uint8_t velocity, MidiNote *midiNote, OscillatorController *oc) {
  if (noiseModulated) {
    // Channel 10: a built-in drum schedule instead of a tonal voice.
    _DrumNoteOn(note, velocity, midiNote, oc);
    return;
  }
  midiNote->pitch = note;
  midiNote->envelope.Reset(attack, decay, sustain, release);
  midiNote->modEnvelope.Reset(modAttack, modDecay, modSustain, modRelease);
  midiNote->velocityScale = midiValMap[velocity];
  _AddOscillatorToNote(BendHz(midiNote, midiTuneCents[detune]), midiNote, oc, int(detuneAbs) - int(DEFAULT_DETUNE));
  if (detune2 != DEFAULT_DETUNE || detune2Abs != DEFAULT_DETUNE) {
    _AddOscillatorToNote(BendHz(midiNote, midiTuneCents[detune2]), midiNote, oc, int(detune2Abs) - int(DEFAULT_DETUNE));
  }
  // Tonal voice: clear any stale noise window left on an oscillator recycled
  // from the free pool (a previous channel-10 drum hit may have set one). The
  // percussion channel never reaches here -- it returns via _DrumNoteOn above.
  for (OscillatorDeque::const_iterator o = midiNote->oscillators.begin(); o != midiNote->oscillators.end(); ++o) {
    Oscillator *oscillator = *o;
    oscillator->audible = true;
    oscillator->envelope = &(midiNote->envelope);
    oscillator->SetNoiseRange(0, 0);
  }
  _StampFM(midiNote);
  _midiNotes.push_back(midiNote);
  _noteMap[note] = midiNote;
}

void MidiChannel::ReleaseNote(uint8_t note) {
  MidiNote *midiNote = LookupNote(note);
  if (midiNote) {
    midiNote->envelope.Release();
    midiNote->modEnvelope.Release();
  }
}

bool MidiChannel::ResetNote(uint8_t note) {
  MidiNote *midiNote = LookupNote(note);
  if (midiNote) {
    if (noiseModulated) {
      // Re-hit a still-ringing channel-10 drum: restart its schedule in place.
      const DrumPreset *preset = LookupDrumPreset(note);
      if (preset) {
        _RestartDrum(midiNote, preset);
      }
      return true;
    }
    midiNote->pitch = note;
    midiNote->envelope.Reset(attack, decay, sustain, release);
    midiNote->modEnvelope.Reset(modAttack, modDecay, modSustain, modRelease);
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
  for (MidiNoteDeque::iterator i = _midiNotes.begin(); i != _midiNotes.end(); ++i) {
    MidiNote *midiNote = *i;
    midiNote->HandleControl();
    if (midiNote->envelope.IsIdle()) {
      _idleNotes.push(midiNote);
      _midiNotes.erase(i--);
      continue;
    }
  }
}

void MidiChannel::SetPitchBendRange(uint8_t semitones) {
  _pitchBender.SetPitchBendRange(semitones);
}
