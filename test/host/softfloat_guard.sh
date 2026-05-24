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
# It compiles each TU in TWO preprocessor configurations so the gate matches what
# the device compiler actually sees, not just the host-test build:
#   - host-test config (-DCR_HOST_TEST): every TU, incl. CRIO (whose real-pin /
#     register code only builds on host under CR_HOST_TEST);
#   - device config (-DARDUINO_ARCH_SAM, no CR_HOST_TEST): the arch-independent
#     synth TUs as the on-target build preprocesses them (CRIO is excluded -- its
#     device path needs the Arduino register macros that don't exist off-target).
# The synth's audio/control arithmetic is arch-independent, so this off-target
# check is exactly the on-target arithmetic; the SAM/SAMD board CI builds remain
# the ultimate full-arch check.
#
# FixedPoints and the host stubs are included with -isystem, so only the synth's
# own code is policed. The genuinely benign promotions (compile-time constant
# lookup tables and the master/control clock constants, all folded at build time)
# are exempted at their source with localised `#pragma GCC diagnostic ignored`
# blocks (see AdsrEnvelope.cpp / constants.h), not here, so the exemption is
# visible where it applies.
#
# Runs in the `softfloat` Docker stage (test/host/Dockerfile) as a build gate, and
# standalone for local use. Paths default to the container layout; override via
# CR_SRC / CR_FP / CR_STUBS.
set -uo pipefail

SRC="${CR_SRC:-/src}"
FP="${CR_FP:-/opt/FixedPoints/src}"
STUBS="${CR_STUBS:-${SRC}/test/host/stubs}"

WFLAGS="-Werror=double-promotion -Werror=float-conversion"
INC="-I${SRC} -isystem ${FP} -isystem ${STUBS}"

# The real synth translation units (the set the MIDI suite + simulator link).
TUS_ALL="Oscillator AdsrEnvelope MidiChannel MidiNote OscillatorController CRMidi Lfo CRIO"
# CRIO's device path needs Arduino register macros, so it is host-config only.
TUS_DEVICE="Oscillator AdsrEnvelope MidiChannel MidiNote OscillatorController CRMidi Lfo"

status=0
check() {  # check <label> <extra-defines> <tu-list>
  local label="$1" defs="$2" tus="$3" f
  for f in ${tus}; do
    if ! g++ -std=c++17 -O2 ${defs} ${INC} ${WFLAGS} -fsyntax-only "${SRC}/${f}.cpp"; then
      echo "soft-float guard: FAIL in ${f}.cpp [${label}] -- a runtime float/double op"
      echo "  crept into the no-FPU (Arduino Due) path. Keep it fixed-point (cr_fp_t), or"
      echo "  if it is a compile-time constant, exempt it with a localised #pragma."
      status=1
    fi
  done
}

check "host-test config"  "-DCR_HOST_TEST"      "${TUS_ALL}"
check "device config"     "-DARDUINO_ARCH_SAM"  "${TUS_DEVICE}"

if [ "${status}" -eq 0 ]; then
  echo "soft-float guard: OK -- no new runtime FP promotion (host-test + device configs)"
fi
exit "${status}"
