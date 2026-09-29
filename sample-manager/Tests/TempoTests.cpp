#include "AudioProcessing.h"
#include "Tempo.h"
#include <TempoData.h>
#include <iostream>

namespace core {
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
    // fits an integer-tempo regular loop. This must invoke the C++ detector.
    juce::AudioBuffer<float> data(1, int(reader->lengthInSamples) + 7440);
    data.clear();
    require(reader->read(&data, 0, int(reader->lengthInSamples), 0, true, false),
            "Read tempo fixture");
    auto extended = root.getChildFile("tail-" + String(i) + ".wav");
    juce::WavAudioFormat format;
    std::unique_ptr<juce::OutputStream> stream(extended.createOutputStream());
    auto writer = format.createWriterFor(stream, juce::AudioFormatWriterOptions()
                                                     .withSampleRate(reader->sampleRate)
                                                     .withNumChannels(1)
                                                     .withBitsPerSample(16));
    require(writer != nullptr, "Write tempo fixture with tail");
    require(writer->writeFromAudioSampleBuffer(data, 0, data.getNumSamples()), "Save tempo tail");
    writer.reset();
    auto withTail = audio.reader(extended);
    auto fallback = detectTempo(*withTail, "untagged.wav");
    require(fallback.method.startsWith("SoundTouch") && std::abs(fallback.bpm - 135) < 2.,
            "Vendored detector handles real loops whose length is not an exact beat count");
    bool cancelledRead = false;
    int cancelChecks = 0;
    try {
      detectTempo(*withTail, "untagged.wav", [&] { return ++cancelChecks > 3; });
    } catch (const std::exception &e) {
      cancelledRead = String(e.what()) == "Cancelled";
    }
    require(cancelledRead, "Tempo analysis cancels between decoded blocks");
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
  std::cout << "PASS explicit, embedded, loop-length and SoundTouch tempo detection\n";
}
} // namespace core
