#include "Preview.h"
namespace core {
Preview::Preview() {
  formats.registerBasicFormats();
  readAhead.startThread();
}
Preview::~Preview() {
  transport.stop();
  transport.setSource(nullptr);
  player.setSource(nullptr);
  device.removeAudioCallback(&player);
  device.closeAudioDevice();
  source.reset();
  readAhead.stopThread(5000);
}
void Preview::play(const File &f, const String &id, double start, double stop) {
  diagnostics::Scope trace("PREVIEW", "Play sample=" + id + " file=" +
      f.getFullPathName() + " start=" + String(start, 6) + " stop=" + String(stop, 6));
  require(
      f.existsAsFile(),
      "Completed preview audio is unavailable. Reconnect the project volume.");
  if (!started) {
    auto error = device.initialise(0, 2, nullptr, true);
    require(error.isEmpty(), error);
    player.setSource(&transport);
    device.addAudioCallback(&player);
    started = true;
    if (auto *output = device.getCurrentAudioDevice())
      diagnostics::log("PREVIEW", "Output=" + output->getName() +
          " rate=" + String(output->getCurrentSampleRate()) +
          " buffer_size=" + String(output->getCurrentBufferSizeSamples()));
  }
  transport.stop();
  transport.setSource(nullptr);
  source.reset();
  std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(f));
  require(reader != nullptr, "Cannot open preview audio");
  auto frames = reader->lengthInSamples;
  double rate = reader->sampleRate;
  auto first = juce::int64(start * double(frames)),
       last = juce::int64(stop * double(frames));
  require(first >= 0 && last > first && last <= frames,
          "Invalid slice audition range");
  startSeconds = double(first) / rate;
  lengthSeconds = double(frames) / rate;
  auto subsection = std::make_unique<juce::AudioSubsectionReader>(
      reader.release(), first, last - first, true);
  source = std::make_unique<juce::AudioFormatReaderSource>(subsection.release(),
                                                           true);
  transport.setSource(source.get(), 65536, &readAhead, rate);
  transport.setPosition(0);
  currentId = id;
  transport.start();
}
void Preview::stop() {
  diagnostics::log("PREVIEW", "Stop sample=" + currentId);
  transport.stop();
}
} // namespace core
