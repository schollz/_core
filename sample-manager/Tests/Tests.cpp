#include "NameMetadata.h"
#include "Manager.h"
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
void renderStatusTests();
void midiSettingsTests();
void firmwareTests();
void firmwareReleaseTests();
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
void spliceTimingChecks() {
  struct TimingCase {
    double duration, bpm;
    int slices, imported, even;
  };
  // Independent expected values from the website's import and Even slices
  // calculations, including the two loops from the firmware export comparison.
  for (auto c : {TimingCase{11.034479166666667, 174, 16, 384, 384},
                 TimingCase{5.647074829931973, 170, 16, 192, 192},
                 TimingCase{5.6, 120, 16, 132, 144},
                 TimingCase{1., 120, 11, 35, 24},
                 TimingCase{.01, 120, 16, 2, 2},
                 TimingCase{400., 120, 1, 32767, 32767}}) {
    Sample s;
    s.sourceDuration = c.duration;
    s.sourceBpm = c.bpm;
    s.slices.clear();
    for (int n = 0; n < c.slices; ++n)
      s.slices.push_back({double(n) / c.slices, double(n + 1) / c.slices, 0});
    s.updateSpliceTrigger(SpliceTimingCalculation::initialImport);
    check(s.spliceTrigger == c.imported, "Imported splice interval matches website");
    s.updateSpliceTrigger(SpliceTimingCalculation::evenSlices);
    check(s.spliceTrigger == c.even, "Even splice interval matches website rounding");
    auto key = audioKey(s);
    s.renderBpm = c.bpm / 2.;
    s.updateSpliceTrigger(SpliceTimingCalculation::evenSlices);
    check(s.spliceTrigger == c.even, "Render tempo preserves loop beat count");
    s.renderBpm = 0;
    s.spliceTrigger = 72;
    check(Sample::fromJson(s.json()).spliceTrigger == 72,
          "Loading a saved interval does not recalculate it");
    s.updateSpliceTrigger(SpliceTimingCalculation::evenSlices);
    check(audioKey(s) == key, "Splice interval remains metadata-only");
  }
  Sample oneShot;
  oneShot.sourceDuration = 4.;
  oneShot.oneShot = true;
  oneShot.updateSpliceTrigger(SpliceTimingCalculation::initialImport);
  check(oneShot.spliceTrigger == 192, "One-shot import uses the website's one beat");
  oneShot.slices.clear();
  rejects([&] { oneShot.updateSpliceTrigger(SpliceTimingCalculation::evenSlices); },
          "An empty slice list cannot calculate an interval");
}
} // namespace
int runTests(const String &suite) {
  Temp privateState;
  stateRootOverride = privateState.dir;
  try {
    // Focused suites let worker/recovery regressions run independently of DSP
    // estimation fixtures. The default continues to run the full contract.
    if (suite == "companion-repair") {
      Temp t;
      auto primary = child(t.dir, "bank1/0.0.wav");
      ensureDirectory(primary.getParentDirectory());
      wav(primary, 2, 44100, 88200);
      Sample sample;
      sample.channels = 2;
      auto info = card::encode(sampleInfo(sample, 44100));
      durableWrite(child(t.dir, "bank1/0.0.wav.info"), info.getData(), info.getSize());
      // Reproduce the online pack: stereo metadata with a mono WAV header.
      wav(child(t.dir, "bank1/0.1.wav"), 1, 44100, 749700);
      info = card::encode(sampleInfo(sample, 352800, true));
      durableWrite(child(t.dir, "bank1/0.1.wav.info"), info.getData(), info.getSize());
      const auto before = hashFile(primary);
      {
        Storage store(t.dir);
        auto adopted = store.open();
        check(!adopted.samples[0].protectedEntry && adopted.samples[0].companionPending &&
                  adopted.warnings.isEmpty(), "Broken companion queues silent recovery");
        auto &legacy = adopted.samples[0];
        legacy.protectedEntry = true;
        legacy.problem = "WAV and metadata disagree about format or circular padding";
        adopted.warnings.add("Bank 1 slot 1: " + legacy.problem);
        adopted.warnings.add(legacy.name + ": Transient lane 1 has a position outside the device's encoding range.");
        durableJson(child(t.dir, ".core-manager/project.json"), adopted.json());
        auto reopened = store.open();
        check(!reopened.samples[0].protectedEntry && reopened.samples[0].companionPending &&
                  reopened.warnings.isEmpty(), "Existing protected manifest recovers and clears stale warnings");
        check(hashFile(primary) == before, "Repair preserves primary WAV bytes");
      }
      {
        Manager manager;
        manager.open(t.dir);
        for (int n = 0; n < 2000; ++n) {
          auto state = manager.snapshot();
          if (state.available && !state.busy && !state.backgroundBusy)
            break;
          juce::Thread::sleep(10);
        }
        auto state = manager.snapshot();
        check(state.available && state.error.isEmpty() && state.companionError.isEmpty() &&
                  state.pendingCompanions == 0 && state.project.warnings.isEmpty(),
              "Background worker completes silent companion repair");
        juce::MemoryBlock bytes;
        check(child(t.dir, "bank1/0.1.wav.info").loadFileAsData(bytes), "Repaired metadata exists");
        card::validatePair(card::inspect(child(t.dir, "bank1/0.1.wav")), card::decode(bytes));
        check(hashFile(primary) == before, "Completed repair preserves primary WAV");
      }
      // Invalid primaries must never be released by companion recovery.
      Storage store(t.dir);
      wav(primary, 1, 44100, 88200);
      auto broken = store.adopt(false);
      check(broken.samples[0].protectedEntry, "Damaged primary remains protected");
      return 0;
    }
    if (suite == "manager") {
      audioTests();
      managerTests();
      renderStatusTests();
      recoveryTests();
      return 0;
    }
    if (suite == "render-status") {
      renderStatusTests();
      return 0;
    }
    if (suite == "midi-settings") {
      midiSettingsTests();
      return 0;
    }
    if (suite == "firmware") {
      firmwareTests();
      return 0;
    }
    if (suite == "firmware-release") {
      firmwareReleaseTests();
      return 0;
    }
    if (suite == "tempo") {
      tempoTests();
      return 0;
    }
    require(suite.isEmpty(), "Unknown self-test suite: " + suite);
    spliceTimingChecks();
    firmwareTests();
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
      check(defaults.at("sample_cv_mapping") == "bank" &&
                card::sampleCVMappingContents({}) == "bank\n" &&
                card::sampleCVMappingContents({{"sample_cv_mapping", "1voct"}}) == "1voct\n",
            "Default and explicit Sample CV mapping use exact firmware text");
      check(card::settingsFiles({{"sample_cv_mapping", "1voct"}}).empty(),
            "Sample CV mapping never creates competing marker files");
      rejects([] { card::sampleCVMappingContents({{"sample_cv_mapping", "invalid"}}); },
              "Invalid mapping cannot be written");
      check(card::midiChannelContents({}) == "1\n", "Missing MIDI channel defaults to 1");
      for (int channel = 1; channel <= 16; ++channel) {
        const card::Settings midi{{"midi_channel", String(channel)}};
        check(card::midiChannelContents(midi) == String(channel) + "\n" && card::settingsFiles(midi).empty(),
              "MIDI channel uses one canonical text setting");
      }
      rejects([] { card::midiChannelContents({{"midi_channel", "17"}}); }, "Reject invalid MIDI channel writes");
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
        check(child(blank.dir, card::sampleCVMappingPath).loadFileAsString() == "bank\n",
              "Project initialization writes the default mapping with a newline");
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
      check(adopted.settings.size() == 2 && adopted.settings.at("midi_channel") == "1" &&
                adopted.settings.at("grimoire/rune1/effect1") == "on" &&
                !child(existing.dir, "settings/grimoire/rune1/effect5-on").exists() &&
                child(existing.dir, "settings/unrelated.txt").loadFileAsString() == "keep",
            "Opening existing settings preserves chosen effects and unrelated files");
    }
    {
      Temp midiFolder;
      juce::StringArray midiWarnings;
      textFile(child(midiFolder.dir, "midi_channel"), "10\n");
      check(card::readSettings(midiFolder.dir, midiWarnings).at("midi_channel") == "10", "Root MIDI channel is adopted");
      for (int channel = 1; channel <= 16; ++channel) {
        textFile(child(midiFolder.dir, card::midiChannelPath), String(channel) + "\r\n");
        check(card::readSettings(midiFolder.dir, midiWarnings).at("midi_channel") == String(channel), "Settings override root MIDI channel, including CRLF");
      }
      for (auto invalid : {"", "0", "17", "01", "-1", " 2", "1.0", "10x", "12345678901234567890"}) {
        midiWarnings.clear(); textFile(child(midiFolder.dir, card::midiChannelPath), invalid);
        check(card::readSettings(midiFolder.dir, midiWarnings).at("midi_channel") == "1" && !midiWarnings.isEmpty(), "Invalid MIDI channel warns and defaults to 1");
      }
      Temp cardFolder;
      juce::StringArray warnings;
      auto read = [&] { return card::readSettings(cardFolder.dir, warnings); };
      check(card::sampleCVMappingContents(read()) == "bank\n" && warnings.isEmpty(),
            "Missing mapping uses Bank divisions");
      textFile(child(cardFolder.dir, "sample_cv_mapping"), "1voct\n");
      {
        Storage storage(cardFolder.dir);
        check(storage.open().settings.at("sample_cv_mapping") == "1voct",
              "Adopting a root mapping does not initialize over it");
      }
      auto mapping = child(cardFolder.dir, card::sampleCVMappingPath);
      textFile(mapping, "bank\n");
      check(read().at("sample_cv_mapping") == "bank", "Settings directory overrides root mapping");
      textFile(mapping, "1voct\r\n");
      check(read().at("sample_cv_mapping") == "1voct", "Firmware mapping accepts CRLF");
      for (const auto &invalid : {String(), String("unknown"), String(" 1voct"),
                                  String("1voct0123456789012345")}) {
        warnings.clear();
        textFile(mapping, invalid);
        check(read().at("sample_cv_mapping") == "bank" && !warnings.isEmpty(),
              "Malformed mapping falls back to Bank divisions and reports a warning");
      }
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
    renderStatusTests();
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
