// Copyright 2019 Josh Bailey (josh@vandervecken.com)

// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


#include "types.h"

#ifndef ADSRENVELOPE_H
#define ADSRENVELOPE_H

class AdsrEnvelope {
 public:
  AdsrEnvelope();
  void Reset(uint8_t attack, uint8_t decay, uint8_t sustain, uint8_t release);
  bool IsIdle();
  void Decay();
  void Sustain();
  void Idle();
  void Release();
  void HandleControl();
  cr_fp_t level;
  bool isNull;
 private:
  enum { ENV_ATTACK, ENV_DECAY, ENV_SUSTAIN, ENV_RELEASE, ENV_IDLE };
  void SetInitialPhase();
  void ChangePhase(uint8_t newPhase);
  uint8_t _phase;
  cr_fp_t _attack;
  cr_fp_t _attackInc;
  cr_fp_t _decay;
  cr_fp_t _decayDec;
  cr_fp_t _sustain;
  cr_fp_t _release;
  cr_fp_t _releaseDec;
  cr_fp_t _ageMs;
};

#endif
