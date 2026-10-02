#include "AudioProcessing.h"
#include "Tempo.h"
#include <TempoData.h>
#include <iostream>

namespace core {
namespace {
void writeTempoAudio(const File &file, const juce::AudioBuffer<float> &data, double rate) {
  juce::WavAudioFormat format;
  std::unique_ptr<juce::OutputStream> stream(file.createOutputStream());
  auto writer = format.createWriterFor(stream, juce::AudioFormatWriterOptions()
                                                   .withSampleRate(rate)
                                                   .withNumChannels(data.getNumChannels())
                                                   .withBitsPerSample(16));
  require(writer != nullptr, "Write tempo fixture");
  require(writer->writeFromAudioSampleBuffer(data, 0, data.getNumSamples()), "Save tempo audio");
}
} // namespace
void tempoTests() {
  for (const auto &name :
       juce::StringArray{"Vintage Medium DnB 3 - 135 bpm.wav", "135BPM.wav", "loop_bpm135.wav",
                         "BPM: 135.xrni", String::fromUTF8("amen — 135 bpm.flac")})
    require(filenameTempo(name) == 135, "Explicit filename BPM: " + name);
  require(filenameTempo("loop-123.5-BpM.wav") == 123.5 &&
              filenameTempo("loop_bpm123.5.mp3") == 123.5,
          "Fractional BPM works before and after the label");
  for (const auto &name :
       juce::StringArray{"DnB 3.wav", "2026 take 135.wav", "loop-bpm3.wav", "loop-3135bpm.wav",
                         "135bpm-174bpm.wav", "/135bpm/untagged.wav", "loop-999bpm.wav"})
    require(filenameTempo(name) == 0, "Unrelated/ambiguous filename numbers are not BPM: " + name);
  auto root = File::getSpecialLocation(File::tempDirectory).getChildFile("core-tempo-" + uuid());
  require(root.createDirectory().wasOk(), "Tempo fixture folder");
  struct Clean {
    File root;
    ~Clean() { root.deleteRecursively(); }
  } clean{root};
  AudioProcessing audio;
  Storage storage(root);
  for (int i = 0; i < TempoData::namedResourceListSize; ++i) {
    int size = 0;
    const auto *bytes = TempoData::getNamedResource(TempoData::namedResourceList[i], size);
    auto file = root.getChildFile("untagged-" + String(i) + ".wav");
    durableWrite(file, bytes, size_t(size));
    auto reader = audio.reader(file);
    auto estimate = detectTempo(*reader, file.getFileName());
    require(estimate.bpm == 135 && estimate.method == "loop length",
            "Actual 135 BPM drum loop is recovered without a filename or metadata hint");
    auto imported = audio.import(storage, file, 0, i);
    require(imported.sourceBpm == 135 && !imported.preservePitch && imported.slices.size() == 16,
            "Detected tempo, sixteen slices and pitch default reach the imported model");
    require(Sample::fromJson(imported.json()).sourceBpm == 135,
            "Detected source tempo survives project serialization");
    reader->metadataValues.set(juce::WavAudioFormat::acidTempo, "128.5");
    require(detectTempo(*reader, "untagged.wav").bpm == 128.5 &&
                detectTempo(*reader, "135 bpm.wav").bpm == 135,
            "Embedded ACID tempo is used, with explicit filenames taking precedence");
    reader->metadataValues.clear();
    reader->metadataValues.set(juce::AiffAudioFormat::appleBeats, "16");
    require(std::abs(detectTempo(*reader, "untagged.aif").bpm - 135) < .1,
            "Embedded Apple loop beat count supplies tempo");
    reader->metadataValues.clear();
    // Extend the same real loop with a short tail so its duration no longer
    // fits an integer-tempo regular loop. SoundTouch cannot resolve these
    // syncopated loops; onset correlation must recover the beat from the audio.
    juce::AudioBuffer<float> data(1, int(reader->lengthInSamples) + 7440);
    data.clear();
    require(reader->read(&data, 0, int(reader->lengthInSamples), 0, true, false),
            "Read tempo fixture");
    auto extended = root.getChildFile("tail-" + String(i) + ".wav");
    writeTempoAudio(extended, data, reader->sampleRate);
    const auto originalHash = hashFile(extended);
    auto withTail = audio.reader(extended);
    auto fallback = detectTempo(*withTail, "untagged.wav");
    require(fallback.method == "onset correlation" && std::abs(fallback.bpm - 135) < 2.,
            "Audio analysis handles real loops whose length is not an exact beat count: " +
                String(TempoData::namedResourceList[i]) + " detected " + String(fallback.bpm) +
                " BPM using " + fallback.method);
    // Cancel during the initial analysis, fallback decoding and correlation.
    for (int after : {3, 150, 300}) {
      bool cancelledRead = false;
      int cancelChecks = 0;
      try {
        detectTempo(*withTail, "untagged.wav", [&] { return ++cancelChecks > after; });
      } catch (const std::exception &e) {
        cancelledRead = String(e.what()) == "Cancelled";
      }
      require(cancelledRead, "Tempo analysis cancels during each analysis pass");
    }
    require(hashFile(extended) == originalHash, "Tempo analysis leaves the source untouched");
    if (i == 0)
      for (int rate : {44100, 48000}) {
        juce::AudioBuffer<float> stereo(2, int(std::round(double(data.getNumSamples()) * rate /
                                                        reader->sampleRate)));
        for (int frame = 0; frame < stereo.getNumSamples(); ++frame) {
          const double position = frame * reader->sampleRate / rate;
          const int a = int(position), b = std::min(a + 1, data.getNumSamples() - 1);
          const float value = .25f * (data.getSample(0, a) + float(position - a) *
                                         (data.getSample(0, b) - data.getSample(0, a)));
          stereo.setSample(0, frame, value);
          stereo.setSample(1, frame, -value);
        }
        auto variant = root.getChildFile("stereo-" + String(rate) + ".wav");
        writeTempoAudio(variant, stereo, rate);
        auto stereoReader = audio.reader(variant);
        const auto detected = detectTempo(*stereoReader, "untagged.wav");
        require(detected.method == "onset correlation" && std::abs(detected.bpm - 135) < 2.,
                "Fallback handles quiet opposite-phase stereo at " + String(rate) +
                    " Hz: " + String(detected.bpm) + " using " + detected.method);
      }
  }
  auto silence = root.getChildFile("silence.wav");
  {
    auto out = silence.createOutputStream();
    card::writeHeader(*out, 88200, 44100, 1);
    for (int i = 0; i < 88200; ++i)
      out->writeShort(0);
  }
  auto silent = audio.reader(silence);
  require(detectTempo(*silent, "silence.wav").bpm == 0,
          "A silent even-length file does not invent a detected tempo");
  auto steady = root.getChildFile("steady.wav");
  juce::AudioBuffer<float> constant(1, 24000 * 8 + 7440);
  juce::FloatVectorOperations::fill(constant.getWritePointer(0), .25f, constant.getNumSamples());
  writeTempoAudio(steady, constant, 24000);
  auto steadyReader = audio.reader(steady);
  require(detectTempo(*steadyReader, "untagged.wav").bpm == 0,
          "Non-silent audio without repeating attacks does not invent a fallback tempo");
  auto regular = root.getChildFile("regular.wav");
  juce::AudioBuffer<float> clicks(1, int(13.123 * 24000));
  clicks.clear();
  for (int beat = 0; beat < 26; ++beat)
    for (int frame = 0; frame < 720; ++frame) {
      const double time = frame / 24000.;
      clicks.setSample(0, beat * 12000 + frame,
                       float(.7 * std::sin(2 * juce::MathConstants<double>::pi * 1000 * time) *
                             std::exp(-100 * time)));
    }
  writeTempoAudio(regular, clicks, 24000);
  auto regularReader = audio.reader(regular);
  const auto soundTouch = detectTempo(*regularReader, "untagged.wav");
  require(soundTouch.method.startsWith("SoundTouch") && std::abs(soundTouch.bpm - 120) < 2.,
          "Successful SoundTouch estimates remain the preferred audio result");
  std::cout << "PASS explicit, embedded, loop-length and audio tempo detection\n";
}
} // namespace core
