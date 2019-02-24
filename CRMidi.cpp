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
  _midiChannelMap[PERC_CHAN] = _midiChannels + maxMidiChannel;
  ResetAll();
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

void CRMidi::ResetChannel(MidiChannel *midiChannel) {
  midiChannel->IdleAllNotes();
  ExpireNotes(midiChannel);
  midiChannel->Reset();
}

void CRMidi::ResetAll() {
  FOR_ALL_CHAN(ResetChannel(midiChannel));
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
      _crio->pollPots();
      break;
    case 3:
      handlePitchBend(PERC_CHAN, random(0, 16383) - 8192);
      break;
    case 4:
      FMModulate();
      break;
    case 5:
      _crio->updateCoeff();
      break;
    case 6:
      _crio->updateLcdCoeff();
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
  midiChannel->NoteOn(note, velocity, _crio->maxPitch, midiNote, _oc);
  for (OscillatorDeque::const_iterator o = midiNote->oscillators.begin(); o != midiNote->oscillators.end(); ++o) {
    Oscillator *oscillator = *o;
    _oscillatorChannelMap[oscillator->index] = midiChannel;
  }
}

void CRMidi::handleNoteOff(byte channel, byte note) {
  MidiChannel *midiChannel = ChannelNoteValid(channel, note);
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
  midiChannel->SetBend(bend, _crio->maxPitch, _oc);
}

void CRMidi::handleControlChange(byte channel, byte number, byte value) {
  MidiChannel *midiChannel = ChannelEnabled(channel);
  if (midiChannel == NULL) {
    return;
  }
  switch (number) {
    case 123:
      ResetChannel(midiChannel);
      break;
    case 92:
      SET_CC(midiChannel->tremoloRange, value);
      break;
    case 73:
      SET_CC(midiChannel->attack, value);
      break;
    case 76:
      _oc->vibratoLfo->SetHz(midiValMap[value] * cr_fp_t(lfoMaxHz));
      break;
    case 75:
      SET_CC(midiChannel->decay, value);
      break;
    case 72:
      SET_CC(midiChannel->release, value);
      break;
    case 27:
      _oc->configurableLfo->SetHz(midiValMap[value] * cr_fp_t(lfoMaxHz));
      break;
    case 24:
      SET_CC(midiChannel->sustain, value);
      break;
    case 22:
      _oc->tremoloLfo->SetHz(midiValMap[value] * cr_fp_t(lfoMaxHz));
      break;
    case 20: {
      if (value) {
          SET_CC(midiChannel->pulserCount, value);
        }
      }
      break;
    case 21:
      SET_CC(midiChannel->pulserSpread, value);
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

void CRMidi::FMModulate() {
  // TODO: simulate vibrato with pitchBend overload for testing.
  for (uint8_t o = 0; o < oscillatorCount; ++o) {
    Oscillator *oscillator = _oc->getOscillator(o);
    MidiChannel *midiChannel = getOscillatorChannel(oscillator);
    if (midiChannel == NULL)  {
      continue;
    }
    if (midiChannel->coarseModulation) {
      cr_fp_t bend = cr_fp_t(maxMidiPitchBend / 4);
      bend *= midiValMap[midiChannel->coarseModulation];
      bend *= _oc->vibratoLfo->Level();
      midiChannel->SetBend(bend.getInteger(), _crio->maxPitch, _oc);
    }
  }
}
