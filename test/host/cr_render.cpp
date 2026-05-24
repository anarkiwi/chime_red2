// Copyright 2019 Josh Bailey (josh@vandervecken.com)

// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

// Off-target software simulation of the synth: render a Standard MIDI File (SMF)
// to a WAV by running the REAL synth code on the host.
//
// It compiles the same sources the MIDI unit tests do (CRMidi + MidiChannel +
// OscillatorController + MidiNote + Lfo + AdsrEnvelope + Oscillator + base CRIO)
// under CR_HOST_TEST, feeds the file's note/CC/pitch-bend/program events into the
// real CRMidi handlers at their scheduled times, and advances the master-clock
// ISR state machine (IsrDriver, the same replica the tests use) one firing per
// output sample. The device drives a coil gate pin; the CR_HOST_TEST recording
// DigitalPin exposes that pin's level each tick, so the rendered waveform IS the
// coil gate signal the firmware would produce -- pitch, polyphony, envelopes,
// detune, vibrato/tremolo, pitch bend and channel-10 pitched noise all come from
// the real code, not a re-model.
//
// Output model: one sample per master ISR (native rate = masterClockHz). Each
// sample is the coil gate level (1 while the pulse is high, else 0); a sub-tick
// "short" pulse that begins and ends within one ISR still registers as one high
// sample via its rising edge. By default the gate is DC-blocked (a one-pole
// high-pass) and peak-normalised so it plays as an AC audio signal whose energy
// sits at the switching edges -- a reasonable proxy for the impulsive sound of a
// hard-switched coil. --raw emits the literal unipolar gate instead.

#include "config.h"
#include "types.h"
#include "constants.h"
#include "pins.h"
#include "CRDigitalPin.h"
#include "OscillatorController.h"
#include "MidiChannel.h"
#include "MidiNote.h"
#include "CRIO.h"
#include "CRMidi.h"
#include "IsrDriver.h"

#include "smf.h"
#include "wav.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {

struct Options {
  const char *inPath = nullptr;
  const char *outPath = nullptr;
  double tailSeconds = 1.0;    // extra render after the last event (release/decay)
  double maxSeconds = 0.0;     // hard cap on render length (0 = no user cap)
  double gain = 1.0;           // output gain multiplier
  uint32_t rate = 0;           // output sample rate (0 = native masterClockHz)
  bool raw = false;            // emit unipolar gate instead of DC-blocked AC
  bool normalize = true;       // peak-normalise (ignored in --raw? still applies)
  unsigned seed = 1;           // RNG seed for channel-10 pitched noise
};

void Usage(const char *argv0) {
  std::fprintf(stderr,
      "usage: %s INPUT.mid OUTPUT.wav [options]\n"
      "  --tail SECONDS     silence/release tail after last event (default 1.0)\n"
      "  --seconds SECONDS  cap total render length\n"
      "  --gain G           output gain multiplier (default 1.0)\n"
      "  --rate HZ          output sample rate (default native %lu Hz; resamples)\n"
      "  --raw              emit the literal unipolar coil gate (no DC block)\n"
      "  --no-normalize     do not peak-normalise the output\n"
      "  --seed N           RNG seed for channel-10 pitched noise (default 1)\n",
      argv0, (unsigned long)masterClockHz);
}

bool ParseArgs(int argc, char **argv, Options &o) {
  std::vector<const char *> positional;
  for (int i = 1; i < argc; ++i) {
    const char *a = argv[i];
    auto next = [&](double &dst) -> bool {
      if (i + 1 >= argc) return false;
      dst = std::atof(argv[++i]);
      return true;
    };
    if (!std::strcmp(a, "--tail")) {
      if (!next(o.tailSeconds)) return false;
    } else if (!std::strcmp(a, "--seconds")) {
      if (!next(o.maxSeconds)) return false;
    } else if (!std::strcmp(a, "--gain")) {
      if (!next(o.gain)) return false;
    } else if (!std::strcmp(a, "--rate")) {
      if (i + 1 >= argc) return false;
      o.rate = static_cast<uint32_t>(std::atol(argv[++i]));
    } else if (!std::strcmp(a, "--seed")) {
      if (i + 1 >= argc) return false;
      o.seed = static_cast<unsigned>(std::atol(argv[++i]));
    } else if (!std::strcmp(a, "--raw")) {
      o.raw = true;
    } else if (!std::strcmp(a, "--no-normalize")) {
      o.normalize = false;
    } else if (a[0] == '-' && a[1] != '\0') {
      std::fprintf(stderr, "unknown option: %s\n", a);
      return false;
    } else {
      positional.push_back(a);
    }
  }
  if (positional.size() != 2) return false;
  o.inPath = positional[0];
  o.outPath = positional[1];
  return true;
}

void Dispatch(CRMidi &crmidi, const cr_smf::Event &e) {
  switch (e.kind) {
    case cr_smf::Event::kNoteOn:
      crmidi.handleNoteOn(e.channel, e.d1, e.d2);
      break;
    case cr_smf::Event::kNoteOff:
      crmidi.handleNoteOff(e.channel, e.d1);
      break;
    case cr_smf::Event::kCC:
      crmidi.handleControlChange(e.channel, e.d1, e.d2);
      break;
    case cr_smf::Event::kProgram:
      crmidi.handleProgramChange(e.channel, e.d1);
      break;
    case cr_smf::Event::kPitchBend:
      crmidi.handlePitchBend(e.channel, e.bend14);
      break;
  }
}

}  // namespace

