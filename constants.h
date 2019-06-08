// Copyright 2019 Josh Bailey (josh@vandervecken.com)

// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


#ifndef CONSTANTS_H
#define CONSTANTS_H

const cr_fp_t coronaUs = 8;
const cr_fp_t pulseWindowUs = 120;
const uint8_t pulseGuardTicks = 1;

const uint8_t oscillatorCount = 16;
const cr_tick_t masterClockHz = 50000;
const cr_tick_t masterClockPeriodUs = 1e6 / masterClockHz;
const cr_tick_t masterClockMax = masterClockHz - 1;


const cr_slowtick_t controlClockRelHz = 10;
const cr_slowtick_t controlClockHz = masterClockHz / controlClockRelHz;
const cr_slowtick_t controlClockMax = controlClockRelHz - 1;
const float controlClockTickMs = 1e3 / float(controlClockHz);

const cr_slowtick_t lfoClockRelHz = 50;
const cr_slowtick_t lfoClockHz = masterClockHz / lfoClockRelHz;
const cr_slowtick_t lfoClockMax = lfoClockRelHz - 1;
const cr_tick_t lfoTickMax = lfoClockHz - 1;
const uint16_t maxLfoTable = 1e3 - 1;
const cr_fp_t lfoMaxHz = 20;

const uint8_t stdMidiChannels = 16;
const uint8_t maxMidiChannel = 8;
const uint8_t midiChannelStorage = maxMidiChannel + 1; // +1 reserved for ch 10 automatically
const cr_fp_t maxMidiPitchBend = 8192;
const uint8_t absMaxMidiPitch = 127;
const uint8_t maxMidiPitch = 96;
const uint8_t maxMidiVal = 127;

const uint8_t lfoCount = 3;

#include "miditables.h"

#endif
