// Host-test recording DigitalPin. Pulled in by CRDigitalPin.h under
// CR_HOST_TEST (see there). Lives under the test-only include path
// (test/host/stubs) so device builds -- and the repo-root cppcheck run in
// build.sh, which can't find this header -- only ever see the one real
// DigitalPin, avoiding a spurious one-definition-rule violation.
//
// Host unit tests have no real GPIO, but the synth still drives the pins via
// CRIO::pulseOn()/pulseOff() (handlePulse), and that output is exactly what the
// end-to-end test (test/host/test_midi.cpp) measures: a sounding voice makes
// the coil pin oscillate at the note frequency. So rather than a pure no-op,
// this DigitalPin records each virtual pin's level and edges, timestamped with
// a master-clock tick the test driver advances (crHostPinTick). A test reads
// these records back to reconstruct the output waveform and measure its
// frequency. C++17 inline gives one shared instance across translation units
// (CRIO.cpp instantiates the pins; the test includes this to read the records)
// with no extra .cpp.
#ifndef CR_HOST_DIGITAL_PIN_H
#define CR_HOST_DIGITAL_PIN_H
// cppcheck-suppress missingIncludeSystem
#include <climits>
// cppcheck-suppress missingIncludeSystem
#include <stdint.h>

struct CrPinRecord {
  bool level = false;            // current pin level
  unsigned long risingEdges = 0; // low->high transitions == pulse starts
  unsigned long fallingEdges = 0;
  unsigned long firstRisingTick = 0;
  unsigned long lastRisingTick = 0;
  unsigned long minGapTicks =
      ULONG_MAX;                 // shortest interval between pulse starts
  unsigned long maxGapTicks = 0; // longest interval between pulse starts
  void Reset() { *this = CrPinRecord(); }
};

// Master-clock tick the test driver writes before each ISR firing, so pin edges
// are timestamped on the synth's own clock (see test/host/test_midi.cpp).
inline unsigned long crHostPinTick = 0;

// One record per possible pin number (uint8_t, so 256 covers every pin).
inline CrPinRecord &crHostPin(uint8_t pin) {
  static CrPinRecord records[256];
  return records[pin];
}

inline void crHostPinWrite(uint8_t pin, bool value) {
  CrPinRecord &r = crHostPin(pin);
  if (value && !r.level) { // rising edge: a pulse begins
    if (r.risingEdges == 0) {
      r.firstRisingTick = crHostPinTick;
    } else {
      const unsigned long gap = crHostPinTick - r.lastRisingTick;
      if (gap < r.minGapTicks)
        r.minGapTicks = gap;
      if (gap > r.maxGapTicks)
        r.maxGapTicks = gap;
    }
    r.lastRisingTick = crHostPinTick;
    ++r.risingEdges;
  } else if (!value && r.level) { // falling edge: the pulse ends
    ++r.fallingEdges;
  }
  r.level = value;
}

template <uint8_t PinNumber> class DigitalPin {
public:
  DigitalPin(bool, bool) {}
  void high() { crHostPinWrite(PinNumber, true); }
  void low() { crHostPinWrite(PinNumber, false); }
  bool read() const { return false; }
  void write(bool value) { crHostPinWrite(PinNumber, value); }
};

#endif // CR_HOST_DIGITAL_PIN_H
