// Host-test stub for the Arduino core header. The synth's frequency logic
// (Oscillator + fixed-point tables) needs no Arduino runtime, and the
// FixedPoints library only #includes this header out of habit. The MIDI tests
// additionally drive CRMidi/CRIO, so this stub also provides the few Arduino
// core symbols those use (byte, delayMicroseconds, a deterministic random()).
#ifndef CR_HOST_ARDUINO_STUB_H
#define CR_HOST_ARDUINO_STUB_H
#include <stdint.h>
#include <cstdlib>

typedef uint8_t byte;

// Pin mode / level constants used in CRIO's DigitalPin declarations.
enum { LOW = 0, HIGH = 1, INPUT = 0, OUTPUT = 1, INPUT_PULLUP = 2 };

// Pulse output timing is not exercised by host tests.
inline void delayMicroseconds(unsigned int) {}

// Deterministic stand-in: return the low bound so percussion's randomBend() is
// repeatable across runs (real firmware uses the hardware RNG).
inline long random(long howsmall, long howbig) {
  (void)howbig;
  return howsmall;
}
inline long random(long howbig) { return random(0, howbig); }

#endif
