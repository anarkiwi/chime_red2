# Self-contained host image for the synth. It serves two purposes from one build:
#   1. the host unit tests + coverage + cppcheck static-analysis gate (default CMD), and
#   2. an off-target software simulation, `cr-render`, that renders a Standard MIDI
#      File to a WAV by running the real synth code (see test/host/cr_render.cpp).
#
# Everything (compiler + the one header-only dependency) lives in the image, so
# CI only has to `docker build` this file -- no toolchain or Arduino library
# setup on the runner. Build context is the repo root:
#   docker build -f test/host/Dockerfile -t chime-red-host-tests .
#
# Multi-stage so the slow parts (toolchain image, cloning FixedPoints, apt +
# pip installs) live in a cached `base` layer and are not redone on a source
# change: only the cheap compile stages (`test-build`, `sim-build`) re-run, and
# under BuildKit they build in parallel. `final` just bundles their outputs.

# ---- base: toolchain + pinned dependencies (cached across source edits) -------
FROM gcc:13-bookworm AS base

# FixedPoints is header-only and platform-independent integer math; pin a ref
# for reproducible CI. Override with --build-arg FIXEDPOINTS_REF=<tag|sha>.
ARG FIXEDPOINTS_REF=v1.1.2
RUN git clone --depth 1 --branch "${FIXEDPOINTS_REF}" \
      https://github.com/Pharap/FixedPointsArduino.git /opt/FixedPoints

