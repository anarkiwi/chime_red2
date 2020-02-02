// Copyright 2019 Josh Bailey (josh@vandervecken.com)

// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


#include "AdsrEnvelope.h"
#include "constants.h"


// print('const cr_fp_t AdsrCurveMs[] = {')
// for i in range(128):
//  print('  %f, // %u' % (i+((i**4)>>16), i))
//print('};')
const cr_fp_t AdsrCurveMs[] = {
  0.000000, // 0
  1.000000, // 1
  2.000000, // 2
  3.000000, // 3
  4.000000, // 4
  5.000000, // 5
  6.000000, // 6
  7.000000, // 7
  8.000000, // 8
  9.000000, // 9
  10.000000, // 10
  11.000000, // 11
  12.000000, // 12
  13.000000, // 13
  14.000000, // 14
  15.000000, // 15
  17.000000, // 16
  18.000000, // 17
  19.000000, // 18
  20.000000, // 19
  22.000000, // 20
  23.000000, // 21
  25.000000, // 22
  27.000000, // 23
  29.000000, // 24
  30.000000, // 25
  32.000000, // 26
  35.000000, // 27
  37.000000, // 28
  39.000000, // 29
  42.000000, // 30
  45.000000, // 31
  48.000000, // 32
  51.000000, // 33
  54.000000, // 34
  57.000000, // 35
  61.000000, // 36
  65.000000, // 37
  69.000000, // 38
  74.000000, // 39
  79.000000, // 40
  84.000000, // 41
  89.000000, // 42
  95.000000, // 43
  101.000000, // 44
  107.000000, // 45
  114.000000, // 46
  121.000000, // 47
  129.000000, // 48
  136.000000, // 49
  145.000000, // 50
  154.000000, // 51
  163.000000, // 52
  173.000000, // 53
  183.000000, // 54
  194.000000, // 55
  206.000000, // 56
  218.000000, // 57
  230.000000, // 58
  243.000000, // 59
  257.000000, // 60
  272.000000, // 61
  287.000000, // 62
  303.000000, // 63
  320.000000, // 64
  337.000000, // 65
  355.000000, // 66
  374.000000, // 67
  394.000000, // 68
  414.000000, // 69
  436.000000, // 70
  458.000000, // 71
  482.000000, // 72
  506.000000, // 73
  531.000000, // 74
  557.000000, // 75
  585.000000, // 76
  613.000000, // 77
  642.000000, // 78
  673.000000, // 79
  705.000000, // 80
  737.000000, // 81
  771.000000, // 82
  807.000000, // 83
  843.000000, // 84
  881.000000, // 85
  920.000000, // 86
  961.000000, // 87
  1003.000000, // 88
  1046.000000, // 89
  1091.000000, // 90
  1137.000000, // 91
  1185.000000, // 92
  1234.000000, // 93
  1285.000000, // 94
  1337.000000, // 95
  1392.000000, // 96
  1447.000000, // 97
  1505.000000, // 98
  1564.000000, // 99
  1625.000000, // 100
  1688.000000, // 101
  1753.000000, // 102
  1820.000000, // 103
  1889.000000, // 104
  1959.000000, // 105
  2032.000000, // 106
  2107.000000, // 107
  2183.000000, // 108
  2262.000000, // 109
  2344.000000, // 110
  2427.000000, // 111
  2513.000000, // 112
  2600.000000, // 113
  2691.000000, // 114
  2783.000000, // 115
  2878.000000, // 116
  2976.000000, // 117
  3076.000000, // 118
  3178.000000, // 119
  3284.000000, // 120
  3391.000000, // 121
  3502.000000, // 122
  3615.000000, // 123
  3731.000000, // 124
  3850.000000, // 125
  3971.000000, // 126
  4096.000000, // 127
};

