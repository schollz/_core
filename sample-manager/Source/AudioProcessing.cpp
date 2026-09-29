#include "AudioProcessing.h"
#include "Tempo.h"
#include "Onsets/OnsetDetector.h"
#include <cmath>
#include <rubberband/RubberBandStretcher.h>
namespace core {
namespace {
constexpr int block = 4096;
juce::StringPairArray oggTags(const File &file) {
  juce::StringPairArray tags;
  auto in = file.createInputStream();
  require(in != nullptr, "Cannot read Ogg metadata");
  juce::MemoryOutputStream packet;
  auto le = [](const uint8_t *p) {
    return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 |
           uint32_t(p[3]) << 24;
  };
  // Only the first header packets, bounded independently of the audio length.
  for (int page = 0; page < 256 && in->getPosition() < 2 * 1024 * 1024;
       ++page) {
    std::array<uint8_t, 27> header{};
    if (in->read(header.data(), 27) != 27 ||
        std::memcmp(header.data(), "OggS", 4))
      break;
    std::array<uint8_t, 255> lengths{};
    int segments = header[26];
    require(in->read(lengths.data(), segments) == segments,
            "Truncated Ogg page");
    for (int s = 0; s < segments; ++s) {
      std::array<uint8_t, 255> bytes{};
      int n = lengths[size_t(s)];
      require(in->read(bytes.data(), n) == n &&
                  packet.getDataSize() + size_t(n) <= 1024 * 1024,
              "Invalid Ogg header");
      packet.write(bytes.data(), size_t(n));
      if (n == 255)
        continue;
      auto *p = static_cast<const uint8_t *>(packet.getData());
      size_t size = packet.getDataSize();
      if (size >= 11 && p[0] == 3 && std::memcmp(p + 1, "vorbis", 6) == 0) {
        size_t pos = 7;
        auto field = [&]() {
          require(pos + 4 <= size, "Truncated Vorbis comment");
          auto count = le(p + pos);
          pos += 4;
          require(count <= size - pos, "Invalid Vorbis comment length");
          auto text = String::fromUTF8(reinterpret_cast<const char *>(p + pos),
                                       int(count));
          pos += count;
          return text;
        };
        field();
        require(pos + 4 <= size, "Truncated Vorbis tags");
        auto count = le(p + pos);
        pos += 4;
        require(count <= 65536, "Too many Vorbis tags");
        for (uint32_t i = 0; i < count; ++i) {
          auto text = field();
          auto key =
              text.upToFirstOccurrenceOf("=", false, false).toUpperCase();
          auto value = text.fromFirstOccurrenceOf("=", false, false);
          if (key == "ARTIST")
            tags.set(juce::OggVorbisAudioFormat::id3artist, value);
          if (key == "ALBUM")
            tags.set(juce::OggVorbisAudioFormat::id3album, value);
          if (key == "COMMENT" || key == "DESCRIPTION")
            tags.set(juce::OggVorbisAudioFormat::id3comment,
                     tags[juce::OggVorbisAudioFormat::id3comment] + " " +
                         value);
        }
        return tags;
      }
      packet.reset();
    }
  }
  return tags;
}
constexpr double pi = 3.14159265358979323846;
void checkRead(juce::AudioFormatReader &r, juce::AudioBuffer<float> &b,
               juce::int64 offset, int count) {
  require(r.read(&b, 0, count, offset, true, true), "Audio decode failed");
  for (int c = 0; c < b.getNumChannels(); ++c)
    for (int n = 0; n < count; ++n)
      require(std::isfinite(b.getSample(c, n)), "Non-finite audio sample");
}
std::vector<Marker> boundaries(std::vector<double> points) {
  points.push_back(0);
  std::sort(points.begin(), points.end());
  points.erase(std::unique(points.begin(), points.end()), points.end());
  std::vector<Marker> out;
  for (size_t n = 0; n < points.size(); ++n)
    if (points[n] >= 0 && points[n] < 1)
      out.push_back({points[n],
                     n + 1 < points.size() ? std::min(1., points[n + 1]) : 1.,
                     0});
  return out;
}
} // namespace
AudioProcessing::AudioProcessing() { formats.registerBasicFormats(); }
std::unique_ptr<juce::AudioFormatReader>
AudioProcessing::reader(const File &f) {
  std::unique_ptr<juce::AudioFormatReader> r(formats.createReaderFor(f));
  require(r && r->lengthInSamples > 0 && r->sampleRate >= 8000 &&
              r->sampleRate <= 384000 && r->numChannels >= 1 &&
              r->numChannels <= 2,
          "Unsupported, empty, or damaged audio: " + f.getFileName());
  if (f.hasFileExtension("ogg"))
    r->metadataValues.addArray(oggTags(f));
  diagnostics::log("AUDIO", "Read " + f.getFullPathName() +
      " rate=" + String(r->sampleRate) + " channels=" + String(int(r->numChannels)) +
      " frames=" + String(r->lengthInSamples));
  return r;
}
std::unique_ptr<juce::AudioFormatWriter>
AudioProcessing::floatWriter(const File &f, int channels, double rate) {
  ensureDirectory(f.getParentDirectory());
  std::unique_ptr<juce::OutputStream> stream(f.createOutputStream());
  require(stream != nullptr, "Cannot create rendered audio");
  stream->setPosition(0);
  require(
      static_cast<juce::FileOutputStream *>(stream.get())->truncate().wasOk(),
      "Cannot truncate audio cache");
  juce::WavAudioFormat format;
  auto result = format.createWriterFor(
      stream,
      juce::AudioFormatWriterOptions()
          .withNumChannels(channels)
          .withSampleRate(rate)
          .withBitsPerSample(32)
          .withSampleFormat(
              juce::AudioFormatWriterOptions::SampleFormat::floatingPoint));
  require(result != nullptr, "Cannot create float WAV writer");
  return result;
}
std::vector<Marker>
AudioProcessing::embeddedMarkers(const File &file,
                                 const juce::AudioFormatReader &r) {
  const auto &m = r.metadataValues;
  std::vector<double> points;
  std::vector<Marker> slices;
  if (file.hasFileExtension("ogg")) {
    if (m[juce::OggVorbisAudioFormat::id3comment].containsIgnoreCase(
            "singleslice"))
      return {{0, 1, 0}};
    auto starts = juce::JSON::parse(m[juce::OggVorbisAudioFormat::id3artist]);
    auto stops = juce::JSON::parse(m[juce::OggVorbisAudioFormat::id3album]);
    auto *a = starts.getArray();
    auto *b = stops.getArray();
    double duration = double(r.lengthInSamples) / r.sampleRate;
    if (a && b && a->size() == b->size() && a->size() <= 65536)
      for (int n = 0; n < a->size(); ++n) {
        double start = double(a->getReference(n)) / duration,
               stop = double(b->getReference(n)) / duration;
        if (std::isfinite(start) && std::isfinite(stop) && start >= 0 &&
            start < stop && stop <= 1.)
          slices.push_back({start, stop, 0});
      }
    if (!slices.empty())
      return slices;
  }
  int cues = m["NumCuePoints"].getIntValue();
  require(cues <= 65536, "Too many embedded cue points");
  for (int n = 0; n < cues; ++n)
    points.push_back(
        double(m["Cue" + String(n) + "Offset"].getLargeIntValue()) /
        double(r.lengthInSamples));
  if (!points.empty())
    return boundaries(points);
  int loops = m["NumSampleLoops"].getIntValue();
  require(loops <= 65536, "Too many loop markers");
  for (int n = 0; n < loops; ++n) {
    auto prefix = "Loop" + String(n);
    double a = double(m[prefix + "Start"].getLargeIntValue()) /
               double(r.lengthInSamples),
           b = double(m[prefix + "End"].getLargeIntValue() + 1) /
               double(r.lengthInSamples);
    if (a >= 0 && a < b && b <= 1)
      slices.push_back({a, b, 0});
  }
  if (!slices.empty())
    return slices;
  auto custom = [&](const var &json, bool op1) {
    auto *a = json[op1 ? "start" : "s"].getArray();
    auto *b = json[op1 ? "end" : "e"].getArray();
    if (!a || !b || a->size() != b->size())
      return;
    for (int n = 0; n < a->size(); ++n) {
      double start = double(a->getReference(n)),
             stop = double(b->getReference(n));
      if (op1) {
        start = std::max(0., std::round(start / 4058.) - 441) /
                double(r.lengthInSamples);
        stop = (std::round(stop / 4058.) - 800) / double(r.lengthInSamples);
      }
      if (std::isfinite(start) && std::isfinite(stop) && start >= 0 &&
          start < stop && stop <= 1 &&
          (slices.empty() || slices.back().start != start))
        slices.push_back({start, stop, 0});
    }
  };
  for (const auto &key : m.getAllKeys()) {
    auto value = m[key];
    if (value.trimStart().startsWithChar('{'))
      custom(juce::JSON::parse(value), false);
  }
  if (!slices.empty())
    return slices;
  if (file.hasFileExtension("aif;aiff")) {
    auto in = file.createInputStream();
    uint8_t h[12];
    if (in && in->read(h, 12) == 12 && std::memcmp(h, "FORM", 4) == 0) {
      auto be = [](const uint8_t *p) {
        return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 |
               uint32_t(p[2]) << 8 | uint32_t(p[3]);
      };
      auto end = uint64_t(be(h + 4)) + 8;
      require(end <= uint64_t(file.getSize()), "Truncated AIFF");
      while (uint64_t(in->getPosition()) + 8 <= end) {
        uint8_t c[8];
        require(in->read(c, 8) == 8, "Truncated AIFF chunk");
        auto len = uint64_t(be(c + 4)), pos = uint64_t(in->getPosition());
        require(len <= end - pos, "Invalid AIFF chunk size");
        if ((std::memcmp(c, "APPL", 4) == 0 || std::memcmp(c, "ANNO", 4) == 0 ||
             std::memcmp(c, "COMT", 4) == 0) &&
            len < 1024 * 1024) {
          juce::MemoryBlock data;
          in->readIntoMemoryBlock(data, size_t(len));
          auto parseText = [&](const String &text) {
            auto start = text.indexOfChar('{');
            if (start < 0)
              return;
            int depth = 0;
            bool quoted = false, escaped = false;
            for (int n = start; n < text.length(); ++n) {
              auto ch = text[n];
              if (quoted) {
                if (escaped)
                  escaped = false;
                else if (ch == '\\')
                  escaped = true;
                else if (ch == '"')
                  quoted = false;
              } else if (ch == '"')
                quoted = true;
              else if (ch == '{')
                ++depth;
              else if (ch == '}' && --depth == 0) {
                auto json = juce::JSON::parse(text.substring(start, n + 1));
                custom(json, json.hasProperty("start"));
                return;
              }
            }
          };
          auto *raw = static_cast<const uint8_t *>(data.getData());
          if (std::memcmp(c, "COMT", 4) == 0 && len >= 2) {
            auto be16 = [](const uint8_t *p) {
              return unsigned(p[0]) * 256 + p[1];
            };
            size_t offset = 2;
            for (unsigned n = 0; n < be16(raw); ++n) {
              require(offset + 8 <= len, "Truncated AIFF comment");
              auto count = be16(raw + offset + 6);
              offset += 8;
              require(offset + count <= len, "Truncated AIFF comment text");
              parseText(String::fromUTF8(
                  reinterpret_cast<const char *>(raw + offset), int(count)));
              offset += count + (count & 1);
            }
          } else {
            size_t offset = std::memcmp(c, "APPL", 4) == 0 ? 4 : 0;
            if (len > offset)
              parseText(
                  String::fromUTF8(reinterpret_cast<const char *>(raw + offset),
                                   int(len - offset)));
          }
        }
        in->setPosition(juce::int64(pos + len + (len & 1)));
      }
    }
  }
  return slices;
}
Sample AudioProcessing::import(Storage &storage, const File &input, int bank,
                               int slot, const Cancel &cancel) {
  diagnostics::Scope trace("AUDIO", "Import " + input.getFullPathName() +
      " bank=" + String(bank + 1) + " slot=" + String(slot + 1));
  cancelled(cancel);
  Sample s;
  s.bank = bank;
  s.slot = slot;
  s.name = input.getFileName();
  s.originalFilename = input.getFileName();
  File audio = input;
  std::vector<double> xrniPoints;
  if (input.hasFileExtension("xrni")) {
    juce::ZipFile zip(input);
    int xmlIndex = zip.getIndexOfFileName("Instrument.xml");
    require(xmlIndex >= 0, "XRNI has no Instrument.xml");
    std::unique_ptr<juce::InputStream> xmlStream(
        zip.createStreamForEntry(xmlIndex));
    require(xmlStream != nullptr, "Cannot read XRNI instrument");
    auto xml = juce::parseXML(xmlStream->readEntireStreamAsString());
    require(xml != nullptr, "Invalid XRNI XML");
    auto *samples = xml->getChildByName("Samples");
    auto *sample = samples ? samples->getChildByName("Sample") : nullptr;
    require(sample != nullptr, "XRNI contains no sample");
    String filename = sample->getChildElementAllSubText("FileName", {});
    int audioIndex = -1;
    for (int n = 0; n < zip.getNumEntries(); ++n) {
      auto name = zip.getEntry(n)->filename;
      if (name.startsWith("SampleData/") &&
          (filename.isEmpty() || name.endsWith(filename)) &&
          !name.endsWithChar('/')) {
        audioIndex = n;
        break;
      }
    }
    require(audioIndex >= 0, "XRNI sample data is missing");
    auto *entry = zip.getEntry(audioIndex);
    require(entry->uncompressedSize > 0 && entry->uncompressedSize <= INT32_MAX,
            "XRNI sample size is unsupported");
    audio = child(storage.root,
                  ".core-manager/cache/xrni-" + s.id + "." +
                      entry->filename.fromLastOccurrenceOf(".", false, false));
    std::unique_ptr<juce::InputStream> stream(
        zip.createStreamForEntry(audioIndex));
    auto out = audio.createOutputStream();
    require(stream && out, "Cannot extract XRNI audio");
    require(out->writeFromInputStream(*stream, entry->uncompressedSize) ==
                entry->uncompressedSize,
            "Truncated XRNI audio");
    out->flush();
    require(out->getStatus().wasOk(), "Cannot save XRNI audio");
    out.reset();
    if (auto *markers = sample->getChildByName("SliceMarkers"))
      for (auto *marker : markers->getChildIterator())
        xrniPoints.push_back(
            marker->getChildElementAllSubText("SamplePosition", "0")
                .getDoubleValue());
    String archiveHash;
    s.originalArchive =
        storage.importSource(input, archiveHash); // retain the actual imported original too
  }
  auto r = reader(audio);
  s.channels = int(r->numChannels);
  s.sourceDuration = double(r->lengthInSamples) / r->sampleRate;
  {
    diagnostics::Scope tempoTrace("TEMPO", "Detect " + input.getFileName());
    const auto estimate = detectTempo(*r, input.getFileName(), cancel);
    if (estimate.bpm > 0)
      s.sourceBpm = estimate.bpm;
    diagnostics::log("TEMPO", "Source BPM=" + String(s.sourceBpm, 2) +
                                  " method=" + estimate.method + " file=" + input.getFileName());
  }
  s.slices = embeddedMarkers(audio, *r);
  if (!xrniPoints.empty()) {
    for (auto &x : xrniPoints)
      x /= double(r->lengthInSamples);
    s.slices = boundaries(xrniPoints);
  }
  auto comment = r->metadataValues["comment"] +
                 r->metadataValues[juce::OggVorbisAudioFormat::id3comment];
  if (comment.containsIgnoreCase("oneshot")) {
    s.oneShot = true;
    s.tempoMatch = false;
    s.playMode = 1;
  }
  if (s.slices.empty()) {
    // Leave the source's render anchors unchanged: default editor slices are
    // metadata, just like clicking Even slices after importing.
    const auto outputFrames = juce::int64(std::llround(s.sourceDuration * s.rate));
    const auto alignedRanges = outputFrames * s.channels / 2;
    const int count = int(juce::jlimit<juce::int64>(1, 16, alignedRanges));
    for (int n = 0; n < count; ++n)
      s.slices.push_back({double(n) / count, double(n + 1) / count, 0});
    s.spliceVariable = false;
  } else
    s.renderAnchors = s.slices;
  s.updateSpliceTrigger(SpliceTimingCalculation::initialImport);
  s.source = storage.importSource(audio, s.sourceHash);
  cancelled(cancel);
  return s;
}
std::shared_ptr<const zv::Wave> AudioProcessing::sourceWaveform(const File &file,
                                                                const Cancel &cancel) {
  diagnostics::Scope trace("WAVEFORM", "Prepare imported source " + file.getFullPathName());
  cancelled(cancel);
  auto r = reader(file);
  auto wave = std::make_shared<zv::Wave>();
  wave->channels = int(r->numChannels);
  wave->sampleRate = int(r->sampleRate);
  wave->duration = double(r->lengthInSamples) / r->sampleRate;
  const auto bins = int(std::min<juce::int64>(4096, r->lengthInSamples));
  wave->peaks.resize(size_t(wave->channels));
  for (auto &channel : wave->peaks) {
    channel.resize(size_t(bins * 2));
    for (int bin = 0; bin < bins; ++bin) {
      channel[size_t(bin * 2)] = 32767;
      channel[size_t(bin * 2 + 1)] = -32768;
    }
  }
  // Sequential decoded blocks keep compressed imports cheap. Only min/max
  // peaks are needed for the editor: no conversion, padding, FFT or stretching.
  juce::AudioBuffer<float> buffer(wave->channels, block);
  int bin = 0;
  auto binEnd = r->lengthInSamples / bins;
  for (juce::int64 offset = 0; offset < r->lengthInSamples; offset += block) {
    cancelled(cancel);
    const int count = int(std::min<juce::int64>(block, r->lengthInSamples - offset));
    checkRead(*r, buffer, offset, count);
    for (int frame = 0; frame < count; ++frame) {
      if (offset + frame >= binEnd) {
        ++bin;
        binEnd = juce::int64(bin + 1) * r->lengthInSamples / bins;
      }
      for (int channel = 0; channel < wave->channels; ++channel) {
        const auto value = int16_t(
            std::lround(juce::jlimit(-1.f, 1.f, buffer.getSample(channel, frame)) * 32767.f));
        auto &peaks = wave->peaks[size_t(channel)];
        peaks[size_t(bin * 2)] = std::min(peaks[size_t(bin * 2)], value);
        peaks[size_t(bin * 2 + 1)] = std::max(peaks[size_t(bin * 2 + 1)], value);
      }
    }
  }
  return wave;
}
void AudioProcessing::resample(const File &from, const File &to, int channels, int rate,
                               double speed, const Cancel &cancel) {
  auto r = reader(from);
  require(speed > 0 && std::isfinite(speed), "Invalid speed");
  auto writer = floatWriter(to, channels, rate);
  double step = r->sampleRate / rate * speed;
  auto frames = juce::int64(std::llround(double(r->lengthInSamples) / step));
  require(frames > 0 && uint64_t(frames) <= uint64_t(INT32_MAX) / (channels * 2),
          "Rendered sample exceeds device limit");
  // Windowed-sinc low-pass. Wider support follows the downsampling ratio;
  // bounded blocks.
  double cutoff = std::min(1., 1. / step) * 0.95;
  int radius = int(std::ceil(32. / cutoff));
  juce::AudioBuffer<float> output(channels, block);
  for (juce::int64 offset = 0; offset < frames; offset += block) {
    cancelled(cancel);
    int count = int(std::min<juce::int64>(block, frames - offset));
    auto first = juce::int64(std::floor(double(offset) * step)) - radius;
    auto last =
        juce::int64(std::floor(double(offset + count - 1) * step)) + radius + 1;
    auto begin = std::max<juce::int64>(0, first),
         end = std::min(r->lengthInSamples, last);
    int available = int(end - begin);
    require(available > 0, "Resampler outside source");
    juce::AudioBuffer<float> input(int(r->numChannels), available);
    checkRead(*r, input, begin, available);
    for (int n = 0; n < count; ++n) {
      double pos = double(offset + n) * step;
      auto centre = juce::int64(std::floor(pos));
      std::array<double, 2> sums{};
      double weightSum = 0;
      for (int k = -radius + 1; k <= radius; ++k) {
        auto index = centre + k;
        double distance = pos - double(index), x = distance * cutoff;
        double sinc = std::abs(x) < 1e-10 ? 1. : std::sin(pi * x) / (pi * x);
        double window = 0.42 + 0.5 * std::cos(pi * distance / radius) +
                        0.08 * std::cos(2 * pi * distance / radius);
        double w = sinc * window * cutoff;
        weightSum += w;
        auto bounded =
            juce::jlimit<juce::int64>(0, r->lengthInSamples - 1, index);
        int local = int(bounded - begin);
        for (int c = 0; c < int(r->numChannels); ++c)
          sums[size_t(c)] += input.getSample(c, local) * w;
      }
      if (channels == 1)
        output.setSample(0, n,
                         float((sums[0] + (r->numChannels == 2 ? sums[1] : 0)) /
                               (weightSum * r->numChannels)));
      else
        for (int c = 0; c < 2; ++c)
          output.setSample(
              c, n,
              float(sums[size_t(std::min(c, int(r->numChannels) - 1))] /
                    weightSum));
    }
    require(writer->writeFromAudioSampleBuffer(output, 0, count),
            "Resampled audio write failed");
  }
  require(writer->flush(), "Cannot flush resampled audio");
}
void AudioProcessing::stretch(const File &from, const File &to, double ratio,
                              const std::vector<Marker> &markers,
                              const Cancel &cancel) {
  auto r = reader(from);
  int ch = int(r->numChannels);
  require(ratio > 0 && std::isfinite(ratio), "Invalid time ratio");
  require(double(r->lengthInSamples) * ratio * ch * 2 <= INT32_MAX,
          "Stretched sample exceeds firmware limit");
  using RB = RubberBand::RubberBandStretcher;
  RB rb(size_t(r->sampleRate), size_t(ch),
        RB::OptionProcessOffline | RB::OptionEngineFiner |
            RB::OptionChannelsTogether | RB::OptionThreadingNever,
        ratio, 1.0);
  rb.setMaxProcessSize(block);
  rb.setExpectedInputDuration(size_t(r->lengthInSamples));
  std::map<size_t, size_t> keyframes;
  for (auto marker : markers)
    for (double n : {marker.start, marker.stop})
      if (n > 0 && n < 1) {
        auto f = size_t(std::llround(n * double(r->lengthInSamples)));
        keyframes[f] = size_t(std::llround(double(f) * ratio));
      }
  rb.setKeyFrameMap(keyframes);
  juce::AudioBuffer<float> input(ch, block), output(ch, block);
  std::array<const float *, 2> ins{};
  std::array<float *, 2> outs{};
  for (int c = 0; c < ch; ++c) {
    ins[size_t(c)] = input.getReadPointer(c);
    outs[size_t(c)] = output.getWritePointer(c);
  }
  for (juce::int64 pos = 0; pos < r->lengthInSamples; pos += block) {
    cancelled(cancel);
    int count = int(std::min<juce::int64>(block, r->lengthInSamples - pos));
    checkRead(*r, input, pos, count);
    rb.study(ins.data(), size_t(count), pos + count == r->lengthInSamples);
  }
  auto writer = floatWriter(to, ch, r->sampleRate);
  uint64_t written = 0;
  auto drain = [&] {
    while (rb.available() > 0) {
      cancelled(cancel);
      size_t n =
          rb.retrieve(outs.data(), size_t(std::min(block, rb.available())));
      require(n > 0, "Rubber Band failed to drain");
      require(writer->writeFromAudioSampleBuffer(output, 0, int(n)),
              "Stretched audio write failed");
      written += n;
    }
  };
  for (juce::int64 pos = 0; pos < r->lengthInSamples; pos += block) {
    cancelled(cancel);
    int count = int(std::min<juce::int64>(block, r->lengthInSamples - pos));
    checkRead(*r, input, pos, count);
    rb.process(ins.data(), size_t(count), pos + count == r->lengthInSamples);
    drain();
  }
  drain();
  require(rb.available() == -1 && written > 0,
          "Rubber Band did not complete output");
  require(writer->flush(), "Cannot flush stretched audio");
}
void AudioProcessing::pcm(const File &from, const File &to, bool pad,
                          const Cancel &cancel) {
  auto r = reader(from);
  int ch = int(r->numChannels), rate = int(r->sampleRate);
  juce::AudioBuffer<float> buffer(ch, block);
  float peak = 0;
  for (juce::int64 pos = 0; pos < r->lengthInSamples; pos += block) {
    cancelled(cancel);
    int n = int(std::min<juce::int64>(block, r->lengthInSamples - pos));
    checkRead(*r, buffer, pos, n);
    for (int c = 0; c < ch; ++c)
      peak = std::max(peak, buffer.getMagnitude(c, 0, n));
  }
  double gain = peak > 1e-12 ? std::pow(10., -6. / 20.) / peak : 1.;
  auto padding = pad ? rate / 2 : 0;
  auto frames = r->lengthInSamples + 2 * padding;
  auto out = to.createOutputStream();
  require(out != nullptr, "Cannot write PCM output");
  out->setPosition(0);
  out->truncate();
  card::writeHeader(*out, uint64_t(frames), rate, ch);
  // Modulo indexing makes half-second circular padding work even for very short
  // samples.
  juce::MemoryOutputStream encoded(size_t(block * ch * 2));
  for (juce::int64 pos = 0; pos < frames;) {
    cancelled(cancel);
    auto source = (pos - padding) % r->lengthInSamples;
    if (source < 0)
      source += r->lengthInSamples;
    int n = int(std::min<juce::int64>(
        {block, frames - pos, r->lengthInSamples - source}));
    checkRead(*r, buffer, source, n);
    encoded.reset();
    for (int f = 0; f < n; ++f)
      for (int c = 0; c < ch; ++c) {
        double v = juce::jlimit(-1., 1., double(buffer.getSample(c, f)) * gain);
        encoded.writeShort(short(std::lround(v * 32767.)));
      }
    require(out->write(encoded.getData(), encoded.getDataSize()),
            "PCM write failed (check disk space)");
    pos += n;
  }
  out->flush();
  require(out->getStatus().wasOk(), "Cannot flush PCM output");
}
Rendered AudioProcessing::render(const File &root, const Sample &sample,
                                 const Cancel &cancel) {
  diagnostics::Scope trace("AUDIO", "Render sample=" + sample.id +
      " name=" + sample.name + " rate=" + String(sample.rate) +
      " channels=" + String(sample.channels) + " ratio=" + String(sample.ratio(), 6) +
      " preserve_pitch=" + String(sample.preservePitch ? 1 : 0));
  cancelled(cancel);
  require(hashFile(child(root, sample.source)) == sample.sourceHash,
          "Immutable source was modified");
  Rendered result;
  result.key = audioKey(sample);
  auto relative = ".core-manager/cache/" + result.key + "/";
  auto dir = child(root, relative);
  ensureDirectory(dir);
  auto ready = dir.getChildFile("complete.json");
  var j = object();
  if (ready.existsAsFile()) {
    try {
      j = parseJson(ready);
    } catch (const std::exception &) {
    }
  }
  auto valid = [&](const String &name) {
    auto f = dir.getChildFile(name);
    return f.existsAsFile() &&
           j[juce::Identifier(name)].toString().isNotEmpty() &&
           hashFile(f) == j[juce::Identifier(name)].toString();
  };
  auto converted = dir.getChildFile("converted.wav"),
       stretched = dir.getChildFile("stretched.wav");
  bool stretching = sample.preservePitch && std::abs(sample.ratio() - 1) > 1e-9;
  File renderSource = stretching ? stretched : converted;
  result.preview = relative + "preview.wav";
  result.padded = relative + "0.wav";
  if (!valid("preview.wav") || !valid("0.wav") ||
      !valid(renderSource.getFileName())) {
    diagnostics::log("AUDIO", "Render cache miss; converting source");
    resample(child(root, sample.source), converted, sample.channels,
             sample.rate, sample.preservePitch ? 1. : 1. / sample.ratio(),
             cancel);
    if (stretching)
      stretch(converted, stretched, sample.ratio(), sample.renderAnchors,
              cancel);
    pcm(renderSource, child(root, result.preview), false, cancel);
    pcm(renderSource, child(root, result.padded), true, cancel);
    j = object();
  }
  result.frames = card::inspect(child(root, result.preview)).frames;
  if (!(sample.oneShot && !sample.tempoMatch)) {
    result.companionPreview = relative + "companion.wav";
    result.companionPadded = relative + "1.wav";
    if (!valid("companion.wav") || !valid("1.wav")) {
      diagnostics::log("AUDIO", "Rendering eight-times companion");
      auto longFile = dir.getChildFile("long.wav");
      stretch(renderSource, longFile, 8., sample.renderAnchors, cancel);
      pcm(longFile, child(root, result.companionPreview), false, cancel);
      pcm(longFile, child(root, result.companionPadded), true, cancel);
    }
    result.companionFrames =
        card::inspect(child(root, result.companionPreview)).frames;
  }
  for (auto name :
       {String("preview.wav"), String("0.wav"), renderSource.getFileName(),
        String("companion.wav"), String("1.wav")})
    if (dir.getChildFile(name).existsAsFile())
      put(j, name, hashFile(dir.getChildFile(name)));
  put(j, "frames", juce::int64(result.frames));
  put(j, "companionFrames", juce::int64(result.companionFrames));
  cancelled(cancel);
  durableJson(ready, j);
  diagnostics::log("AUDIO", "Rendered frames=" + String(juce::int64(result.frames)) +
      " companion_frames=" + String(juce::int64(result.companionFrames)));
  return result;
}
std::vector<Marker> AudioProcessing::detect(const File &source, String method,
                                            double spacingMs,
                                            const Cancel &cancel,
                                            int targetSlices) {
  diagnostics::Scope trace("SLICES", "Detect " + source.getFullPathName() +
      " method=" + method + " target=" + String(targetSlices));
  require(targetSlices >= 0 && targetSlices <= 1024, "Invalid slice target");
  require(std::isfinite(spacingMs) && spacingMs >= 0,
          "Minimum slice spacing must be zero or greater");
  require(juce::StringArray{"hfc", "energy", "complex", "phase", "wphase",
                            "specdiff", "kl", "mkl", "specflux"}
              .contains(method),
          "Unsupported onset method");
  auto r = reader(source);
  if (targetSlices == 1)
    return {{0., 1., 0}};
  Onsets::OnsetDetector detector(method.toStdString(), 1024, 256,
                                 size_t(r->sampleRate));
  // As in amenbreak's targeted slicer, collect sensitive candidates, then
  // choose the strongest attacks instead of taking the first N transients.
  if (targetSlices > 0)
    detector.setThreshold(0.02);
  detector.setMinioiMs(spacingMs);
  Onsets::Fvec frame(256), onset(1);
  juce::AudioBuffer<float> input(int(r->numChannels), 256);
  std::vector<double> points;
  for (juce::int64 pos = 0; pos < r->lengthInSamples; pos += 256) {
    cancelled(cancel);
    int n = int(std::min<juce::int64>(256, r->lengthInSamples - pos));
    input.clear();
    checkRead(*r, input, pos, n);
    for (int f = 0; f < 256; ++f) {
      double sum = 0;
      for (int c = 0; c < input.getNumChannels(); ++c)
        sum += input.getSample(c, f);
      frame.getData()[f] = sum / input.getNumChannels();
    }
    detector.processFrame(frame, onset);
    if (onset.getData()[0] > 0)
      points.push_back(detector.getLastOnsetS());
  }
  // Adapted amenbreak SliceAnalyzer variance refinement, bounded to 15 ms.
  int window = int(r->sampleRate * 0.015);
  juce::AudioBuffer<float> refine(int(r->numChannels), window);
  for (auto &point : points) {
    cancelled(cancel);
    auto start = std::max<juce::int64>(0, juce::int64(point * r->sampleRate) -
                                              window / 2);
    int n = int(std::min<juce::int64>(window, r->lengthInSamples - start));
    if (n < 11)
      continue;
    checkRead(*r, refine, start, n);
    std::vector<double> sums(size_t(n + 1), 0.), squares(size_t(n + 1), 0.);
    for (int j = 0; j < n; ++j) {
      double v = 0;
      for (int c = 0; c < refine.getNumChannels(); ++c)
        v += refine.getSample(c, j);
      v /= refine.getNumChannels();
      sums[size_t(j + 1)] = sums[size_t(j)] + v;
      squares[size_t(j + 1)] = squares[size_t(j)] + v * v;
    }
    double best = -1e100;
    int at = 0;
    for (int j = 5; j < n - 5; ++j) {
      double l = squares[size_t(j)] / j - std::pow(sums[size_t(j)] / j, 2),
             rr = (squares[size_t(n)] - squares[size_t(j)]) / (n - j) -
                  std::pow((sums[size_t(n)] - sums[size_t(j)]) / (n - j), 2),
             diff = rr - l;
      if (diff > best) {
        best = diff;
        at = j;
      }
    }
    point = double(start + at) / r->sampleRate;
  }
  const double duration = double(r->lengthInSamples) / r->sampleRate;
  const double minimumSpacing = std::max(1. / r->sampleRate, spacingMs / 1000.);
  struct Candidate {
    double time, energy;
  };
  std::vector<Candidate> candidates;
  const int energyWindow = std::max(1, int(r->sampleRate * 0.05));
  juce::AudioBuffer<float> energy(int(r->numChannels), energyWindow);
  for (auto point : points) {
    cancelled(cancel);
    if (point < minimumSpacing || point >= duration)
      continue;
    double strength = 0.;
    if (targetSlices > 0) {
      auto start = juce::int64(point * r->sampleRate);
      int n = int(std::min<juce::int64>(energyWindow, r->lengthInSamples - start));
      if (n <= 0)
        continue;
      checkRead(*r, energy, start, n);
      for (int c = 0; c < energy.getNumChannels(); ++c)
        for (int j = 0; j < n; ++j) {
          const double v = energy.getSample(c, j);
          strength += v * v;
        }
      strength /= n * energy.getNumChannels();
    }
    candidates.push_back({point, strength});
  }
  std::sort(candidates.begin(), candidates.end(),
            [targetSlices](const Candidate &a, const Candidate &b) {
              if (targetSlices > 0 && a.energy != b.energy)
                return a.energy > b.energy;
              return a.time < b.time;
            });
  // Zero is the first slice, so N slices need at most N - 1 internal cuts.
  std::vector<double> spaced{0.};
  for (const auto &candidate : candidates) {
    cancelled(cancel);
    auto next = std::lower_bound(spaced.begin(), spaced.end(), candidate.time);
    if (candidate.time - *(next - 1) < minimumSpacing ||
        (next != spaced.end() && *next - candidate.time < minimumSpacing) ||
        (targetSlices > 0 && duration - candidate.time < minimumSpacing))
      continue;
    spaced.insert(next, candidate.time);
    if (targetSlices > 0 && int(spaced.size()) >= targetSlices)
      break;
  }
  for (auto &p : spaced)
    p /= duration;
  diagnostics::log("SLICES", "Candidates=" + String(int(candidates.size())) +
      " actual_slices=" + String(int(spaced.size())) + " target=" + String(targetSlices));
  return boundaries(spaced);
}
File AudioProcessing::merge(const File &root,
                            const std::vector<Sample> &samples, Sample &merged,
                            const Cancel &cancel) {
  require(samples.size() >= 2, "Select at least two samples to merge");
  auto path = child(root, ".core-manager/cache/merge-" + merged.id + ".wav");
  int channels = 1;
  double total = 0;
  for (const auto &s : samples) {
    require(!s.protectedEntry, "Cannot merge damaged entries");
    channels = std::max(channels, s.channels);
  }
  // Merge what the user hears, using each current rendering and channel mode.
  // Originals remain separate immutable sources; the merge is a new source.
  std::vector<File> parts;
  std::vector<double> durations;
  total = 0;
  for (const auto &s : samples) {
    cancelled(cancel);
    auto primary = s;
    primary.oneShot = true;
    primary.tempoMatch = false;
    auto rendered = render(root, primary, cancel);
    auto temp = path.getSiblingFile("merge-part-" + s.id + ".wav");
    resample(child(root, rendered.preview), temp, channels, 44100, 1., cancel);
    auto r = reader(temp);
    double duration = double(r->lengthInSamples) / r->sampleRate;
    parts.push_back(temp);
    durations.push_back(duration);
    total += duration;
  }
  auto writer = floatWriter(path, channels, 44100);
  merged.slices.clear();
  for (auto &lane : merged.transients)
    lane.clear();
  double offset = 0;
  juce::AudioBuffer<float> buffer(channels, block);
  for (size_t i = 0; i < samples.size(); ++i) {
    const auto &s = samples[i];
    const double duration = durations[i];
    auto r = reader(parts[i]);
    for (juce::int64 pos = 0; pos < r->lengthInSamples; pos += block) {
      cancelled(cancel);
      int n = int(std::min<juce::int64>(block, r->lengthInSamples - pos));
      checkRead(*r, buffer, pos, n);
      require(writer->writeFromAudioSampleBuffer(buffer, 0, n),
              "Cannot write merged audio");
    }
    for (auto marker : s.slices)
      merged.slices.push_back({(offset + marker.start * duration) / total,
                               (offset + marker.stop * duration) / total,
                               marker.type});
    for (size_t l = 0; l < 3; ++l)
      for (auto t : s.transients[l])
        merged.transients[l].push_back(offset +
                                       t * duration / s.sourceDuration);
    offset += duration;
    r.reset();
    require(parts[i].deleteFile(), "Cannot remove temporary merge audio");
  }
  merged.sourceBpm = samples.front().renderBpm > 0 ? samples.front().renderBpm
                                                   : samples.front().sourceBpm;
  merged.renderBpm = 0;
  require(writer->flush(), "Cannot flush merged source");
  merged.renderAnchors = merged.slices;
  merged.channels = channels;
  merged.sourceDuration = total;
  return path;
}
} // namespace core
