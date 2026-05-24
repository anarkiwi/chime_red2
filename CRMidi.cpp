// Copyright 2019 Josh Bailey (josh@vandervecken.com)

// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


#include "CRMidi.h"
#include "Oscillator.h"
#include "constants.h"

#define GET_CHAN(channel) _midiChannelMap[channel]
#define FOR_ALL_CHAN(channel_func) \
  { for (MidiChannel *midiChannel = _midiChannels; midiChannel < _midiChannels + midiChannelStorage; ++midiChannel) { channel_func; } }
#define MIDI_TO_HZ(value) (midiValMap[value] * cr_fp_t(lfoMaxHz))

bool CRMidi::setCC(uint8_t *value, uint8_t newValue) {
  if (*value != newValue) {
    *value = newValue;
    return true;
  }
  return false;
}

CRMidi::CRMidi(OscillatorController *oc, CRIO *crio) {
  _oc = oc;
  _crio = crio;
  _oc->ResetAll();
  _controlCounter = 0;
  bzero(&_oscillatorChannelMap, sizeof(_oscillatorChannelMap));
  for (uint8_t n = 0; n < oscillatorCount; ++n) {
    MidiNote *midiNote = _midiNotes + n;
    _freeMidiNotes.push(midiNote);
  }
  bzero(&_midiChannelMap, sizeof(_midiChannelMap));
  for (uint8_t c = 1; c <= maxMidiChannel; ++c) {
    _midiChannelMap[c] = _midiChannels + (c - 1);
  }
  _percussionChannel = _midiChannels + maxMidiChannel;
  _midiChannelMap[PERC_CHAN] = _percussionChannel;
  _percussionChannel->noiseModulated = true;
  ResetAll();
  updateCoeff();
}

MidiChannel *CRMidi::getOscillatorChannel(Oscillator *oscillator) {
  return _oscillatorChannelMap[oscillator->index];
}

MidiChannel *CRMidi::ChannelEnabled(byte channel) {
  if (channel == PERC_CHAN) {
    if (!_crio->percussionEnabled()) {
       return NULL;
    }
  } else if (channel > maxMidiChannel) {
    return NULL;
  }
  return GET_CHAN(channel);
}

void CRMidi::AllNotesOff(MidiChannel *midiChannel) {
  midiChannel->IdleAllNotes();
  ExpireNotes(midiChannel);
}

void CRMidi::ResetChannel(MidiChannel *midiChannel) {
  AllNotesOff(midiChannel);
  midiChannel->Reset();
}

void CRMidi::ResetAll() {
  FOR_ALL_CHAN(ResetChannel(midiChannel));
}

void CRMidi::updateCoeff() {
  _crio->updateCoeff();
  _crio->updateLcdCoeff();
  _oc->SetMaxPitch(_crio->maxPitch);
  FOR_ALL_CHAN(midiChannel->SetMaxPitch(_crio->maxPitch));
}

bool CRMidi::HandleControl() {
  bool complete = false;
  switch (_controlCounter) {
    case 0:
      FOR_ALL_CHAN(midiChannel->HandleControl());
      break;
    case 1:
      FOR_ALL_CHAN(ExpireNotes(midiChannel));
      break;
    case 2:
      if (!_crio->pollPots()) {
        complete = true;
      }
      break;
    case 3:
      updateCoeff();
      break;
    default:
      complete = true;
  }
  if (complete) {
    _controlCounter = 0;
  } else {
    ++_controlCounter;
  }
  return complete;
}

MidiChannel *CRMidi::ChannelNoteValid(byte channel, byte note) {
  if (note > _crio->maxPitch) {
    return NULL;
  }
  return ChannelEnabled(channel);
}