const cr_fp_t AdsrCurveLevelStep[] = {
  1.0 / (0.000000 / controlClockTickMs), // 0
  1.0 / (1.000000 / controlClockTickMs), // 1
  1.0 / (2.000000 / controlClockTickMs), // 2
  1.0 / (3.000000 / controlClockTickMs), // 3
  1.0 / (4.000000 / controlClockTickMs), // 4
  1.0 / (5.000000 / controlClockTickMs), // 5
  1.0 / (6.000000 / controlClockTickMs), // 6
  1.0 / (7.000000 / controlClockTickMs), // 7
  1.0 / (8.000000 / controlClockTickMs), // 8
  1.0 / (9.000000 / controlClockTickMs), // 9
  1.0 / (10.000000 / controlClockTickMs), // 10
  1.0 / (11.000000 / controlClockTickMs), // 11
  1.0 / (12.000000 / controlClockTickMs), // 12
  1.0 / (13.000000 / controlClockTickMs), // 13
  1.0 / (14.000000 / controlClockTickMs), // 14
  1.0 / (15.000000 / controlClockTickMs), // 15
  1.0 / (17.000000 / controlClockTickMs), // 16
  1.0 / (18.000000 / controlClockTickMs), // 17
  1.0 / (19.000000 / controlClockTickMs), // 18
  1.0 / (20.000000 / controlClockTickMs), // 19
  1.0 / (22.000000 / controlClockTickMs), // 20
  1.0 / (23.000000 / controlClockTickMs), // 21
  1.0 / (25.000000 / controlClockTickMs), // 22
  1.0 / (27.000000 / controlClockTickMs), // 23
  1.0 / (29.000000 / controlClockTickMs), // 24
  1.0 / (30.000000 / controlClockTickMs), // 25
  1.0 / (32.000000 / controlClockTickMs), // 26
  1.0 / (35.000000 / controlClockTickMs), // 27
  1.0 / (37.000000 / controlClockTickMs), // 28
  1.0 / (39.000000 / controlClockTickMs), // 29
  1.0 / (42.000000 / controlClockTickMs), // 30
  1.0 / (45.000000 / controlClockTickMs), // 31
  1.0 / (48.000000 / controlClockTickMs), // 32
  1.0 / (51.000000 / controlClockTickMs), // 33
  1.0 / (54.000000 / controlClockTickMs), // 34
  1.0 / (57.000000 / controlClockTickMs), // 35
  1.0 / (61.000000 / controlClockTickMs), // 36
  1.0 / (65.000000 / controlClockTickMs), // 37
  1.0 / (69.000000 / controlClockTickMs), // 38
  1.0 / (74.000000 / controlClockTickMs), // 39
  1.0 / (79.000000 / controlClockTickMs), // 40
  1.0 / (84.000000 / controlClockTickMs), // 41
  1.0 / (89.000000 / controlClockTickMs), // 42
  1.0 / (95.000000 / controlClockTickMs), // 43
  1.0 / (101.000000 / controlClockTickMs), // 44
  1.0 / (107.000000 / controlClockTickMs), // 45
  1.0 / (114.000000 / controlClockTickMs), // 46
  1.0 / (121.000000 / controlClockTickMs), // 47
  1.0 / (129.000000 / controlClockTickMs), // 48
  1.0 / (136.000000 / controlClockTickMs), // 49
  1.0 / (145.000000 / controlClockTickMs), // 50
  1.0 / (154.000000 / controlClockTickMs), // 51
  1.0 / (163.000000 / controlClockTickMs), // 52
  1.0 / (173.000000 / controlClockTickMs), // 53
  1.0 / (183.000000 / controlClockTickMs), // 54
  1.0 / (194.000000 / controlClockTickMs), // 55
  1.0 / (206.000000 / controlClockTickMs), // 56
  1.0 / (218.000000 / controlClockTickMs), // 57
  1.0 / (230.000000 / controlClockTickMs), // 58
  1.0 / (243.000000 / controlClockTickMs), // 59
  1.0 / (257.000000 / controlClockTickMs), // 60
  1.0 / (272.000000 / controlClockTickMs), // 61
  1.0 / (287.000000 / controlClockTickMs), // 62
  1.0 / (303.000000 / controlClockTickMs), // 63
  1.0 / (320.000000 / controlClockTickMs), // 64
  1.0 / (337.000000 / controlClockTickMs), // 65
  1.0 / (355.000000 / controlClockTickMs), // 66
  1.0 / (374.000000 / controlClockTickMs), // 67
  1.0 / (394.000000 / controlClockTickMs), // 68
  1.0 / (414.000000 / controlClockTickMs), // 69
  1.0 / (436.000000 / controlClockTickMs), // 70
  1.0 / (458.000000 / controlClockTickMs), // 71
  1.0 / (482.000000 / controlClockTickMs), // 72
  1.0 / (506.000000 / controlClockTickMs), // 73
  1.0 / (531.000000 / controlClockTickMs), // 74
  1.0 / (557.000000 / controlClockTickMs), // 75
  1.0 / (585.000000 / controlClockTickMs), // 76
  1.0 / (613.000000 / controlClockTickMs), // 77
  1.0 / (642.000000 / controlClockTickMs), // 78
  1.0 / (673.000000 / controlClockTickMs), // 79
  1.0 / (705.000000 / controlClockTickMs), // 80
  1.0 / (737.000000 / controlClockTickMs), // 81
  1.0 / (771.000000 / controlClockTickMs), // 82
  1.0 / (807.000000 / controlClockTickMs), // 83
  1.0 / (843.000000 / controlClockTickMs), // 84
  1.0 / (881.000000 / controlClockTickMs), // 85
  1.0 / (920.000000 / controlClockTickMs), // 86
  1.0 / (961.000000 / controlClockTickMs), // 87
  1.0 / (1003.000000 / controlClockTickMs), // 88
  1.0 / (1046.000000 / controlClockTickMs), // 89
  1.0 / (1091.000000 / controlClockTickMs), // 90
  1.0 / (1137.000000 / controlClockTickMs), // 91
  1.0 / (1185.000000 / controlClockTickMs), // 92
  1.0 / (1234.000000 / controlClockTickMs), // 93
  1.0 / (1285.000000 / controlClockTickMs), // 94
  1.0 / (1337.000000 / controlClockTickMs), // 95
  1.0 / (1392.000000 / controlClockTickMs), // 96
  1.0 / (1447.000000 / controlClockTickMs), // 97
  1.0 / (1505.000000 / controlClockTickMs), // 98
  1.0 / (1564.000000 / controlClockTickMs), // 99
  1.0 / (1625.000000 / controlClockTickMs), // 100
  1.0 / (1688.000000 / controlClockTickMs), // 101
  1.0 / (1753.000000 / controlClockTickMs), // 102
  1.0 / (1820.000000 / controlClockTickMs), // 103
  1.0 / (1889.000000 / controlClockTickMs), // 104
  1.0 / (1959.000000 / controlClockTickMs), // 105
  1.0 / (2032.000000 / controlClockTickMs), // 106
  1.0 / (2107.000000 / controlClockTickMs), // 107
  1.0 / (2183.000000 / controlClockTickMs), // 108
  1.0 / (2262.000000 / controlClockTickMs), // 109
  1.0 / (2344.000000 / controlClockTickMs), // 110
  1.0 / (2427.000000 / controlClockTickMs), // 111
  1.0 / (2513.000000 / controlClockTickMs), // 112
  1.0 / (2600.000000 / controlClockTickMs), // 113
  1.0 / (2691.000000 / controlClockTickMs), // 114
  1.0 / (2783.000000 / controlClockTickMs), // 115
  1.0 / (2878.000000 / controlClockTickMs), // 116
  1.0 / (2976.000000 / controlClockTickMs), // 117
  1.0 / (3076.000000 / controlClockTickMs), // 118
  1.0 / (3178.000000 / controlClockTickMs), // 119
  1.0 / (3284.000000 / controlClockTickMs), // 120
  1.0 / (3391.000000 / controlClockTickMs), // 121
  1.0 / (3502.000000 / controlClockTickMs), // 122
  1.0 / (3615.000000 / controlClockTickMs), // 123
  1.0 / (3731.000000 / controlClockTickMs), // 124
  1.0 / (3850.000000 / controlClockTickMs), // 125
  1.0 / (3971.000000 / controlClockTickMs), // 126
  1.0 / (4096.000000 / controlClockTickMs), // 127
};

