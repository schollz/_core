#pragma once
#include "Uf2.h"
#include <condition_variable>
#include <mutex>
#include <thread>

namespace core {
enum class FirmwareHardware { ezeptocore, zeptocore, ectocore };
enum class FirmwareBuild {
  normal,
  lowLatency,
  ultraLowLatency,
  noOverclocking,
  noOverclockingLowLatency,
  noOverclockingUltraLowLatency,
  visualizer
};
struct FirmwareEntry {
  FirmwareHardware hardware;
  FirmwareBuild build;
  String version, filename, url;
};
const std::vector<FirmwareEntry> &firmwareCatalog();
String firmwareHardwareName(FirmwareHardware);
String firmwareBuildName(FirmwareBuild);
String firmwareDescription(const FirmwareEntry &);
String firmwareGuideUrl(FirmwareHardware);
File firmwareDownloadsDirectory();
String firmwareReleasesUrl();
std::vector<FirmwareEntry> parseFirmwareReleases(const String &json);

class FirmwareReleases final {
public:
  enum class Status { idle, loading, succeeded, failed, cancelled };
  struct State {
    Status status = Status::idle;
    std::vector<FirmwareEntry> entries;
    String message;
    bool active() const { return status == Status::loading; }
  };
  ~FirmwareReleases();
  void start(String endpoint = firmwareReleasesUrl(), int timeoutMs = 30000);
  void cancel();
  State snapshot() const;

private:
  void run(String endpoint, int timeoutMs);
  mutable std::mutex mutex;
  std::condition_variable changed;
  State state;
  std::shared_ptr<juce::WebInputStream> stream;
  std::thread worker;
  std::atomic<bool> stopped{false}, timedOut{false};
};

// No MIDI or flashing access: a successful transfer only produces a local file.
class FirmwareDownload final {
public:
  enum class Status { idle, downloading, succeeded, failed, cancelled };
  struct State {
    Status status = Status::idle;
    juce::int64 received = 0, total = -1;
    File file;
    String message;
    bool active() const { return status == Status::downloading; }
  };
  ~FirmwareDownload();
  void start(FirmwareEntry, File directory, int timeoutMs = 120000);
  void cancel();
  State snapshot() const;

private:
  void run(FirmwareEntry, File, int timeoutMs);
  mutable std::mutex mutex;
  std::condition_variable changed;
  State state;
  std::shared_ptr<juce::WebInputStream> stream;
  std::thread worker;
  std::atomic<bool> stopped{false}, timedOut{false};
};
} // namespace core
