// Copyright 2019 Josh Bailey (josh@vandervecken.com)

// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


#include "AdsrEnvelope.h"
#include "constants.h"


// The two lookup tables below are compile-time constants: every entry is folded
// at build time, so the float/double arithmetic in their initializers is NOT a
// runtime floating-point op (which would be software-emulated on the Due's
// FPU-less Cortex-M3). Exempt the tables from the soft-float regression gate
// (test/host/softfloat_guard.sh) without changing any value.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdouble-promotion"
#pragma GCC diagnostic ignored "-Wfloat-conversion"

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

const cr_env_t AdsrCurveLevelStep[] = {
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

// controlClockTickMs=0.190006  K=ln(1000)=6.907755
const cr_env_t AdsrCurveExpMul[] = {
  0.0000000000, // 0  (T=0ms)
  0.2691428826, // 1  (T=1ms)
  0.5187898251, // 2  (T=2ms)
  0.6456457548, // 3  (T=3ms)
  0.7202706610, // 4  (T=4ms)
  0.7691243833, // 5  (T=5ms)
  0.8035208490, // 6  (T=6ms)
  0.8290275930, // 7  (T=7ms)
  0.8486876109, // 8  (T=8ms)
  0.8643005047, // 9  (T=9ms)
  0.8769973679, // 10  (T=10ms)
  0.8875243223, // 11  (T=11ms)
  0.8963932446, // 12  (T=12ms)
  0.9039669047, // 13  (T=13ms)
  0.9105095238, // 14  (T=14ms)
  0.9162180850, // 15  (T=15ms)
  0.9256985476, // 16  (T=17ms)
  0.9296776348, // 17  (T=18ms)
  0.9332523667, // 18  (T=19ms)
  0.9364813762, // 19  (T=20ms)
  0.9420850929, // 20  (T=22ms)
  0.9445319385, // 21  (T=23ms)
  0.9488538323, // 22  (T=25ms)
  0.9525510400, // 23  (T=27ms)
  0.9557498486, // 24  (T=29ms)
  0.9571928150, // 25  (T=30ms)
  0.9598137465, // 26  (T=32ms)
  0.9631940633, // 27  (T=35ms)
  0.9651484830, // 28  (T=37ms)
  0.9669058257, // 29  (T=39ms)
  0.9692329373, // 30  (T=42ms)
  0.9712542973, // 31  (T=45ms)
  0.9730264450, // 32  (T=48ms)
  0.9745927899, // 33  (T=51ms)
  0.9759872130, // 34  (T=54ms)
  0.9772365459, // 35  (T=57ms)
  0.9787132262, // 36  (T=61ms)
  0.9800099975, // 37  (T=65ms)
  0.9811578494, // 38  (T=69ms)
  0.9824197082, // 39  (T=74ms)
  0.9835231657, // 40  (T=79ms)
  0.9844962861, // 41  (T=84ms)
  0.9853608737, // 42  (T=89ms)
  0.9862790768, // 43  (T=95ms)
  0.9870888957, // 44  (T=101ms)
  0.9878084510, // 45  (T=107ms)
  0.9885527532, // 46  (T=114ms)
  0.9892114050, // 47  (T=121ms)
  0.9898770669, // 48  (T=129ms)
  0.9903955898, // 49  (T=136ms)
  0.9909890314, // 50  (T=145ms)
  0.9915134053, // 51  (T=154ms)
  0.9919801061, // 52  (T=163ms)
  0.9924419273, // 53  (T=173ms)
  0.9928534573, // 54  (T=183ms)
  0.9932573044, // 55  (T=194ms)
  0.9936488327, // 56  (T=206ms)
  0.9939973868, // 57  (T=218ms)
  0.9943096739, // 58  (T=230ms)
  0.9946132732, // 59  (T=243ms)
  0.9949059648, // 60  (T=257ms)
  0.9951862086, // 61  (T=272ms)
  0.9954372256, // 62  (T=287ms)
  0.9956776427, // 63  (T=303ms)
  0.9959067973, // 64  (T=320ms)
  0.9961128775, // 65  (T=337ms)
  0.9963096069, // 66  (T=355ms)
  0.9964967580, // 67  (T=374ms)
  0.9966742915, // 68  (T=394ms)
  0.9968346992, // 69  (T=414ms)
  0.9969941759, // 70  (T=436ms)
  0.9971383537, // 71  (T=458ms)
  0.9972806484, // 72  (T=482ms)
  0.9974094623, // 73  (T=506ms)
  0.9975312767, // 74  (T=531ms)
  0.9976463776, // 75  (T=557ms)
  0.9977589033, // 76  (T=585ms)
  0.9978611603, // 77  (T=613ms)
  0.9979576758, // 78  (T=642ms)
  0.9980516585, // 79  (T=673ms)
  0.9981400115, // 80  (T=705ms)
  0.9982206990, // 81  (T=737ms)
  0.9982990968, // 82  (T=771ms)
  0.9983749119, // 83  (T=807ms)
  0.9984442566, // 84  (T=843ms)
  0.9985113102, // 85  (T=881ms)
  0.9985743727, // 86  (T=920ms)
  0.9986351540, // 87  (T=961ms)
  0.9986922687, // 88  (T=1003ms)
  0.9987459945, // 89  (T=1046ms)
  0.9987976868, // 90  (T=1091ms)
  0.9988463011, // 91  (T=1137ms)
  0.9988930074, // 92  (T=1185ms)
  0.9989369407, // 93  (T=1234ms)
  0.9989791107, // 94  (T=1285ms)
  0.9990187967, // 95  (T=1337ms)
  0.9990575472, // 96  (T=1392ms)
  0.9990933533, // 97  (T=1447ms)
  0.9991282786, // 98  (T=1505ms)
  0.9991611494, // 99  (T=1564ms)
  0.9991926259, // 100  (T=1625ms)
  0.9992227472, // 101  (T=1688ms)
  0.9992515564, // 102  (T=1753ms)
  0.9992790991, // 103  (T=1820ms)
  0.9993054224, // 104  (T=1889ms)
  0.9993302331, // 105  (T=1959ms)
  0.9993542869, // 106  (T=2032ms)
  0.9993772643, // 107  (T=2107ms)
  0.9993989380, // 108  (T=2183ms)
  0.9994199239, // 109  (T=2262ms)
  0.9994402110, // 110  (T=2344ms)
  0.9994593498, // 111  (T=2427ms)
  0.9994778471, // 112  (T=2513ms)
  0.9994953148, // 113  (T=2600ms)
  0.9995123772, // 114  (T=2691ms)
  0.9995284932, // 115  (T=2783ms)
  0.9995440536, // 116  (T=2878ms)
  0.9995590647, // 117  (T=2976ms)
  0.9995733963, // 118  (T=3076ms)
  0.9995870856, // 119  (T=3178ms)
  0.9996004109, // 120  (T=3284ms)
  0.9996130171, // 121  (T=3391ms)
  0.9996252807, // 122  (T=3502ms)
  0.9996369918, // 123  (T=3615ms)
  0.9996482761, // 124  (T=3731ms)
  0.9996591457, // 125  (T=3850ms)
  0.9996695301, // 126  (T=3971ms)
  0.9996796136, // 127  (T=4096ms)
};
#pragma GCC diagnostic pop

AdsrEnvelope::AdsrEnvelope() {
  Reset(0, 0, maxMidiVal, 0);
}

void AdsrEnvelope::Reset(uint8_t attack, uint8_t decay, uint8_t sustain, uint8_t release) {
  _attack = AdsrCurveMs[attack];
  _attackInc = AdsrCurveLevelStep[attack];
  _decay = AdsrCurveMs[decay];
  // Decay and release are exponential (a struck bell rings down fast at first,
  // then a long faint tail), so they multiply the level by a per-tick factor
  // (AdsrCurveExpMul) rather than subtracting a fixed step. Attack stays linear.
  _decayMul = AdsrCurveExpMul[decay];
  _sustain = cr_env_t(midiValMap[sustain]);
  _release = AdsrCurveMs[release];
  _releaseMul = AdsrCurveExpMul[release];
  isNull = (attack == 0 && decay == 0 && sustain == maxMidiVal && release == 0);
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
  level = cr_env_t(1);
  ChangePhase(ENV_DECAY);
}

void AdsrEnvelope::Sustain() {
  // Sustain 0 means "die after decay": idle now instead of holding at level 0, so
  // the voice is freed and stops firing the coil's breakout-time minimum pulse.
  // Otherwise a long-held note whose amplitude has already decayed to zero (e.g. a
  // struck FM bell -- long decay, zero sustain) keeps emitting a faint breakout-
  // floor tone until note-off. A non-zero sustain holds as before.
  if (_sustain <= cr_env_t(0)) {
    Idle();
    return;
  }
  level = _sustain;
  ChangePhase(ENV_SUSTAIN);
}

void AdsrEnvelope::Idle() {
  if (_phase < ENV_IDLE) {
    level = cr_env_t(0);
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
    case ENV_SUSTAIN:
        break;
    case ENV_IDLE:
        break;
    case ENV_ATTACK: {
        level += _attackInc;
        _ageMs += controlClockTickMsFp;
        if (level >= cr_env_t(1) || _ageMs > _attack) {
          Decay();
        }
      }
      break;
    case ENV_DECAY: {
        // Exponential approach to the sustain level: level decays by a constant
        // FRACTION each tick, so it falls fast at first then tails off -- a bell-
        // like ring. (level - sustain) is the part that decays; sustain is the
        // floor it settles to.
        level = _sustain + cr_env_t((level - _sustain) * _decayMul);
        _ageMs += controlClockTickMsFp;
        if (level <= _sustain || _ageMs > _decay) {
          Sustain();
        }
      }
      break;
    case ENV_RELEASE: {
        // Exponential ring-down to silence (same shape as decay, floor 0).
        level = cr_env_t(level * _releaseMul);
        _ageMs += controlClockTickMsFp;
        if (level <= cr_env_t(0) || _ageMs > _release) {
          Idle();
        }
      }
      break;
    default:
      break;
  }
}

void AdsrEnvelope::ChangePhase(uint8_t newPhase) {
  _ageMs = 0;
  _phase = newPhase;
}