AdsrEnvelope::AdsrEnvelope() {
  Reset(0, 0, maxMidiVal, 0);
}

void AdsrEnvelope::Reset(uint8_t attack, uint8_t decay, uint8_t sustain, uint8_t release) {
  _attack = AdsrCurveMs[attack];
  _attackInc = AdsrCurveLevelStep[attack];
  _decay = AdsrCurveMs[decay];
  _decayDec = 0;
  _sustain = midiValMap[sustain];
  _release = AdsrCurveMs[release];
  _releaseDec = AdsrCurveLevelStep[release];
  if (attack == 0 && decay == 0 && sustain == maxMidiVal && release == 0) {
    isNull = true;
  } else {
    isNull = false;
    // TODO: optimize this case - decay not often used.
    if (decay && sustain < maxMidiVal) {
      _decayDec = (1.0 - _sustain) * (1.0 / _decay) * controlClockTickMs;
    }
  }
  level = 0;
  _ageMs = 0;
  SetInitialPhase();
}

void AdsrEnvelope::SetInitialPhase() {
  if (_attack > 0) {
    ChangePhase(ENV_ATTACK);
  } else if (_decay > 0) {
    Decay();
  } else {
    Sustain();
  }
}

bool AdsrEnvelope::IsIdle() {
  return _phase == ENV_IDLE;
}

void AdsrEnvelope::Decay() {
  level = 1.0;
  ChangePhase(ENV_DECAY);
}

void AdsrEnvelope::Sustain() {
  level = _sustain;
  ChangePhase(ENV_SUSTAIN);
}

void AdsrEnvelope::Idle() {
  if (_phase < ENV_IDLE) {
    level = 0;
    ChangePhase(ENV_IDLE);
  }
}

void AdsrEnvelope::Release() {
  if (_phase < ENV_RELEASE) {
    ChangePhase(ENV_RELEASE);
  }
}

void AdsrEnvelope::HandleControl() {
  switch (_phase) {
    case ENV_ATTACK: {
        level += _attackInc;
        _ageMs += controlClockTickMs;
        if (level >= 1.0 || _ageMs > _attack) {
          Decay();
        }
      }
      break;
    case ENV_DECAY: {
        level -= _decayDec;
        _ageMs += controlClockTickMs;
        if (level <= _sustain || _ageMs > _decay) {
          Sustain();
        }
      }
      break;
    case ENV_RELEASE: {
        level -= _releaseDec;
        _ageMs += controlClockTickMs;
        if (level <= 0 || _ageMs > _release) {
          Idle();
        }
      }
      break;
    case ENV_SUSTAIN:
    case ENV_IDLE:
    default:
      break;
  }
}

void AdsrEnvelope::ChangePhase(uint8_t newPhase) {
  _ageMs = 0;
  _phase = newPhase;
}
