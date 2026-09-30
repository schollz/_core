#include "AppView.h"
#include <chrono>
#include <iostream>

namespace core {
namespace {
using Status = FirmwareDownload::Status;
void check(bool value, const String &message) { require(value, "Firmware: " + message); }
struct Temp {
  File directory =
      File::getSpecialLocation(File::tempDirectory).getChildFile("core-firmware-test-" + uuid());
  Temp() { check(directory.createDirectory().wasOk(), "create test directory"); }
  ~Temp() { directory.deleteRecursively(); }
};
// USB descriptor bytes from the published Ectocore/Ezeptocore v8.0.3
// non-MIDI payload. These builds contain no model-name string.
const std::string serialIdentity("\x12\x01\x10\x02\xef\x02\x01\x40\x8a"
                                 "\x2e\x37\x18\x00\x01\x01\x02\x03\x01", 18);
juce::MemoryBlock uf2(const std::string &identity = "ezeptocore") {
  const auto blocks = std::max(size_t(1), (identity.size() + 255) / 256);
  juce::MemoryBlock bytes(blocks * 512, true);
  for (size_t b = 0; b < blocks; ++b) {
    auto *p = static_cast<uint8_t *>(bytes.getData()) + b * 512;
    auto word = [&](int offset, uint32_t value) {
      for (int i = 0; i < 4; ++i)
        p[offset + i] = uint8_t(value >> (i * 8));
    };
    word(0, 0x0a324655);
    word(4, 0x9e5d5157);
    word(8, 0x2000);
    word(12, 0x10000000 + uint32_t(b) * 256);
    word(16, 256);
    word(20, uint32_t(b));
    word(24, uint32_t(blocks));
    word(28, 0xe48bff56);
    word(508, 0x0ab16f30);
    if (b * 256 < identity.size())
      std::memcpy(p + 32, identity.data() + b * 256,
                  std::min(size_t(256), identity.size() - b * 256));
  }
  return bytes;
}
// Real HTTP on loopback exercises JUCE redirects, response headers and stream
// cancellation on each platform, without GitHub or a connected instrument.
class Server {
public:
  Server() {
    check(listener.createListener(0, "127.0.0.1"), "open loopback HTTP listener");
    port = listener.getBoundPort();
    thread = std::thread([this] { run(); });
  }
  ~Server() {
    stopped = true;
    if (thread.joinable())
      thread.join();
  }
  String url(const String &path) const { return "http://127.0.0.1:" + String(port) + path; }
  std::atomic<int> requests{0};

private:
  void run() {
    while (!stopped) {
      if (listener.waitUntilReady(true, 20) != 1)
        continue;
      std::unique_ptr<juce::StreamingSocket> client(listener.waitForNextConnection());
      if (!client)
        continue;
      String request;
      while (!stopped && !request.contains("\r\n\r\n") && request.length() < 8192) {
        if (client->waitUntilReady(true, 20) != 1)
          continue;
        char data[1024];
        const int n = client->read(data, sizeof(data), false);
        if (n <= 0)
          break;
        request += String::fromUTF8(data, n);
      }
      if (stopped)
        break;
      ++requests;
      auto path =
          request.fromFirstOccurrenceOf(" ", false, false).upToFirstOccurrenceOf(" ", false, false);
      auto writeText = [&](const String &text) {
        client->write(text.toRawUTF8(), text.getNumBytesAsUTF8());
      };
      if (path == "/redirect") {
        writeText("HTTP/1.1 302 Found\r\nLocation: " + url("/ok") +
                  "\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
        continue;
      }
      if (path == "/loop") {
        writeText("HTTP/1.1 302 Found\r\nLocation: " + url("/loop") +
                  "\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
        continue;
      }
      if (path == "/missing") {
        writeText("HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
        continue;
      }
      if (path == "/no-headers") {
        while (!stopped)
          std::this_thread::sleep_for(std::chrono::milliseconds(5));
        continue;
      }
      const bool unknown = path == "/unknown";
      const int length = path == "/oversized" ? 67108865 : 512;
      writeText("HTTP/1.1 200 OK\r\n" +
                (unknown ? String() : "Content-Length: " + String(length) + "\r\n") +
                "Connection: close\r\n\r\n");
      if (path == "/stall") {
        while (!stopped)
          std::this_thread::sleep_for(std::chrono::milliseconds(5));
        continue;
      }
      auto bytes = uf2(path == "/serial" || path == "/wrong-serial-family" ? serialIdentity
                       : path == "/unidentified"                         ? "Board CDC"
                                                                         : "ezeptocore");
      if (path == "/invalid")
        static_cast<uint8_t *>(bytes.getData())[0] = 0;
      if (path != "/oversized")
        client->write(bytes.getData(), path == "/truncated" ? 256 : int(bytes.getSize()));
    }
  }
  juce::StreamingSocket listener;
  std::atomic<bool> stopped{false};
  int port = 0;
  std::thread thread;
};
FirmwareEntry localEntry(const Server &server, const String &path) {
  auto entry = *std::find_if(firmwareCatalog().begin(), firmwareCatalog().end(), [](const auto &e) {
    return e.hardware == FirmwareHardware::ezeptocore && e.build == FirmwareBuild::normal;
  });
  if (path == "/wrong-family" || path == "/wrong-serial-family")
    entry = firmwareCatalog().front();
  entry.url = server.url(path);
  return entry;
}
template <typename F> void waitFor(F predicate, const String &reason) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (!predicate()) {
    check(std::chrono::steady_clock::now() < deadline, "timed out waiting for " + reason);
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
}
FirmwareDownload::State waitFor(FirmwareDownload &download) {
  waitFor([&] { return !download.snapshot().active(); }, "download completion");
  return download.snapshot();
}
void noPartials(const File &directory) {
  check(directory.findChildFiles(File::findFiles, false, ".core-download-*").isEmpty(),
        "remove partial files");
}
void serialUf2Tests() {
  Temp temp;
  auto file = temp.directory.getChildFile("ectocore_v8.0.3.uf2");
  for (const auto &identity : {serialIdentity, std::string(250, '\0') + serialIdentity}) {
    const auto bytes = uf2(identity);
    check(file.replaceWithData(bytes.getData(), bytes.getSize()), "write serial UF2 fixture");
    check(inspectUf2(file).product == "Ezeptocore / Ectocore",
          "recognize USB serial firmware, including a descriptor split across blocks");
  }
  // A Core filename, a partial ID, another vendor/PID, or a malformed
  // descriptor must not make an unrelated RP2040 image eligible for flashing.
  std::vector<std::string> invalid = {"Board CDC", serialIdentity.substr(8, 4),
                                      serialIdentity.substr(0, 17)};
  for (size_t offset : {size_t(0), size_t(1), size_t(8), size_t(10), size_t(17)}) {
    auto identity = serialIdentity;
    identity[offset] = '\0';
    invalid.push_back(identity);
  }
  for (const auto &identity : invalid) {
    const auto bytes = uf2(identity);
    check(file.replaceWithData(bytes.getData(), bytes.getSize()), "write foreign UF2 fixture");
    bool rejected = false;
    try {
      inspectUf2(file);
    } catch (const std::exception &) {
      rejected = true;
    }
    check(rejected, "reject foreign USB serial firmware despite a Core filename");
  }
}
} // namespace
void firmwareViewTests() {
  Temp temp;
  Server server;
  Device device;
  Look look;
  for (int presentation = 0; presentation < 3; ++presentation) {
    look.presentation(presentation);
    DeviceView deviceView(device, look, FirmwareHardware(presentation));
    auto &view = deviceView.firmware;
    view.setSize(930, 765);
    check(view.selectedEntry().hardware == FirmwareHardware(presentation),
          "initial model follows presentation");
    check(view.selectedEntry().build == FirmwareBuild::normal, "initial build is normal");
    check(view.entries.size() == (presentation == 1 ? 3 : 5), "model-specific build choices");
    check(server.requests == 0, "opening firmware does not download");
    look.presentation((presentation + 1) % 3);
    deviceView.setHardware(FirmwareHardware((presentation + 1) % 3));
    view.sendLookAndFeelChange();
    check(view.selectedEntry().hardware == FirmwareHardware((presentation + 1) % 3) &&
              view.selectedEntry().build == FirmwareBuild::normal &&
              view.release.getText().contains(view.selectedEntry().filename) &&
              view.docs.getButtonText().startsWith(firmwareHardwareName(view.selectedEntry().hardware)),
          "presentation changes select matching hardware, filename and guide");
    check(server.requests == 0, "changing presentation does not start a download");
    look.presentation(presentation);
    deviceView.setHardware(FirmwareHardware(presentation));
    const auto preview = juce::SystemStats::getEnvironmentVariable("CORE_FIRMWARE_PREVIEW_DIR", "");
    if (File::isAbsolutePath(preview)) {
      File folder(preview);
      check(folder.createDirectory().wasOk(), "create screenshot directory");
      for (const int width : {930, 540}) {
        view.setSize(width, 765);
        auto shot = view.createComponentSnapshot(view.getLocalBounds(), true, 2.f);
        auto out = folder.getChildFile(String(presentation) + "-" + String(width) + ".png")
                       .createOutputStream();
        check(out && out->setPosition(0) && out->truncate().wasOk(), "replace previous preview");
        juce::PNGImageFormat png;
        check(out && png.writeImageToStream(shot, *out), "render firmware preview");
        if (width == 540) {
          view.viewport.setViewPosition(0, view.guideHeading.getY());
          auto guide = view.createComponentSnapshot(view.getLocalBounds(), true, 2.f);
          auto guideOut =
              folder.getChildFile(String(presentation) + "-guide.png").createOutputStream();
          check(guideOut && guideOut->setPosition(0) && guideOut->truncate().wasOk(),
                "replace guide preview");
          check(png.writeImageToStream(guide, *guideOut), "render scrolled guide");
        }
      }
    }
    view.setSize(540, 480);
    check(view.content.getHeight() > view.viewport.getHeight(), "small window scrolls");
    check(view.viewport.getViewWidth() >= view.content.getWidth(),
          "small window has no horizontal clipping");
    auto &scrollbar = view.viewport.getVerticalScrollBar();
    check(scrollbar.isVisible(), "guide remains reachable at small sizes");
    check(view.hardware.getExplicitFocusOrder() < view.get.getExplicitFocusOrder(),
          "keyboard order starts with selectors");
    view.setVisible(true);
    juce::KeyboardFocusTraverser focus;
    check(focus.getNextComponent(&view.hardware) == &view.build &&
              focus.getNextComponent(&view.build) == &view.get,
          "Tab traverses hardware, build, then download");
    const int direction = presentation == 2 ? -1 : 1;
    check(view.hardware.keyPressed(
              juce::KeyPress(direction > 0 ? juce::KeyPress::downKey : juce::KeyPress::upKey)) &&
              view.hardware.getSelectedId() == presentation + 1 + direction,
          "arrow keys select hardware");
    view.hardware.setSelectedId(presentation + 1, juce::sendNotificationSync);
    check(view.build.keyPressed(juce::KeyPress(juce::KeyPress::downKey)) &&
              view.build.getSelectedId() == 2,
          "arrow keys select firmware build");
    view.build.setSelectedId(1, juce::sendNotificationSync);
  }
  FirmwareView view(device, look, FirmwareHardware::ezeptocore);
  view.setSize(930, 765);
  auto prior = temp.directory.getChildFile("previous.uf2");
  const auto bytes = uf2();
  check(prior.replaceWithData(bytes.getData(), bytes.getSize()), "create previously selected UF2");
  view.selectImage(prior, "Existing firmware");
  view.awaitingDownload = true;
  view.download.start(localEntry(server, "/missing"), temp.directory);
  view.timerCallback();
  check(!view.hardware.isEnabled() && !view.choose.isEnabled() && !view.write.isEnabled(),
        "lock actions during download");
  waitFor(view.download);
  view.timerCallback();
  check(view.image == prior && !view.working,
        "failed download preserves selection and cannot flash");
  view.awaitingDownload = true;
  view.download.start(localEntry(server, "/serial"), temp.directory);
  waitFor(view.download);
  // Completion can arrive just before the presentation callback. Process it
  // using the original hardware before applying the new model selection.
  view.setHardware(FirmwareHardware::zeptocore);
  check(view.image != prior && view.image.existsAsFile() && !view.working,
        "success selects checked file without flashing");
  check(view.selectedEntry().hardware == FirmwareHardware::zeptocore &&
            view.fileLabel.getText().startsWith("Ezeptocore:"),
        "theme change cannot relabel a completed download as another model");
  check(view.reveal.isEnabled() && !view.write.isEnabled(),
        "show downloaded file but require bootloader before flashing");
  Server stalled;
  view.setHardware(FirmwareHardware::ezeptocore);
  view.awaitingDownload = true;
  view.download.start(localEntry(stalled, "/stall"), temp.directory);
  waitFor([&] { return stalled.requests.load() > 0; }, "stalled transfer");
  view.setHardware(FirmwareHardware::zeptocore);
  view.setHardware(FirmwareHardware::ectocore);
  check(view.selectedEntry().hardware == FirmwareHardware::ezeptocore &&
            !view.hardware.isEnabled(),
        "theme changes defer while a download is active");
  auto selected = view.image;
  view.cancelDownload();
  waitFor(view.download);
  view.timerCallback();
  check(view.image == selected && view.download.snapshot().status == Status::cancelled,
        "window-close cancellation preserves selection");
  check(view.selectedEntry().hardware == FirmwareHardware::ectocore,
        "latest theme selection applies after cancellation");
  view.working = true;
  view.setHardware(FirmwareHardware::zeptocore);
  check(view.selectedEntry().hardware == FirmwareHardware::ectocore,
        "theme changes defer during flashing");
  view.working = false;
  view.timerCallback();
  check(view.selectedEntry().hardware == FirmwareHardware::zeptocore,
        "deferred selection applies when flashing finishes");
  noPartials(temp.directory);

  // Exercise the app's actual presentation callback with an existing hidden
  // Device window. Keep this app instance's preferences and project state private.
  Temp appState;
  juce::ScopedValueSetter<File> isolatedState(stateRootOverride, appState.directory);
  check(preferencesFile().replaceWithText("{}"), "create isolated app preferences");
  AppView app;
  app.deviceButton.onClick();
  auto *deviceView = dynamic_cast<DeviceView *>(app.deviceWindow->getContentComponent());
  check(deviceView != nullptr, "create Device window from the app");
  for (const int presentation : {2, 1, 0}) {
    app.deviceWindow->closeButtonPressed();
    app.presentationBox.setSelectedId(presentation + 1, juce::sendNotificationSync);
    check(deviceView->firmware.selectedEntry().hardware == FirmwareHardware(presentation),
          "app theme updates an existing hidden Device window");
    app.deviceButton.onClick();
    check(app.deviceWindow->isVisible() && deviceView->tabs.getCurrentTabIndex() == 1 &&
              deviceView->firmware.download.snapshot().status == Status::idle,
          "reopened Firmware tab retains the matching model without downloading");
  }
}
void firmwareTests() {
  serialUf2Tests();
  check(firmwareCatalog().size() == 13, "all 13 build combinations exist");
  for (const auto &entry : firmwareCatalog()) {
    const auto model = firmwareHardwareName(entry.hardware).toLowerCase();
    check(entry.filename.startsWith(model + "_" + entry.version),
          "asset uses exact hardware prefix and version");
    check(entry.url == "https://github.com/schollz/_core/releases/download/" + entry.version + "/" +
                           entry.filename,
          "asset URL matches pinned filename");
  }
  for (const String path : {"/ok", "/redirect", "/unknown", "/serial"}) {
    Temp temp;
    Server server;
    FirmwareDownload download;
    auto entry = localEntry(server, path);
    auto existing = temp.directory.getChildFile(entry.filename);
    check(existing.replaceWithText("preserve existing file"), "create collision fixture");
    download.start(entry, temp.directory);
    const auto state = waitFor(download);
    check(state.status == Status::succeeded, path + " succeeds: " + state.message);
    check(state.file != existing && state.file.getFileExtension() == ".uf2",
          "numbered collision keeps UF2 extension");
    check(existing.loadFileAsString() == "preserve existing file",
          "never overwrite an existing download");
    check(state.file.getSize() == 512 && inspectUf2(state.file).blocks == 1,
          "completed file validates");
    check(state.received == 512 && state.total == 512, "success reports complete progress");
    check(server.requests == (path == "/redirect" ? 2 : 1), "follow redirects only when requested");
    noPartials(temp.directory);
  }
  for (const String path :
       {"/missing", "/truncated", "/invalid", "/wrong-family", "/wrong-serial-family",
        "/unidentified", "/oversized", "/loop"}) {
    Temp temp;
    Server server;
    FirmwareDownload download;
    download.start(localEntry(server, path), temp.directory);
    const auto state = waitFor(download);
    check(state.status == Status::failed && state.file == File(),
          path + " cannot select an invalid download");
    if (path == "/missing")
      check(state.message.contains("404") && state.message.contains("unavailable"),
            "explain missing release asset");
    check(temp.directory.findChildFiles(File::findFiles, false).isEmpty(),
          "failure leaves no firmware or partial file");
  }
  for (const String path : {"/stall", "/no-headers"}) {
    Temp temp;
    Server server;
    FirmwareDownload download;
    download.start(localEntry(server, path), temp.directory, 200);
    auto state = waitFor(download);
    check(state.status == Status::failed && state.message.contains("timed out"),
          "bounded timeout for " + path + ": " + state.message);
    noPartials(temp.directory);
  }
  {
    Temp temp;
    Server server;
    FirmwareDownload download;
    download.start(localEntry(server, "/stall"), temp.directory);
    waitFor([&] { return server.requests.load() > 0; }, "active transfer");
    download.cancel();
    check(waitFor(download).status == Status::cancelled, "cancel blocked read");
    noPartials(temp.directory);
  }
  {
    Temp temp;
    Server server;
    {
      FirmwareDownload download;
      download.start(localEntry(server, "/stall"), temp.directory);
      waitFor([&] { return server.requests.load() > 0; }, "transfer before destructor");
    }
    noPartials(temp.directory);
    auto file = temp.directory.getChildFile("not-a-directory");
    check(file.replaceWithText("keep"), "write failure fixture");
    FirmwareDownload download;
    download.start(localEntry(server, "/ok"), file);
    check(waitFor(download).status == Status::failed && file.loadFileAsString() == "keep",
          "unwritable destination preserves existing file");
  }
  {
    Temp temp;
    Server server;
    auto marker = temp.directory.getChildFile("INFO_UF2.TXT");
    marker.replaceWithText("RPI-RP2");
    FirmwareDownload download;
    download.start(localEntry(server, "/ok"), temp.directory);
    check(waitFor(download).status == Status::failed && server.requests == 0,
          "downloads cannot target a bootloader drive");
  }
  firmwareViewTests();
  std::cout << "PASS firmware catalog, loopback downloads, cancellation and UI contracts\n";
}
// Explicit manual acceptance only: never part of CTest or the offline suite.
// Exercise the production downloader against every README URL in a private
// temporary directory, without selecting a volume, resetting or flashing.
void firmwareReleaseTests() {
  Temp temp;
  for (const auto &entry : firmwareCatalog()) {
    FirmwareDownload download;
    std::cout << "Downloading release asset " << entry.filename << std::endl;
    download.start(entry, temp.directory);
    while (download.snapshot().active())
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
    const auto state = download.snapshot();
    check(state.status == Status::succeeded, entry.filename + ": " + state.message);
    check(state.file.getFileName() == entry.filename &&
              state.file.getSize() == state.received && inspectUf2(state.file).blocks > 0,
          "complete release download validates again as a local file");
    noPartials(temp.directory);
    std::cout << "PASS " << entry.filename << " bytes=" << state.received << std::endl;
  }
}
} // namespace core
