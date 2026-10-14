#include "AppView.h"
#include "Storage.h"
#include <chrono>
#include <iostream>
#include <numeric>
#if JUCE_MAC
#include <sys/socket.h>
#endif

namespace core {
void defaultSamplesViewTests();
namespace {
using Status = DefaultSamplesInstaller::Status;
void check(bool ok, const String &message) { require(ok, "Default samples: " + message); }
struct Temp {
  File root =
      File::getSpecialLocation(File::tempDirectory).getChildFile("core-defaults-test-" + uuid());
  Temp() { check(root.createDirectory().wasOk(), "create temporary directory"); }
  ~Temp() { root.deleteRecursively(); }
};
template <typename F> void waitFor(F predicate, int seconds = 15) {
  const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
  while (!predicate()) {
    check(std::chrono::steady_clock::now() < until, "operation timed out");
    juce::Thread::sleep(2);
  }
}
ManagerState settle(Manager &manager) {
  waitFor([&] { return manager.idle() && !manager.snapshot().busy; });
  auto state = manager.snapshot();
  check(state.error.isEmpty(), "manager ready: " + state.error);
  return state;
}
class Server {
public:
  struct Response {
    juce::MemoryBlock body;
    int status = 200;
    int length = -1;
    String headers;
    bool stall = false, omitLength = false, stallBody = false;
  };
  explicit Server(std::map<String, Response> routes) : routes(std::move(routes)) {
    check(listener.createListener(0, "127.0.0.1"), "listen on loopback");
    port = listener.getBoundPort();
    worker = std::thread([this] { run(); });
  }
  ~Server() {
    stopped = true;
    if (worker.joinable())
      worker.join();
  }
  String url(const String &path) const { return "http://127.0.0.1:" + String(port) + path; }
  std::atomic<int> requests{0};

private:
  void run() {
    while (!stopped) {
      if (listener.waitUntilReady(true, 10) != 1)
        continue;
      std::unique_ptr<juce::StreamingSocket> client(listener.waitForNextConnection());
      if (!client)
        continue;
#if JUCE_MAC
      const int suppressSigpipe = 1;
      setsockopt(client->getRawSocketHandle(), SOL_SOCKET, SO_NOSIGPIPE, &suppressSigpipe, sizeof(suppressSigpipe));
#endif
      String request;
      while (!stopped && !request.contains("\r\n\r\n")) {
        if (client->waitUntilReady(true, 10) != 1)
          continue;
        char buffer[2048];
        const int n = client->read(buffer, sizeof(buffer), false);
        if (n <= 0)
          break;
        request += String::fromUTF8(buffer, n);
      }
      if (stopped)
        break;
      ++requests;
      const auto path =
          request.fromFirstOccurrenceOf(" ", false, false).upToFirstOccurrenceOf(" ", false, false);
      auto found = routes.find(path);
      if (found == routes.end())
        continue;
      const auto &response = found->second;
      if (response.stall) {
        while (!stopped)
          juce::Thread::sleep(2);
        continue;
      }
      String header = "HTTP/1.1 " + String(response.status) + " Test\r\nConnection: close\r\n";
      if (!response.omitLength)
        header += "Content-Length: " +
                  String(response.length >= 0 ? response.length : int(response.body.getSize())) +
                  "\r\n";
      header += response.headers + "\r\n";
      auto writeBytes = [&](const char *data, int remaining) {
        while (remaining > 0 && !stopped) {
          const int ready = client->waitUntilReady(false, 20);
          if (ready < 0)
            break;
          if (ready == 0)
            continue;
          const int n = client->write(data, std::min(remaining, 16384));
          if (n <= 0)
            break;
          remaining -= n;
          data += n;
        }
      };
      writeBytes(header.toRawUTF8(), header.getNumBytesAsUTF8());
      if (response.stallBody) {
        while (!stopped)
          juce::Thread::sleep(2);
        continue;
      }
      writeBytes(static_cast<const char *>(response.body.getData()), int(response.body.getSize()));
    }
  }
  juce::StreamingSocket listener;
  std::map<String, Response> routes;
  std::atomic<bool> stopped{false};
  int port = 0;
  std::thread worker;
};
struct Fixture {
  Temp temp;
  File project = temp.root.getChildFile("source"), downloads = temp.root.getChildFile("downloads"),
       cache = temp.root.getChildFile("cache"), archive;
  DefaultSamplesEntry entry{"Test", "test.zip", "", "", 0, {1}};
  Project original;
  Fixture() {
    check(project.createDirectory().wasOk() && downloads.createDirectory().wasOk(),
          "create fixture directories");
    const auto input = temp.root.getChildFile("bpm120.wav");
    {
      auto out = input.createOutputStream();
      check(out != nullptr, "create audio fixture");
      card::writeHeader(*out, 44100 * 2, 44100, 2);
      for (int n = 0; n < 44100 * 4; ++n)
        out->writeShort(short(std::sin(n * .03) * 10000));
    }
    {
      Manager manager{std::chrono::milliseconds(20)};
      manager.open(project);
      settle(manager);
      manager.import({input.getFullPathName()}, 0);
      original = settle(manager).project;
    }
    makeZip();
  }
  void makeZip(const String &extraPath = {}, const File &extraFile = {}) {
    archive = temp.root.getChildFile(uuid() + ".zip");
    juce::ZipFile::Builder builder;
    for (const auto &file : project.findChildFiles(File::findFiles, true)) {
      const auto path = file.getRelativePathFrom(project).replaceCharacter('\\', '/');
      if (path.startsWith(".core-manager/") && path != ".core-manager/project.json" &&
          !path.startsWith(".core-manager/sources/") && !path.startsWith(".core-manager/cache/"))
        continue;
      builder.addFile(file, 6, path);
    }
    if (extraPath.isNotEmpty())
      builder.addFile(extraFile == File() ? child(project, "bank1/0.0.wav") : extraFile, 6,
                      extraPath);
    auto out = archive.createOutputStream();
    check(out && builder.writeToStream(*out, nullptr), "write fixture ZIP");
    out->flush();
    out.reset();
    entry.bytes = archive.getSize();
    entry.sha256 = hashFile(archive);
  }
  juce::MemoryBlock bytes() const {
    juce::MemoryBlock data;
    check(archive.loadFileAsData(data), "read fixture ZIP");
    return data;
  }
  void seedCache() {
    check(cache.createDirectory().wasOk() &&
              archive.copyFileTo(cache.getChildFile(entry.sha256 + ".zip")),
          "seed verified cache");
  }
};
DefaultSamplesInstaller::State install(DefaultSamplesInstaller &installer, const Fixture &fixture,
                                       DefaultSamplesEntry entry) {
  installer.start(std::move(entry), fixture.downloads, fixture.cache);
  waitFor([&] { return !installer.snapshot().active(); });
  return installer.snapshot();
}
void failInstall(Fixture &fixture, DefaultSamplesEntry entry, const String &reason) {
  DefaultSamplesInstaller installer;
  const auto result = install(installer, fixture, std::move(entry));
  check(result.status == Status::failed && result.project == File(),
        reason + ": " + result.message);
  check(fixture.downloads.findChildFiles(File::findFilesAndDirectories, false).isEmpty(),
        "failure cleans staging");
  for (const auto &file : fixture.cache.findChildFiles(File::findFiles, false))
    check(!file.getFileName().endsWith(".part"), "failure cleans partial download");
}
} // namespace

void defaultSamplesTests() {
  const auto &catalog = defaultSamplesCatalog();
  check(catalog[0].name == "Ezeptocore" && catalog[1].name == "Ectocore" &&
            catalog[0].bytes == 127137219 && catalog[1].bytes == 340626420,
        "hardcoded names and sizes");
  check(catalog[0].bankSizes == std::vector<int>({16, 16, 16, 16, 16, 16}) &&
            catalog[1].bankSizes == std::vector<int>({16, 16, 8, 8, 8, 8, 8, 8, 6}),
        "hardcoded bank layouts");
  for (const auto &entry : catalog)
    check(entry.sha256.length() == 64 &&
              entry.url == "https://github.com/schollz/_core/releases/download/default-samples/" +
                               entry.filename,
          "pinned release URL and checksum");
  Fixture fixture;
  const auto data = fixture.bytes();
  std::map<String, Server::Response> routes;
  routes["/ok"].body = data;
  routes["/redirect"].status = 302;
  routes["/redirect"].headers = "Location: /ok\r\n";
  routes["/truncated"].body = data;
  routes["/truncated"].body.setSize(data.getSize() / 2);
  routes["/truncated"].length = int(data.getSize());
  routes["/oversized"].body = data;
  routes["/oversized"].length = int(data.getSize()) + 1;
  routes["/overflow"].body = data;
  routes["/overflow"].body.append("x", 1);
  routes["/overflow"].omitLength = true;
  routes["/unknown"].body = data;
  routes["/unknown"].omitLength = true;
  routes["/missing"].status = 404;
  routes["/checksum"].body = data;
  static_cast<char *>(routes["/checksum"].body.getData())[0] ^= 1;
  Server server(std::move(routes));
  for (auto path : {"/truncated", "/oversized", "/overflow", "/missing", "/checksum"}) {
    auto entry = fixture.entry;
    entry.url = server.url(path);
    failInstall(fixture, entry, path);
  }
  auto entry = fixture.entry;
  entry.url = server.url("/redirect");
  DefaultSamplesInstaller installer;
  const auto first = install(installer, fixture, entry);
  check(first.status == Status::succeeded && !first.cached,
        "redirect downloads and installs: " + first.message);
  check(first.project.getFileName() == "Test Default Samples" &&
            child(first.project, ".core-manager/project.json").existsAsFile(),
        "hidden editable manifest and project root preserved");
  auto original = Project::fromJson(parseJson(child(first.project, ".core-manager/project.json")));
  check(original.id != fixture.original.id && hashFile(child(first.project, "bank1/0.0.wav")) ==
                                                  hashFile(child(fixture.project, "bank1/0.0.wav")),
        "new identity retains original hardware bytes");
  // Simulate edits in both the prior folder and its local recovery record.
  original.samples[0].name = "previously edited";
  original.revision += 1;
  durableJson(stateRoot().getChildFile("recovery/" + original.id + ".json"), original.json());
  check(child(first.project, "user-edits.txt").replaceWithText("keep me"),
        "create earlier user file");
  const int requests = server.requests;
  const auto second = install(installer, fixture, entry);
  check(second.status == Status::succeeded && second.cached && server.requests == requests &&
            second.project != first.project,
        "verified cache creates a distinct copy without HTTP");
  auto fresh = Project::fromJson(parseJson(child(second.project, ".core-manager/project.json")));
  check(fresh.id != original.id && fresh.samples[0].name == fixture.original.samples[0].name &&
            child(first.project, "user-edits.txt").loadFileAsString() == "keep me",
        "earlier edits retained and recovery isolated");
  {
    Manager manager{std::chrono::milliseconds(20)};
    manager.open(second.project);
    const auto ready = settle(manager);
    check(ready.status == "Saved" && ready.project.samples[0].name == fresh.samples[0].name &&
              ready.project.warnings.isEmpty(),
          "installed copy opens ready without old recovery");
  }
  const auto cached = fixture.cache.getChildFile(entry.sha256 + ".zip");
  check(cached.replaceWithText("broken cache"), "corrupt cache");
  entry.url = server.url("/unknown");
  const auto repaired = install(installer, fixture, entry);
  check(repaired.status == Status::succeeded && !repaired.cached &&
            server.requests == requests + 1 && hashFile(cached) == entry.sha256,
        "corrupt cache is redownloaded, including unknown response length");
  for (int mode = 0; mode < 4; ++mode) {
    const bool timeout = (mode % 2) != 0;
    Fixture blocked;
    Server::Response stall;
    stall.stall = mode < 2;
    stall.stallBody = mode >= 2;
    stall.length = int(blocked.entry.bytes);
    Server stalled({{"/stall", stall}});
    auto local = blocked.entry;
    local.url = stalled.url("/stall");
    DefaultSamplesInstaller transfer;
    transfer.start(local, blocked.downloads, blocked.cache, timeout ? 150 : 30000);
    waitFor([&] { return stalled.requests > 0; });
    if (!timeout)
      transfer.cancel();
    waitFor([&] { return !transfer.snapshot().active(); });
    check(transfer.snapshot().status == (timeout ? Status::failed : Status::cancelled),
          "timeout or cancel interrupts a blocked connect/read");
  }
  {
    Fixture extracting;
    const auto large = extracting.temp.root.getChildFile("large.bin");
    {
      auto out = large.createOutputStream();
      check(out != nullptr, "create extraction cancellation fixture");
      std::array<char, 1024 * 1024> zeros{};
      for (int i = 0; i < 64; ++i)
        check(out->write(zeros.data(), zeros.size()), "write extraction fixture");
    }
    extracting.makeZip("large.bin", large);
    extracting.seedCache();
    DefaultSamplesInstaller transfer;
    transfer.start(extracting.entry, extracting.downloads, extracting.cache);
    waitFor([&] { return transfer.snapshot().status == Status::extracting; });
    transfer.cancel();
    waitFor([&] { return !transfer.snapshot().active(); });
    check(transfer.snapshot().status == Status::cancelled &&
              extracting.downloads.findChildFiles(File::findFilesAndDirectories, false).isEmpty() &&
              extracting.cache.getChildFile(extracting.entry.sha256 + ".zip").existsAsFile(),
          "extraction cancellation cleans staging and retains verified cache");
  }
  for (auto path :
       {"../outside.wav", "/outside.wav", "bank1/../../outside.wav", "bank1\\outside.wav",
        "C:/outside.wav", "BANK1/0.0.wav", "bank1/0.0.wav/child"}) {
    Fixture unsafe;
    unsafe.makeZip(path);
    unsafe.seedCache();
    failInstall(unsafe, unsafe.entry, "reject unsafe ZIP path " + String(path));
    check(!unsafe.temp.root.getChildFile("outside.wav").exists(),
          "extraction never escapes staging");
  }
  {
    Fixture unsafe;
    const auto link = unsafe.temp.root.getChildFile("link.wav");
    std::error_code error;
    std::filesystem::create_symlink(
        std::filesystem::u8path(
            child(unsafe.project, "bank1/0.0.wav").getFullPathName().toStdString()),
        std::filesystem::u8path(link.getFullPathName().toStdString()), error);
    if (!error) {
      unsafe.makeZip("link.wav", link);
      unsafe.seedCache();
      failInstall(unsafe, unsafe.entry, "reject symbolic-link ZIP entry");
    }
  }
  for (int variant = 0; variant < 4; ++variant) {
    Fixture invalid;
    if (variant == 0)
      check(child(invalid.project, ".core-manager/project.json").deleteFile(), "remove manifest");
    if (variant == 1)
      check(child(invalid.project, invalid.original.samples[0].source).deleteFile(),
            "remove source");
    if (variant == 2)
      invalid.entry.bankSizes = {2};
    if (variant == 3)
      check(child(invalid.project, "bank1/0.name.json").replaceWithText("{}"), "damage sidecar");
    invalid.makeZip();
    invalid.seedCache();
    failInstall(invalid, invalid.entry, "reject incomplete or mismatched project");
  }
  defaultSamplesViewTests();
  std::cout << "Default sample installer and app tests passed\n";
}

void defaultSamplesViewTests() {
  Fixture fixture;
  fixture.seedCache();
  Temp isolated;
  juce::ScopedValueSetter<File> privateState(stateRootOverride, isolated.root);
  AppView app;
  app.stopTimer();
  app.setSize(1120, 840);
  auto screenshot = [&](const String &name) {
    const auto directory = juce::SystemStats::getEnvironmentVariable("CORE_DEFAULT_SAMPLES_PREVIEW_DIR", "");
    if (!File::isAbsolutePath(directory))
      return;
    const File folder(directory);
    check(folder.createDirectory().wasOk(), "create UI preview directory");
    auto out = folder.getChildFile(name + ".png").createOutputStream();
    check(out && out->setPosition(0) && out->truncate().wasOk(), "write UI preview");
    juce::PNGImageFormat png;
    check(png.writeImageToStream(app.createComponentSnapshot(app.getLocalBounds()), *out),
          "render UI preview");
  };
  check(app.defaultSamples.isVisible() && !app.cancelDefaults.isVisible() &&
            !app.defaultSamples.getBounds().intersects(app.empty.getBounds()),
        "empty-state default selector layout");
  for (int presentation = 0; presentation < 3; ++presentation) {
    app.presentationBox.setSelectedId(presentation + 1, juce::sendNotificationSync);
    check(app.defaultSamples.isEnabled(), "default choices available in every presentation");
    screenshot(String(presentation) + "-empty");
  }
  app.manager.open(fixture.project);
  settle(app.manager);
  app.changeListenerCallback(nullptr);
  const int presentation = app.presentation;
  auto complete = [&] {
    waitFor([&] {
      if (app.manager.snapshot().generation != app.state.generation)
        app.changeListenerCallback(nullptr);
      app.pollDefaultSamples();
      return !app.defaultsPending && !app.defaultsOpening && !app.pendingGuard && !app.pendingTransition;
    });
    app.changeListenerCallback(nullptr);
    app.timerCallback();
  };
  app.beginDefaultSamples(fixture.entry, fixture.downloads, fixture.cache);
  app.timerCallback();
  check(app.cancelDefaults.isVisible() && app.activity.isVisible() &&
            !app.cancelDefaults.getBounds().intersects(app.statusLabel.getBounds()),
        "footer progress and cancel remain visible without overlap");
  screenshot("progress");
  bool refused = false;
  try {
    app.beginDefaultSamples(fixture.entry, fixture.downloads, fixture.cache);
  } catch (...) {
    refused = true;
  }
  check(refused, "only one installation at a time");
  // The analyzer marks its worker idle before delivering its UI callback.
  app.onlineCompletionPending = true;
  waitFor([&] { return !app.defaults.snapshot().active(); });
  app.pollDefaultSamples();
  check(app.defaultsPending && app.manager.snapshot().root == fixture.project,
        "automatic open waits for the queued analysis completion callback");
  app.onlineCompletionPending = false;
  complete();
  auto ready = settle(app.manager);
  check(ready.root != fixture.project && ready.status == "Saved" &&
            app.presentation == presentation && app.selectedId == ready.project.samples[0].id &&
            app.recents[0] == ready.root.getFullPathName(),
        "automatic open uses normal selection and recents without changing presentation");
  auto installed = ready.root;
  app.manager.edit(ready.project.samples[0].id, "Edit before switch",
                   [](Sample &sample) { sample.name = "Saved before default switch"; });
  app.beginDefaultSamples(fixture.entry, fixture.downloads, fixture.cache);
  complete();
  check(Project::fromJson(parseJson(child(installed, ".core-manager/project.json")))
                .samples[0]
                .name == "Saved before default switch",
        "current edits save before automatic switch");
  ready = settle(app.manager);
  installed = ready.root;
  app.beginDefaultSamples(fixture.entry, fixture.downloads, fixture.cache);
  app.cancelDefaultSamples();
  waitFor([&] { return !app.defaults.snapshot().active(); });
  app.pollDefaultSamples();
  check(app.manager.snapshot().root == installed && !app.defaultsPending,
        "cancel suppresses automatic open");
  app.beginDefaultSamples(fixture.entry, fixture.downloads, fixture.cache);
  app.filesDropped({fixture.project.getFullPathName()}, 0, 0);
  waitFor([&] { return !app.defaults.snapshot().active(); });
  waitFor([&] { app.pollTransitions(); return app.manager.snapshot().root == fixture.project && !app.manager.snapshot().busy; });
  app.pollDefaultSamples();
  check(app.manager.snapshot().root == fixture.project && !app.defaultsPending,
        "explicit project open cancels pending automatic switch");
  app.manager.open(installed);
  ready = settle(app.manager);
  app.changeListenerCallback(nullptr);
  // Real failed render leaves unsaved changes; installing must not bypass it.
  const auto id = ready.project.samples[0].id;
  const auto source = child(installed, ready.project.samples[0].source);
  check(source.deleteFile(), "remove source to provoke a save failure");
  app.manager.edit(id, "Fail current save", [](Sample &sample) { sample.renderBpm = 150; });
  waitFor([&] { return app.manager.idle() && !app.manager.snapshot().busy; });
  check(app.manager.snapshot().primaryUpdateFailed, "current project has unresolved save failure");
  app.changeListenerCallback(nullptr);
  app.beginDefaultSamples(fixture.entry, fixture.downloads, fixture.cache);
  complete();
  check(app.manager.snapshot().root == installed && app.localError.contains("Project saved at") &&
            app.localError.contains("pending changes"),
        "save failure retains current project and reports new folder");
}

// Explicit opt-in acceptance run: exercises the public assets with isolated
// state and Downloads. It is not part of CTest or normal application startup.
void defaultSamplesLiveTests() {
  const auto path =
      juce::SystemStats::getEnvironmentVariable("CORE_DEFAULT_SAMPLES_ACCEPTANCE_DIR", "");
  check(File::isAbsolutePath(path), "set CORE_DEFAULT_SAMPLES_ACCEPTANCE_DIR for live acceptance");
  const File root(path), downloads = root.getChildFile("projects"),
                         cache = root.getChildFile("cache");
  check(downloads.createDirectory().wasOk(), "create acceptance project directory");
  juce::ScopedValueSetter<File> privateState(stateRootOverride, root.getChildFile("state"));
  AppView app;
  app.stopTimer();
  app.setSize(1320, 920);
  for (const auto &entry : defaultSamplesCatalog()) {
    app.beginDefaultSamples(entry, downloads, cache);
    auto last = std::chrono::steady_clock::now();
    waitFor(
        [&] {
          app.pollDefaultSamples();
          app.changeListenerCallback(nullptr);
          if (std::chrono::steady_clock::now() - last > std::chrono::seconds(10)) {
            std::cout << app.defaults.snapshot().message << std::endl;
            last = std::chrono::steady_clock::now();
          }
          return !app.defaultsPending && !app.defaultsOpening && !app.pendingGuard && !app.pendingTransition;
        },
        1800);
    auto ready = settle(app.manager);
    check(app.localError.isEmpty() && ready.status == "Saved" && ready.project.warnings.isEmpty() &&
              int(ready.project.samples.size()) ==
                  std::accumulate(entry.bankSizes.begin(), entry.bankSizes.end(), 0),
          "public collection opens Ready: " + app.localError);
    const auto first = ready.root;
    const auto id = ready.project.id;
    for (const auto &sample : ready.project.samples) {
      check(!sample.protectedEntry, "all public samples editable");
      app.preview.play(child(ready.root, sample.rendered), sample.id);
      check(app.preview.playing() && app.preview.duration() > 0, "public sample playback preview");
      app.preview.stop();
    }
    app.manager.edit(ready.project.samples[0].id, "Acceptance edit",
                     [](Sample &sample) { sample.name = "Acceptance edited copy"; });
    settle(app.manager);
    app.changeListenerCallback(nullptr);
    app.beginDefaultSamples(entry, downloads, cache);
    waitFor(
        [&] {
          app.pollDefaultSamples();
          return !app.defaultsPending && !app.defaultsOpening && !app.pendingGuard && !app.pendingTransition;
        },
        1800);
    ready = settle(app.manager);
    app.changeListenerCallback(nullptr);
    check(app.localError.isEmpty() && app.defaults.snapshot().cached && ready.root != first &&
              ready.project.id != id && ready.project.samples[0].name != "Acceptance edited copy",
          "repeat selection uses cache and leaves edits in earlier project");
    check(
        Project::fromJson(parseJson(child(first, ".core-manager/project.json"))).samples[0].name ==
            "Acceptance edited copy",
        "earlier public project edit retained");
    std::cout << entry.name << ": Ready, previews passed, fresh cached copy "
              << ready.root.getFullPathName() << std::endl;
  }
}
} // namespace core