void CRMidi::handleNoteOn(byte channel, byte note, byte velocity) {
  if (velocity == 0) {
    // MIDI: note on with velocity 0 is a note off. Must return -- otherwise the
    // fall-through re-triggers the just-released note via ResetNote() below.
    handleNoteOff(channel, note);
    return;
  }
  MidiChannel *midiChannel = ChannelNoteValid(channel, note);
  if (midiChannel == NULL) {
    return;
  }
  if (midiChannel->ResetNote(note)) {
    return;
  }
  MidiNote *midiNote = GetFreeNote();
  if (midiNote == NULL) {
    return;
  }
  if (midiChannel->lfoRestart) {
    _oc->RestartLFOs();
  }
  midiChannel->NoteOn(note, velocity, midiNote, _oc);
  for (OscillatorDeque::const_iterator o = midiNote->oscillators.begin(); o != midiNote->oscillators.end(); ++o) {
    Oscillator *oscillator = *o;
    _oscillatorChannelMap[oscillator->index] = midiChannel;
  }
}

void CRMidi::handleNoteOff(byte channel, byte note) {
  MidiChannel *midiChannel = ChannelEnabled(channel);
  if (midiChannel == NULL) {
    return;
  }
  midiChannel->ReleaseNote(note);
}

void CRMidi::handlePitchBend(byte channel, int bend) {
  MidiChannel *midiChannel = ChannelEnabled(channel);
  if (midiChannel == NULL) {
    return;
  }
  midiChannel->SetBend(bend, _oc);
}

void CRMidi::handleControlChange(byte channel, byte number, byte value) {
  MidiChannel *midiChannel = ChannelEnabled(channel);
  if (midiChannel == NULL) {
    return;
  }
  bool requireRetune = false;
  // https://www.midi.org/midi/specifications/item/table-1-summary-of-midi-message
  // https://www.midi.org/specifications-old/item/table-3-control-change-messages-data-bytes-2
  // See AdsrEnvelope.h for envelope timers - 0 is 0ms, 127 is 4096ms.
  // See lfoMaxHz in constants.h for LFO timers - maximum is 10Hz.
  switch (number) {
    case 127: // Poly Mode On (Mono Off) (Note: These four messages also cause All Notes Off)
    case 126: // Mono Mode On (Poly Off) where M is the number of channels (Omni Off) or 0 (Omni On)
    case 125: // Omni Mode On
    case 124: // Omni Mode Off
    case 123: // All Notes Off
      AllNotesOff(midiChannel);
      break;
    case 121: // Reset All Controllers
      midiChannel->ResetCC();
      requireRetune = true;
      break;
    case 95:
      // Set channel detune of 2nd oscillator in cents.
      requireRetune = setCC(&(midiChannel->detune2), value);
      break;
    case 94:
      // Set channel detune in cents.
      requireRetune = setCC(&(midiChannel->detune), value);
      break;
    case 92:
      // Set tremolo modulation level of oscillators on this channel.
      setCC(&(midiChannel->tremoloRange), value);
      break;
    case 90:
      // Set channel detune of 2nd oscillator in clock periods (20us steps).
      requireRetune = setCC(&(midiChannel->detune2Abs), value);
      break;
    case 89:
      // Set channel detune in clock periods (20us steps).
      requireRetune = setCC(&(midiChannel->detuneAbs), value);
      break;
    case 87:
      // Set LFO restart - 0 LFOs free run, > 0 LFO restart on note on (default).
      midiChannel->lfoRestart = value > 0;
      break;
    case 86:
      // Set vibrato LFO shape: 0 (default) sine, 1 downsaw, 2 upsaw.
      _oc->vibratoLfo->SetTable(value);
      break;
    case 85:
      // Set tremolo LFO shape: 0 (default) sine, 1 downsaw, 2 upsaw.
      _oc->tremoloLfo->SetTable(value);
      break;
    case 76:
      // Set global vibrato LFO speed.
      _oc->vibratoLfo->SetHz(MIDI_TO_HZ(value));
      break;
    case 75:
      // Evelope decay time (see AdsrEnvelope.h)
      setCC(&(midiChannel->decay), value);
      break;
    case 73:
      // Envelope attack time (see AdsrEnvelope.h)
      setCC(&(midiChannel->attack), value);
      break;
    case 72:
      // Envelope release time (see AdsrEnvelope.h)
      setCC(&(midiChannel->release), value);
      break;
    case 31:
      // Set pitchbend range in semitones (max 12).
      // Officially we should RPNs, but Ableton among others can't send RPNs out of the box.
      midiChannel->SetPitchBendRange(std::min(value, midiPitchBendRange));
      break;
    case 30:
      // Same as CC 121. Officially we should only use CC 121, but Ableton among others can't
      // manually send 121 and we want to be able reset all parameters easily.
      midiChannel->ResetCC();
      break;
    case 27:
      // Set global configurable LFO speed (configurable LFO not currently used).
      _oc->configurableLfo->SetHz(MIDI_TO_HZ(value));
      break;
    case 24:
      // Envelope sustain level.
      setCC(&(midiChannel->sustain), value);
      break;
    case 22:
      // Set global tremolo LFO speed.
      _oc->tremoloLfo->SetHz(MIDI_TO_HZ(value));
      break;
    case 105:
      // FM modulation-index envelope release (AdsrEnvelope curve). Next note on.
      setCC(&(midiChannel->modRelease), value);
      break;
    case 104:
      // FM modulation-index envelope sustain. Next note on.
      setCC(&(midiChannel->modSustain), value);
      break;
    case 103:
      // FM modulation-index envelope decay -- a bell's brightness fall. Next note on.
      setCC(&(midiChannel->modDecay), value);
      break;
    case 102:
      // FM modulation-index envelope attack. Next note on.
      setCC(&(midiChannel->modAttack), value);
      break;
    case 21:
      // FM modulation index / peak depth (0 = none). Re-stamped on held notes.
      requireRetune = setCC(&(midiChannel->fmIndex), value);
      break;
    case 20:
      // FM carrier:modulator ratio in tenths (0 = FM off, 14 = 1.4, the classic
      // Chowning bell). Re-stamped on held notes.
      requireRetune = setCC(&(midiChannel->fmRatio), value);
      break;
    case 19:
      // FM ratio fine adjust in hundredths (added to CC20's tenths) -- sub-tenth
      // ratios for fatter/cleaner timbres off the coarse grid. Re-stamped on held.
      requireRetune = setCC(&(midiChannel->fmRatioFine), value);
      break;
    case 7:
      // Set channel volume.
      setCC(&(midiChannel->volume), value);
      break;
    case 1:
      // Set vibrato modulation level of oscillators on this channel.
      setCC(&(midiChannel->coarseModulation), value);
      break;
    default:
      break;
  }
  if (requireRetune) {
    midiChannel->RetuneNotes(_oc);
  }
}

