// Minimal PCM16 mono WAV writer for the host simulator
// (test/host/cr_render.cpp). Header-only, no dependencies beyond the C++
// stdlib, so it links into the single-translation-unit renderer without
// touching the device build or the repo-root cppcheck set (which only scans the
// repo root, not test/host).
#ifndef CR_HOST_WAV_H
#define CR_HOST_WAV_H

#include <cstdint>
#include <cstdio>
#include <vector>

namespace cr_wav {

// Write a mono 16-bit PCM WAV. Returns false (and leaves errno-style detail to
// the caller's stderr) if the file cannot be opened or written.
inline bool WriteMono16(const char *path, const std::vector<int16_t> &samples,
                        uint32_t sampleRate) {
  FILE *f = std::fopen(path, "wb");
  if (!f) {
    return false;
  }
  const uint16_t channels = 1;
  const uint16_t bitsPerSample = 16;
  const uint32_t dataBytes =
      static_cast<uint32_t>(samples.size()) * (bitsPerSample / 8);
  const uint32_t byteRate = sampleRate * channels * (bitsPerSample / 8);
  const uint16_t blockAlign = channels * (bitsPerSample / 8);
  const uint32_t riffSize = 36 + dataBytes;

  auto u32 = [&](uint32_t v) {
    uint8_t b[4] = {uint8_t(v), uint8_t(v >> 8), uint8_t(v >> 16),
                    uint8_t(v >> 24)};
    std::fwrite(b, 1, 4, f);
  };
  auto u16 = [&](uint16_t v) {
    uint8_t b[2] = {uint8_t(v), uint8_t(v >> 8)};
    std::fwrite(b, 1, 2, f);
  };

  std::fwrite("RIFF", 1, 4, f);
  u32(riffSize);
  std::fwrite("WAVE", 1, 4, f);
  std::fwrite("fmt ", 1, 4, f);
  u32(16); // PCM fmt chunk size
  u16(1);  // audio format: PCM
  u16(channels);
  u32(sampleRate);
  u32(byteRate);
  u16(blockAlign);
  u16(bitsPerSample);
  std::fwrite("data", 1, 4, f);
  u32(dataBytes);
  if (!samples.empty()) {
    std::fwrite(samples.data(), sizeof(int16_t), samples.size(), f);
  }
  const bool ok = (std::ferror(f) == 0);
  std::fclose(f);
  return ok;
}

} // namespace cr_wav

#endif // CR_HOST_WAV_H
