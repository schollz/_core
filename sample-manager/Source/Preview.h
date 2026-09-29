#pragma once
#include "Common.h"
namespace core {
class Preview final {
public:
  Preview();
  ~Preview();
  void play(const File &, const String &sampleId, double start = 0,
            double stop = 1);
  void stop();
  bool playing() const { return transport.isPlaying(); }
  double position() const {
    return startSeconds + transport.getCurrentPosition();
  }
  const String &sampleId() const { return currentId; }
  double duration() const { return lengthSeconds; }

private:
  juce::AudioFormatManager formats;
  juce::TimeSliceThread readAhead{"Preview buffered file reads"};
  juce::AudioDeviceManager device;
  juce::AudioSourcePlayer player;
  juce::AudioTransportSource transport;
  std::unique_ptr<juce::AudioFormatReaderSource> source;
  bool started = false;
  String currentId;
  double startSeconds = 0, lengthSeconds = 0;
};
} // namespace core
