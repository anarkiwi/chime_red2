// Copyright 2019 Josh Bailey (josh@vandervecken.com)

// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

// TODO: DigitalIO library doesn't yet support Due.

#ifndef DigitalPin_h
#define DigitalPin_h
#include <Arduino.h>

#define PINDESC(PinNumber)	g_APinDescription[PinNumber].ulPin
#define	PINPPORT(PinNumber, PPIO)	g_APinDescription[PinNumber].pPort->PPIO

template<uint8_t PinNumber>
class DigitalPin {
 public:
  DigitalPin(bool mode, bool value) {
    pinMode(PinNumber, mode);
    write(value);
  }
  inline __attribute__((always_inline))
  void high() { write(true); }
  inline __attribute__((always_inline))
  void low() { write(false); }
  inline __attribute__((always_inline))
  bool read() const {
    return PINPPORT(PinNumber, PIO_PDSR) & PINDESC(PinNumber);
  }
  inline __attribute__((always_inline))
  void write(bool value) {
    if (value) {
      PINPPORT(PinNumber, PIO_SODR) = PINDESC(PinNumber);
    } else {
      PINPPORT(PinNumber, PIO_CODR) = PINDESC(PinNumber);
    }
  }
};
#endif  // DigitalPin_h
