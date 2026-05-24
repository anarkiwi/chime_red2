#!/bin/bash
# Render a Standard MIDI File (SMF/.mid) to a WAV using the off-target software
# simulation of the synth (test/host/cr_render.cpp), entirely inside Docker --
# no host toolchain needed. Builds the same image the host tests use (the
# `cr-render` tool ships in it) and runs it with the in/out files bind-mounted.
#
# Usage (from anywhere):
#   test/host/render.sh INPUT.mid OUTPUT.wav [cr-render options...]
# e.g.
#   test/host/render.sh song.mid song.wav --tail 2 --gain 1.5
#   test/host/render.sh song.mid song.wav --rate 44100 --raw
set -e

if [ "$#" -lt 2 ]; then
  echo "usage: $0 INPUT.mid OUTPUT.wav [cr-render options...]" >&2
  echo "       (any extra options are passed through to cr-render)" >&2
  exit 2
fi

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
root="$(cd "${here}/../.." && pwd)"

in="$1"; shift
out="$1"; shift

if [ ! -f "${in}" ]; then
  echo "input not found: ${in}" >&2
  exit 1
fi

in_abs="$(cd "$(dirname "${in}")" && pwd)/$(basename "${in}")"
out_dir="$(cd "$(dirname "${out}")" && pwd)"
out_base="$(basename "${out}")"

img=chime-red-host-tests
docker build -f "${here}/Dockerfile" -t "${img}" "${root}"

# Mount the input's directory read-only and the output's directory writable.
docker run --rm \
  -v "$(dirname "${in_abs}"):/in:ro" \
  -v "${out_dir}:/out" \
  "${img}" cr-render "/in/$(basename "${in_abs}")" "/out/${out_base}" "$@"

echo "wrote ${out_dir}/${out_base}"
