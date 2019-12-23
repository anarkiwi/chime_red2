// Copyright 2019 Josh Bailey (josh@vandervecken.com)

// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


#include "CRMidi.h"
#include "Oscillator.h"
#include "constants.h"

#define GET_CHAN(channel) _midiChannelMap[channel]
#define FOR_ALL_CHAN(channel_func) \
  { for (uint8_t c = 0; c < midiChannelStorage; ++c) { MidiChannel *midiChannel = _midiChannels + c; channel_func; } }
#define SET_CC(cc_attr, value) { if (cc_attr != value) { cc_attr = value; } }
#define MIDI_TO_HZ(value) (midiValMap[value] * cr_fp_t(lfoMaxHz))

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

inline int16_t randomBend() {
  return random(0, 16383) - 8192;
}

void CRMidi::updateCoeff() {
  _crio->updateCoeff();
  _crio->updateLcdCoeff();
  _oc->SetMaxHz(pitchToHz[_crio->maxPitch]);
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
      _percussionChannel->SetBend(randomBend(), _oc);
      break;
    case 3:
      if (!_crio->pollPots()) {
        complete = true;
      }
      break;
    case 4:
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
    handleNoteOff(channel, note);
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
  // https://www.midi.org/midi/specifications/item/table-1-summary-of-midi-message
  // https://www.midi.org/specifications-old/item/table-3-control-change-messages-data-bytes-2
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
      break;
    case 95:
      SET_CC(midiChannel->detune2, value);
      midiChannel->RetuneNotes(_oc);
      break;
    case 94:
      SET_CC(midiChannel->detune, value);
      midiChannel->RetuneNotes(_oc);
      break;
    case 92:
      SET_CC(midiChannel->tremoloRange, value);
      break;
    case 90:
      SET_CC(midiChannel->detune2Abs, value);
      midiChannel->RetuneNotes(_oc);
      break;
    case 89:
      SET_CC(midiChannel->detuneAbs, value);
      midiChannel->RetuneNotes(_oc);
      break;
    case 76:
      _oc->vibratoLfo->SetHz(MIDI_TO_HZ(value));
      break;
    case 75:
      SET_CC(midiChannel->decay, value);
      break;
    case 73:
      SET_CC(midiChannel->attack, value);
      break;
    case 72:
      SET_CC(midiChannel->release, value);
      break;
    case 27:
      _oc->configurableLfo->SetHz(MIDI_TO_HZ(value));
      break;
    case 24:
      SET_CC(midiChannel->sustain, value);
      break;
    case 22:
      _oc->tremoloLfo->SetHz(MIDI_TO_HZ(value));
      break;
    case 1:
      SET_CC(midiChannel->coarseModulation, value);
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

inline cr_fp_t amModulate(cr_fp_t p, cr_fp_t depth, Lfo *lfo) {
  cr_fp_t dp = (p / cr_fp_t(2)) * depth;
  return p - (dp + (dp * lfo->Level()));
}

cr_fp_t CRMidi::Modulate(Oscillator *audibleOscillator) {
  cr_fp_t p = _crio->pw;
  MidiChannel *midiChannel = getOscillatorChannel(audibleOscillator);
  if (!_crio->fixedPulseEnabled()) {
    p -= coronaUs;
    p *= audibleOscillator->pulseUsScale;
    if (midiChannel->tremoloRange) {
      p = amModulate(p, midiValMap[midiChannel->tremoloRange], _oc->tremoloLfo);
    }
    p *= audibleOscillator->envelope->level;
    p += coronaUs;
  }
  if (_percussionChannel != midiChannel) {
    FMModulate(midiChannel);
  }
  return p;
}
