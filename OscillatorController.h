// Copyright 2019 Josh Bailey (josh@vandervecken.com)

// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


#ifndef OSCILLATORCONTROLLER_H
#define OSCILLATORCONTROLLER_H

#include "types.h"
#include "constants.h"
#include "Oscillator.h"
#include "Lfo.h"

class OscillatorController {
 public:
  OscillatorController();
  void Tick();
  void RestartLFOs();
  void ResetAll();
  bool Triggered();
  Oscillator *GetFreeOscillator();
  void ReturnFreeOscillator(Oscillator *oscillator);
  void SetMaxPitch(uint8_t maxPitch);
  bool SetFreq(Oscillator *oscillator, cr_fp_t hzInv, uint8_t newPitch, cr_fp_t velocityScale, int periodOffset);
  bool SetFreqLazy(Oscillator *oscillator, cr_fp_t hzInv, uint8_t newPitch, cr_fp_t velocityScale, int periodOffset);
  bool controlTriggered;
  Lfo *tremoloLfo;
  Lfo *vibratoLfo;
  Lfo *configurableLfo;
  Oscillator *audibleOscillator;
 private:
  void _Reschedule();
  Lfo _lfos[lfoCount];
  Oscillator _oscillators[oscillatorCount];
  cr_tick_t _masterClock;
  cr_slowtick_t _controlClock;
  cr_slowtick_t _lfoClock;
  Oscillator *_nextTriggeredOscillator;
  std::stack<Oscillator*> _freeOscillators;
  bool _reschedulePending;
};

#endif
