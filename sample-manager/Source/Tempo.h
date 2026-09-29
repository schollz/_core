#pragma once
#include "Common.h"

namespace core {
struct TempoEstimate {
  double bpm = 0;
  String method = "undetermined";
};
// A basename only: directory names and unrelated numbers never supply tempo.
double filenameTempo(const String &);
// Explicit filename tempo, then embedded metadata, then bounded audio analysis.
TempoEstimate detectTempo(juce::AudioFormatReader &, const String &filename,
                          const std::function<bool()> &cancel = {});
} // namespace core
