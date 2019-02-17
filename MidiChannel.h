// Copyright 2019 Josh Bailey (josh@vandervecken.com)

// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


#ifndef MIDICHANNEL_H
#define MIDICHANNEL_H

#include "Oscillator.h"
#include "OscillatorController.h"
#include "MidiNote.h"

typedef std::deque<MidiNote*> MidiNoteDeque;

class MidiChannel {
  public:
    MidiChannel();
    void Reset();
    void ResetCC();
    void IdleAllNotes();
    MidiNote *LookupNote(uint8_t note);
    void SetBend(int newBend, uint8_t maxPitch, OscillatorController *oc);
    cr_fp_t BendHz(MidiNote *midiNote, uint8_t maxPitch);
    void NoteOn(uint8_t note, uint8_t velocity, uint8_t maxPitch, MidiNote *midiNote, OscillatorController *oc);
    void ReleaseNote(uint8_t note);
    bool ResetNote(uint8_t note);
    MidiNote *NotesExpire(OscillatorController *oc);
    void HandleControl();
    int bend;
    uint8_t attack;
    uint8_t decay;
    uint8_t sustain;
    uint8_t release;
    uint8_t pulserCount;
    uint8_t pulserSpread;
    uint8_t tremoloRange;
    uint8_t coarseModulation;
   private:
    uint8_t _pitchBendRange;
    cr_fp_t _pitchBendScale;
    int8_t _pitchBendDiff;
    MidiNoteDeque _midiNotes;
    std::stack<MidiNote*> _idleNotes;
    MidiNote *_noteMap[maxMidiPitch];
};

#endif
