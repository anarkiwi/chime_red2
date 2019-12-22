// Copyright 2019 Josh Bailey (josh@vandervecken.com)

// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


#ifndef TYPES_H
#define TYPES_H

// cppcheck-suppress missingIncludeSystem
#include <stdint.h>
// cppcheck-suppress missingIncludeSystem
#include <ArduinoSTL.h>
// cppcheck-suppress missingIncludeSystem
#include <iterator>
// cppcheck-suppress missingIncludeSystem
#include <stack>
// cppcheck-suppress missingIncludeSystem
#include <deque>
// cppcheck-suppress missingIncludeSystem
#include <FixedPoints.h>
// cppcheck-suppress missingIncludeSystem
#include <FixedPointsCommon.h>

typedef uint16_t cr_pulse_t;
typedef uint16_t cr_tick_t;
typedef uint32_t cr_slowtick_t;
typedef SFixed<16, 15> cr_fp_t;

#endif
