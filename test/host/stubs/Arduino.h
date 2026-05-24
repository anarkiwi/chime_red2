// Host-test stub for the Arduino core header. The synth's frequency logic
// (Oscillator + fixed-point tables) needs no Arduino runtime, and the
// FixedPoints library only #includes this header out of habit. The MIDI tests
// additionally drive CRMidi/CRIO, so this stub also provides the few Arduino
// core symbols those use (byte, delayMicroseconds, a deterministic random()).
#ifndef CR_HOST_ARDUINO_STUB_H
#define CR_HOST_ARDUINO_STUB_H
#include <cstdlib>
#include <stdint.h>

typedef uint8_t byte;

// Pin mode / level constants used in CRIO's DigitalPin declarations.
enum { LOW = 0, HIGH = 1, INPUT = 0, OUTPUT = 1, INPUT_PULLUP = 2 };

// Pulse output timing is not exercised by host tests.
inline void delayMicroseconds(unsigned int) {}

// Two stand-ins for the Arduino RNG, selected at compile time:
//  - default (unit tests): return the low bound, so percussion's pitched-noise
//    period pick is repeatable and the MIDI tests can assert exact periods.
//  - CR_SIM_RANDOM (the SMF->WAV simulator, test/host/cr_render.cpp): draw a
//  real
//    pseudo-random value so channel-10 noise actually sounds noisy. Defined
//    only for the renderer build, so test determinism is untouched.
#ifdef CR_SIM_RANDOM
inline long random(long howsmall, long howbig) {
  if (howbig <= howsmall) {
    return howsmall;
  }
  return howsmall + (std::rand() % (howbig - howsmall));
}
#else
inline long random(long howsmall, long howbig) {
  (void)howbig;
  return howsmall;
}
#endif
inline long random(long howbig) { return random(0, howbig); }

#endif
