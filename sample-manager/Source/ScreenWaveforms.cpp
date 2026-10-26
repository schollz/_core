#include "ScreenWaveforms.h"
namespace core::screenwaves {
juce::MemoryBlock encode(const zv::Wave &wave, const juce::MemoryBlock &infoBytes,
                         const String &audioHash) {
  const auto info = card::decode(infoBytes);
  require(audioHash.length() == 64 && audioHash.containsOnly("0123456789abcdefABCDEF"),
          "Invalid screen waveform audio identity");
  require(wave.channels == info.channels && wave.sampleRate == info.rate &&
              std::abs(wave.duration - double(info.size) / (info.rate * info.channels * 2)) <
                  0.5 / info.rate &&
              wave.peaks.size() == size_t(info.channels) && !wave.peaks.front().empty(),
          "Screen waveform does not match completed audio");
  const size_t bins = wave.peaks.front().size() / 2;
  for (const auto &channel : wave.peaks)
    require(channel.size() == bins * 2, "Invalid screen waveform peaks");
  juce::MemoryBlock bytes(SW_FILE_BYTES, true);
  auto *p = static_cast<uint8_t *>(bytes.getData());
  std::memcpy(p, "ZCW1", 4);
  sw_put16(p + 4, 1); sw_put16(p + 6, SW_COLUMNS);
  sw_put32(p + 8, info.size); sw_put32(p + 12, uint32_t(info.rate));
  p[16] = uint8_t(info.channels); p[17] = 1;
  sw_put32(p + 20, sw_info_hash(static_cast<const uint8_t *>(infoBytes.getData()),
                               infoBytes.getSize()));
  for (int n = 0; n < 32; ++n)
    p[24 + n] = uint8_t(audioHash.substring(n * 2, n * 2 + 2).getHexValue32());
  for (size_t x = 0; x < SW_COLUMNS; ++x) {
    const size_t first = x * bins / SW_COLUMNS;
    const size_t last = std::min(bins, ((x + 1) * bins + SW_COLUMNS - 1) / SW_COLUMNS);
    int peak = 0;
    for (const auto &channel : wave.peaks)
      for (size_t b = first; b < last; ++b)
        peak = std::max({peak, std::abs(int(channel[2 * b])),
                         std::abs(int(channel[2 * b + 1]))});
    p[SW_HEADER_BYTES + x] = uint8_t((peak * 255 + 16384) / 32768);
  }
  sw_put32(p + 56, sw_crc32(p + SW_HEADER_BYTES, SW_COLUMNS));
  sw_put32(p + 60, sw_crc32(p, 60));
  return bytes;
}
}
