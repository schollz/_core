#include "NameMetadata.h"
#include "Parallel.h"
#include "SettingsView.h"
#include "Storage.h"
#include <FixtureData.h>
#include <iostream>
int runVisualizerTests();
namespace core {
void audioTests();
void importTests();
void tempoTests();
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
    {
      Temp t;
      auto file = t.dir.getChildFile("hash.bin");
      for (size_t size : {size_t(0), size_t(63), size_t(64), size_t(65),
                          size_t(256 * 1024), size_t(256 * 1024 + 1)}) {
        juce::MemoryBlock bytes(size);
        auto *data = static_cast<uint8_t *>(bytes.getData());
        for (size_t i = 0; i < size; ++i)
          data[i] = uint8_t(i * 17);
        durableWrite(file, bytes.getData(), bytes.getSize());
        check(hashFile(file) == juce::SHA256(bytes).toHexString(),
              "Buffered file SHA-256 matches full bytes across block/buffer "
              "boundaries");
      }
      int reads = 0;
      rejects([&] { hashFile(file, [&] { return ++reads > 1; }); },
              "An in-progress hash can be cancelled between buffered reads");
      check(reads > 1, "Cancellation interrupts a hash after reading has begun");
      std::vector<int> results(33, -1);
      const auto caller = std::this_thread::get_id();
      size_t reported = 0;
      parallelFor(
          results.size(), [&](size_t i) { results[i] = int(i); },
          [&](size_t completed) {
            check(std::this_thread::get_id() == caller &&
                      completed >= reported && completed <= results.size(),
                  "Parallel checks report monotonic progress on caller thread");
            reported = completed;
          });
      check(reported == results.size(), "Parallel checks finish progress");
      for (size_t i = 0; i < results.size(); ++i)
        check(results[i] == int(i),
              "Parallel checks process each file exactly once");
      rejects(
          [&] {
            parallelFor(results.size(), [](size_t i) {
              if (i == 3)
                throw std::runtime_error("failed file read");
            });
          },
          "Parallel file errors propagate after joining workers");
    }
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
        auto edgeMarkers = i;
        for (auto &lane : edgeMarkers.transients)
          lane = {0, 15, 16, 1048560, 1048575};
        check(card::compatibility(edgeMarkers).isEmpty(),
              "Zero, first-block and maximum transient positions are encodable");
        auto decodedEdges = card::decode(card::encode(edgeMarkers));
        for (const auto &lane : decodedEdges.transients)
          check(lane == std::vector<uint32_t>{0, 0, 16, 1048560, 1048560},
                "Counted zero markers survive the firmware's 16-frame encoding");
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
      auto nameFile = child(t.dir, "bank1/0.name.json");
      auto nameStage = child(t.dir, ".core-manager/cache/staged-name");
      durableJson(nameFile,
                  names::encode("Before", "original.wav", hashFile(active)));
      durableJson(nameStage,
                  names::encode("After", "original.wav", hashFile(stage)));
      auto beforeName = hashFile(nameFile);
      std::vector<Replacement> replacements{
          {"bank1/0.0.wav", stage, hashFile(active)},
          {"bank1/0.name.json", nameStage, beforeName}};
      p.revision = p.completedRevision = 1;
      Transaction tx(t.dir);
      auto changedSource = replacements;
      changedSource[0].contentHash = hashFile(stage);
      textFile(stage, "changed after planning");
      rejects([&] { tx.commit(changedSource, p); },
              "A source changed after save planning cannot commit stale hashes");
      check(active.loadFileAsString() == "before" && hashFile(nameFile) == beforeName,
            "Changed staged content preserves active audio and names");
      textFile(stage, "after");
      for (int interruption : {0, 1, 2}) {
        tx.afterMutation = [interruption](int mutation) {
          if (mutation == interruption)
            throw std::runtime_error("simulated interrupted write");
        };
        rejects([&] { tx.commit(replacements, p); }, "Injected write interruption");
        check(active.loadFileAsString() == "before" && hashFile(nameFile) == beforeName,
              "Audio and filename metadata roll back together at every mutation");
      }
      check(active.loadFileAsString() == "before",
            "Rollback keeps completed audio");
      check(Project::fromJson(
                parseJson(t.dir.getChildFile(".core-manager/project.json")))
                    .completedRevision == 0,
            "Rollback keeps manifest");
      tx.afterMutation = {};
      tx.commit(replacements, p);
      check(active.loadFileAsString() == "after", "Commit replaces audio");
      check(hashFile(nameFile) == hashFile(nameStage),
            "Commit replaces name metadata");
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
      auto sidecar = t.dir.getChildFile("bank1/0.name.json");
      durableWrite(sidecar, FixtureData::websitename_json,
                   size_t(FixtureData::websitename_jsonSize));
      auto websiteName = parseJson(sidecar);
      auto before = hashFile(f);
      Storage s(t.dir);
      auto p = s.open();
      check(p.samples.size() == 2, "Adopt good and damaged entries");
      check(!p.samples[0].protectedEntry && p.samples[1].protectedEntry,
            "Protect damaged entry individually");
      check(p.samples[0].name == websiteName["name"].toString() &&
                p.samples[0].originalFilename ==
                    websiteName["originalFilename"].toString() &&
                p.fingerprints.at("bank1/0.name.json") == hashFile(sidecar),
            "Website pack fixture restores Unicode names and owns matching "
            "sidecar");
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
      auto emptyName = names::encode("", "original.wav", before);
      durableJson(sidecar, emptyName);
      check(s.adopt(false).samples[0].name == "original.wav",
            "Empty display name falls back to original filename");
      durableJson(sidecar, names::encode("", "", before));
      check(s.adopt(false).samples[0].name == "Recovered card audio 1",
            "Empty names fall back to generic label");
      std::vector<String> invalid{
          "{bad JSON",
          "[]",
          "{\"schema\":2}",
          "{\"schema\":4294967297}",
          juce::JSON::toString(names::encode("Stale", "stale.wav",
                                             String::repeatedString("0", 64))),
          String::repeatedString("x", names::maxBytes + 1)};
      auto wrongType = names::encode("Name", "original.wav", before);
      invalid.push_back(juce::JSON::toString(wrongType) + " trailing data");
      invalid.push_back("{\"nested\":" + String::repeatedString("[", 32) + "0" +
                        String::repeatedString("]", 32) + "}");
      put(wrongType, "name", 123);
      invalid.push_back(juce::JSON::toString(wrongType));
      auto directory = names::encode("Name", "original.wav", before);
      put(directory, "originalFilename", "/private/source.wav");
      invalid.push_back(juce::JSON::toString(directory));
      for (const auto &contents : invalid) {
        textFile(sidecar, contents);
        auto nameHash = hashFile(sidecar);
        if (contents.length() > names::maxBytes)
          check(fingerprint(sidecar) == "oversized filename metadata",
                "Owned metadata fingerprinting also bounds reads to 64 KiB");
        auto recovered = s.adopt(false);
        check(!recovered.samples[0].protectedEntry &&
                  recovered.samples[0].name == "Recovered card audio 1" &&
                  recovered.samples[0].originalFilename.isEmpty() &&
                  recovered.fingerprints.count("bank1/0.name.json") == 0 &&
                  recovered.warnings.joinIntoString(" ").contains(
                      "Filename persistence skipped") &&
                  hashFile(sidecar) == nameHash && hashFile(f) == before,
              "Invalid, unsupported, oversized or stale sidecar preserves "
              "audio and falls back");
      }
      require(sidecar.deleteFile(), "Remove sidecar fixture");
      durableJson(t.dir.getChildFile("bank1/2.name.json"),
                  names::encode("Wrong slot", "wrong.wav", before));
      auto noNames = s.adopt(false);
      check(noNames.samples[0].name == "Recovered card audio 1" &&
                noNames.samples[0].originalFilename.isEmpty(),
            "Never search other slots for names even when audio hash matches");
      auto legacy = noNames.samples[0].json();
      legacy.getDynamicObject()->removeProperty("originalFilename");
      check(Sample::fromJson(legacy).originalFilename.isEmpty(),
            "Old schema-1 projects omit originalFilename without inventing it");
    }
    {
      card::Settings settings{{"clock_stop_sync", "on"},
                              {"grimoire/rune1/effect1", "off"}};
      auto files = card::settingsFiles(settings);
      check(files["settings/clock_stop_sync-on"] && !files["settings/clock_stop_sync-off"],
            "Exclusive settings replacements");
      check(files.size() == 4, "Only known edited settings are owned");
      auto defaults = card::defaultSettings();
      for (int effect = 1; effect <= 16; ++effect)
        check(defaults.at("grimoire/rune1/effect" + String(effect)) == (effect == 5 ? "on" : "off"),
              "Default first effect bank enables only Time Stretch");
      check(defaults.at("grimoire/rune2/effect6") == "on" &&
                defaults.at("grimoire/rune2/effect13") == "on" &&
                defaults.at("grimoire/rune7/effect16") == "off",
            "Other effect banks keep the website presets");
      Project p;
      for (int i = 0; i < 16; ++i) {
        Sample s;
        s.slot = i;
        s.protectedEntry = true;
        p.samples.push_back(s);
      }
      check(p.freeSlot(0) == -1 && p.freeSlot(1) == 0, "Sixteen slots per bank");
      p.validate();
    }
    {
      Temp blank;
      {
        Storage storage(blank.dir);
        auto initial = storage.open();
        check(initial.settings == card::defaultSettings(),
              "New settings are recorded in the initial manifest");
      }
      {
        Storage storage(blank.dir);
        auto reopened = storage.open();
        check(reopened.settings == card::defaultSettings() && reopened.warnings.isEmpty(),
              "Default settings reopen without external-change warnings");
      }
      Temp existing;
      durableWrite(child(existing.dir, "settings/grimoire/rune1/effect1-on"), "", 0);
      durableWrite(child(existing.dir, "settings/unrelated.txt"), "keep", 4);
      Storage storage(existing.dir);
      auto adopted = storage.open();
      check(adopted.settings.size() == 1 && adopted.settings.at("grimoire/rune1/effect1") == "on" &&
                !child(existing.dir, "settings/grimoire/rune1/effect5-on").exists() &&
                child(existing.dir, "settings/unrelated.txt").loadFileAsString() == "keep",
            "Opening existing settings preserves chosen effects and unrelated files");
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
    tempoTests();
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
