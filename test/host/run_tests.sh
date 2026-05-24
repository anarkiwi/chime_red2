#!/bin/bash
# Build and run the host frequency-uncertainty unit tests in Docker.
# Usage (from anywhere): test/host/run_tests.sh
set -e

# Resolve repo root from this script's location so it works from any cwd.
here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
root="$(cd "${here}/../.." && pwd)"

img=chime-red-host-tests
docker build -f "${here}/Dockerfile" -t "${img}" "${root}"
# Running the image executes both suites under coverage and propagates their exit
# code (non-zero on any failed assertion) to CI. The coverage report prints to
# stdout; the Cobertura XML is written into the mounted dir for CI to archive.
covout="${here}/coverage"
mkdir -p "${covout}"
docker run --rm -v "${covout}:/coverage" "${img}"
