#pragma once
#include "Project.h"
#include <condition_variable>
#include <mutex>
#include <thread>

namespace core {
struct DefaultSamplesEntry {
  String name, filename, url, sha256;
  juce::int64 bytes = 0;
  std::vector<int> bankSizes;
};
const std::array<DefaultSamplesEntry, 2> &defaultSamplesCatalog();

// Installs a separate editable copy; it never writes into an open project.
class DefaultSamplesInstaller final {
public:
  enum class Status {
    idle,
    downloading,
    verifying,
    extracting,
    validating,
    succeeded,
    failed,
    cancelled
  };
  struct State {
    Status status = Status::idle;
    String message;
    File project;
    juce::int64 received = 0, total = 0;
    int files = 0, totalFiles = 0;
    bool cached = false;
    bool active() const {
      return status == Status::downloading || status == Status::verifying ||
             status == Status::extracting || status == Status::validating;
    }
  };
  ~DefaultSamplesInstaller();
  void start(DefaultSamplesEntry, File downloads, File cache, int timeoutMs = 30 * 60 * 1000);
  void cancel();
  State snapshot() const;

private:
  void run(DefaultSamplesEntry, File downloads, File cache, int timeoutMs);
  void stage(Status, String message);
  mutable std::mutex mutex;
  std::condition_variable changed;
  State state;
  std::shared_ptr<juce::WebInputStream> stream;
  std::thread worker;
  std::atomic<bool> stopped{false}, timedOut{false};
};
} // namespace core
