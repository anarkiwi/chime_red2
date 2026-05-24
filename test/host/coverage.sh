#!/bin/bash
# Run both host test suites under gcov instrumentation and emit coverage:
#   - a per-file line/branch table to stdout (shows up in CI logs), and
#   - a Cobertura XML at ${COVERAGE_OUT:-/coverage}/coverage.xml for CI tooling.
# The instrumented binaries (and their .gcno notes) are built by the Dockerfile
# (test/host/Dockerfile) into /cov/{freq,midi}. This script runs them -- which is
# also the authoritative pass/fail for the suites -- then reports coverage.
set -uo pipefail

out="${COVERAGE_OUT:-/coverage}"
mkdir -p "${out}"

# Start each run from clean counts (gcov otherwise accumulates across runs).
find /cov -name '*.gcda' -delete

status=0
( cd /cov/freq && ./test_frequency ) || status=$?
( cd /cov/midi && ./test_midi ) || status=$?

# Report only the synth's own sources: drop the test harness and stubs, and the
# large generated data tables (no meaningful "coverage" there).
# Report the synth's own sources only: drop the test harness/stubs and the large
# generated data tables (no meaningful "coverage" there). gcovr matches these
# against repo-relative paths, so the patterns have no leading slash.
gcovr_args=(
  --root /src /cov
  --filter '/src/'
  --exclude '.*test/host.*'
  --exclude '.*miditables\.h'
  --exclude '.*wavetable\.h'
  --exclude-unreachable-branches
)

echo
echo "===== host test coverage (synth sources) ====="
gcovr "${gcovr_args[@]}"

# Cobertura XML artifact (--cobertura-pretty on gcovr >=5.1, else --xml-pretty).
gcovr "${gcovr_args[@]}" --cobertura-pretty --output "${out}/coverage.xml" 2>/dev/null \
  || gcovr "${gcovr_args[@]}" --xml-pretty --output "${out}/coverage.xml"
echo "wrote ${out}/coverage.xml"

exit "${status}"
