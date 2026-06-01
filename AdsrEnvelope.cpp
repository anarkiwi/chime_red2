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

// Exponential decay/release per-tick multipliers, regenerated for a
// 3x-longer ring: r[i] = exp(-ln(1000)*controlClockTickMs/(AdsrCurveMs[i]*3)),
// i.e. the level reaches -60 dB over envRingScale*decayMs, not decayMs. The
// age cap (_decay/_release) is scaled by the same envRingScale in Reset(), so
// the phase still ends right at the -60 dB floor (no snap).
// controlClockTickMs=0.190006  K=ln(1000)=6.907755  envRingScale=3
const cr_env_t AdsrCurveExpMul[] = {
  0.0000000000, // 0  (ring T=0ms)
  0.6456457548, // 1  (ring T=3ms)
  0.8035208490, // 2  (ring T=6ms)
  0.8643005047, // 3  (ring T=9ms)
  0.8963932446, // 4  (ring T=12ms)
  0.9162180850, // 5  (ring T=15ms)
  0.9296776348, // 6  (ring T=18ms)
  0.9394124867, // 7  (ring T=21ms)
  0.9467804627, // 8  (ring T=24ms)
  0.9525510400, // 9  (ring T=27ms)
  0.9571928150, // 10  (ring T=30ms)
  0.9610074502, // 11  (ring T=33ms)
  0.9641979231, // 12  (ring T=36ms)
  0.9669058257, // 13  (ring T=39ms)
  0.9692329373, // 14  (ring T=42ms)
  0.9712542973, // 15  (ring T=45ms)
  0.9745927899, // 16  (ring T=51ms)
  0.9759872130, // 17  (ring T=54ms)
  0.9772365459, // 18  (ring T=57ms)
  0.9783623127, // 19  (ring T=60ms)
  0.9803098746, // 20  (ring T=66ms)
  0.9811578494, // 21  (ring T=69ms)
  0.9826520669, // 22  (ring T=75ms)
  0.9839267137, // 23  (ring T=81ms)
  0.9850268740, // 24  (ring T=87ms)
  0.9855223474, // 25  (ring T=90ms)
  0.9864210283, // 26  (ring T=96ms)
  0.9875776794, // 27  (ring T=105ms)
  0.9882451936, // 28  (ring T=111ms)
  0.9888446290, // 29  (ring T=117ms)
  0.9896372978, // 30  (ring T=126ms)
  0.9903247913, // 31  (ring T=135ms)
  0.9909267399, // 32  (ring T=144ms)
  0.9914581749, // 33  (ring T=153ms)
  0.9919308008, // 34  (ring T=162ms)
  0.9923538676, // 35  (ring T=171ms)
  0.9928534573, // 36  (ring T=183ms)
  0.9932917661, // 37  (ring T=195ms)
  0.9936794175, // 38  (ring T=207ms)
  0.9941052226, // 39  (ring T=222ms)
  0.9944772776, // 40  (ring T=237ms)
  0.9948051557, // 41  (ring T=252ms)
  0.9950962841, // 42  (ring T=267ms)
  0.9954052798, // 43  (ring T=285ms)
  0.9956776427, // 44  (ring T=303ms)
  0.9959195227, // 45  (ring T=321ms)
  0.9961695978, // 46  (ring T=342ms)
  0.9963907910, // 47  (ring T=363ms)
  0.9966142385, // 48  (ring T=387ms)
  0.9967882255, // 49  (ring T=408ms)
  0.9969872764, // 50  (ring T=435ms)
  0.9971630947, // 51  (ring T=462ms)
  0.9973195235, // 52  (ring T=489ms)
  0.9974742685, // 53  (ring T=519ms)
  0.9976121217, // 54  (ring T=549ms)
  0.9977473642, // 55  (ring T=582ms)
  0.9978784464, // 56  (ring T=618ms)
  0.9979951121, // 57  (ring T=654ms)
  0.9980996154, // 58  (ring T=690ms)
  0.9982011906, // 59  (ring T=729ms)
  0.9982990968, // 60  (ring T=771ms)
  0.9983928212, // 61  (ring T=816ms)
  0.9984767561, // 62  (ring T=861ms)
  0.9985571334, // 63  (ring T=909ms)
  0.9986337333, // 64  (ring T=960ms)
  0.9987026100, // 65  (ring T=1011ms)
  0.9987683526, // 66  (ring T=1065ms)
  0.9988308864, // 67  (ring T=1122ms)
  0.9988901993, // 68  (ring T=1182ms)
  0.9989437845, // 69  (ring T=1242ms)
  0.9989970531, // 70  (ring T=1308ms)
  0.9990452066, // 71  (ring T=1374ms)
  0.9990927266, // 72  (ring T=1446ms)
  0.9991357407, // 73  (ring T=1518ms)
  0.9991764141, // 74  (ring T=1593ms)
  0.9992148429, // 75  (ring T=1671ms)
  0.9992524090, // 76  (ring T=1755ms)
  0.9992865445, // 77  (ring T=1839ms)
  0.9993187613, // 78  (ring T=1926ms)
  0.9993501306, // 79  (ring T=2019ms)
  0.9993796191, // 80  (ring T=2115ms)
  0.9994065475, // 81  (ring T=2211ms)
  0.9994327105, // 82  (ring T=2313ms)
  0.9994580103, // 83  (ring T=2421ms)
  0.9994811497, // 84  (ring T=2529ms)
  0.9995035236, // 85  (ring T=2643ms)
  0.9995245649, // 86  (ring T=2760ms)
  0.9995448442, // 87  (ring T=2883ms)
  0.9995638994, // 88  (ring T=3009ms)
  0.9995818233, // 89  (ring T=3138ms)
  0.9995990682, // 90  (ring T=3273ms)
  0.9996152857, // 91  (ring T=3411ms)
  0.9996308662, // 92  (ring T=3555ms)
  0.9996455213, // 93  (ring T=3702ms)
  0.9996595877, // 94  (ring T=3855ms)
  0.9996728252, // 95  (ring T=4011ms)
  0.9996857503, // 96  (ring T=4176ms)
  0.9996976930, // 97  (ring T=4341ms)
  0.9997093417, // 98  (ring T=4515ms)
  0.9997203049, // 99  (ring T=4692ms)
  0.9997308028, // 100  (ring T=4875ms)
  0.9997408486, // 101  (ring T=5064ms)
  0.9997504565, // 102  (ring T=5259ms)
  0.9997596419, // 103  (ring T=5460ms)
  0.9997684205, // 104  (ring T=5667ms)
  0.9997766945, // 105  (ring T=5877ms)
  0.9997847159, // 106  (ring T=6096ms)
  0.9997923783, // 107  (ring T=6321ms)
  0.9997996058, // 108  (ring T=6549ms)
  0.9998066039, // 109  (ring T=6786ms)
  0.9998133688, // 110  (ring T=7032ms)
  0.9998197508, // 111  (ring T=7281ms)
  0.9998259187, // 112  (ring T=7539ms)
  0.9998317433, // 113  (ring T=7800ms)
  0.9998374327, // 114  (ring T=8073ms)
  0.9998428064, // 115  (ring T=8349ms)
  0.9998479948, // 116  (ring T=8634ms)
  0.9998530000, // 117  (ring T=8928ms)
  0.9998577785, // 118  (ring T=9228ms)
  0.9998623429, // 119  (ring T=9534ms)
  0.9998667859, // 120  (ring T=9852ms)
  0.9998709891, // 121  (ring T=10173ms)
  0.9998750780, // 122  (ring T=10506ms)
  0.9998789826, // 123  (ring T=10845ms)
  0.9998827449, // 124  (ring T=11193ms)
  0.9998863690, // 125  (ring T=11550ms)
  0.9998898312, // 126  (ring T=11913ms)
  0.9998931931, // 127  (ring T=12288ms)
};
#pragma GCC diagnostic pop