int main(int argc, char **argv) {
  Options o;
  if (!ParseArgs(argc, argv, o)) {
    Usage(argv[0]);
    return 2;
  }
  std::srand(o.seed);

  cr_smf::File smf;
  std::string err;
  if (!cr_smf::Load(o.inPath, smf, err)) {
    std::fprintf(stderr, "error reading %s: %s\n", o.inPath, err.c_str());
    return 1;
  }

  const double nativeRate = static_cast<double>(masterClockHz);
  double totalSeconds = smf.endSeconds + (o.tailSeconds > 0 ? o.tailSeconds : 0);
  if (o.maxSeconds > 0 && o.maxSeconds < totalSeconds) {
    totalSeconds = o.maxSeconds;
  }
  const double kHardCapSeconds = 3600.0;
  if (totalSeconds > kHardCapSeconds) {
    std::fprintf(stderr,
        "warning: render length %.1f s exceeds %.0f s cap; truncating "
        "(use --seconds to set explicitly)\n", totalSeconds, kHardCapSeconds);
    totalSeconds = kHardCapSeconds;
  }
  const unsigned long long totalTicks =
      static_cast<unsigned long long>(std::ceil(totalSeconds * nativeRate));

  // Precompute each event's master-tick time and keep them in time order.
  struct Timed { unsigned long long tick; cr_smf::Event ev; };
  std::vector<Timed> timed;
  timed.reserve(smf.events.size());
  for (const cr_smf::Event &e : smf.events) {
    timed.push_back(
        {static_cast<unsigned long long>(std::llround(e.seconds * nativeRate)), e});
  }

  // The real synth, exactly as the MIDI tests instantiate it (base, non-LCD CRIO).
  OscillatorController oc;
  CRIO crio;
  CRMidi crmidi(&oc, &crio);
  IsrDriver driver(oc, crmidi, crio);

  CrPinRecord &coil = crHostPin(coilOutPin);
  coil.Reset();
  crHostPin(diagOutPin).Reset();
  crHostPin(speakerOutPin).Reset();

  std::vector<float> sig;
  sig.reserve(static_cast<size_t>(totalTicks));

  unsigned long long evIdx = 0;
  unsigned long prevRising = coil.risingEdges;
  unsigned long long pulses = 0;
  for (unsigned long long t = 0; t < totalTicks; ++t) {
    while (evIdx < timed.size() && timed[evIdx].tick <= t) {
      Dispatch(crmidi, timed[evIdx].ev);
      ++evIdx;
    }
    driver.Tick();
    // High if the gate is up at end of tick, or a pulse rose+fell within it.
    const bool edge = coil.risingEdges != prevRising;
    const bool high = coil.level || edge;
    if (edge) {
      prevRising = coil.risingEdges;
      ++pulses;
    }
    sig.push_back(high ? 1.0f : 0.0f);
  }
  // Flush any events past the render window (e.g. a long tail truncated away).
  while (evIdx < timed.size()) {
    ++evIdx;
  }

  // Post-process: by default DC-block the unipolar gate into an AC signal whose
  // energy lands at the switching edges. --raw keeps the literal gate.
  if (!o.raw) {
    const float R = 0.999f;  // one-pole high-pass pole (~8 Hz corner at 52631 Hz)
    float xPrev = 0.0f, yPrev = 0.0f;
    for (float &x : sig) {
      const float in = x;
      const float y = in - xPrev + R * yPrev;
      xPrev = in;
      yPrev = y;
      x = y;
    }
  }

  float peak = 0.0f;
  for (float x : sig) {
    peak = std::max(peak, std::fabs(x));
  }
  float scale = static_cast<float>(o.gain);
  if (o.normalize && peak > 0.0f) {
    scale = static_cast<float>(o.gain) * 0.9f / peak;
  }

  // Resample (linear) to the requested output rate if it differs from native.
  const uint32_t outRate = o.rate ? o.rate : masterClockHz;
  std::vector<int16_t> pcm;
  auto toPcm = [](float v) -> int16_t {
    long s = std::lround(v * 32767.0f);
    if (s > 32767) s = 32767;
    if (s < -32768) s = -32768;
    return static_cast<int16_t>(s);
  };
  if (outRate == masterClockHz) {
    pcm.reserve(sig.size());
    for (float x : sig) {
      pcm.push_back(toPcm(x * scale));
    }
  } else {
    const double ratio = static_cast<double>(masterClockHz) / outRate;
    const size_t outN =
        static_cast<size_t>(std::floor(sig.size() / ratio));
    pcm.reserve(outN);
    for (size_t j = 0; j < outN; ++j) {
      const double srcPos = j * ratio;
      const size_t i0 = static_cast<size_t>(srcPos);
      const double frac = srcPos - i0;
      const float a = sig[i0];
      const float b = (i0 + 1 < sig.size()) ? sig[i0 + 1] : a;
      pcm.push_back(toPcm((a + static_cast<float>(frac) * (b - a)) * scale));
    }
  }

  if (!cr_wav::WriteMono16(o.outPath, pcm, outRate)) {
    std::fprintf(stderr, "error writing %s\n", o.outPath);
    return 1;
  }

  std::fprintf(stderr,
      "rendered %s -> %s\n"
      "  events: %zu (%s) | master clock: %lu Hz\n"
      "  duration: %.2f s | output: %zu samples @ %u Hz, mono 16-bit\n"
      "  coil pulses emitted: %llu%s\n",
      o.inPath, o.outPath, smf.events.size(),
      smf.events.empty() ? "none -- silent render" : "ok",
      (unsigned long)masterClockHz, totalSeconds, pcm.size(), outRate, pulses,
      o.raw ? " | raw gate" : " | DC-blocked");
  return 0;
}
