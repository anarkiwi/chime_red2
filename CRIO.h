// Copyright 2019 Josh Bailey (josh@vandervecken.com)

// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.



#ifndef CRIO_H
#define CRIO_H

#include "config.h"
#include "types.h"
#include "constants.h"

const uint8_t lcdWidth = 16;
const uint8_t lcdLines = 2;
const uint8_t analogBits = 12;
const cr_fp_t maxAnalogRead = cr_fp_t(1) / cr_fp_t(4095);
const uint8_t oneShotPulsePadUs = 1;
const uint8_t oneShotRemainderPulsePadUs = masterClockPeriodUs - oneShotPulsePadUs;
const uint8_t potPins = 3;
const uint8_t potSampleWindow = 16;

typedef struct {
  uint8_t pin;
  cr_fp_t currVal;
  cr_fp_t newVal;
  uint8_t newValSamples;
} potPinType;

class CRIO {
 public:
  CRIO();
  void startPulse();
  void schedulePulse(cr_fp_t pulseUs);
  virtual bool handlePulse();
  virtual void updateLcd();
  virtual bool pollPots();
  virtual void updateCoeff();
  virtual void updateLcdCoeff();
  virtual bool percussionEnabled();
  virtual bool fixedPulseEnabled();
  bool scheduled;
  bool slipTick;
  cr_fp_t pw;
  uint8_t maxPitch;
  cr_fp_t breakoutUs;
private:
  void pulseOff();
  void pulseOn();
  bool _pulseState;
  cr_pulse_t _oneShotPulseUs;
  uint8_t _multiShotPulses;
  uint16_t _ticksSinceLastPulse;
};

#ifdef CR_UI
class CRIOLcd : public CRIO {
 public:
  CRIOLcd();
  void updateLcd() override;
  bool pollPots() override;
  void updateCoeff() override;
  void updateLcdCoeff() override;
  bool percussionEnabled() override;
  bool fixedPulseEnabled() override;
 private:
  cr_fp_t _scalePot(uint8_t);
  char _lcdBuffer[lcdLines][lcdWidth+1];
  char _lcdFrameBuffer[lcdLines][lcdWidth+1];
  uint8_t _lcdRow;
  uint8_t _lcdCol;
  uint8_t _lastLcdRow;
  uint8_t _lastLcdCol;
  uint8_t _nextPotPin;
  uint8_t _potSampleCount;
  char *_lcdLine1;
  potPinType potPinState[potPins];
  potPinType *_prPot;
  potPinType *_pwPot;
  potPinType *_maxChargePot;
};
#endif

#endif
