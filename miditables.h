// Copyright 2019 Josh Bailey (josh@vandervecken.com)

// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


#ifndef MIDICONSTANTS_H
#define MIDICONSTANTS_H

const cr_fp_t pitchToHz[] = {
  8.1757989156, // MIDI note 0 C
  8.6619572180, // MIDI note 1 Db
  9.1770239974, // MIDI note 2 D
  9.7227182413, // MIDI note 3 Eb
  10.3008611535, // MIDI note 4 E
  10.9133822323, // MIDI note 5 F
  11.5623257097, // MIDI note 6 Gb
  12.2498573744, // MIDI note 7 G
  12.9782717994, // MIDI note 8 Ab
  13.7500000000, // MIDI note 9 A
  14.5676175474, // MIDI note 10 Bb
  15.4338531643, // MIDI note 11 B
  16.3515978313, // MIDI note 12 C
  17.3239144361, // MIDI note 13 Db
  18.3540479948, // MIDI note 14 D
  19.4454364826, // MIDI note 15 Eb
  20.6017223071, // MIDI note 16 E
  21.8267644646, // MIDI note 17 F
  23.1246514195, // MIDI note 18 Gb
  24.4997147489, // MIDI note 19 G
  25.9565435987, // MIDI note 20 Ab
  27.5000000000, // MIDI note 21 A
  29.1352350949, // MIDI note 22 Bb
  30.8677063285, // MIDI note 23 B
  32.7031956626, // MIDI note 24 C
  34.6478288721, // MIDI note 25 Db
  36.7080959897, // MIDI note 26 D
  38.8908729653, // MIDI note 27 Eb
  41.2034446141, // MIDI note 28 E
  43.6535289291, // MIDI note 29 F
  46.2493028390, // MIDI note 30 Gb
  48.9994294977, // MIDI note 31 G
  51.9130871975, // MIDI note 32 Ab
  55.0000000000, // MIDI note 33 A
  58.2704701898, // MIDI note 34 Bb
  61.7354126570, // MIDI note 35 B
  65.4063913251, // MIDI note 36 C
  69.2956577442, // MIDI note 37 Db
  73.4161919794, // MIDI note 38 D
  77.7817459305, // MIDI note 39 Eb
  82.4068892282, // MIDI note 40 E
  87.3070578583, // MIDI note 41 F
  92.4986056779, // MIDI note 42 Gb
  97.9988589954, // MIDI note 43 G
  103.8261743950, // MIDI note 44 Ab
  110.0000000000, // MIDI note 45 A
  116.5409403795, // MIDI note 46 Bb
  123.4708253140, // MIDI note 47 B
  130.8127826503, // MIDI note 48 C
  138.5913154884, // MIDI note 49 Db
  146.8323839587, // MIDI note 50 D
  155.5634918610, // MIDI note 51 Eb
  164.8137784564, // MIDI note 52 E
  174.6141157165, // MIDI note 53 F
  184.9972113558, // MIDI note 54 Gb
  195.9977179909, // MIDI note 55 G
  207.6523487900, // MIDI note 56 Ab
  220.0000000000, // MIDI note 57 A
  233.0818807590, // MIDI note 58 Bb
  246.9416506281, // MIDI note 59 B
  261.6255653006, // MIDI note 60 C
  277.1826309769, // MIDI note 61 Db
  293.6647679174, // MIDI note 62 D
  311.1269837221, // MIDI note 63 Eb
  329.6275569129, // MIDI note 64 E
  349.2282314330, // MIDI note 65 F
  369.9944227116, // MIDI note 66 Gb
  391.9954359817, // MIDI note 67 G
  415.3046975799, // MIDI note 68 Ab
  440.0000000000, // MIDI note 69 A
  466.1637615181, // MIDI note 70 Bb
  493.8833012561, // MIDI note 71 B
  523.2511306012, // MIDI note 72 C
  554.3652619537, // MIDI note 73 Db
  587.3295358348, // MIDI note 74 D
  622.2539674442, // MIDI note 75 Eb
  659.2551138257, // MIDI note 76 E
  698.4564628660, // MIDI note 77 F
  739.9888454233, // MIDI note 78 Gb
  783.9908719635, // MIDI note 79 G
  830.6093951599, // MIDI note 80 Ab
  880.0000000000, // MIDI note 81 A
  932.3275230362, // MIDI note 82 Bb
  987.7666025122, // MIDI note 83 B
  1046.5022612024, // MIDI note 84 C
  1108.7305239075, // MIDI note 85 Db
  1174.6590716696, // MIDI note 86 D
  1244.5079348883, // MIDI note 87 Eb
  1318.5102276515, // MIDI note 88 E
  1396.9129257320, // MIDI note 89 F
  1479.9776908465, // MIDI note 90 Gb
  1567.9817439270, // MIDI note 91 G
  1661.2187903198, // MIDI note 92 Ab
  1760.0000000000, // MIDI note 93 A
  1864.6550460724, // MIDI note 94 Bb
  1975.5332050245, // MIDI note 95 B
  2093.0045224048, // MIDI note 96 C
  2217.4610478150, // MIDI note 97 Db
  2349.3181433393, // MIDI note 98 D
  2489.0158697766, // MIDI note 99 Eb
  2637.0204553030, // MIDI note 100 E
  2793.8258514640, // MIDI note 101 F
  2959.9553816931, // MIDI note 102 Gb
  3135.9634878540, // MIDI note 103 G
  3322.4375806396, // MIDI note 104 Ab
  3520.0000000000, // MIDI note 105 A
  3729.3100921447, // MIDI note 106 Bb
  3951.0664100490, // MIDI note 107 B
  4186.0090448096, // MIDI note 108 C
  4434.9220956300, // MIDI note 109 Db
  4698.6362866785, // MIDI note 110 D
  4978.0317395533, // MIDI note 111 Eb
  5274.0409106059, // MIDI note 112 E
  5587.6517029281, // MIDI note 113 F
  5919.9107633862, // MIDI note 114 Gb
  6271.9269757080, // MIDI note 115 G
  6644.8751612791, // MIDI note 116 Ab
  7040.0000000000, // MIDI note 117 A
  7458.6201842894, // MIDI note 118 Bb
  7902.1328200980, // MIDI note 119 B
  8372.0180896192, // MIDI note 120 C
  8869.8441912599, // MIDI note 121 Db
  9397.2725733570, // MIDI note 122 D
  9956.0634791066, // MIDI note 123 Eb
  10548.0818212118, // MIDI note 124 E
  11175.3034058561, // MIDI note 125 F
  11839.8215267723, // MIDI note 126 Gb
  12543.8539514160, // MIDI note 127 G
};

