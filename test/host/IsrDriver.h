// Host-side replica of the master-clock ISR state machine in chime_red2.ino
// (nextISR / modulateISR / slipTickISR, minus the serial MIDI.read()). On the
// device a hardware timer fires masterISR at masterClockHz; here one Tick() is
// one such firing. This is the part that turns a sounding voice into pin pulses:
// when an oscillator triggers, the next firing computes the pulse width
// (CRMidi::Modulate) and schedules it (CRIO::schedulePulse), and following
// firings drive the pin via CRIO::handlePulse (pulseOn/pulseOff). Each Tick()
// stamps crHostPinTick with the master-tick count so the recording DigitalPin
// timestamps pin edges on the synth's own clock.
//
// Extracted from test/host/test_midi.cpp so both the MIDI unit tests and the
// SMF->WAV simulator (test/host/cr_render.cpp) drive the synth through the exact
// same ISR replica -- there is then one place to keep in sync with the .ino.
// Header-only (a plain class, methods implicitly inline) and under the test-only
// include path, so the device build and the repo-root cppcheck never see it.
#ifndef CR_HOST_ISR_DRIVER_H
#define CR_HOST_ISR_DRIVER_H

#include "types.h"
#include "constants.h"
#include "CRDigitalPin.h"  // crHostPinTick (CR_HOST_TEST recording pin)
#include "OscillatorController.h"
#include "CRIO.h"
#include "CRMidi.h"

class IsrDriver {
 public:
  IsrDriver(OscillatorController &oc, CRMidi &crmidi, CRIO &crio)
      : oc_(oc), crmidi_(crmidi), crio_(crio), state_(&IsrDriver::nextISR) {}

  void Tick() {
    crHostPinTick = masterTicks_;
    (this->*state_)();
  }

  // Total master-clock advances so far == elapsed time * masterClockHz.
  unsigned long masterTicks() const { return masterTicks_; }

  // Worst-case latency instrumentation. MIDI.read() (the only point an incoming
  // message is parsed) runs exactly when HandleControl() completes a cycle, so
  // the spacing between those completions bounds how long a just-missed message
  // waits to be processed. Call ResetLatency() to start a clean measurement
  // window, then read maxReadGap() (in master ISRs).
  void ResetLatency() {
    maxReadGap_ = 0;
    readCount_ = 0;
  }
  unsigned long maxReadGap() const { return maxReadGap_; }
  unsigned long readCount() const { return readCount_; }

 private:
  // Every oc.Triggered() is one master-clock tick; count them so callers have an
  // exact, monotonic time base for measuring the pin's pulse rate.
  bool Triggered() {
    ++masterTicks_;
    return oc_.Triggered();
  }

  void slipTickISR() {
    Triggered();
    Triggered();
    state_ = &IsrDriver::nextISR;
  }

  void modulateISR() {
    cr_fp_t p = crmidi_.Modulate(oc_.audibleOscillator);
    crio_.schedulePulse(p);
    Triggered();
    state_ = &IsrDriver::nextISR;
  }

  void nextISR() {
    if (crio_.handlePulse()) {
      state_ = &IsrDriver::slipTickISR;
      return;
    }
    if (Triggered()) {
      if (oc_.audibleOscillator) {
        state_ = &IsrDriver::modulateISR;
      }
      return;
    }
    if (oc_.controlTriggered) {
      if (crmidi_.HandleControl()) {
        oc_.controlTriggered = false;
        // chime_red2.ino calls MIDI.read() here -- the sole point a MIDI byte is
        // parsed. Record the ISR spacing between these completions.
        if (readCount_ > 0) {
          const unsigned long gap = masterTicks_ - lastReadTick_;
          if (gap > maxReadGap_) {
            maxReadGap_ = gap;
          }
        }
        lastReadTick_ = masterTicks_;
        ++readCount_;
      }
    }
  }

  OscillatorController &oc_;
  CRMidi &crmidi_;
  CRIO &crio_;
  void (IsrDriver::*state_)();
  unsigned long masterTicks_ = 0;
  unsigned long lastReadTick_ = 0;
  unsigned long maxReadGap_ = 0;
  unsigned long readCount_ = 0;
};

#endif  // CR_HOST_ISR_DRIVER_H
