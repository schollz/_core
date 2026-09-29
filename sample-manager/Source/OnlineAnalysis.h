#pragma once
#include "AudioProcessing.h"
#include <mutex>
#include <thread>
namespace core {
class OnlineAnalysis final {
public:
  using Lanes = std::array<std::vector<double>, 3>;
  ~OnlineAnalysis();
  void start(const File &source, double duration,
             std::function<void(Lanes, String)>);
  void cancel();
  bool busy() const { return running; }
  static Lanes parse(const var &, double duration);

private:
  std::thread worker;
  std::atomic<bool> stopped{false}, running{false};
  std::mutex mutex;
  std::shared_ptr<juce::WebInputStream> stream;
};
} // namespace core
