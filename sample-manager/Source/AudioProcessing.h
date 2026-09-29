#pragma once
#include "Storage.h"
namespace core {
struct Rendered {
  String preview, padded, companionPreview, companionPadded, key;
  uint64_t frames = 0, companionFrames = 0;
};
class AudioProcessing {
public:
  using Cancel = std::function<bool()>;
  AudioProcessing();
  std::unique_ptr<juce::AudioFormatReader> reader(const File &);
  Sample import(Storage &, const File &, int bank, int slot,
                const Cancel & = {});
  Rendered render(const File &root, const Sample &, const Cancel & = {});
  std::vector<Marker> detect(const File &, String method = "hfc",
                             double spacingMs = 80, const Cancel & = {});
  void resample(const File &, const File &, int channels, int rate,
                double speed, const Cancel & = {});
  void stretch(const File &, const File &, double ratio,
               const std::vector<Marker> &, const Cancel & = {});
  void pcm(const File &, const File &, bool circularPadding,
           const Cancel & = {});
  File merge(const File &root, const std::vector<Sample> &ordered,
             Sample &merged, const Cancel & = {});

private:
  juce::AudioFormatManager formats;
  std::unique_ptr<juce::AudioFormatWriter>
  floatWriter(const File &, int channels, double rate);
  std::vector<Marker> embeddedMarkers(const File &,
                                      const juce::AudioFormatReader &);
};
} // namespace core