AdsrEnvelope::AdsrEnvelope() {
  Reset(0, 0, maxMidiVal, 0);
}

void AdsrEnvelope::Reset(uint8_t attack, uint8_t decay, uint8_t sustain, uint8_t release) {
  _attack = AdsrCurveMs[attack];
  _attackInc = AdsrCurveLevelStep[attack];
  _sustain = cr_env_t(midiValMap[sustain]);
  if (_linear) {
    // Amp uses linear decay/release: subtract a fixed step per tick so the
    // amplitude rolls down on a straight line and lands exactly on the sustain
    // (decay) / silence (release) floor at AdsrCurveMs ms. No -60 dB tail to
    // stretch, so envRingScale does not apply.
    _decay = AdsrCurveMs[decay];
    _decayCoef = cr_env_t(0);
    if (decay && sustain < maxMidiVal) {
      // (1/_decay)*controlClockTickMs == AdsrCurveLevelStep[decay], scaled by
      // the (1 - sustain) drop, so reuse the precomputed reciprocal instead of
      // a runtime divide on the no-FPU target.
      _decayCoef = (cr_env_t(1) - _sustain) * AdsrCurveLevelStep[decay];
    }
    _release = AdsrCurveMs[release];
    _releaseCoef = AdsrCurveLevelStep[release];
  } else {
    // FM mod-index envelope: exponential decay/release (a struck bell rings
    // down fast at first, then a long faint tail). Multiplies the level by a
    // per-tick factor (AdsrCurveExpMul). The age cap is scaled by envRingScale
    // to match AdsrCurveExpMul (generated with the same factor) so the phase
    // ends right at the -60 dB floor, not early.
    _decay = AdsrCurveMs[decay] * envRingScale;
    _decayCoef = AdsrCurveExpMul[decay];
    _release = AdsrCurveMs[release] * envRingScale;
    _releaseCoef = AdsrCurveExpMul[release];
  }
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
        if (_linear) {
          // Straight-line drop from 1.0 toward _sustain over AdsrCurveMs[decay] ms.
          level -= _decayCoef;
        } else {
          // Exponential approach to the sustain level: (level - sustain) decays
          // by a constant FRACTION each tick -- fast initial drop, long faint
          // tail, bell-like.
          level = _sustain + cr_env_t((level - _sustain) * _decayCoef);
        }
        _ageMs += controlClockTickMsFp;
        if (level <= _sustain || _ageMs > _decay) {
          Sustain();
        }
      }
      break;
    case ENV_RELEASE: {
        if (_linear) {
          // Straight-line drop from the current level to silence; reaches 0 in
          // (current_level * AdsrCurveMs[release]) ms (full ms from level=1.0).
          level -= _releaseCoef;
        } else {
          // Exponential ring-down to silence (same shape as decay, floor 0).
          level = cr_env_t(level * _releaseCoef);
        }
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