void CRMidi::handleProgramChange(byte channel, byte number) {
  MidiChannel *midiChannel = ChannelEnabled(channel);
  if (midiChannel == NULL) {
    return;
  }
  AllNotesOff(midiChannel);
  midiChannel->ResetCC();
  switch (number) {
    case 14:
      // FM bell preset (also fully reachable via the CC set above): ratio 1.4
      // carrier:modulator, instant attack + long ring on both amplitude and a
      // zero-sustain modulation-index envelope, so brightness and loudness fade
      // together like a struck bell.
      midiChannel->fmRatio = 14;
      midiChannel->fmIndex = 100;
      midiChannel->decay = 124;
      midiChannel->sustain = 0;
      midiChannel->release = 40;
      midiChannel->modDecay = 110;
      midiChannel->modSustain = 0;
      break;
    case 38:
      // FM pluck bass: ratio 1.2 with a fast-decaying, zero-sustain modulation
      // index -- a bright FM attack transient over a clean, sustained body.
      midiChannel->fmRatio = 12;
      midiChannel->fmIndex = 115;
      midiChannel->decay = 55;
      midiChannel->sustain = 60;
      midiChannel->release = 30;
      midiChannel->modDecay = 30;
      midiChannel->modSustain = 0;
      midiChannel->modRelease = 20;
      break;
    case 39:
      // FM growl bass: ratio 1.2 with a sustained modulation index -- an animated,
      // buzzy, mid-forward bass.
      midiChannel->fmRatio = 12;
      midiChannel->fmIndex = 110;
      midiChannel->decay = 60;
      midiChannel->sustain = 85;
      midiChannel->release = 40;
      midiChannel->modDecay = 90;
      midiChannel->modSustain = 70;
      midiChannel->modRelease = 30;
      break;
    case 40:
      // FM fat bass: a near-unity ratio (1.08, via the fine adjust) clusters the
      // sidebands tightly around the fundamental for a thick, chorused bass.
      midiChannel->fmRatio = 10;
      midiChannel->fmRatioFine = 8;
      midiChannel->fmIndex = 70;
      midiChannel->decay = 40;
      midiChannel->sustain = 95;
      midiChannel->release = 40;
      midiChannel->modDecay = 30;
      midiChannel->modSustain = 40;
      midiChannel->modRelease = 30;
      break;
    default:
      break;
  }
}

