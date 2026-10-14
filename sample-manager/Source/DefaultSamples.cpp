#include "DefaultSamples.h"
#include "Storage.h"
#include <cerrno>
#include <chrono>
#include <numeric>
#if JUCE_WINDOWS
#include <windows.h>
#elif JUCE_MAC
#include <sys/stdio.h>
#elif JUCE_LINUX
#include <fcntl.h>
#include <linux/fs.h>
#include <sys/syscall.h>
#include <unistd.h>
#endif

namespace core {
namespace {
// Both the file cache and project folder are published without replacement.
std::error_code publish(const File &from, const File &to) {
#if JUCE_WINDOWS
  if (MoveFileW(from.getFullPathName().toWideCharPointer(),
                to.getFullPathName().toWideCharPointer()))
    return {};
  return {int(GetLastError()), std::system_category()};
#elif JUCE_MAC
  if (renamex_np(from.getFullPathName().toRawUTF8(), to.getFullPathName().toRawUTF8(),
                 RENAME_EXCL) == 0)
    return {};
  return {errno, std::generic_category()};
#elif JUCE_LINUX
  if (syscall(SYS_renameat2, AT_FDCWD, from.getFullPathName().toRawUTF8(), AT_FDCWD,
              to.getFullPathName().toRawUTF8(), RENAME_NOREPLACE) == 0)
    return {};
  return {errno, std::generic_category()};
#endif
}
constexpr juce::int64 maxExpandedBytes = 1024LL * 1024 * 1024;
void validateProject(const File &root, const DefaultSamplesEntry &entry,
                     const std::function<bool()> &stop) {
  require(child(root, "settings").isDirectory(), "Default samples are missing settings");
  auto project = Project::fromJson(parseJson(child(root, ".core-manager/project.json")));
  require(project.revision == project.completedRevision && project.warnings.isEmpty(),
          "Default samples contain unfinished edits or warnings");
  const int count = std::accumulate(entry.bankSizes.begin(), entry.bankSizes.end(), 0);
  require(int(project.samples.size()) == count, "Default samples have an unexpected sample count");
  std::vector<int> sizes(entry.bankSizes.size());
  std::map<String, String> hashes;
  auto hash = [&](const String &path) {
    auto found = hashes.find(path);
    if (found != hashes.end())
      return found->second;
    auto file = child(root, path);
    require(file.existsAsFile(), "Default samples are missing " + path);
    return hashes.emplace(path, hashFile(file, stop)).first->second;
  };
  for (const auto &[path, expected] : project.fingerprints) {
    cancelled(stop);
    // Settings fingerprints also record deliberately absent on/off markers.
    require(expected == "missing" ? !child(root, path).exists() : hash(path) == expected,
            "Default sample file failed verification: " + path);
  }
  for (const auto &sample : project.samples) {
    cancelled(stop);
    require(!sample.protectedEntry && sample.revision == sample.completedRevision,
            "Default samples contain an uneditable or unfinished sample");
    require(sample.bank < int(sizes.size()) && sample.slot < entry.bankSizes[size_t(sample.bank)],
            "Default samples have an unexpected bank or slot");
    ++sizes[size_t(sample.bank)];
    require(hash(sample.source) == sample.sourceHash, "Default sample source failed verification");
    require(sample.rendered.isNotEmpty() && child(root, sample.rendered).existsAsFile(),
            "Default samples are missing preview audio");
    for (const auto &path : sample.ownedPaths)
      require(child(root, path).existsAsFile(), "Default samples are missing " + path);
    const auto audio = card::path(sample.bank, sample.slot);
    const auto infoPath = audio + ".info";
    const auto namePath =
        "bank" + String(sample.bank + 1) + "/" + String(sample.slot) + ".name.json";
    require(project.fingerprints.count(audio) && project.fingerprints.count(infoPath) &&
                project.fingerprints.count(namePath),
            "Default samples are missing slot fingerprints");
    juce::MemoryBlock bytes;
    require(child(root, infoPath).loadFileAsData(bytes), "Cannot read default sample metadata");
    auto info = card::decode(bytes);
    card::validatePair(card::inspect(child(root, audio)), info);
    require(card::compatibility(info).isEmpty(), "Default sample markers cannot be encoded");
    const auto name = parseJson(child(root, namePath));
    require(name["audioSha256"].toString() == hash(audio) && name["name"].toString() == sample.name,
            "Default sample name metadata does not match its audio");
  }
  require(sizes == entry.bankSizes, "Default samples have an unexpected bank layout");
  // Recovery is keyed by project ID, so each fresh installation needs its own.
  project.id = uuid();
  cancelled(stop);
  durableJson(child(root, ".core-manager/project.json"), project.json());
}
} // namespace
const std::array<DefaultSamplesEntry, 2> &defaultSamplesCatalog() {
  static const std::array<DefaultSamplesEntry, 2> catalog{{
      {"Ezeptocore",
       "1-ezeptocore-default-banks.zip",
       "https://github.com/schollz/_core/releases/download/default-samples/"
       "1-ezeptocore-default-banks.zip",
       "1f45e8cb7049dd4dc27e1e6f2df247ab254a71f03e604f6549b577de5fb8fc30",
       127137219,
       {16, 16, 16, 16, 16, 16}},
      {"Ectocore",
       "2-ectocore-default-banks.zip",
       "https://github.com/schollz/_core/releases/download/default-samples/"
       "2-ectocore-default-banks.zip",
       "6f6286ffd2d60a25fe006e79b2a5a2e7a5e2463d012af474a15a51cc1bf192a9",
       340626420,
       {16, 16, 8, 8, 8, 8, 8, 8, 6}},
  }};
  return catalog;
}
DefaultSamplesInstaller::~DefaultSamplesInstaller() {
  cancel();
  if (worker.joinable())
    worker.join();
}
DefaultSamplesInstaller::State DefaultSamplesInstaller::snapshot() const {
  std::lock_guard<std::mutex> lock(mutex);
  return state;
}
void DefaultSamplesInstaller::stage(Status status, String message) {
  diagnostics::log("DEFAULTS", message);
  std::lock_guard<std::mutex> lock(mutex);
  state.status = status;
  state.message = std::move(message);
}
void DefaultSamplesInstaller::cancel() {
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
void DefaultSamplesInstaller::start(DefaultSamplesEntry entry, File downloads, File cache,
                                    int timeoutMs) {
  require(!snapshot().active(), "A default sample installation is already running");
  require(timeoutMs > 0, "Invalid download timeout");
  if (worker.joinable())
    worker.join();
  stopped = false;
  timedOut = false;
  {
    std::lock_guard<std::mutex> lock(mutex);
    state = {};
    state.status = Status::verifying;
    state.message = "Checking cached " + entry.name + " default samples...";
    state.total = entry.bytes;
  }
  worker = std::thread(
      [this, entry, downloads, cache, timeoutMs] { run(entry, downloads, cache, timeoutMs); });
}
void DefaultSamplesInstaller::run(DefaultSamplesEntry entry, File downloads, File cache,
                                  int timeoutMs) {
  diagnostics::Scope operation("DEFAULTS", "Install " + entry.name);
  File partial, staging;
  bool finished = false;
  Status outcome = Status::succeeded;
  String failure;
  auto stop = [this] { return stopped.load(); };
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
    require(entry.bytes > 0 && entry.sha256.length() == 64 && !entry.sha256.containsAnyOf("/\\.") &&
                !entry.bankSizes.empty(),
            "Invalid default sample catalog entry");
    require(downloads.isDirectory(),
            "Downloads folder is unavailable: " + downloads.getFullPathName());
    ensureDirectory(cache);
    const auto archive = child(cache, entry.sha256 + ".zip");
    bool cached = archive.existsAsFile() && archive.getSize() == entry.bytes &&
                  hashFile(archive, stop) == entry.sha256;
    cancelled(stop);
    if (!cached) {
      require(!archive.exists() || archive.deleteFile(),
              "Cannot replace the invalid default sample cache");
      partial = child(cache, ".download-" + uuid() + ".part");
      auto out = partial.createOutputStream();
      require(out && out->openedOk(),
              "Cannot write the default sample download; check free space and permissions");
      stage(Status::downloading, "Connecting to download " + entry.name + " default samples...");
      auto request = std::make_shared<juce::WebInputStream>(juce::URL(entry.url), false);
      request->withConnectionTimeout(30000).withNumRedirectsToFollow(5).withExtraHeaders(
          "User-Agent: CoreSampleManager\r\n");
      {
        std::lock_guard<std::mutex> lock(mutex);
        stream = request;
      }
      cancelled(stop);
      const bool connected = request->connect(nullptr);
      cancelled(stop);
      const int status = request->getStatusCode();
      require(connected && status == 200,
              "Default sample download failed" +
                  (status > 0 ? " (HTTP " + String(status) + ")" : String()) +
                  ". Check your connection and retry.");
      const auto length = request->getTotalLength();
      require(length < 0 || length == entry.bytes,
              "Default sample download has an unexpected size");
      juce::int64 received = 0;
      std::array<char, 256 * 1024> buffer{};
      while (!request->isExhausted()) {
        cancelled(stop);
        const int n = request->read(buffer.data(), int(buffer.size()));
        require(n >= 0 && !request->isError(),
                "Default sample download was interrupted. Retry the download.");
        if (n == 0)
          break;
        received += n;
        require(received <= entry.bytes, "Default sample download exceeds its expected size");
        require(out->write(buffer.data(), size_t(n)),
                "Cannot write default samples; check free space and permissions");
        std::lock_guard<std::mutex> lock(mutex);
        state.received = received;
        state.message = "Downloading " + entry.name + " default samples... " +
                        String(100 * received / entry.bytes) + "%";
      }
      cancelled(stop);
      diagnostics::log("DEFAULTS", "HTTP=" + String(status) + " received=" + String(received) +
                                       " expected=" + String(entry.bytes) +
                                       " length=" + String(length) +
                                       " stream_error=" + String(request->isError() ? 1 : 0));
      require(!request->isError() && received == entry.bytes,
              "Default sample download is incomplete. Retry the download.");
      out->flush();
      require(out->getStatus().wasOk(), "Cannot finish writing the default sample download");
      out.reset();
      stage(Status::verifying, "Verifying " + entry.name + " default samples...");
      require(hashFile(partial, stop) == entry.sha256,
              "Default sample checksum does not match. Retry the download.");
      cancelled(stop);
      auto error = publish(partial, archive);
      if (error == std::errc::file_exists) {
        require(archive.existsAsFile() && archive.getSize() == entry.bytes &&
                    hashFile(archive, stop) == entry.sha256,
                "Another download created an invalid default sample cache");
      } else
        require(!error,
                "Cannot save the verified default sample cache: " + String(error.message()));
      if (!error)
        partial = File();
    }
    {
      std::lock_guard<std::mutex> lock(mutex);
      stream.reset();
      state.cached = cached;
      state.received = entry.bytes;
    }
    cancelled(stop);
    const auto candidate = downloads.getChildFile(".core-defaults-" + uuid());
    std::error_code error;
    require(std::filesystem::create_directory(
                std::filesystem::u8path(candidate.getFullPathName().toStdString()), error) &&
                !error,
            "Cannot create a default sample project in Downloads");
    staging = candidate;
    juce::ZipFile zip(archive);
    require(zip.getNumEntries() > 0 && zip.getNumEntries() <= 10000,
            "Default sample ZIP is invalid");
    std::set<String> paths;
    juce::int64 expanded = 0;
    for (int i = 0; i < zip.getNumEntries(); ++i) {
      const auto *item = zip.getEntry(i);
      const auto path =
          item->filename.endsWithChar('/') ? item->filename.dropLastCharacters(1) : item->filename;
      require(!item->isSymbolicLink && !path.containsChar(':') && path.isNotEmpty() &&
                  paths.insert(path.toLowerCase()).second,
              "Default sample ZIP has conflicting or unsafe paths");
      child(staging, path);
      require(item->uncompressedSize >= 0 && item->uncompressedSize <= maxExpandedBytes - expanded,
              "Default sample ZIP exceeds its extraction limit");
      expanded += item->uncompressedSize;
    }
    stage(Status::extracting, "Extracting " + entry.name + " default samples...");
    {
      std::lock_guard<std::mutex> lock(mutex);
      state.totalFiles = zip.getNumEntries();
    }
    for (int i = 0; i < zip.getNumEntries(); ++i) {
      cancelled(stop);
      const auto *item = zip.getEntry(i);
      if (item->filename.endsWithChar('/')) {
        require(child(staging, item->filename.dropLastCharacters(1)).createDirectory().wasOk(),
                "Cannot create a default sample directory");
      } else {
        const auto target = child(staging, item->filename);
        require(target.getParentDirectory().createDirectory().wasOk(),
                "Cannot create a default sample directory");
        require(!target.exists(), "Default sample ZIP contains conflicting paths");
        std::unique_ptr<juce::InputStream> in(zip.createStreamForEntry(i));
        auto out = target.createOutputStream();
        require(in && out && out->openedOk(),
                "Cannot extract default samples; check free space and permissions");
        juce::int64 written = 0;
        std::array<char, 256 * 1024> buffer{};
        while (!in->isExhausted()) {
          cancelled(stop);
          const int n = in->read(buffer.data(), int(buffer.size()));
          require(n >= 0, "Default sample ZIP cannot be read");
          if (n == 0)
            break;
          written += n;
          require(written <= item->uncompressedSize && out->write(buffer.data(), size_t(n)),
                  "Default sample extraction failed");
        }
        out->flush();
        require(written == item->uncompressedSize && out->getStatus().wasOk(),
                "Default sample extraction is incomplete");
      }
      std::lock_guard<std::mutex> lock(mutex);
      state.files = i + 1;
      state.message = "Extracting " + entry.name + " default samples... " + String(i + 1) + "/" +
                      String(zip.getNumEntries());
    }
    stage(Status::validating, "Checking " + entry.name + " project files...");
    validateProject(staging, entry, stop);
    for (;;) {
      cancelled(stop);
      const auto destination =
          downloads.getNonexistentChildFile(entry.name + " Default Samples", "", true);
      std::lock_guard<std::mutex> lock(mutex);
      cancelled(stop);
      error = publish(staging, destination);
      if (!error) {
        state.status = Status::succeeded;
        state.project = destination;
        state.message =
            "Installed " + entry.name + " default samples: " + destination.getFullPathName();
        staging = File();
        finished = true;
        break;
      }
      require(error == std::errc::file_exists,
              "Cannot publish the default sample project: " + String(error.message()));
    }
  } catch (const std::exception &error) {
    outcome = stopped && !timedOut ? Status::cancelled : Status::failed;
    failure = timedOut  ? "Default sample installation timed out. Check your connection and retry."
              : stopped ? "Default sample installation cancelled."
                        : String(error.what());
  }
  if (partial != File())
    partial.deleteFile();
  if (staging != File())
    staging.deleteRecursively();
  {
    std::lock_guard<std::mutex> lock(mutex);
    finished = true;
    stream.reset();
    if (outcome != Status::succeeded) {
      state.status = outcome;
      state.message = failure;
    }
  }
  changed.notify_all();
  deadline.join();
  if (failure.isNotEmpty())
    diagnostics::log("DEFAULTS", failure);
}
} // namespace core
