// Host-test stub for the Arduino MIDI Library. CRMidi.cpp #includes <MIDI.h> but
// only uses the `byte` type from it (the actual serial MIDI parsing lives in the
// .ino, which host tests bypass by calling CRMidi's handlers directly). Pull in
// the Arduino stub for `byte` and otherwise stay empty.
#ifndef CR_HOST_MIDI_STUB_H
#define CR_HOST_MIDI_STUB_H
#include <Arduino.h>
#endif
