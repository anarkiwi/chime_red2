// Copyright 2019 Josh Bailey (josh@vandervecken.com)

// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#include "pins.h"
#include "CRIO.h"

// TODO: DigitalIO library doesn't yet support Due.
#include "CRDigitalPin.h"
DigitalPin<coilOutPin> _coilOutPin(OUTPUT, LOW);
DigitalPin<diagOutPin> _diagOutPin(OUTPUT, LOW);
DigitalPin<speakerOutPin> _speakerOutPin(OUTPUT, LOW);

CRIO::CRIO() : scheduled(false), slipTick(false), _oneShotPulseUs(0), _multiShotPulses(0), _pulseState(0), pw(pulseWindowUs), maxPitch(maxMidiPitch), breakoutUs(minBreakoutUs), _ticksSinceLastPulse(0) {
}

bool CRIO::percussionEnabled() {
  return true;
}

bool CRIO::fixedPulseEnabled() {
  return false;
}

inline void CRIO::pulseOn() {
  _speakerOutPin.high();
  _diagOutPin.high();
  _coilOutPin.high();
  _pulseState = true;
}

inline void CRIO::pulseOff() {
  _coilOutPin.low();
  _diagOutPin.low();
  _speakerOutPin.low();
  _pulseState = false;
}

bool CRIO::handlePulse() {
  ++_ticksSinceLastPulse;
  if (scheduled) {
    return false;
  }
  if (_multiShotPulses == 0) {
    if (_oneShotPulseUs) {
      pulseOn();
      delayMicroseconds(_oneShotPulseUs);
      pulseOff();
      _oneShotPulseUs = 0;
      return true;
    }
    pulseOff();
    return false;
  }
  pulseOn();
  --_multiShotPulses;
  return false;
}

void CRIO::startPulse() {
  if (scheduled) {
    _ticksSinceLastPulse = 0;
    scheduled = false;
  }
}

void CRIO::schedulePulse(cr_fp_t pulseUs) {
  if (scheduled) {
    return;
  }
  if (pulseUs == 0) {
    return;
  }
  if (_ticksSinceLastPulse < pulseGuardTicks) {
    return;
  }
  scheduled = true;
  _oneShotPulseUs = pulseUs.getInteger();
  if (_oneShotPulseUs >= cr_pulse_t(masterClockPeriodUs)) {
    _multiShotPulses = _oneShotPulseUs / masterClockPeriodUs;
    _oneShotPulseUs -= _multiShotPulses * masterClockPeriodUs;
  } else {
    _multiShotPulses = 0;
  }
  if (_oneShotPulseUs) {
    if (_oneShotPulseUs > oneShotRemainderPulsePadUs) {
      ++_multiShotPulses;
      _oneShotPulseUs = 0;
    } else if (_oneShotPulseUs < oneShotPulsePadUs) {
      _oneShotPulseUs = oneShotPulsePadUs;
    }
  }
}


bool CRIO::pollPots() {
  return false;
}

void CRIO::updateLcd() {
}

void CRIO::updateCoeff() {
}

void CRIO::updateLcdCoeff() {
}

#ifdef CR_UI
DigitalPin<fixedVarPulseInPin> _fixedVarInPin(INPUT, LOW);
DigitalPin<percussionEnableInPin> _percussionEnableInPin(INPUT, LOW);

// TODO: Regular LCD library uses timers and isn't interruptable.
#include "CRLiquidCrystal.h"
CRLiquidCrystal lcd(lcd_rs, lcd_rw, lcd_en, lcd_d4, lcd_d5, lcd_d6, lcd_d7);

CRIOLcd::CRIOLcd() {
  _lcdRow = 0;
  _lcdCol = 0;
  _lastLcdRow = 0;
  _lastLcdCol = 0;
  bzero(&potPinState, sizeof(potPinState));
  _maxChargePot = potPinState;
  _maxChargePot->pin = A9;
  _prPot = potPinState + 1;
  _prPot->pin = A10;
  _pwPot = potPinState + 2;
  _pwPot->pin = A11;
  _nextPotPin = 0;
  _potSampleCount = 0;
  analogReadResolution(analogBits);
  REG_ADC_MR = (REG_ADC_MR & 0xFFF0FFFF) | 0x00020000; // ADC startup time
  REG_ADC_MR = (REG_ADC_MR & 0xFFFFFF0F) | 0x00000080; // enable FREERUN mode
  memset(&_lcdBuffer, ' ', sizeof(_lcdBuffer));
  memset(&_lcdFrameBuffer, ' ', sizeof(_lcdFrameBuffer));
  _lcdLine1 = _lcdBuffer[1];
  lcd.begin();
}

bool CRIOLcd::percussionEnabled() {
  return _percussionEnableInPin.read();
}

bool CRIOLcd::fixedPulseEnabled() {
  return _fixedVarInPin.read();
}

inline cr_fp_t CRIOLcd::_scalePot(uint8_t pin) {
  uint16_t sample = analogRead(pin);
  sample = sample >> 2 << 2;
  return cr_fp_t(1) - (cr_fp_t(sample) * maxAnalogRead);
}

bool CRIOLcd::pollPots() {
  potPinType *potPin = potPinState + _nextPotPin;
  cr_fp_t sampleVal = _scalePot(potPin->pin);
  if (++_potSampleCount == potSampleWindow) {
    _potSampleCount = 0;
    if (++_nextPotPin == potPins) {
      _nextPotPin = 0;
    }
  }
  if (sampleVal != potPin->currVal) {
    if (sampleVal != potPin->newVal) {
      potPin->newVal = sampleVal;
      potPin->newValSamples = 0;
    } else if (++(potPin->newValSamples) == potSampleWindow - 1) {
      potPin->currVal = potPin->newVal;
      return true;
    }
  }
  return false;
}

void CRIOLcd::updateLcd() {
  char c = _lcdBuffer[_lcdRow][_lcdCol];
  if (c == 0) {
    c = ' ';
  }
  char *fc = &(_lcdFrameBuffer[_lcdRow][_lcdCol]);
  if (*fc != c) {
    *fc = c;
    // save a setcursor if last write positioned us.
    if (!(_lastLcdRow == _lcdRow && _lcdCol == _lastLcdCol + 1)) {
      lcd.setCursor(_lcdCol, _lcdRow);
    }
    lcd.write(c);
    _lastLcdRow = _lcdRow;
    _lastLcdCol = _lcdCol;
  }
  if (++_lcdCol == lcdWidth) {
    _lcdCol = 0;
    if (++_lcdRow == lcdLines) {
      _lcdRow = 0;
    }
  }
}

void CRIOLcd::updateCoeff() {
  breakoutUs = ((maxBreakoutUs - minBreakoutUs) * _maxChargePot->currVal) + minBreakoutUs;
  pw = _pwPot->currVal * pulseWindowUs;
  if (pw < breakoutUs) {
    pw = breakoutUs;
  }
  maxPitch = roundFixed(_prPot->currVal * cr_fp_t(maxMidiPitch)).getInteger();
}

void CRIOLcd::updateLcdCoeff() {
  memset(_lcdLine1, ' ', lcdWidth);
  itoa(pitchToHz[maxPitch].getInteger(), _lcdLine1, 10);
  *(_lcdLine1 + 4) = 'H';
  *(_lcdLine1 + 5) = 'z';
  itoa(pw.getInteger(), _lcdLine1+7, 10);
  *(_lcdLine1 + 10) = 'u';
  itoa(breakoutUs.getInteger(), _lcdLine1+12, 10);
  *(_lcdLine1 + 15) = 'u';
}
#endif
