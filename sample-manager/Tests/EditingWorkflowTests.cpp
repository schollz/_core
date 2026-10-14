#include "AppView.h"
#include "Manager.h"
#include "WaveformEditor.h"
#include <iostream>
namespace core {
namespace {
ManagerState waitFor(Manager &manager, const std::function<bool(const ManagerState &)> &predicate,
                     int timeoutMs = 10000) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
  do {
    auto state = manager.snapshot();
    if (predicate(state))
      return state;
    juce::Thread::sleep(5);
  } while (std::chrono::steady_clock::now() < deadline);
  throw std::runtime_error(
      "Editing workflow timed out: " + manager.snapshot().status.toStdString() + " " +
      manager.snapshot().error.toStdString());
}
ManagerState settled(Manager &m, int timeout = 10000) {
  return waitFor(m, [](const auto &s) { return !s.busy; }, timeout);
}
void success(const ManagerState &s) { require(s.error.isEmpty(), s.error); }
struct Fixture {
  File root = File::getSpecialLocation(File::tempDirectory).getChildFile("core-editing-" + uuid());
  File input;
  Fixture() {
    require(root.createDirectory().wasOk(), "Create workflow fixture");
    input = root.getChildFile("input-bpm120.wav");
    auto output = input.createOutputStream();
    card::writeHeader(*output, 44100, 44100, 1);
    for (int n = 0; n < 44100; ++n)
      output->writeShort(short(std::sin(n * .05) * 12000));
  }
  ~Fixture() {
    root.setReadOnly(false, true);
    root.deleteRecursively();
  }
  File project(const String &name) {
    auto folder = root.getChildFile(name);
    require(folder.createDirectory().wasOk(), "Create project");
    return folder;
  }
};
String imported(Manager &manager, const Fixture &fixture, const File &root, int count = 1) {
  manager.open(root);
  success(settled(manager));
  juce::StringArray paths;
  for (int n = 0; n < count; ++n)
    paths.add(fixture.input.getFullPathName());
  manager.import(paths, 0);
  const auto ready = settled(manager);
  success(ready);
  require(ready.project.samples.size() == size_t(count), "Workflow imports settle");
  return ready.project.samples.front().id;
}
void manualAndDebounce(Fixture &fixture) {
  const auto folder = fixture.project("manual");
  String id;
  String savedHash;
  {
    Manager manager;
    id = imported(manager, fixture, folder);
    const auto info = child(folder, card::path(0, 0) + ".info");
    savedHash = hashFile(info);
    manager.setAutoSave(false);
    success(settled(manager));
    manager.edit(id, "Manual edit", [](Sample &s) { s.sourceBpm = 151; });
    auto pending = settled(manager);
    success(pending);
    require(pending.hasUnsavedChanges && !pending.isSaving && hashFile(info) == savedHash,
            "Manual mode leaves hardware unchanged");
    require(pending.status == "Unsaved changes", "Manual mode has explicit pending status");
    manager.edit(id, "Pending render", [](Sample &s) { s.renderBpm = 130; });
    pending = settled(manager);
    require(pending.renderStatus(id).text().contains("pending save") &&
                !pending.renderStatus(id).text().contains("rendering"),
            "Manual target is not described as rendering");
    waitFor(manager, [](const auto &s) { return !s.recoveryPending; });
    auto local = parseJson(stateRoot().getChildFile("recovery/" + pending.project.id + ".json"));
    require(Project::fromJson(local["project"]).find(id)->sourceBpm == 151 &&
                local["history"]["past"].getArray()->size() >= 2,
            "Manual edits and undo history are checkpointed");
  }
  {
    Manager manager{std::chrono::milliseconds(20)};
    manager.setAutoSave(false);
    manager.open(folder);
    auto reopened = settled(manager);
    success(reopened);
    require(reopened.hasUnsavedChanges && reopened.project.find(id)->sourceBpm == 151 &&
                reopened.canUndo,
            "Manual pending state and history survive reopen");
    manager.discardPending();
    success(settled(manager));
  }
  {
    Manager manager{std::chrono::milliseconds(20)};
    manager.setAutoSave(false);
    manager.open(folder);
    auto reopened = settled(manager);
    success(reopened);
    require(!reopened.hasUnsavedChanges && reopened.project.find(id)->sourceBpm == 120 &&
                hashFile(child(folder, card::path(0, 0) + ".info")) == savedHash &&
                reopened.canUndo,
            "Discard cannot resurrect portable or local pending edits");
    manager.edit(id, "Explicit save", [](Sample &s) { s.sourceBpm = 152; });
    settled(manager);
    std::atomic<bool> saving{false};
    manager.beforeSave = [&] {
      saving = true;
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    };
    manager.saveNow();
    waitFor(manager, [&](const auto &) { return saving.load(); });
    manager.saveNow();
    auto saved = settled(manager);
    success(saved);
    require(!saved.hasUnsavedChanges && saved.project.find(id)->sourceBpm == 152,
            "Save commits manual edits");
    manager.edit(id, "Manual edit after duplicate Save", [](Sample &s) { s.sourceBpm = 153; });
    auto later = settled(manager);
    require(later.hasUnsavedChanges && later.completedProject.find(id)->sourceBpm == 152,
            "Repeated Save during an active save does not enable saving later manual edits");
    manager.discardPending();
    success(settled(manager));
  }
  {
    Manager manager;
    manager.open(folder);
    success(settled(manager));
    const auto started = std::chrono::steady_clock::now();
    manager.edit(id, "Debounced edit", [](Sample &s) { s.sourceBpm = 153; });
    waitFor(manager, [](const auto &s) { return s.hasUnsavedChanges; });
    juce::Thread::sleep(600);
    manager.edit(id, "Reset debounce", [](Sample &s) { s.sourceBpm = 154; });
    waitFor(manager, [&](const auto &s) { return s.project.find(id)->sourceBpm == 154; });
    juce::Thread::sleep(500);
    require(manager.snapshot().completedProject.find(id)->sourceBpm == 152,
            "Autosave waits after the last edit");
    auto saved = settled(manager);
    success(saved);
    require(std::chrono::steady_clock::now() - started >= std::chrono::milliseconds(1600) &&
                saved.project.find(id)->sourceBpm == 154,
            "Production autosave delay is one second");
  }
}
void slowSave(Fixture &fixture) {
  const auto folder = fixture.project("slow");
  Manager manager{std::chrono::milliseconds(20)};
  const auto id = imported(manager, fixture, folder);
  const auto wav = child(folder, card::path(0, 0));
  const auto hash = hashFile(wav);
  const auto modified = wav.getLastModificationTime();
  std::atomic<bool> entered{false};
  manager.beforeSave = [&] {
    if (!entered.exchange(true))
      std::this_thread::sleep_for(std::chrono::seconds(30));
  };
  manager.edit(id, "Start blocked save", [](Sample &s) { s.sourceBpm = 177; });
  waitFor(manager, [&](const auto &) { return entered.load(); });
  const auto started = std::chrono::steady_clock::now();
  manager.setAutoSave(false);
  manager.editMarkerLane(id, -1, {{0, .3, 0}, {.3, 1, 0}}, {}, 1);
  manager.editMarkerLane(id, 0, {}, {.3, .6}, 2);
  manager.editMarkerLane(id, 1, {}, {.4}, 3);
  manager.undo();
  manager.redo();
  manager.edit(id, "Edit during slow I/O", [](Sample &s) { s.sourceBpm = 178; });
  auto live = waitFor(
      manager,
      [&](const auto &s) {
        const auto *sample = s.project.find(id);
        return sample && sample->sourceBpm == 178 &&
               sample->transients[1] == std::vector<double>{.4};
      },
      2000);
  require(std::chrono::steady_clock::now() - started < std::chrono::seconds(2) && live.isSaving,
          "Marker edits and undo/redo are accepted while the 30-second save is blocked");
  waitFor(manager, [](const auto &s) { return !s.recoveryPending; }, 3000);
  const auto checkpoint =
      parseJson(stateRoot().getChildFile("recovery/" + live.project.id + ".json"));
  require(Project::fromJson(checkpoint["project"]).find(id)->sourceBpm == 178,
          "Local checkpoint advances independently of blocked card I/O");
  auto older = settled(manager, 35000);
  success(older);
  require(older.hasUnsavedChanges && older.project.find(id)->sourceBpm == 178 &&
              older.completedProject.find(id)->sourceBpm == 177 &&
              older.project.find(id)->slices.size() == 2,
          "Older save completion does not overwrite newer live edits");
  manager.saveNow();
  auto final = settled(manager);
  success(final);
  require(!final.hasUnsavedChanges &&
              final.completedProject.find(id)->transients[0] == std::vector<double>({.3, .6}) &&
              hashFile(wav) == hash && wav.getLastModificationTime() == modified &&
              final.library.prepared == 0,
          "Final revision saves markers without rewriting or analysing audio");
}
void affectedFilesAndFailure(Fixture &fixture) {
  auto folder = fixture.project("affected");
  Manager manager{std::chrono::milliseconds(20)};
  const auto id = imported(manager, fixture, folder, 2);
  const auto other = child(folder, card::path(0, 1));
  other.appendText("external");
  const auto otherHash = hashFile(other);
  manager.edit(id, "Only selected slot", [](Sample &s) { s.sourceBpm = 155; });
  auto state = settled(manager);
  success(state);
  require(hashFile(other) == otherHash && state.library.prepared == 0,
          "Unrelated external change is preserved and does not block an affected-slot save");
  const auto damagedId = state.project.samples[1].id;
  require(state.project.find(damagedId)->protectedEntry,
          "Newly detected damage in an already open project enables recovery removal");
  manager.remove({damagedId});
  state = settled(manager);
  success(state);
  require(!state.project.find(damagedId),
          "An already open damaged slot can be removed without reconcile");
  manager.undo();
  state = settled(manager);
  success(state);
  require(state.project.find(damagedId)->protectedEntry && hashFile(other) == otherHash,
          "Undo restores the exact newly damaged output");
  const auto own = child(folder, card::path(0, 0));
  juce::MemoryBlock original;
  require(own.loadFileAsData(original), "Read original");
  own.appendText("changed");
  const auto changedHash = hashFile(own);
  manager.edit(id, "Conflict", [](Sample &s) { s.sourceBpm = 156; });
  state = settled(manager);
  require(state.error.contains("External change") && state.hasUnsavedChanges &&
              hashFile(own) == changedHash,
          "Changed audio in the affected slot blocks a metadata save");
  const auto generation = state.generation;
  juce::Thread::sleep(1200);
  require(manager.snapshot().generation == generation, "Failed autosave does not retry in a loop");
  durableWrite(own, original.getData(), original.getSize());
  manager.retry();
  state = settled(manager);
  success(state);
  require(!state.hasUnsavedChanges && state.project.find(id)->sourceBpm == 156,
          "Retry saves retained edits");
}
void damagedVariants(Fixture &fixture) {
  for (bool adopt : {false, true})
    for (int kind = 0; kind < 5; ++kind) {
      const auto folder =
          fixture.project("damaged-" + String(adopt ? "card-" : "native-") + String(kind));
      {
        Manager manager{std::chrono::milliseconds(20)};
        imported(manager, fixture, folder, 2);
      }
      const auto wav = child(folder, card::path(0, 0));
      const auto info = child(folder, card::path(0, 0) + ".info");
      if (kind == 0)
        require(info.deleteFile(), "Remove metadata fixture");
      if (kind == 1)
        require(wav.deleteFile(), "Remove WAV fixture");
      if (kind == 2)
        durableWrite(wav, "invalid WAV", 11);
      if (kind == 3) {
        juce::MemoryBlock bytes;
        require(info.loadFileAsData(bytes), "Read metadata fixture");
        static_cast<uint8_t *>(bytes.getData())[7] |= 0x80;
        durableWrite(info, bytes.getData(), bytes.getSize());
      }
      if (kind == 4) {
        durableWrite(info, "bad", 3);
        juce::MemoryBlock oversized(128 * 1024, true);
        const auto name = child(folder, "bank1/0.name.json");
        durableWrite(name, oversized.getData(), oversized.getSize());
        const auto before = hashFile(name);
        Project completed =
            Project::fromJson(parseJson(child(folder, ".core-manager/project.json")));
        Transaction transaction(folder);
        transaction.afterMutation = [](int) {
          throw std::runtime_error("Injected raw removal interruption");
        };
        bool failed = false;
        try {
          transaction.commit({{"bank1/0.name.json", {}, before, {}, true}}, completed);
        } catch (...) {
          failed = true;
        }
        require(failed && hashFile(name) == before,
                "Interrupted raw sidecar removal restores its exact oversized backup");
      }
      if (adopt)
        require(child(folder, ".core-manager/project.json").deleteFile(),
                "Adopt card without native manifest");
      Manager manager{std::chrono::milliseconds(20)};
      manager.setAutoSave(false);
      manager.open(folder);
      auto state = settled(manager);
      success(state);
      const auto damaged = state.project.samples.front().id;
      require(state.project.find(damaged)->protectedEntry,
              "Missing, invalid and unsupported slots remain protected");
      manager.remove({damaged});
      settled(manager);
      manager.saveNow();
      state = settled(manager);
      success(state);
      require(state.project.samples.size() == 1, "Damaged removal leaves healthy entries intact");
      manager.import({fixture.input.getFullPathName()}, 0);
      settled(manager);
      manager.saveNow();
      state = settled(manager);
      success(state);
      require(state.project.samples.size() == 2 && !state.project.samples.back().protectedEntry,
              "Absent corrupt files do not leave stale fingerprints that block slot reuse");
    }
}
void malformedNames(Fixture &fixture) {
  const auto folder = fixture.project("malformed-name-only");
  String id;
  {
    Manager manager{std::chrono::milliseconds(20)};
    id = imported(manager, fixture, folder);
  }
  const auto name = child(folder, "bank1/0.name.json");
  juce::MemoryBlock oversized(128 * 1024, true);
  durableWrite(name, oversized.getData(), oversized.getSize());
  const auto nameHash = hashFile(name);
  const auto waveHash = hashFile(child(folder, card::path(0, 0)));
  Manager manager{std::chrono::milliseconds(20)};
  manager.setAutoSave(false);
  manager.open(folder);
  auto state = settled(manager);
  success(state);
  require(state.project.find(id)->nameUnsafe && !state.project.find(id)->protectedEntry &&
              state.editorWaveform(id),
          "Malformed filename metadata keeps healthy audio available");
  manager.remove({id});
  settled(manager);
  manager.saveNow();
  success(settled(manager));
  manager.undo();
  settled(manager);
  manager.saveNow();
  state = settled(manager);
  success(state);
  require(hashFile(name) == nameHash && hashFile(child(folder, card::path(0, 0))) == waveHash &&
              !state.project.find(id)->protectedEntry,
          "Recoverable removal also restores exact malformed sidecars for healthy audio");
  manager.move({id}, 0, 1);
  settled(manager);
  manager.saveNow();
  state = settled(manager);
  success(state);
  require(state.project.find(id)->slot == 1 && !state.project.find(id)->nameUnsafe,
          "Reordering recovered healthy audio clears its old-slot references and refreshes "
          "filename health");
  manager.undo();
  settled(manager);
  manager.saveNow();
  success(settled(manager));
  require(hashFile(name) == nameHash, "Undo of reorder restores raw filename metadata");
  manager.move({id}, 1);
  settled(manager);
  manager.saveNow();
  success(settled(manager));
  manager.undo();
  settled(manager);
  manager.saveNow();
  state = settled(manager);
  success(state);
  require(state.project.find(id)->bank == 0 && hashFile(name) == nameHash,
          "Undoing a move of a recovered healthy sample restores its exact malformed sidecar");
  for (double bpm : {131., 132.}) {
    manager.edit(id, "Edit after raw restore", [bpm](Sample &s) { s.sourceBpm = bpm; });
    settled(manager);
    manager.saveNow();
    state = settled(manager);
    success(state);
    require(hashFile(name) == nameHash,
            "Subsequent metadata edits preserve unusable filename sidecars");
  }
}
void backupFailure(Fixture &fixture) {
#if !JUCE_WINDOWS
  const auto folder = fixture.project("backup-failure");
  {
    Manager manager{std::chrono::milliseconds(20)};
    imported(manager, fixture, folder);
  }
  const auto info = child(folder, card::path(0, 0) + ".info");
  durableWrite(info, "bad", 3);
  const auto before = hashFile(child(folder, card::path(0, 0)));
  Manager manager{std::chrono::milliseconds(20)};
  manager.setAutoSave(false);
  manager.open(folder);
  auto state = settled(manager);
  success(state);
  const auto id = state.project.samples.front().id;
  const auto recovery = child(folder, ".core-manager/recovery");
  require(recovery.setReadOnly(true), "Make backup directory unwritable");
  manager.remove({id});
  state = settled(manager);
  recovery.setReadOnly(false);
  require(state.error.isNotEmpty() && state.project.find(id) && info.loadFileAsString() == "bad" &&
              hashFile(child(folder, card::path(0, 0))) == before,
          "Backup failure leaves the damaged slot and active bytes intact");
#endif
}
void damagedRemoval(Fixture &fixture) {
  const auto folder = fixture.project("damaged");
  String id;
  {
    Manager manager{std::chrono::milliseconds(20)};
    id = imported(manager, fixture, folder);
  }
  const auto info = child(folder, card::path(0, 0) + ".info");
  const auto wav = child(folder, card::path(0, 0));
  const auto wavHash = hashFile(wav);
  const auto sidecar = child(folder, "bank1/0.name.json");
  durableWrite(info, "bad", 3);
  durableWrite(sidecar, "{invalid", 8);
  durableWrite(child(folder, "bank1/0.1.wav"), "legacy", 6);
  durableWrite(child(folder, "bank1/0.unrelated"), "unknown", 7);
  {
    Manager manager{std::chrono::milliseconds(20)};
    manager.setAutoSave(false);
    manager.open(folder);
    auto state = settled(manager);
    success(state);
    require(state.project.find(id)->protectedEntry && state.project.find(id)->problem.isNotEmpty(),
            "Corruption in an existing native project remains selectable and protected");
    manager.clearBank(0);
    state = settled(manager);
    success(state);
    require(state.project.find(id), "Clear bank retains damaged entries");
    manager.remove({id});
    state = settled(manager);
    success(state);
    require(!state.project.find(id) && info.existsAsFile(), "Manual removal is pending until Save");
    manager.undo();
    state = settled(manager);
    success(state);
    require(state.project.find(id)->protectedEntry &&
                !state.project.find(id)->recoveryFiles.empty(),
            "Undo before Save retains the damaged slot and recovery references");
    manager.redo();
    settled(manager);
    manager.saveNow();
    state = settled(manager);
    success(state);
    require(!info.exists() && !wav.exists() && !sidecar.exists() &&
                child(folder, "bank1/0.1.wav").exists() &&
                child(folder, "bank1/0.unrelated").exists(),
            "Removal clears only recognized selected-slot files");
  }
  {
    Manager manager{std::chrono::milliseconds(20)};
    manager.setAutoSave(false);
    manager.open(folder);
    auto state = settled(manager);
    success(state);
    require(state.canUndo, "Saved damaged removal restores history");
    manager.undo();
    state = settled(manager);
    success(state);
    require(state.project.find(id) && state.project.find(id)->protectedEntry,
            "Undo after reopen restores the protected sample record");
    manager.saveNow();
    state = settled(manager);
    success(state);
    require(hashFile(wav) == wavHash && info.loadFileAsString() == "bad" &&
                sidecar.loadFileAsString() == "{invalid",
            "Undo restores exact damaged bytes without repairing them");
    manager.redo();
    settled(manager);
    manager.saveNow();
    success(settled(manager));
    manager.import({fixture.input.getFullPathName()}, 0);
    settled(manager);
    manager.saveNow();
    state = settled(manager);
    success(state);
    require(state.project.samples.size() == 1 && state.project.samples[0].slot == 0 &&
                state.project.samples[0].id != id,
            "Freed damaged slot can be reused for import");
  }
}
} // namespace
void waveformEditorTests() {
  Look look;
  WaveformEditor editor(look);
  editor.setSize(224, 160);
  Sample sample;
  sample.sourceDuration = 2;
  sample.slices = {{0, .25, 0}, {.25, .5, 0}, {.5, 1, 0}};
  sample.transients[1] = {.8};
  editor.set(&sample, {});
  editor.lane = 0;
  require(editor.snapped(70, false) == .25 && editor.snapped(73, false) == editor.at(73),
          "Snap uses a ten-pixel threshold");
  require(editor.snapped(12, false) == 0 && editor.snapped(212, false) == 1 &&
              editor.snapTarget < 0,
          "Zero snaps; terminal end is not a target");
  editor.sample.slices = {{0, .1, 0}, {.1, .2, 0}, {.2, 1, 0}};
  require(editor.snapped(42, false) == .1, "Equal-distance tie selects the earlier slice start");
  editor.sample = sample;
  editor.zoom = 4;
  editor.pan = .2;
  require(editor.snapped(editor.x(.25) + 9, false) == .25 &&
              editor.snapped(editor.x(.25) + 9, true) != .25,
          "Zoom/pan and Option/Alt bypass work in screen pixels");
  editor.setSnapToSlices(false);
  require(editor.snapped(editor.x(.25) + 4, false) != .25, "Disabled snapping is free movement");
  editor.setSnapToSlices(true);
  editor.zoom = 1;
  editor.pan = 0;
  uint64_t sequence = 0;
  int patchedLane = -2;
  editor.onEdit = [&](const Sample &, int lane, uint64_t seq) {
    patchedLane = lane;
    sequence = seq;
  };
  for (int lane = 0; lane < 3; ++lane) {
    editor.lane = lane;
    const auto normalised = editor.snapped(65, false);
    editor.sample.transients[size_t(lane)] = {normalised * sample.sourceDuration,
                                              normalised * sample.sourceDuration};
    editor.marker = 1;
    editor.changed = true;
    editor.commit();
    require(editor.sample.transients[size_t(lane)] == std::vector<double>{.5} &&
                patchedLane == lane,
            "All lanes store source seconds, avoid snap duplicates, and submit lane patches");
    editor.set(&sample, {}, 0);
    require(editor.sample.transients[size_t(lane)] == std::vector<double>{.5},
            "Unacknowledged draft survives older publications");
  }
  editor.set(&sample, {}, sequence);
  require(editor.sample.transients[1] == sample.transients[1],
          "Acknowledged publication replaces the local draft");
  editor.lane = 0;
  editor.sample.transients[0] = {0., 0., .5, .5};
  editor.marker = 3;
  editor.snapTarget = .25;
  editor.changed = true;
  editor.commit();
  require(editor.sample.transients[0] == std::vector<double>({0., 0., .5}),
          "Snapping preserves pre-existing duplicate markers elsewhere in the lane");
  require(!editor.sample.spliceVariable, "Transient gestures preserve splice timing");
}
void editingWorkflowViewTests() {
  Fixture fixture;
  const auto folder = fixture.project("view");
  AppView app;
  app.stopTimer();
  const auto id = imported(app.manager, fixture, folder);
  app.changeListenerCallback(nullptr);
  app.manager.setAutoSave(false);
  settled(app.manager);
  app.changeListenerCallback(nullptr);
  app.autoSaveToggle.setToggleState(false, juce::dontSendNotification);
  app.snapToggle.setToggleState(true, juce::dontSendNotification);
  app.savePreferences();
  const auto preferences = parseJson(preferencesFile());
  require(!bool(preferences["autoSave"]) && bool(preferences["snapToSlices"]),
          "App-wide saving and snapping preferences persist");
  auto pump = [&] {
    app.changeListenerCallback(nullptr);
    app.pollTransitions();
  };
  auto until = [&](const auto &predicate) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (!predicate()) {
      pump();
      juce::Thread::sleep(5);
      require(std::chrono::steady_clock::now() < deadline, "UI transition timeout");
    }
  };
  auto dirty = [&] {
    app.manager.edit(id, "UI pending edit", [](Sample &sample) { sample.sourceBpm += 1; });
    settled(app.manager);
    pump();
  };
  bool continued = false;
  dirty();
  app.unsavedPrompt = [](bool discard, auto done) {
    require(discard, "Close offers Discard");
    done(0);
  };
  app.requestClose([&] { continued = true; });
  until([&] { return !app.pendingGuard; });
  require(!continued && app.manager.snapshot().hasUnsavedChanges,
          "Cancel keeps pending edits and the current view");
  app.unsavedPrompt = [](bool, auto done) { done(1); };
  app.requestClose([&] { continued = true; });
  until([&] { return continued; });
  require(!app.manager.snapshot().hasUnsavedChanges,
          "Save continuation waits for the committed revision");
  continued = false;
  dirty();
  bool rechecked = false;
  app.unsavedPrompt = [&](bool, auto done) {
    rechecked = true;
    done(0);
  };
  app.transitionNeedsGuard = true;
  app.transitionDiscarding = true;
  app.pendingTransition = [&] { app.guardUnsaved([&] { continued = true; }); };
  until([&] { return rechecked && !app.pendingGuard && !app.pendingTransition; });
  require(!continued && app.manager.snapshot().hasUnsavedChanges,
          "Transitions waiting on import or other I/O recheck new edits without implicitly saving "
          "manual mode");
  app.unsavedPrompt = [](bool, auto done) { done(2); };
  app.requestClose([&] { continued = true; });
  until([&] { return continued; });
  require(!app.manager.snapshot().hasUnsavedChanges,
          "Discard continuation waits for discard and recovery completion");
  // A focused name field must be submitted before Save decides what is pending.
  app.name.setText("Saved focused name", false);
  app.keyPressed(juce::KeyPress('S', juce::ModifierKeys::commandModifier, 0));
  settled(app.manager);
  pump();
  require(app.manager.snapshot().completedProject.find(id)->name == "Saved focused name",
          "Cmd/Ctrl+S flushes the current text fields");
  auto screenshot = [&](const String &name) {
    const auto directory =
        juce::SystemStats::getEnvironmentVariable("CORE_EDITING_PREVIEW_DIR", "");
    if (!File::isAbsolutePath(directory))
      return;
    app.timerCallback();
    auto folder = File(directory);
    require(folder.createDirectory().wasOk(), "Create preview folder");
    auto output = folder.getChildFile(name + ".png").createOutputStream();
    juce::PNGImageFormat png;
    require(output &&
                png.writeImageToStream(app.createComponentSnapshot(app.getLocalBounds()), *output),
            "Render editing UI");
  };
  for (int presentation = 0; presentation < 3; ++presentation) {
    app.presentationBox.setSelectedId(presentation + 1, juce::sendNotificationSync);
    app.markerMode.setSelectedId(presentation == 1 ? 1 : 2, juce::sendNotificationSync);
    for (int width : {1120, 1320}) {
      app.setSize(width, 920);
      pump();
      require(!app.saveButton.getBounds().intersects(app.autoSaveToggle.getBounds()) &&
                  !app.autoSaveToggle.getBounds().intersects(app.projectButton.getBounds()),
              "Save controls do not overlap at supported widths");
      require(app.snapToggle.isVisible() == (presentation != 1),
              "Snapping follows presentation visibility");
      screenshot(String(presentation) + "-" + String(width));
    }
  }
  app.presentationBox.setSelectedId(1, juce::sendNotificationSync);
  auto damaged = *app.state.project.find(id);
  damaged.protectedEntry = true;
  damaged.problem = "Truncated metadata";
  *app.state.project.find(id) = damaged;
  app.updateEditor();
  require(app.removeButton.isEnabled() && !app.waveform.isEnabled() && !app.mergeButton.isEnabled(),
          "Damaged slot enables Remove and disables editing");
  app.damagedPrompt = [](auto done) { done(0); };
  app.requestRemove();
  require(app.manager.snapshot().project.find(id), "Cancelling damaged removal preserves the slot");
  screenshot("damaged");
}
void editingWorkflowTests() {
  Fixture fixture;
  Project unsafe;
  Sample unsafeSample;
  unsafeSample.protectedEntry = true;
  unsafeSample.recoveryHashes["bank2/0.0.wav"] = "missing";
  unsafe.samples.push_back(unsafeSample);
  bool rejected = false;
  try {
    unsafe.validate();
  } catch (...) {
    rejected = true;
  }
  require(rejected, "Recovery authorisation cannot include another slot's files");
  waveformEditorTests();
  editingWorkflowViewTests();
  manualAndDebounce(fixture);
  affectedFilesAndFailure(fixture);
  damagedVariants(fixture);
  malformedNames(fixture);
  backupFailure(fixture);
  damagedRemoval(fixture);
  slowSave(fixture);
  std::cout << "PASS saving modes, debounce, recovery, exact damaged undo, snapping and 30-second "
               "blocked save\n";
}
} // namespace core
