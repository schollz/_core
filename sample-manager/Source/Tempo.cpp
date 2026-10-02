#include "Tempo.h"
#include <BPMDetect.h>
#include <cmath>
#include <juce_dsp/juce_dsp.h>
#include <numeric>
#include <regex>

namespace core {
namespace {
bool validTempo(double bpm) { return std::isfinite(bpm) && bpm >= 30 && bpm <= 300; }

double metadataTempo(const juce::AudioFormatReader &reader) {
  static const std::regex numeric(R"([0-9]+(\.[0-9]+)?)");
  const auto &metadata = reader.metadataValues;
  for (int i = 0; i < metadata.size(); ++i) {
    auto key = metadata.getAllKeys()[i].toLowerCase().removeCharacters(" _-");
    if (key == "bpm" || key == "tbpm" || key == "tempo" || key == "acidtempo" ||
        key == "aswgtempo") {
      const auto text = metadata.getAllValues()[i].trim();
      const auto bpm = text.getDoubleValue();
      if (std::regex_match(text.toStdString(), numeric) && validTempo(bpm))
        return bpm;
    }
  }
  const double beats = metadata[juce::AiffAudioFormat::appleBeats].getDoubleValue();
  const double bpm = beats * 60. * reader.sampleRate / double(reader.lengthInSamples);
  return validTempo(bpm) ? bpm : 0.;
}

struct LoopTempo {
  double bpm;
  int beats;
  bool regular() const { return (beats & (beats - 1)) == 0; }
};
std::vector<LoopTempo> loopTempos(double duration) {
  std::vector<LoopTempo> result;
  if (duration < .5 || duration > 128.)
    return result;
  for (int beats = 2; beats <= 512; beats += 2) {
    const double bpm = beats * 60. / duration;
    if (bpm >= 70. && bpm <= 200.)
      result.push_back({bpm, beats});
  }
  return result;
}
double roundedTempo(double bpm) {
  return std::abs(bpm - std::round(bpm)) < .15 ? std::round(bpm) : std::round(bpm * 100.) / 100.;
}

// Syncopated loops can put SoundTouch's strongest correlation at its search
// boundary, where getBpm() returns zero. In that case compare repeating attacks
// at half-, single- and double-beat intervals instead of relying on waveform
// similarity or the total file length (which may include a reverb/silent tail).
double onsetTempo(juce::AudioFormatReader &reader, int channel,
                  const std::function<bool()> &cancel) {
  const int order = std::clamp(int(std::round(std::log2(reader.sampleRate * .04))), 8, 13);
  const int size = 1 << order, hop = std::max(1, int(reader.sampleRate / 200.));
  const double frameRate = reader.sampleRate / hop;
  juce::dsp::FFT fft(order);
  std::vector<float> ring(size), window(size), spectrum(size * 2), previous(size / 2 + 1);
  juce::dsp::WindowingFunction<float>::fillWindowingTables(
      window.data(), size_t(size), juce::dsp::WindowingFunction<float>::hann, false);
  std::vector<double> flux;
  const auto frames = std::min(reader.lengthInSamples, juce::int64(reader.sampleRate * 45.));
  juce::AudioBuffer<float> input(int(reader.numChannels), 4096);
  int cursor = 0, filled = 0, sinceFrame = 0;
  bool first = true;
  for (juce::int64 offset = 0; offset < frames; offset += input.getNumSamples()) {
    cancelled(cancel);
    const int count = int(std::min<juce::int64>(input.getNumSamples(), frames - offset));
    require(reader.read(&input, 0, count, offset, true, true),
            "Cannot read audio for tempo analysis");
    for (int n = 0; n < count; ++n) {
      ring[cursor] = input.getSample(channel, n);
      cursor = (cursor + 1) % size;
      if (filled < size && ++filled < size)
        continue;
      if (++sinceFrame < hop)
        continue;
      sinceFrame = 0;
      for (int i = 0; i < size; ++i)
        spectrum[i] = ring[(cursor + i) % size] * window[i];
      fft.performFrequencyOnlyForwardTransform(spectrum.data(), true);
      double onset = 0;
      for (int i = 0; i <= size / 2; ++i) {
        const float magnitude = std::log1p(10.f * spectrum[i]);
        onset += std::max(0.f, magnitude - previous[i]);
        previous[i] = magnitude;
      }
      if (!first)
        flux.push_back(onset);
      first = false;
    }
  }
  const int lastLag = int(std::ceil(120. * frameRate / 70.)) + 1;
  if (int(flux.size()) <= 2 * lastLag)
    return 0.;
  const double mean = std::accumulate(flux.begin(), flux.end(), 0.) / flux.size();
  for (auto &value : flux)
    value -= mean;
  const double energy = std::inner_product(flux.begin(), flux.end(), flux.begin(), 0.);
  if (energy < 1e-8)
    return 0.;
  std::vector<double> correlation(size_t(lastLag + 1));
  for (int lag = 0; lag <= lastLag; ++lag) {
    cancelled(cancel);
    const auto count = flux.size() - size_t(lag);
    const double sum = std::inner_product(flux.begin(), flux.begin() + count,
                                         flux.begin() + lag, 0.);
    correlation[lag] = sum / energy * double(flux.size()) / double(count);
  }
  auto at = [&](double lag) {
    const auto i = size_t(lag);
    return correlation[i] + (lag - double(i)) * (correlation[i + 1] - correlation[i]);
  };
  int bestLag = 0;
  double bestScore = .6;
  for (int lag = int(std::ceil(60. * frameRate / 200.));
       lag <= int(std::floor(60. * frameRate / 70.)); ++lag) {
    if (correlation[lag] < .2 || correlation[lag] < correlation[lag - 1] ||
        correlation[lag] < correlation[lag + 1])
      continue;
    const double score = at(lag / 2.) + at(lag) + at(lag * 2.);
    if (score > bestScore) {
      bestScore = score;
      bestLag = lag;
    }
  }
  if (bestLag == 0)
    return 0.;
  double lag = bestLag;
  // Equally strong intervening attacks support the faster pulse, e.g. 160
  // rather than 80 BPM. Keep the same 70–200 BPM range as loop analysis.
  if (120. * frameRate / lag <= 200. && at(lag / 2.) > .9 * at(lag))
    lag /= 2.;
  return 60. * frameRate / lag;
}

TempoEstimate audioTempo(juce::AudioFormatReader &reader, const std::function<bool()> &cancel) {
  constexpr int block = 4096;
  const double duration = double(reader.lengthInSamples) / reader.sampleRate;
  if (duration < .5)
    return {};
  const auto candidates = loopTempos(duration);
  // Exact, integer-tempo loops with 2/4/8/16/... beats are common. Use this
  // inexpensive answer only when there is one plausible regular-loop tempo.
  const LoopTempo *exact = nullptr;
  int exactCount = 0;
  for (const auto &candidate : candidates)
    if (candidate.regular() && std::abs(candidate.bpm - std::round(candidate.bpm)) <= .08) {
      exact = &candidate;
      ++exactCount;
    }
  // Even the fast path checks signal energy; silence has no detectable BPM.
  juce::AudioBuffer<float> input(int(reader.numChannels), block);
  double energy = 0;
  const auto probeFrames = std::min(reader.lengthInSamples, juce::int64(reader.sampleRate * 4.));
  for (juce::int64 offset = 0; offset < probeFrames; offset += block) {
    cancelled(cancel);
    const int count = int(std::min<juce::int64>(block, probeFrames - offset));
    require(reader.read(&input, 0, count, offset, true, true),
            "Cannot read audio for tempo analysis");
    for (int channel = 0; channel < input.getNumChannels(); ++channel) {
      const auto rms = input.getRMSLevel(channel, 0, count);
      energy += double(rms) * rms * count;
    }
  }
  if (energy / std::max<juce::int64>(1, probeFrames) < 1e-10)
    return {};
  if (exactCount == 1)
    return {std::round(exact->bpm), "loop length"};
  // SoundTouch is the fallback when loop duration is ambiguous or doesn't
  // fit a regular beat count. Feed bounded blocks, never render/stretch audio.
  // Separate channel detectors avoid cancellation in opposite-phase stereo.
  std::vector<std::unique_ptr<soundtouch::BPMDetect>> detectors;
  std::vector<double> channelEnergy(size_t(reader.numChannels), 0.);
  for (unsigned channel = 0; channel < reader.numChannels; ++channel)
    detectors.push_back(std::make_unique<soundtouch::BPMDetect>(1, int(reader.sampleRate)));
  const auto limit = juce::int64(reader.sampleRate * 45.);
  const int repeats = std::max(1, std::min(8, int(std::ceil(12. / duration))));
  juce::int64 processed = 0;
  for (int repeat = 0; repeat < repeats && processed < limit; ++repeat)
    for (juce::int64 offset = 0; offset < reader.lengthInSamples && processed < limit;
         offset += block) {
      cancelled(cancel);
      const int count =
          int(std::min({juce::int64(block), reader.lengthInSamples - offset, limit - processed}));
      require(reader.read(&input, 0, count, offset, true, true),
              "Cannot read audio for tempo analysis");
      for (unsigned channel = 0; channel < reader.numChannels; ++channel) {
        const auto rms = input.getRMSLevel(int(channel), 0, count);
        channelEnergy[channel] += double(rms) * rms * count;
        detectors[channel]->inputSamples(input.getReadPointer(int(channel)), count);
      }
      processed += count;
    }
  double bpm = 0, strongest = -1;
  for (size_t channel = 0; channel < detectors.size(); ++channel) {
    const double detected = detectors[channel]->getBpm();
    if (validTempo(detected) && channelEnergy[channel] > strongest) {
      bpm = detected;
      strongest = channelEnergy[channel];
    }
  }
  if (!validTempo(bpm)) {
    const int channel = int(std::max_element(channelEnergy.begin(), channelEnergy.end()) -
                            channelEnergy.begin());
    const double onset = onsetTempo(reader, channel, cancel);
    return validTempo(onset) ? TempoEstimate{roundedTempo(onset), "onset correlation"}
                            : TempoEstimate{};
  }
  while (bpm < 70.)
    bpm *= 2.;
  // Refine a detected pulse against complete even-beat loops. SoundTouch can
  // report half-time on breakbeats; prefer a regular loop in the usual drum
  // loop range when its doubled pulse agrees with the same audio evidence.
  const LoopTempo *best = nullptr;
  double bestScore = 1e9;
  for (const auto &candidate : candidates)
    for (const double multiplier : {1., 2., .5}) {
      const double error = std::abs(candidate.bpm / (bpm * multiplier) - 1.);
      if (error > .01)
        continue;
      double score = error + (candidate.regular() ? 0. : .02);
      if (candidate.bpm < 90.)
        score += .015;
      if (multiplier != 1.)
        score += .005;
      if (score < bestScore) {
        best = &candidate;
        bestScore = score;
      }
    }
  return best ? TempoEstimate{roundedTempo(best->bpm), "SoundTouch + loop length"}
              : TempoEstimate{roundedTempo(bpm), "SoundTouch"};
}
} // namespace

double filenameTempo(const String &filename) {
  // Require a BPM label, not a sample index, year, sample rate or directory.
  static const std::regex patterns[]{
      std::regex(R"((^|[^a-z0-9.])([0-9]{2,3}(\.[0-9]+)?)[ _:-]*bpm(?=$|[^a-z0-9]))",
                 std::regex::icase),
      std::regex(R"((^|[^a-z0-9])bpm[ _:=\-]*([0-9]{2,3}(\.[0-9]+)?)(?=$|[^a-z0-9.]))",
                 std::regex::icase)};
  auto basename = filename.replaceCharacter('\\', '/').fromLastOccurrenceOf("/", false, false);
  const auto dot = basename.lastIndexOfChar('.');
  if (dot >= 0 && juce::StringArray{"wav", "aif", "aiff", "flac", "ogg", "mp3", "xrni"}.contains(
                      basename.substring(dot + 1), true))
    basename = basename.substring(0, dot);
  const auto text = basename.toStdString();
  double result = 0;
  for (const auto &pattern : patterns)
    for (auto it = std::sregex_iterator(text.begin(), text.end(), pattern);
         it != std::sregex_iterator(); ++it) {
      const double bpm = String((*it)[2].str()).getDoubleValue();
      if (!validTempo(bpm))
        continue;
      if (result > 0 && result != bpm)
        return 0;
      result = bpm;
    }
  return result;
}
TempoEstimate detectTempo(juce::AudioFormatReader &reader, const String &filename,
                          const std::function<bool()> &cancel) {
  cancelled(cancel);
  if (const auto bpm = filenameTempo(filename); bpm > 0)
    return {bpm, "filename"};
  if (const auto bpm = metadataTempo(reader); bpm > 0)
    return {bpm, "metadata"};
  return audioTempo(reader, cancel);
}
} // namespace core