# gcovr turns the gcov data into the coverage report (test/host/coverage.sh).
# Use a current gcovr from pip: Debian's gcovr 5.2 can't parse gcc 13's gcov
# branch output ("UnknownLineType: branch ... (fallthrough)"). cppcheck (Debian
# bookworm's 2.10) is the project's static-analysis gate: it runs here once, with
# one pinned version, instead of each board CI job rebuilding cppcheck from
# source (see build.sh).
# Toolchain for the gates: cppcheck (static analysis), clang-format (C++ style),
# gcovr (coverage), black + pylint (Python style / unused checks).
RUN apt-get update && apt-get install -y --no-install-recommends python3-pip cppcheck clang-format \
      && rm -rf /var/lib/apt/lists/* \
      && pip3 install --break-system-packages --no-cache-dir 'gcovr>=7,<8' black pylint

# Include paths shared by every build below: repo source, the host-test stubs
# (Arduino.h / MIDI.h / ArduinoSTL.h / recording DigitalPin), and FixedPoints.
ENV CR_INC="-I/src -I/src/test/host/stubs -I/opt/FixedPoints/src"

# The real synth sources the MIDI suite and the simulator both compile + link.
ENV CR_SYNTH_SRC="/src/CRMidi.cpp /src/MidiChannel.cpp /src/OscillatorController.cpp \
/src/MidiNote.cpp /src/Lfo.cpp /src/AdsrEnvelope.cpp /src/Oscillator.cpp /src/CRIO.cpp"

# ---- cppcheck: static-analysis gate on the device sources ---------------------
# In its own stage keyed ONLY on the repo-root device sources (the exact set the
# board CI checks), so editing the host tests or the simulator under test/host/
# -- which cppcheck does not scan -- leaves this layer cached and skips the ~minute
# it costs. A finding of the enabled severities fails the build via --error-exitcode,
# and `final` depends on this stage's marker, so the gate still blocks the image.
# --inline-suppr honours the in-source `// cppcheck-suppress` comments.
FROM base AS cppcheck
WORKDIR /src
COPY *.cpp *.h *.ino ./
RUN cppcheck --enable=all --language=c++ --error-exitcode=1 --inline-suppr \
      *.cpp *.h *.ino && touch /cppcheck.ok

# ---- softfloat: no-FPU regression gate (Arduino Due is a Cortex-M3, no FPU) ----
# Compiles the real synth TUs with -Werror=double-promotion -Werror=float-conversion
# so a newly introduced runtime float/double op (software-emulated on the Due, and a
# way to blow the ~19 us ISR budget) fails the build. Keyed only on the device
# sources + stubs, so editing the simulator/tests leaves it cached; `final` depends
# on its marker. See test/host/softfloat_guard.sh for the rationale and exemptions.
FROM base AS softfloat
WORKDIR /src
COPY *.cpp *.h *.ino ./
COPY test/host/stubs ./test/host/stubs
COPY test/host/softfloat_guard.sh ./test/host/softfloat_guard.sh
RUN bash test/host/softfloat_guard.sh && touch /softfloat.ok

# ---- lint: C++ clang-format (LLVM) + Python black / pylint(unused) gates -------
# Style gates on the code this repo hand-maintains. clang-format (LLVM, per the
# test/host/.clang-format) covers the host test + simulator C++ only -- the
# original firmware and the generated tables keep their existing layout. black
# and pylint (unused-import / unused-variable only, per request) cover all the
# Python under test/. Keyed solely on test/, so edits elsewhere leave it cached;
# `final` depends on its marker, so a style or unused-symbol violation fails the
# image. --dry-run --Werror / --check make these non-mutating checks.
FROM base AS lint
WORKDIR /src
COPY test ./test
RUN clang-format --dry-run --Werror \
      $(find test/host -name '*.cpp' -o -name '*.h')
RUN black --check $(find test -name '*.py' | sort)
RUN pylint --disable=all --enable=unused-import,unused-variable \
      $(find test -name '*.py' | sort) && touch /lint.ok

# ---- test-build: instrumented unit tests --------------------------------------
FROM base AS test-build
WORKDIR /src
# Copy the whole repo so the tests compile the real source as it is now.
COPY . .

# Build both host test suites with gcov instrumentation (-O0 --coverage for
# accurate line mapping; the fixed-point integer math the suites assert on is
# bit-identical at any -O). Each suite's objects + .gcno notes live in their own
# dir so the plain and -DCR_HOST_TEST builds don't collide. Compiling here is the
# build-time compile gate: a build failure fails the image.

# Frequency suite: real Oscillator.cpp + pitch tables.
RUN mkdir -p /cov/freq && cd /cov/freq && \
    g++ --coverage -O0 -g -std=c++17 -Wall ${CR_INC} -c \
      /src/test/host/test_frequency.cpp /src/Oscillator.cpp && \
    g++ --coverage -o test_frequency *.o

# MIDI suite: real CRMidi + voice/oscillator stack and the base (non-LCD) CRIO.
# CR_HOST_TEST selects the base CRIO and a no-op DigitalPin so the off-target
# hardware code builds; the Arduino MIDI library and ISRs are stubbed/bypassed.
RUN mkdir -p /cov/midi && cd /cov/midi && \
    g++ --coverage -O0 -g -std=c++17 -Wall -DCR_HOST_TEST ${CR_INC} -c \
      /src/test/host/test_midi.cpp ${CR_SYNTH_SRC} && \
    g++ --coverage -o test_midi *.o

# ---- sim-build: the SMF->WAV simulator (optimised, no instrumentation) ---------
FROM base AS sim-build
WORKDIR /src
COPY . .
# cr-render runs the same real synth sources as the MIDI suite, plus the SMF
# reader / WAV writer (test/host/*.h). -O2 (this is a tool, not a coverage
# target); CR_HOST_TEST gives the base CRIO + recording pin the renderer samples;
# CR_SIM_RANDOM swaps the deterministic test RNG for real pseudo-random channel-10
# noise (test determinism is untouched -- only this build defines it).
RUN g++ -O2 -std=c++17 -Wall -DCR_HOST_TEST -DCR_SIM_RANDOM ${CR_INC} \
      /src/test/host/cr_render.cpp ${CR_SYNTH_SRC} -o /usr/local/bin/cr-render

# ---- final: ship both. Default CMD runs the tests; cr-render is on PATH --------
FROM test-build AS final
# Pull in cr-render and the gate markers. Depending on the cppcheck, softfloat and
# lint stages here is what makes a static-analysis finding, a new runtime FP op, or
# a C++/Python style (or unused-symbol) violation fail the image build.
COPY --from=sim-build /usr/local/bin/cr-render /usr/local/bin/cr-render
COPY --from=cppcheck /cppcheck.ok /cppcheck.ok
COPY --from=softfloat /softfloat.ok /softfloat.ok
COPY --from=lint /lint.ok /lint.ok

# Run both suites and report coverage. Either suite failing fails CI; the
# Cobertura XML is written to ${COVERAGE_OUT:-/coverage} (run_tests.sh mounts it
# out). The simulation is reachable in the same image, e.g.:
#   docker run --rm -v "$PWD:/io" chime-red-host-tests cr-render /io/song.mid /io/song.wav
# (test/host/render.sh wraps that.)
CMD ["/src/test/host/coverage.sh"]