//print('const cr_fp_t midiValMap[] = {')
//for i in range(128):
//  print('  %f, // %u' % (float(i) / 127, i))
//print('};')
const cr_fp_t midiValMap[] = {
  0.000000, // 0
  0.007874, // 1
  0.015748, // 2
  0.023622, // 3
  0.031496, // 4
  0.039370, // 5
  0.047244, // 6
  0.055118, // 7
  0.062992, // 8
  0.070866, // 9
  0.078740, // 10
  0.086614, // 11
  0.094488, // 12
  0.102362, // 13
  0.110236, // 14
  0.118110, // 15
  0.125984, // 16
  0.133858, // 17
  0.141732, // 18
  0.149606, // 19
  0.157480, // 20
  0.165354, // 21
  0.173228, // 22
  0.181102, // 23
  0.188976, // 24
  0.196850, // 25
  0.204724, // 26
  0.212598, // 27
  0.220472, // 28
  0.228346, // 29
  0.236220, // 30
  0.244094, // 31
  0.251969, // 32
  0.259843, // 33
  0.267717, // 34
  0.275591, // 35
  0.283465, // 36
  0.291339, // 37
  0.299213, // 38
  0.307087, // 39
  0.314961, // 40
  0.322835, // 41
  0.330709, // 42
  0.338583, // 43
  0.346457, // 44
  0.354331, // 45
  0.362205, // 46
  0.370079, // 47
  0.377953, // 48
  0.385827, // 49
  0.393701, // 50
  0.401575, // 51
  0.409449, // 52
  0.417323, // 53
  0.425197, // 54
  0.433071, // 55
  0.440945, // 56
  0.448819, // 57
  0.456693, // 58
  0.464567, // 59
  0.472441, // 60
  0.480315, // 61
  0.488189, // 62
  0.496063, // 63
  0.503937, // 64
  0.511811, // 65
  0.519685, // 66
  0.527559, // 67
  0.535433, // 68
  0.543307, // 69
  0.551181, // 70
  0.559055, // 71
  0.566929, // 72
  0.574803, // 73
  0.582677, // 74
  0.590551, // 75
  0.598425, // 76
  0.606299, // 77
  0.614173, // 78
  0.622047, // 79
  0.629921, // 80
  0.637795, // 81
  0.645669, // 82
  0.653543, // 83
  0.661417, // 84
  0.669291, // 85
  0.677165, // 86
  0.685039, // 87
  0.692913, // 88
  0.700787, // 89
  0.708661, // 90
  0.716535, // 91
  0.724409, // 92
  0.732283, // 93
  0.740157, // 94
  0.748031, // 95
  0.755906, // 96
  0.763780, // 97
  0.771654, // 98
  0.779528, // 99
  0.787402, // 100
  0.795276, // 101
  0.803150, // 102
  0.811024, // 103
  0.818898, // 104
  0.826772, // 105
  0.834646, // 106
  0.842520, // 107
  0.850394, // 108
  0.858268, // 109
  0.866142, // 110
  0.874016, // 111
  0.881890, // 112
  0.889764, // 113
  0.897638, // 114
  0.905512, // 115
  0.913386, // 116
  0.921260, // 117
  0.929134, // 118
  0.937008, // 119
  0.944882, // 120
  0.952756, // 121
  0.960630, // 122
  0.968504, // 123
  0.976378, // 124
  0.984252, // 125
  0.992126, // 126
  1.000000, // 127
};


#endif