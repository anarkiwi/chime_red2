// Copyright 2019 Josh Bailey (josh@vandervecken.com)

// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


#ifndef PINS_H
#define PINS_H

// cppcheck-suppress missingIncludeSystem
#include <stdint.h>

const uint8_t coilOutPin = 11;
const uint8_t diagOutPin = 12;
const uint8_t speakerOutPin = 13;
const uint8_t fixedVarPulseInPin = 32;
const uint8_t percussionEnableInPin = 36;

const uint8_t lcd_d7 = 45;
const uint8_t lcd_d6 = 43;
const uint8_t lcd_d5 = 41;
const uint8_t lcd_d4 = 39;
const uint8_t lcd_rs = 47;
const uint8_t lcd_rw = 49;
const uint8_t lcd_en = 51;


#endif
