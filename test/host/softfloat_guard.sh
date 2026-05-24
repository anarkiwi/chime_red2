#!/bin/bash
# Soft-float regression guard.
#
# The Arduino Due is a Cortex-M3 with NO hardware FPU, so any runtime float/double
# operation becomes a software-emulated routine -- expensive, and a silent way to
# blow the ~19 us master-ISR budget. The synth keeps audio/control math in
# fixed-point (cr_fp_t); this gate locks that in by compiling the real synth
# translation units with -Werror=double-promotion -Werror=float-conversion, so a
# newly-introduced runtime float/double op fails the build.
#
# FixedPoints and the host stubs are included with -isystem, so only the synth's
# own code is policed. The two genuinely benign promotions -- compile-time
# constant lookup tables and the controlClockTickMs definition, all folded at
# build time -- are exempted at their source with localised
# `#pragma GCC diagnostic ignored` blocks (see AdsrEnvelope.cpp / constants.h),
# not here, so the exemption is visible where it applies.
#
# Runs in the `softfloat` Docker stage (test/host/Dockerfile) as a build gate, and
# standalone for local use. Paths default to the container layout; override via
# CR_SRC / CR_FP / CR_STUBS.
set -uo pipefail

SRC="${CR_SRC:-/src}"
FP="${CR_FP:-/opt/FixedPoints/src}"
STUBS="${CR_STUBS:-${SRC}/test/host/stubs}"

# The real synth translation units (the set the MIDI suite + simulator link).
TUS="Oscillator AdsrEnvelope MidiChannel MidiNote OscillatorController CRMidi Lfo CRIO"

status=0
for f in ${TUS}; do
  if ! g++ -std=c++17 -O2 -DCR_HOST_TEST \
        -I"${SRC}" -isystem "${FP}" -isystem "${STUBS}" \
        -Werror=double-promotion -Werror=float-conversion \
        -fsyntax-only "${SRC}/${f}.cpp"; then
    echo "soft-float guard: FAIL in ${f}.cpp -- a runtime float/double op crept into"
    echo "  the no-FPU (Arduino Due) path. Keep it fixed-point (cr_fp_t), or if it is"
    echo "  a compile-time constant, exempt it with a localised #pragma GCC diagnostic."
    status=1
  fi
done

if [ "${status}" -eq 0 ]; then
  echo "soft-float guard: OK -- no new runtime FP promotion in the synth sources"
fi
exit "${status}"
