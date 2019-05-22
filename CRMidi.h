// Copyright 2019 Josh Bailey (josh@vandervecken.com)

// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


#ifndef CRMIDI_H
#define CRMIDI_H

#include "CRIO.h"
#include "OscillatorController.h"
#include "MidiNote.h"
#include "MidiChannel.h"

#define PERC_CHAN 10

class CRMidi {
  public:
    CRMidi(OscillatorController *oc, CRIO *crio);
    void ResetChannel(MidiChannel *midiChannel);
    void ResetAll();
    void handleNoteOn(byte channel, byte note, byte velocity);
    void handleNoteOff(byte channel, byte note);
    void handlePitchBend(byte channel, int bend);
    void handleControlChange(byte channel, byte number, byte value);
    bool HandleControl();
    MidiChannel *getOscillatorChannel(Oscillator *oscillator);
  private:
    MidiChannel *ChannelNoteValid(byte channel, byte note);
    MidiChannel *ChannelEnabled(byte channel);
    void AllNotesOff(MidiChannel *midiChannel);
    void ExpireNotes(MidiChannel *midiChannel);
    MidiNote *GetFreeNote();
    void ReturnFreeNote(MidiNote *midiNote);
    void FMModulate();
    MidiNote _midiNotes[oscillatorCount];
    MidiChannel _midiChannels[midiChannelStorage];
    MidiChannel *_midiChannelMap[stdMidiChannels+1];
    MidiChannel *_oscillatorChannelMap[oscillatorCount];
    std::stack<MidiNote*> _freeMidiNotes;
    OscillatorController *_oc;
    CRIO *_crio;
    uint8_t _controlCounter;
};

#endif
