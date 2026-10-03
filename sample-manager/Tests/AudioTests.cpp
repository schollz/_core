#include "AudioProcessing.h"
#include <cmath>
#include <iostream>
namespace core {
namespace {
void tone(const File &file, int rate, int channels, int frames,
          double frequency, bool silent = false) {
  juce::WavAudioFormat format;
  std::unique_ptr<juce::OutputStream> stream(file.createOutputStream());
  auto writer = format.createWriterFor(stream, juce::AudioFormatWriterOptions()
                                                   .withSampleRate(rate)
                                                   .withNumChannels(channels)
                                                   .withBitsPerSample(24));
  require(writer != nullptr, "Fixture writer");
  juce::AudioBuffer<float> b(channels, 1024);
  for (int pos = 0; pos < frames; pos += 1024) {
    int count = std::min(1024, frames - pos);
    for (int n = 0; n < count; ++n)
      for (int c = 0; c < channels; ++c)
        b.setSample(
            c, n,
            silent ? 0.f
                   : float(0.7 * std::sin(2 * juce::MathConstants<double>::pi *
                                          frequency * (pos + n) / rate)));
    require(writer->writeFromAudioSampleBuffer(b, 0, count), "Fixture write");
  }
}
double frequency(AudioProcessing &audio, const File &f) {
  auto r = audio.reader(f);
  int margin = int(r->sampleRate * 0.2),
      count = int(r->lengthInSamples) - 2 * margin;
  require(count > 0, "Pitch test duration");
  juce::AudioBuffer<float> b(int(r->numChannels), count);
  require(r->read(&b, 0, count, margin, true, true), "Pitch read");
  int crossings = 0;
  for (int n = 1; n < count; ++n)
    if (b.getSample(0, n - 1) <= 0 && b.getSample(0, n) > 0)
      ++crossings;
  return crossings * r->sampleRate / count;
}
} // namespace
void audioTests() {
  auto folder = File::getSpecialLocation(File::tempDirectory)
                    .getChildFile("core-audio-test-" + uuid());
  require(folder.createDirectory().wasOk(), "Audio test folder");
  struct Clean {
    File f;
    ~Clean() { f.deleteRecursively(); }
  } clean{folder};
  AudioProcessing audio;
  auto peakFixture = folder.getChildFile("source-peaks.wav");
  {
    juce::WavAudioFormat format;
    std::unique_ptr<juce::OutputStream> stream(peakFixture.createOutputStream());
    auto writer = format.createWriterFor(
        stream,
        juce::AudioFormatWriterOptions().withSampleRate(48000).withNumChannels(2).withBitsPerSample(
            16));
    require(writer != nullptr, "Source waveform fixture writer");
    juce::AudioBuffer<float> data(2, 8195);
    data.clear();
    data.setSample(0, 0, -1.f);
    data.setSample(0, 4096, .5f);
    data.setSample(0, 8194, 1.f);
    data.setSample(1, 4095, -.5f);
    require(writer->writeFromAudioSampleBuffer(data, 0, 8195), "Source waveform fixture write");
  }
  const auto sourceHash = hashFile(peakFixture);
  auto sourcePeaks = audio.sourceWaveform(peakFixture);
  require(sourcePeaks->channels == 2 && sourcePeaks->sampleRate == 48000 &&
              std::abs(sourcePeaks->duration - 8195. / 48000.) < 1e-9 &&
              sourcePeaks->spectrum.empty() && sourcePeaks->peaks[0].size() == 8192 &&
              sourcePeaks->peaks[1].size() == 8192,
          "Source preview keeps native duration/channels with bounded peaks and no FFT");
  require(sourcePeaks->peaks[0][0] == -32767 && sourcePeaks->peaks[0][1] == 0 &&
              std::abs(sourcePeaks->peaks[0][2047 * 2 + 1] - 16384) <= 1 &&
              std::abs(sourcePeaks->peaks[1][2047 * 2] + 16384) <= 1 &&
              sourcePeaks->peaks[0][8190] == 0 &&
              std::abs(sourcePeaks->peaks[0][8191] - 32767) <= 1 &&
              sourcePeaks->peaks[1][8191] == 0,
          "Source peaks retain polarity, stereo separation, block boundaries and the last frame");
  require(hashFile(peakFixture) == sourceHash, "Source waveform generation leaves audio untouched");
  int peakChecks = 0;
  bool peaksCancelled = false;
  try {
    audio.sourceWaveform(peakFixture, [&] { return ++peakChecks > 2; });
  } catch (const std::exception &e) {
    peaksCancelled = String(e.what()) == "Cancelled";
  }
  require(peaksCancelled, "Source waveform decoding cancels after work has begun");
  auto source = folder.getChildFile("tone.wav");
  tone(source, 48000, 2, 96000, 440);
  auto resampled = folder.getChildFile("resampled.wav");
  audio.resample(source, resampled, 2, 44100, 1.);
  auto r = audio.reader(resampled);
  require(r->lengthInSamples == 88200, "Resampling duration");
  auto stretch = folder.getChildFile("stretched.wav");
  audio.stretch(resampled, stretch, 1.5, {{0, .5, 0}, {.5, 1, 0}});
  auto stretched = audio.reader(stretch);
  require(std::abs(stretched->lengthInSamples - 132300) <= 2,
          "Rubber Band exact offline duration");
  require(std::abs(frequency(audio, stretch) - 440) < 2,
          "Pitch preserved at slower render BPM");
  juce::AudioBuffer<float> stereo(2, 4096);
  require(stretched->read(&stereo, 0, 4096, 10000, true, true), "Stereo read");
  for (int n = 0; n < 4096; ++n)
    require(std::abs(stereo.getSample(0, n) - stereo.getSample(1, n)) < 1e-6,
            "Stereo alignment");
  auto speed = folder.getChildFile("speed.wav");
  audio.resample(source, speed, 2, 44100, 1.5);
  require(std::abs(frequency(audio, speed) - 660) < 2,
          "Explicit speed-and-pitch conversion");
  auto preview = folder.getChildFile("preview.wav"),
       padded = folder.getChildFile("padded.wav");
  audio.pcm(resampled, preview, false);
  audio.pcm(resampled, padded, true);
  auto w = card::inspect(padded);
  require(w.frames == 132300 && w.dataOffset == 44,
          "Half-second circular padding");
  auto p = audio.reader(preview);
  juce::AudioBuffer<float> b(2, 88200);
  p->read(&b, 0, 88200, 0, true, true);
  require(std::abs(b.getMagnitude(0, 88200) - std::pow(10., -6. / 20.)) <
              0.00004,
          "Minus six dB peak normalization");
  auto shortSource = folder.getChildFile("short.wav");
  tone(shortSource, 44100, 1, 13, 1000);
  require(audio.sourceWaveform(shortSource)->peaks[0].size() == 26,
          "Very short source previews retain every frame");
  auto shortOut = folder.getChildFile("short-padded.wav");
  audio.pcm(shortSource, shortOut, true);
  require(card::inspect(shortOut).frames == 44113,
          "Shorter-than-padding source");
  auto silence = folder.getChildFile("silence.wav");
  tone(silence, 44100, 1, 100, 0, true);
  auto silentPeaks = audio.sourceWaveform(silence);
  require(std::all_of(silentPeaks->peaks[0].begin(), silentPeaks->peaks[0].end(),
                      [](int16_t value) { return value == 0; }),
          "Silent imports produce a flat source waveform");
  audio.pcm(silence, folder.getChildFile("silent-padded.wav"), true);
  auto sr = audio.reader(folder.getChildFile("silent-padded.wav"));
  juce::AudioBuffer<float> sb(1, 100);
  sr->read(&sb, 0, 100, 22050, true, true);
  require(sb.getMagnitude(0, 100) == 0, "Silence remains silence");
  auto high = folder.getChildFile("ultrasonic.wav");
  tone(high, 96000, 1, 96000, 30000);
  auto filtered = folder.getChildFile("filtered.wav");
  audio.resample(high, filtered, 1, 44100, 1.);
  auto fr = audio.reader(filtered);
  juce::AudioBuffer<float> fb(1, 40000);
  fr->read(&fb, 0, 40000, 1000, true, true);
  require(fb.getRMSLevel(0, 0, 40000) < 0.001,
          "Band-limited downsampling rejects aliasing");
  auto impulses = folder.getChildFile("impulses.wav");
  {
    auto out = impulses.createOutputStream();
    card::writeHeader(*out, 88200, 44100, 1);
    for (int n = 0; n < 88200; ++n) {
      int distance = std::min(std::abs(n - 22050), std::abs(n - 66150));
      out->writeShort(short(distance < 32 ? 24000 * (32 - distance) / 32 : 0));
    }
  }
  auto aligned = folder.getChildFile("aligned.wav");
  audio.stretch(impulses, aligned, 1.5,
                {{0, .25, 0}, {.25, .75, 0}, {.75, 1, 0}});
  auto alignedReader = audio.reader(aligned);
  juce::AudioBuffer<float> peaks(1, int(alignedReader->lengthInSamples));
  require(alignedReader->read(&peaks, 0, peaks.getNumSamples(), 0, true, true),
          "Marker alignment read");
  for (int expected : {33075, 99225}) {
    int strongest = expected - 2205;
    for (int n = strongest + 1; n < expected + 2205; ++n)
      if (std::abs(peaks.getSample(0, n)) >
          std::abs(peaks.getSample(0, strongest)))
        strongest = n;
    require(std::abs(strongest - expected) <= 441,
            "Stretched attacks stay within 10 ms of mapped slice anchors");
  }
  auto projectRoot = folder.getChildFile("merge-project");
  require(projectRoot.createDirectory().wasOk(), "Merge project folder");
  Storage storage(projectRoot);
  storage.open();
  auto first = audio.import(storage, source, 0, 0);
  first.renderBpm = 80;
  first.slices = {{0, .5, 2}, {.5, 1, 1}};
  first.transients[0] = {1.};
  auto second = audio.import(storage, shortSource, 0, 1);
  require(second.slices.size() == 6,
          "Very short imports limit default slices to encodable nonempty ranges");
  second.slices = {{0, 1, 0}};
  second.oneShot = true;
  second.tempoMatch = false;
  Sample merged;
  auto mergedFile = audio.merge(projectRoot, {first, second}, merged);
  auto mergedReader = audio.reader(mergedFile);
  require(mergedReader->lengthInSamples == 132313 && merged.channels == 2,
          "Merge uses current stretch and channel settings");
  require(merged.slices.size() == 3 &&
              std::abs(merged.slices[1].start * merged.sourceDuration - 1.5) < 1e-6 &&
              std::abs(merged.slices[2].start * merged.sourceDuration - 3.) < 1e-6 &&
              merged.slices[0].type == 2 && merged.transients[0][0] == 1.5 &&
              merged.sourceBpm == 80 && merged.renderBpm == 0,
          "Merge retains ordered slices, mapped transients and audible tempo");
  require(child(projectRoot, first.source).existsAsFile() &&
              child(projectRoot, second.source).existsAsFile(),
          "Merge preserves original sources");
  auto primary = audio.render(projectRoot, first);
  const auto primaryFile = child(projectRoot, primary.preview);
  const auto primaryTime = primaryFile.getLastModificationTime();
  auto cache = primaryFile.getParentDirectory();
  require(!cache.getChildFile("companion.wav").exists(),
          "Primary render completes without generating a companion");
  // Old partial companions remain untouched and are not certified or repaired.
  for (auto name : {"companion.wav", "1.wav"})
    require(shortSource.copyFileTo(cache.getChildFile(name)), "Partial companion fixture");
  auto oldHash = hashFile(cache.getChildFile("companion.wav"));
  auto complete = audio.render(projectRoot, first);
  require(complete.frames == primary.frames && primaryFile.getLastModificationTime() == primaryTime &&
              hashFile(cache.getChildFile("companion.wav")) == oldHash &&
              !parseJson(cache.getChildFile("complete.json")).hasProperty("companion.wav"),
          "Old companion cache files are ignored while primary audio is reused");
  bool stopped = false;
  try {
    audio.stretch(resampled, folder.getChildFile("cancel.wav"), 8., {},
                  [] { return true; });
  } catch (const std::exception &) {
    stopped = true;
  }
  require(stopped, "Stretch cancellation");
  std::cout << "PASS audio duration, pitch, stereo, normalization, "
               "short/silent padding, anti-aliasing, marker alignment, merge, "
               "cancellation\n";
}
} // namespace core
