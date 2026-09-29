#include "SettingsView.h"
#include "Storage.h"
#include <FixtureData.h>
#include <iostream>
int runVisualizerTests();
namespace core {
void audioTests();
void importTests();
void recoveryTests();
void managerTests();
namespace {
int checks = 0;
void check(bool ok, const char *what) {
  ++checks;
  require(ok, what);
}
template <class F> void rejects(F &&f, const char *what) {
  bool failed = false;
  try {
    f();
  } catch (const std::exception &) {
    failed = true;
  }
  check(failed, what);
}
struct Temp {
  File dir = File::getSpecialLocation(File::tempDirectory)
                 .getChildFile("core-manager-test-" + uuid());
  Temp() {
    require(dir.createDirectory().wasOk(), "Cannot create test directory");
  }
  ~Temp() { dir.deleteRecursively(); }
};
void wav(const File &f, int channels, int rate, int frames) {
  auto out = f.createOutputStream();
  require(out != nullptr, "Cannot create fixture");
  card::writeHeader(*out, uint64_t(frames), rate, channels);
  for (int i = 0; i < frames * channels; ++i)
    out->writeShort(short(i % 12000));
  out->flush();
}
void textFile(const File &f, const String &s) {
  durableWrite(f, s.toRawUTF8(), size_t(s.getNumBytesAsUTF8()));
}
} // namespace
int runTests() {
  Temp privateState;
  stateRootOverride = privateState.dir;
  try {
    auto oldPreferences = privateState.dir.getChildFile("old-preferences.json");
    auto newPreferences =
        privateState.dir.getChildFile("migrated-preferences.json");
    textFile(oldPreferences,
             "{\"input\":\"old-input\",\"output\":\"old-output\","
             "\"reduceMotion\":true,\"referenceRoot\":\"/portable/project\"}");
    auto oldHash = hashFile(oldPreferences);
    check(migrateVisualizerPreferences(oldPreferences, newPreferences),
          "Migrate former visualizer preferences once");
    auto migrated = parseJson(newPreferences);
    check(migrated["input"].toString() == "old-input" &&
              bool(migrated["reduceMotion"]) &&
              migrated["lastProject"].toString() == "/portable/project" &&
              !migrated.hasProperty("presentation"),
          "Migration preserves usable preferences and initial default "
          "presentation");
    check(!migrateVisualizerPreferences(oldPreferences, newPreferences) &&
              hashFile(oldPreferences) == oldHash,
          "Migration never overwrites new preferences or modifies old "
          "preferences");
    for (int ch : {1, 2})
      for (int rate : {44100, 88200}) {
        int size = 0;
        auto name = "website" + String(ch) + String(rate) + "_info";
        auto *data = FixtureData::getNamedResource(name.toRawUTF8(), size);
        check(data && size > 0, "Load independent website fixture");
        juce::MemoryBlock bytes(data, size_t(size));
        auto i = card::decode(bytes);
        check(i.channels == ch && i.rate == rate && i.bpm == 172 &&
                  i.playMode == 3 && i.tempoMatch,
              "Decode flags");
        check(i.slices.size() == 2 && i.slices[0].type == 1 &&
                  i.transients[2][0] == 4800,
              "Decode markers");
        check(card::encode(i) == bytes, "Exact little-endian roundtrip");
        Temp t;
        wav(t.dir.getChildFile("a.wav"), ch, rate, rate * 2);
        auto w = card::inspect(t.dir.getChildFile("a.wav"));
        card::validatePair(w, i);
        check(w.dataOffset == 44, "Canonical 44-byte WAV header");
        auto truncated = bytes;
        truncated.setSize(15);
        rejects([&] { card::decode(truncated); }, "Reject truncated metadata");
        i.transients[0].push_back(1048576);
        rejects([&] { card::encode(i); }, "Do not wrap overflowing transient");
        i.transients[0] = {};
        i.slices[0].start = 3;
        rejects([&] { card::encode(i); }, "Reject unaligned slices");
      }
    rejects([] { card::path(0, 16); }, "Bank overflow must not wrap");
    {
      Temp t;
      Storage store(t.dir);
      rejects([&] { Storage duplicateWriter(t.dir); },
              "One writer per project");
      auto p = store.open();
      check(p.samples.empty(), "Open empty folder");
      auto active = t.dir.getChildFile("bank1/0.0.wav");
      textFile(active, "before");
      auto stage = t.dir.getChildFile(".core-manager/cache/staged");
      textFile(stage, "after");
      p.revision = p.completedRevision = 1;
      Transaction tx(t.dir);
      tx.afterMutation = [](int) {
        throw std::runtime_error("simulated interrupted write");
      };
      rejects(
          [&] { tx.commit({{"bank1/0.0.wav", stage, hashFile(active)}}, p); },
          "Injected write interruption");
      check(active.loadFileAsString() == "before",
            "Rollback keeps completed audio");
      check(Project::fromJson(
                parseJson(t.dir.getChildFile(".core-manager/project.json")))
                    .completedRevision == 0,
            "Rollback keeps manifest");
      tx.afterMutation = {};
      tx.commit({{"bank1/0.0.wav", stage, hashFile(active)}}, p);
      check(active.loadFileAsString() == "after", "Commit replaces audio");
      check(Project::fromJson(
                parseJson(t.dir.getChildFile(".core-manager/project.json")))
                    .completedRevision == 1,
            "Manifest commits last");
      rejects([&] { tx.commit({{"bank1/0.0.wav", stage, "outdated"}}, p); },
              "External change rejected");
      check(active.loadFileAsString() == "after", "Conflict preserves file");
      Temp dest;
      store.duplicateTo(dest.dir);
      check(dest.dir.getChildFile("bank1/0.0.wav").loadFileAsString() ==
                "after",
            "Portable project duplication");
      rejects([&] { child(t.dir, "../escape"); }, "Prevent path traversal");
    }
    {
      Temp t;
      auto f = t.dir.getChildFile("bank1/0.0.wav");
      f.getParentDirectory().createDirectory();
      wav(f, 1, 44100, 88200);
      juce::MemoryBlock bytes(FixtureData::website144100_info,
                              FixtureData::website144100_infoSize);
      durableWrite(f.getSiblingFile("0.0.wav.info"), bytes.getData(),
                   bytes.getSize());
      textFile(t.dir.getChildFile("firmware.save"), "keep");
      textFile(t.dir.getChildFile("bank1/1.0.wav"), "broken");
      auto before = hashFile(f);
      Storage s(t.dir);
      auto p = s.open();
      check(p.samples.size() == 2, "Adopt good and damaged entries");
      check(!p.samples[0].protectedEntry && p.samples[1].protectedEntry,
            "Protect damaged entry individually");
      check(p.samples[0].origin == "recovered card audio" &&
                p.samples[0].sourceDuration == 1,
            "Recover unpadded source");
      check(hashFile(f) == before, "Adoption preserves hardware output");
      check(t.dir.getChildFile("firmware.save").loadFileAsString() == "keep",
            "Preserve unrelated firmware files");
      auto copy = Project::fromJson(p.json());
      check(copy.samples[0].id == p.samples[0].id,
            "Stable ID survives manifest reopen");
      check(card::inspect(child(t.dir, p.samples[0].source)).frames == 44100,
            "Recovered source excludes padding");
    }
    {
      card::Settings settings{{"clock_stop_sync", "on"},
                              {"grimoire/rune1/effect1", "off"}};
      auto files = card::settingsFiles(settings);
      check(files["settings/clock_stop_sync-on"] &&
                !files["settings/clock_stop_sync-off"],
            "Exclusive settings replacements");
      check(files.size() == 4, "Only known edited settings are owned");
      Project p;
      for (int i = 0; i < 16; ++i) {
        Sample s;
        s.slot = i;
        s.protectedEntry = true;
        p.samples.push_back(s);
      }
      check(p.freeSlot(0) == -1 && p.freeSlot(1) == 0,
            "Sixteen slots per bank");
      p.validate();
    }
    {
      Manager manager;
      Look look;
      for (int theme = 0; theme < 3; ++theme) {
        look.presentation(theme);
        SettingsView settings(manager, look, theme);
        require(settings.getNumChildComponents() >= 28,
                "Construct complete settings window");
      }
    }
    audioTests();
    importTests();
    managerTests();
    recoveryTests();
    require(runVisualizerTests() == 0, "Visualizer parity tests");
    std::cout << "PASS " << checks << " contract checks\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "FAIL after " << checks << " checks: " << e.what() << "\n";
    return 1;
  }
}
} // namespace core
