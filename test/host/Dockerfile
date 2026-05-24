# Self-contained host unit tests for the synth's frequency quantization.
# Everything (compiler + the one header-only dependency) lives in the image, so
# CI only has to `docker build` this file -- no toolchain or Arduino library
# setup on the runner. Build context is the repo root:
#   docker build -f test/host/Dockerfile -t chime-red-host-tests .
FROM gcc:13-bookworm

# FixedPoints is header-only and platform-independent integer math; pin a ref
# for reproducible CI. Override with --build-arg FIXEDPOINTS_REF=<tag|sha>.
ARG FIXEDPOINTS_REF=v1.1.2
RUN git clone --depth 1 --branch "${FIXEDPOINTS_REF}" \
      https://github.com/Pharap/FixedPointsArduino.git /opt/FixedPoints

# gcovr turns the gcov data into the coverage report (test/host/coverage.sh).
# Use a current gcovr from pip: Debian's gcovr 5.2 can't parse gcc 13's gcov
# branch output ("UnknownLineType: branch ... (fallthrough)"). cppcheck (Debian
# bookworm's 2.10) is the project's static-analysis gate: it runs here once, with
# one pinned version, instead of each board CI job rebuilding cppcheck 2.8 from
# source (see build.sh).
RUN apt-get update && apt-get install -y --no-install-recommends python3-pip cppcheck \
      && rm -rf /var/lib/apt/lists/* \
      && pip3 install --break-system-packages --no-cache-dir 'gcovr>=7,<8'

WORKDIR /src
# Copy the whole repo so the test compiles the real source as it is now.
COPY . .

# Build both host test suites with gcov instrumentation (-O0 --coverage for
# accurate line mapping; the fixed-point integer math the suites assert on is
# bit-identical at any -O). Each suite's objects + .gcno notes live in their own
# dir so the plain and -DCR_HOST_TEST builds don't collide. Compiling here is the
# build-time compile gate: a build failure fails the image.
ENV CR_INC="-I/src -I/src/test/host/stubs -I/opt/FixedPoints/src"

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
      /src/test/host/test_midi.cpp \
      /src/CRMidi.cpp /src/MidiChannel.cpp /src/OscillatorController.cpp \
      /src/MidiNote.cpp /src/Lfo.cpp /src/AdsrEnvelope.cpp /src/Oscillator.cpp \
      /src/CRIO.cpp && \
    g++ --coverage -o test_midi *.o

# Static-analysis gate on the synth's device sources (repo-root *.cpp/*.h/*.ino,
# the same set the board CI used to check). A finding of the enabled severities
# fails the image build, hence the job. --inline-suppr honours the in-source
# `// cppcheck-suppress` comments (e.g. the host-test-only OscillatorController::
# TestOsc, whose only caller is a test cppcheck doesn't see here). The CR_HOST_TEST
# DigitalPin recorder is on the test-only include path (test/host/stubs), unseen
# from /src, so cppcheck sees just the one real DigitalPin (no ODR false positive).
RUN cd /src && cppcheck --enable=all --language=c++ --error-exitcode=1 \
      --inline-suppr *.cpp *.h *.ino

# Run both suites and report coverage. Either suite failing fails CI; the
# Cobertura XML is written to ${COVERAGE_OUT:-/coverage} (run_tests.sh mounts it
# out). Also runnable at build time via the accompanying run_tests.sh.
CMD ["/src/test/host/coverage.sh"]
