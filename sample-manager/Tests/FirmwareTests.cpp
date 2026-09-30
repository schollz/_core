#include "DeviceView.h"
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
juce::MemoryBlock uf2() {
  juce::MemoryBlock bytes(512, true);
  auto *p = static_cast<uint8_t *>(bytes.getData());
  auto word = [&](int offset, uint32_t value) {
    for (int i = 0; i < 4; ++i)
      p[offset + i] = uint8_t(value >> (i * 8));
  };
  word(0, 0x0a324655);
  word(4, 0x9e5d5157);
  word(8, 0x2000);
  word(12, 0x10000000);
  word(16, 256);
  word(20, 0);
  word(24, 1);
  word(28, 0xe48bff56);
  word(508, 0x0ab16f30);
  std::memcpy(p + 32, "ezeptocore", sizeof("ezeptocore") - 1);
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
      auto bytes = uf2();
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
  if (path == "/wrong-family")
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
} // namespace
void firmwareViewTests() {
  Temp temp;
  Server server;
  Device device;
  Look look;
  for (int presentation = 0; presentation < 3; ++presentation) {
    look.presentation(presentation);
    FirmwareView view(device, look, FirmwareHardware(presentation));
    view.setSize(930, 765);
    check(view.selectedEntry().hardware == FirmwareHardware(presentation),
          "initial model follows presentation");
    check(view.selectedEntry().build == FirmwareBuild::normal, "initial build is normal");
    check(view.entries.size() == (presentation == 1 ? 3 : 5), "model-specific build choices");
    check(server.requests == 0, "opening firmware does not download");
    look.presentation((presentation + 1) % 3);
    view.sendLookAndFeelChange();
    check(view.selectedEntry().hardware == FirmwareHardware(presentation),
          "theme changes cannot retarget firmware");
    look.presentation(presentation);
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
  view.download.start(localEntry(server, "/ok"), temp.directory);
  waitFor(view.download);
  view.timerCallback();
  check(view.image != prior && view.image.existsAsFile() && !view.working,
        "success selects checked file without flashing");
  check(view.reveal.isEnabled() && !view.write.isEnabled(),
        "show downloaded file but require bootloader before flashing");
  Server stalled;
  view.awaitingDownload = true;
  view.download.start(localEntry(stalled, "/stall"), temp.directory);
  waitFor([&] { return stalled.requests.load() > 0; }, "stalled transfer");
  auto selected = view.image;
  view.cancelDownload();
  waitFor(view.download);
  view.timerCallback();
  check(view.image == selected && view.download.snapshot().status == Status::cancelled,
        "window-close cancellation preserves selection");
  noPartials(temp.directory);
}
void firmwareTests() {
  check(firmwareCatalog().size() == 13, "all 13 build combinations exist");
  for (const auto &entry : firmwareCatalog()) {
    const auto model = firmwareHardwareName(entry.hardware).toLowerCase();
    check(entry.filename.startsWith(model + "_" + entry.version),
          "asset uses exact hardware prefix and version");
    check(entry.url == "https://github.com/schollz/_core/releases/download/" + entry.version + "/" +
                           entry.filename,
          "asset URL matches pinned filename");
  }
  for (const String path : {"/ok", "/redirect", "/unknown"}) {
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
       {"/missing", "/truncated", "/invalid", "/wrong-family", "/oversized", "/loop"}) {
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
} // namespace core
