#include "Firmware.h"
#include <chrono>
#include <regex>

namespace core {
namespace {
std::array<int, 3> versionParts(const String &tag) {
  auto parts = juce::StringArray::fromTokens(tag.substring(1), ".", "");
  return {parts[0].getIntValue(), parts[1].getIntValue(), parts[2].getIntValue()};
}
void sortReleases(std::vector<FirmwareEntry> &entries) {
  std::sort(entries.begin(), entries.end(), [](const auto &a, const auto &b) {
    const auto av = versionParts(a.version), bv = versionParts(b.version);
    if (av != bv)
      return av > bv;
    if (a.hardware != b.hardware)
      return a.hardware < b.hardware;
    return a.build < b.build;
  });
  entries.erase(std::unique(entries.begin(), entries.end(), [](const auto &a, const auto &b) {
                  return a.url == b.url;
                }), entries.end());
}
} // namespace
String firmwareReleasesUrl() { return "https://api.github.com/repos/schollz/_core/releases"; }
std::vector<FirmwareEntry> parseFirmwareReleases(const String &json) {
  var releases;
  require(juce::JSON::parse(json, releases).wasOk() && releases.isArray(),
          "GitHub returned an invalid firmware release list");
  static const std::regex versionPattern("v[0-9]{1,6}\\.[0-9]{1,6}\\.[0-9]{1,6}");
  const std::pair<FirmwareBuild, String> builds[] = {
      {FirmwareBuild::normal, ""},
      {FirmwareBuild::lowLatency, "_low_latency"},
      {FirmwareBuild::ultraLowLatency, "_ultralow_latency"},
      {FirmwareBuild::noOverclocking, "_no_overclocking"},
      {FirmwareBuild::noOverclockingLowLatency, "_no_overclocking_low_latency"},
      {FirmwareBuild::noOverclockingUltraLowLatency, "_no_overclocking_ultralow_latency"},
      {FirmwareBuild::visualizer, "_visualizer"}};
  std::vector<FirmwareEntry> entries;
  for (const auto &release : *releases.getArray()) {
    const auto tag = release["tag_name"].toString();
    if (!release.isObject() || !release["draft"].isBool() || !release["prerelease"].isBool() ||
        bool(release["draft"]) || bool(release["prerelease"]) ||
        !std::regex_match(tag.toStdString(), versionPattern))
      continue;
    const auto *assets = release["assets"].getArray();
    if (assets == nullptr)
      continue;
    for (const auto &asset : *assets) {
      if (asset["state"].toString() != "uploaded")
        continue;
      const auto name = asset["name"].toString(), url = asset["browser_download_url"].toString();
      for (auto hardware : {FirmwareHardware::ezeptocore, FirmwareHardware::zeptocore,
                            FirmwareHardware::ectocore})
        for (const auto &[build, suffix] : builds) {
          const auto expected = firmwareHardwareName(hardware).toLowerCase() + "_" + tag + suffix + ".uf2";
          // Use the actual asset URL, constrained to this repository/release.
          // No guessed filenames, beta hardware, unrelated assets or external hosts.
          if (name == expected &&
              url == "https://github.com/schollz/_core/releases/download/" + tag + "/" + expected)
            entries.push_back({hardware, build, tag, name, url});
        }
    }
  }
  sortReleases(entries);
  return entries;
}
FirmwareReleases::~FirmwareReleases() {
  cancel();
  if (worker.joinable())
    worker.join();
}
FirmwareReleases::State FirmwareReleases::snapshot() const {
  std::lock_guard<std::mutex> lock(mutex);
  return state;
}
void FirmwareReleases::cancel() {
  std::shared_ptr<juce::WebInputStream> request;
  {
    std::lock_guard<std::mutex> lock(mutex);
    if (!state.active())
      return;
    stopped = true;
    request = stream;
  }
  if (request)
    request->cancel();
  changed.notify_all();
}
void FirmwareReleases::start(String endpoint, int timeoutMs) {
  require(!snapshot().active() && timeoutMs > 0, "Firmware versions are already loading");
  if (worker.joinable())
    worker.join();
  stopped = false;
  timedOut = false;
  {
    std::lock_guard<std::mutex> lock(mutex);
    state = {};
    state.status = Status::loading;
    state.message = "Loading available firmware versions...";
  }
  worker = std::thread([this, endpoint, timeoutMs] { run(endpoint, timeoutMs); });
}
void FirmwareReleases::run(String endpoint, int timeoutMs) {
  State completed;
  bool finished = false;
  std::thread deadline([&] {
    std::unique_lock<std::mutex> lock(mutex);
    if (changed.wait_for(lock, std::chrono::milliseconds(timeoutMs),
                         [&] { return finished || stopped.load(); }))
      return;
    timedOut = true;
    stopped = true;
    auto request = stream;
    lock.unlock();
    if (request)
      request->cancel();
  });
  try {
    juce::int64 received = 0;
    constexpr juce::int64 maxBytes = 8 * 1024 * 1024;
    for (int page = 1;; ++page) {
      require(page <= 20, "Too many firmware release pages; try again later");
      auto url = juce::URL(endpoint).withParameter("per_page", "100").withParameter("page", String(page));
      diagnostics::log("FIRMWARE", "Load versions " + url.toString(true));
      auto request = std::make_shared<juce::WebInputStream>(url, false);
      request->withConnectionTimeout(15000).withNumRedirectsToFollow(5).withExtraHeaders(
          "User-Agent: CoreSampleManager\r\nAccept: application/vnd.github+json\r\n");
      {
        std::lock_guard<std::mutex> lock(mutex);
        stream = request;
      }
      cancelled([this] { return stopped.load(); });
      const bool connected = request->connect(nullptr);
      cancelled([this] { return stopped.load(); });
      const auto status = request->getStatusCode();
      require(status != 403 && status != 429,
              "GitHub is limiting release requests. Try Refresh versions later.");
      require(connected && status == 200,
              "Could not load firmware versions" +
                  (status > 0 ? " (HTTP " + String(status) + ")" : String()) + ".");
      require(request->getTotalLength() <= maxBytes - received, "Firmware release list is too large");
      juce::MemoryOutputStream body;
      std::array<char, 16384> buffer{};
      while (!request->isExhausted()) {
        cancelled([this] { return stopped.load(); });
        const int n = request->read(buffer.data(), int(buffer.size()));
        require(n >= 0 && !request->isError(), "Firmware release list was interrupted");
        if (n == 0)
          break;
        received += n;
        require(received <= maxBytes, "Firmware release list is too large");
        body.write(buffer.data(), size_t(n));
      }
      cancelled([this] { return stopped.load(); });
      require(!request->isError() &&
                  (request->getTotalLength() < 0 || request->getTotalLength() == juce::int64(body.getDataSize())),
              "Firmware release list is incomplete");
      const auto json = body.toUTF8();
      auto entries = parseFirmwareReleases(json);
      completed.entries.insert(completed.entries.end(), entries.begin(), entries.end());
      // Follow pagination on the same endpoint rather than trusting a Link URL.
      if (!request->getResponseHeaders().getValue("Link", "").contains("rel=\"next\"") &&
          juce::JSON::parse(json).size() < 100)
        break;
    }
    require(!completed.entries.empty(), "No supported firmware assets were found in published releases");
    sortReleases(completed.entries);
    cancelled([this] { return stopped.load(); });
    completed.status = Status::succeeded;
    completed.message = "Available versions loaded from GitHub.";
  } catch (const std::exception &error) {
    completed.entries.clear();
    completed.status = stopped && !timedOut ? Status::cancelled : Status::failed;
    completed.message = timedOut ? "Loading firmware versions timed out."
                        : stopped ? "Loading versions cancelled."
                                  : String(error.what());
  }
  diagnostics::log("FIRMWARE", completed.message + " builds=" + String(completed.entries.size()));
  {
    std::lock_guard<std::mutex> lock(mutex);
    finished = true;
    stream.reset();
    state = std::move(completed);
  }
  changed.notify_all();
  deadline.join();
}
} // namespace core
