#include "Firmware.h"
#include <cerrno>
#include <chrono>
#if JUCE_WINDOWS
#include <shlobj.h>
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
#include <FirmwareCatalogData.h>
constexpr juce::int64 maxFirmwareBytes = 64 * 1024 * 1024;
// Publish on the same filesystem without replacing a file that another process
// created after name selection. Unlike hard links, these also support FAT/exFAT.
std::error_code publishDownload(const File &source, const File &destination) {
#if JUCE_WINDOWS
  if (MoveFileW(source.getFullPathName().toWideCharPointer(),
                destination.getFullPathName().toWideCharPointer()))
    return {};
  return {int(GetLastError()), std::system_category()};
#elif JUCE_MAC
  if (renamex_np(source.getFullPathName().toRawUTF8(), destination.getFullPathName().toRawUTF8(),
                 RENAME_EXCL) == 0)
    return {};
  return {errno, std::generic_category()};
#elif JUCE_LINUX
  if (syscall(SYS_renameat2, AT_FDCWD, source.getFullPathName().toRawUTF8(), AT_FDCWD,
              destination.getFullPathName().toRawUTF8(), RENAME_NOREPLACE) == 0)
    return {};
  return {errno, std::generic_category()};
#endif
}
} // namespace
const std::vector<FirmwareEntry> &firmwareCatalog() { return firmwareEntries; }
String firmwareHardwareName(FirmwareHardware hardware) {
  switch (hardware) {
  case FirmwareHardware::zeptocore:
    return "Zeptocore";
  case FirmwareHardware::ectocore:
    return "Ectocore";
  case FirmwareHardware::ezeptocore:
    return "Ezeptocore";
  }
  return {};
}
String firmwareBuildName(FirmwareBuild build) {
  switch (build) {
  case FirmwareBuild::normal:
    return "Normal";
  case FirmwareBuild::lowLatency:
    return "Low latency";
  case FirmwareBuild::noOverclocking:
    return "No overclocking";
  case FirmwareBuild::noOverclockingLowLatency:
    return "No overclocking + low latency";
  case FirmwareBuild::visualizer:
    return "Visualizer";
  }
  return {};
}
String firmwareDescription(const FirmwareEntry &entry) {
  if (entry.build == FirmwareBuild::visualizer)
    return "Full device visualization over USB MIDI. Normal latency" +
           String(entry.hardware == FirmwareHardware::zeptocore ? "." : ", overclocked.");
  const bool low = entry.build == FirmwareBuild::lowLatency ||
                   entry.build == FirmwareBuild::noOverclockingLowLatency;
  String text =
      low ? "Lower latency; less CPU bandwidth for effects." : "Normal latency suits most uses.";
  if (entry.hardware != FirmwareHardware::zeptocore) {
    const bool unclocked = entry.build == FirmwareBuild::noOverclocking ||
                           entry.build == FirmwareBuild::noOverclockingLowLatency;
    text += unclocked
                ? " No overclocking: stable internal timing, less FX headroom."
                : " Overclocked: more FX headroom; external clock recommended to avoid drift.";
  }
  return text;
}
String firmwareGuideUrl(FirmwareHardware hardware) {
  switch (hardware) {
  case FirmwareHardware::zeptocore:
    return "https://shop.infinitedigits.co/collections/zeptocore/#zeptocore-upload";
  case FirmwareHardware::ectocore:
    return "https://shop.infinitedigits.co/collections/ectocore/";
  case FirmwareHardware::ezeptocore:
    return "https://shop.infinitedigits.co/collections/ezeptocore/";
  }
  return {};
}
File firmwareDownloadsDirectory() {
#if JUCE_WINDOWS
  // Use the known folder so redirected Downloads folders are respected.
  static const GUID downloadsId{
      0x374de290, 0x123f, 0x4565, {0x91, 0x64, 0x39, 0xc4, 0x92, 0x5e, 0x46, 0x7b}};
  PWSTR path = nullptr;
  const auto result = SHGetKnownFolderPath(downloadsId, 0, nullptr, &path);
  File folder;
  if (SUCCEEDED(result) && path != nullptr)
    folder = File(String(path));
  CoTaskMemFree(path);
  require(folder != File(), "Cannot locate your Downloads folder");
  return folder;
#else
  const auto home = File::getSpecialLocation(File::userHomeDirectory);
#if JUCE_LINUX
  auto config = juce::SystemStats::getEnvironmentVariable("XDG_CONFIG_HOME", "");
  auto dirs = (File::isAbsolutePath(config) ? File(config) : home.getChildFile(".config"))
                  .getChildFile("user-dirs.dirs");
  for (auto line : juce::StringArray::fromLines(dirs.loadFileAsString())) {
    line = line.trim();
    if (line.startsWith("XDG_DOWNLOAD_DIR=")) {
      auto path = line.fromFirstOccurrenceOf("=", false, false)
                      .trim()
                      .unquoted()
                      .replace("${HOME}", home.getFullPathName())
                      .replace("$HOME", home.getFullPathName());
      if (File::isAbsolutePath(path))
        return File(path);
    }
  }
#endif
  return home.getChildFile("Downloads");
#endif
}
FirmwareDownload::~FirmwareDownload() {
  cancel();
  if (worker.joinable())
    worker.join();
}
FirmwareDownload::State FirmwareDownload::snapshot() const {
  std::lock_guard<std::mutex> lock(mutex);
  return state;
}
void FirmwareDownload::cancel() {
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
void FirmwareDownload::start(FirmwareEntry entry, File directory, int timeoutMs) {
  require(!snapshot().active(), "A firmware download is already running");
  require(timeoutMs > 0, "Invalid download timeout");
  if (worker.joinable())
    worker.join();
  stopped = false;
  timedOut = false;
  {
    std::lock_guard<std::mutex> lock(mutex);
    state = {};
    state.status = Status::downloading;
    state.message = "Connecting to download " + entry.filename + "...";
  }
  worker = std::thread([this, entry, directory, timeoutMs] { run(entry, directory, timeoutMs); });
}
void FirmwareDownload::run(FirmwareEntry entry, File directory, int timeoutMs) {
  File temporary;
  State completed;
  bool finished = false;
  // Cancel even a blocked connect/read when the total transfer deadline expires.
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
    require(entry.filename == File::createLegalFileName(entry.filename) &&
                entry.filename.endsWith(".uf2") && !entry.filename.containsAnyOf("/\\"),
            "Invalid firmware filename");
    require(directory.isDirectory(),
            "Downloads folder is unavailable: " + directory.getFullPathName());
    // A configured Downloads folder must never turn the download into a flash.
    for (auto parent = directory; parent != parent.getParentDirectory();
         parent = parent.getParentDirectory())
      require(!parent.getChildFile("INFO_UF2.TXT").existsAsFile() &&
                  !parent.getChildFile("INFO_UF2.txt").existsAsFile(),
              "Choose local UF2 to flash; Downloads must not be a bootloader volume");
    temporary = directory.getChildFile(".core-download-" + uuid() + ".uf2");
    auto out = temporary.createOutputStream();
    require(out && out->openedOk(), "Cannot write to Downloads: " + directory.getFullPathName());
    auto request = std::make_shared<juce::WebInputStream>(juce::URL(entry.url), false);
    request->withConnectionTimeout(30000).withNumRedirectsToFollow(5).withExtraHeaders(
        "User-Agent: CoreSampleManager\r\n");
    {
      std::lock_guard<std::mutex> lock(mutex);
      stream = request;
    }
    cancelled([this] { return stopped.load(); });
    const bool connected = request->connect(nullptr);
    cancelled([this] { return stopped.load(); });
    const int status = request->getStatusCode();
    require(status != 404,
            "Firmware " + entry.version +
                " is unavailable (HTTP 404). The README's release asset "
                "may not have been published yet. Retry later or choose a local UF2.");
    require(connected && status == 200,
            "Firmware download failed" +
                (status > 0 ? " (HTTP " + String(status) + ")" : String()) +
                ". Check your connection and retry.");
    const auto total = request->getTotalLength();
    require(total <= maxFirmwareBytes, "Firmware download exceeds 64 MiB");
    juce::int64 received = 0;
    std::array<char, 16384> buffer{};
    while (!request->isExhausted()) {
      cancelled([this] { return stopped.load(); });
      const auto n = request->read(buffer.data(), int(buffer.size()));
      require(n >= 0 && !request->isError(),
              "Firmware download was interrupted. Retry the download.");
      if (n == 0)
        break;
      received += n;
      require(received <= maxFirmwareBytes, "Firmware download exceeds 64 MiB");
      require(out->write(buffer.data(), size_t(n)),
              "Cannot write firmware to Downloads; check free space and permissions");
      std::lock_guard<std::mutex> lock(mutex);
      state.received = received;
      state.total = total;
      state.message = "Downloading " + entry.filename;
    }
    cancelled([this] { return stopped.load(); });
    require(!request->isError() && (total < 0 || received == total),
            "Firmware download is incomplete. Retry the download.");
    out->flush();
    require(out->getStatus().wasOk(), "Could not finish writing firmware to Downloads");
    out.reset();
    const auto inspected = inspectUf2(temporary);
    require((entry.hardware == FirmwareHardware::zeptocore) == (inspected.product == "Zeptocore"),
            "Downloaded UF2 identifies a different hardware family. Choose the correct firmware "
            "for your device.");
    // The final filename becomes visible only after the complete file validates.
    for (;;) {
      cancelled([this] { return stopped.load(); });
      auto destination =
          directory.getNonexistentChildFile(entry.filename.dropLastCharacters(4), ".uf2", true);
      std::error_code error;
      {
        std::lock_guard<std::mutex> lock(mutex);
        cancelled([this] { return stopped.load(); });
        error = publishDownload(temporary, destination);
        if (!error) {
          completed.status = Status::succeeded;
          completed.file = destination;
          completed.received = received;
          completed.total = received;
          completed.message =
              "Downloaded and checked. Ready to flash: " + destination.getFileName();
          finished = true;
          break;
        }
      }
      require(error == std::errc::file_exists,
              "Cannot save the completed UF2 in Downloads: " + String(error.message()));
    }
  } catch (const std::exception &error) {
    completed.status = stopped && !timedOut ? Status::cancelled : Status::failed;
    completed.message = timedOut  ? "Firmware download timed out. Check your connection and retry."
                        : stopped ? "Download cancelled."
                                  : String(error.what());
  }
  if (temporary != File())
    temporary.deleteFile();
  {
    std::lock_guard<std::mutex> lock(mutex);
    finished = true;
    stream.reset();
    state = completed;
  }
  changed.notify_all();
  deadline.join();
}
} // namespace core