void CRMidi::ExpireNotes(MidiChannel *midiChannel) {
  for (;;) {
    MidiNote *midiNote = midiChannel->NotesExpire(_oc);
    if (midiNote == NULL) {
      break;
    }
    ReturnFreeNote(midiNote);
  }
}

MidiNote *CRMidi::GetFreeNote() {
  if (_freeMidiNotes.empty()) {
    return NULL;
  }
  MidiNote *midiNote = _freeMidiNotes.top();
  _freeMidiNotes.pop();
  return midiNote;
}

void CRMidi::ReturnFreeNote(MidiNote *midiNote) {
  for (OscillatorDeque::const_iterator o = midiNote->oscillators.begin(); o != midiNote->oscillators.end(); ++o) {
    Oscillator *oscillator = *o;
    _oscillatorChannelMap[oscillator->index] = NULL;
  }
  midiNote->Reset();
  _freeMidiNotes.push(midiNote);
}

void CRMidi::FMModulate(MidiChannel *midiChannel) {
  // TODO: simulate vibrato with pitchBend overload for testing.
  if (midiChannel->coarseModulation) {
    cr_fp_t bend = cr_fp_t(maxMidiPitchBend / 4);
    bend *= midiValMap[midiChannel->coarseModulation];
    bend *= _oc->vibratoLfo->Level();
    midiChannel->SetBend(bend.getInteger(), _oc);
  }
}

inline cr_fp_t amModulate(cr_fp_t p, cr_fp_t depth, cr_fp_t level) {
  cr_fp_t dp = p * cr_fp_t(0.5) * depth;
  return p - (dp + (dp * level));
}

inline cr_fp_t CRMidi::ModulateChain(cr_fp_t p, Oscillator *audibleOscillator, MidiChannel *midiChannel) {
  p *= audibleOscillator->pulseUsScale;
  if (midiChannel->tremoloRange) {
    p = amModulate(p, midiValMap[midiChannel->tremoloRange], _oc->tremoloLfo->Level());
  }
  if (!audibleOscillator->envelope->isNull) {
    p *= audibleOscillator->envelope->level;
  }
  if (midiChannel->volume != maxMidiVal) {
    p *= midiValMap[midiChannel->volume];
  }
  return p;
}

cr_fp_t CRMidi::Modulate(Oscillator *audibleOscillator) {
  cr_fp_t p = _crio->pw;
  MidiChannel *midiChannel = getOscillatorChannel(audibleOscillator);
  if (!_crio->fixedPulseEnabled()) {
    p = ModulateChain(p - _crio->breakoutUs, audibleOscillator, midiChannel) + _crio->breakoutUs;
  }
  if (_percussionChannel == midiChannel) {
    // Pitched noise: give the voice that just pulsed a fresh random period
    // inside its precomputed window. One random pick + a period write -- no
    // bend/retune, and no vibrato LFO (which other channels keep via FMModulate).
    if (audibleOscillator->noiseSpan()) {
      audibleOscillator->ApplyNoisePeriod(random(audibleOscillator->noiseSpan() + 1));
    }
  } else {
    FMModulate(midiChannel);
  }
  return p;
}
