#pragma once
#include "Common.h"
namespace core {
struct Uf2Image {
  uint32_t family = 0, blocks = 0;
  String product;
};
Uf2Image inspectUf2(const File &);
juce::Array<File> bootloaderVolumes();
void flashLocalUf2(const File &image, const File &volume,
                   std::function<void(double)> progress,
                   std::function<bool()> cancel);
} // namespace core
