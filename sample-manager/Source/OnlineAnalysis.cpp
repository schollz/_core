#include "OnlineAnalysis.h"
namespace core {
OnlineAnalysis::~OnlineAnalysis() {
  cancel();
  if (worker.joinable())
    worker.join();
}
void OnlineAnalysis::cancel() {
  stopped = true;
  std::lock_guard<std::mutex> lock(mutex);
  if (stream)
    stream->cancel();
}
OnlineAnalysis::Lanes OnlineAnalysis::parse(const var &response,
                                            double duration) {
  require(response.isObject(), "Invalid drum-analysis response");
  Lanes lanes;
  const char *keys[]{"a", "b", "c"};
  for (size_t lane = 0; lane < 3; ++lane) {
    auto *a = response[keys[lane]].getArray();
    require(a && a->size() <= 65536, "Invalid transient lane response");
    for (const auto &value : *a) {
      require(value.isInt() || value.isInt64() || value.isDouble(),
              "Invalid transient position");
      double position = double(value) / 44100.;
      require(std::isfinite(position) && position >= 0 && position <= duration,
              "Drum marker is outside the uploaded source");
      if (position > 0)
        lanes[lane].push_back(position);
    }
    std::sort(lanes[lane].begin(), lanes[lane].end());
    lanes[lane].erase(std::unique(lanes[lane].begin(), lanes[lane].end()),
                      lanes[lane].end());
  }
  return lanes;
}
void OnlineAnalysis::start(const File &source, double duration,
                           std::function<void(Lanes, String)> done) {
  require(!running, "Analysis is already running");
  if (worker.joinable())
    worker.join();
  stopped = false;
  running = true;
  worker = std::thread([this, source, duration, done = std::move(done)] {
    Lanes lanes;
    String error;
    auto dir = File::getSpecialLocation(File::tempDirectory)
                   .getChildFile("core-drum-analysis-" + uuid());
    dir.createDirectory();
    try {
      AudioProcessing audio;
      auto mono = dir.getChildFile("mono.wav");
      auto cancel = [this] { return stopped.load(); };
      audio.resample(source, mono, 1, 44100, 1., cancel);
      auto reader = audio.reader(mono);
      auto ogg = dir.getChildFile("request.ogg");
      std::unique_ptr<juce::OutputStream> output(ogg.createOutputStream());
      juce::OggVorbisAudioFormat format;
      auto writer =
          format.createWriterFor(output, juce::AudioFormatWriterOptions()
                                             .withSampleRate(44100)
                                             .withNumChannels(1)
                                             .withQualityOptionIndex(5));
      require(writer != nullptr, "Cannot encode drum-analysis request");
      juce::AudioBuffer<float> block(1, 4096);
      for (juce::int64 pos = 0; pos < reader->lengthInSamples; pos += 4096) {
        cancelled(cancel);
        int n = int(std::min<juce::int64>(4096, reader->lengthInSamples - pos));
        require(reader->read(&block, 0, n, pos, true, false),
                "Cannot decode analysis audio");
        require(writer->writeFromAudioSampleBuffer(block, 0, n),
                "Cannot write analysis request");
      }
      writer.reset();
      cancelled(cancel);
      juce::MemoryBlock body;
      require(ogg.getSize() <= 256 * 1024 * 1024 && ogg.loadFileAsData(body),
              "Analysis upload exceeds 256 MB");
      // The application's only network request. Reached only by the explicit UI
      // action.
      auto request = std::make_shared<juce::WebInputStream>(
          juce::URL("https://tool.getectocore.com/drumextract")
              .withPOSTData(body),
          true);
      request->withCustomRequestCommand("PUT")
          .withExtraHeaders("Content-Type: application/octet-stream\r\n")
          .withConnectionTimeout(30000)
          .withNumRedirectsToFollow(0);
      {
        std::lock_guard<std::mutex> lock(mutex);
        stream = request;
        if (stopped)
          stream->cancel();
      }
      require(request->connect(nullptr),
              "Drum analysis could not connect. Local markers are unchanged.");
      require(request->getStatusCode() == 200,
              "Drum-analysis service returned HTTP " +
                  String(request->getStatusCode()));
      juce::MemoryOutputStream response;
      std::array<char, 4096> bytes{};
      double began = juce::Time::getMillisecondCounterHiRes();
      while (!request->isExhausted()) {
        cancelled(cancel);
        require(juce::Time::getMillisecondCounterHiRes() - began < 60000,
                "Drum-analysis response timed out");
        int n = request->read(bytes.data(), int(bytes.size()));
        require(n >= 0, "Drum-analysis read failed");
        if (n == 0)
          break;
        require(response.getDataSize() + size_t(n) <= 1024 * 1024,
                "Drum-analysis response is too large");
        response.write(bytes.data(), size_t(n));
      }
      cancelled(cancel);
      lanes = parse(juce::JSON::parse(response.toString()), duration);
    } catch (const std::exception &e) {
      error = e.what();
    }
    {
      std::lock_guard<std::mutex> lock(mutex);
      stream.reset();
    }
    dir.deleteRecursively();
    running = false;
    juce::MessageManager::callAsync(
        [done, lanes = std::move(lanes), error] { done(lanes, error); });
  });
}
} // namespace core
