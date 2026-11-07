#!/bin/bash
# Bound the worst-case cost of the synth's ISR-path entry points with
# virtuous-cycles' vc-cxx-cost (clang LibTooling, run in Docker) and ratchet it
# against the committed budgets/. Usage: test/cxx-cost/run.sh [check|tighten|report]
# Needs docker, git, python3>=3.11 and `pip install virtuous-cycles` at the same ref.
set -euo pipefail

VC_REF=${VC_REF:-v0.3.0}               # virtuous-cycles release tag (keep in sync with CI)
FIXEDPOINTS_REF=${FIXEDPOINTS_REF:-v1.1.2}
cmd=${1:-check}

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
work="${root}/.work"
mkdir -p "${work}"
cd "${root}"

docker build -q --target cxx-runtime -t vc-cxx-cost \
  "https://github.com/anarkiwi/virtuous-cycles.git#${VC_REF}"
[ -d "${work}/FixedPoints" ] || git clone -q --depth 1 --branch "${FIXEDPOINTS_REF}" \
  https://github.com/Pharap/FixedPointsArduino.git "${work}/FixedPoints"
# clang rejects FixedPoints' in-class `static constexpr SFixed Pi = ...` (the
# class is incomplete, so non-literal); gcc accepts it. The synth uses none of
# these constants, so drop them from this analysis-only copy. Idempotent.
sed -i -E '/static constexpr [SU]Fixed (Epsilon|MinValue|MaxValue|Pi|E|Phi|Tau) =/d' \
  "${work}"/FixedPoints/src/FixedPoints/{SFixed,UFixed}.h

# Compilation database for the device sources, as the host tests build them.
python3 - "${root}" "${work}" <<'PY'
import json, pathlib, sys
root, work = map(pathlib.Path, sys.argv[1:])
flags = f"-std=c++17 -DCR_HOST_TEST -DVC_COST_ANALYSIS -I/usr/local/include -I{root} -I{root}/test/host/stubs -I{work}/FixedPoints/src"
srcs = "CRMidi MidiChannel OscillatorController MidiNote Lfo AdsrEnvelope Oscillator CRIO".split()
db = [{"directory": str(root), "file": f"{root}/{s}.cpp",
       "command": f"c++ {flags} -c {root}/{s}.cpp"} for s in srcs]
(work / "compile_commands.json").write_text(json.dumps(db, indent=1))
PY

# Entry points: everything the master-clock ISR runs (chime_red2.ino nextISR).
docker run --rm -v "${root}:${root}" -w "${root}" vc-cxx-cost \
  -p "${work}" --root "${root}" \
  --entry 'OscillatorController::Triggered' --entry 'CRMidi::HandleControl' \
  --entry 'CRMidi::Modulate' --entry 'CRIO::handlePulse' --entry 'CRIO::schedulePulse' \
  --entry 'CRIO::handleNoPulse' --entry 'CRIO::handleShortPulse' \
  --entry 'CRIO::handlePulseOff' --entry 'CRIO::handleLongPulse' \
  -o "${work}/cxx-cost.json"

virtuous-cycles "${cmd}"
