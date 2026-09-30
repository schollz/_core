#include "Manager.h"
#include "NameMetadata.h"
#include "SettingsView.h"
#include "Visualizer/Session.h"
#include <iostream>
namespace core {
namespace {
ManagerState settle(Manager &m, bool includeCompanions = true) {
  for (int n = 0; n < 2000; ++n) {
    auto s = m.snapshot();
    if (!s.busy && (!includeCompanions || !s.backgroundBusy))
      return s;
    juce::Thread::sleep(10);
  }
  throw std::runtime_error("Manager job did not settle");
}
void successful(const ManagerState &s) {
  require(s.error.isEmpty(), "Manager: " + s.error);
  require(s.companionError.isEmpty(), "Companion: " + s.companionError);
  require(!s.busy, "Manager still busy");
}
void historyAvailability(const File &workspace) {
  const auto root = workspace.getChildFile("history-availability");
  require(root.createDirectory().wasOk(), "History availability fixture folder");
  auto check = [](Manager &manager, bool undo, bool redo, const String &message) {
    const auto state = settle(manager);
    successful(state);
    require(state.canUndo == undo && state.canRedo == redo, message);
  };
  {
    Manager manager;
    require(!manager.snapshot().canUndo && !manager.snapshot().canRedo,
            "No project starts without undo or redo");
    manager.open(root);
    check(manager, false, false, "A fresh project has no history");
    manager.settings({{"sample_cv_mapping", "1voct"}});
    check(manager, true, false, "An edit enables undo only");
    manager.undo();
    check(manager, false, true, "Undoing the only edit enables redo only");
    manager.redo();
    check(manager, true, false, "Redo consumes the future history");
    manager.undo();
    check(manager, false, true, "The edit can be undone again");
    manager.settings({{"sample_cv_mapping", "1voct"}});
    check(manager, true, false, "A new edit after undo clears redo");
    manager.settings({{"sample_cv_mapping", "bank"}});
    check(manager, true, false, "A second edit extends undo history");
    manager.undo();
    check(manager, true, true, "Past and future history can both be available");
  }
  {
    Manager manager;
    manager.open(root);
    check(manager, true, true, "Reopening restores both history availability flags");
    manager.redo();
    check(manager, true, false, "Restored redo remains usable");
    manager.undo();
    check(manager, true, true, "Restored undo remains usable");
    manager.cleanup();
    check(manager, false, false, "Cleaning recovery history clears both flags");
  }
  {
    Manager manager;
    manager.open(root);
    check(manager, false, false, "Cleared history stays unavailable after reopening");
    manager.settings({{"sample_cv_mapping", "bank"}});
    check(manager, true, false, "Edits after cleanup start new history");
    const auto other = workspace.getChildFile("history-other-project");
    require(other.createDirectory().wasOk(), "Other history fixture folder");
    manager.open(other);
    check(manager, false, false, "Switching projects replaces history availability");
  }
}
void backgroundCompanions(const File &workspace) {
  auto root = workspace.getChildFile("background-companions");
  auto other = workspace.getChildFile("other-project");
  auto duplicate = workspace.getChildFile("background-copy");
  require(root.createDirectory().wasOk() && other.createDirectory().wasOk(),
          "Background companion fixture folders");
  auto input = workspace.getChildFile("background-bpm120.wav");
  {
    auto out = input.createOutputStream();
    card::writeHeader(*out, 44100 * 8, 44100, 2);
    for (int n = 0; n < 44100 * 8; ++n)
      for (int c = 0; c < 2; ++c)
        out->writeShort(short(std::sin(n * .1 + c) * 10000));
  }
  String id, secondId, primaryHash;
  double primaryMs = 0;
  {
    Manager manager;
    manager.open(root);
    successful(settle(manager));
    auto started = juce::Time::getMillisecondCounterHiRes();
    manager.import({input.getFullPathName(), input.getFullPathName()}, 0);
    auto ready = settle(manager, false);
    primaryMs = juce::Time::getMillisecondCounterHiRes() - started;
    successful(ready);
    require(ready.canUndo && !ready.canRedo, "History remains available during companion work");
    require(ready.project.samples.size() == 2 && ready.pendingCompanions == 2 &&
                ready.backgroundBusy && manager.idle() && ready.status.startsWith("Ready"),
            "Every imported primary is ready and the manager is usable before companions finish");
    id = ready.project.samples[0].id;
    secondId = ready.project.samples[1].id;
    AudioProcessing audio;
    for (const auto &sample : ready.project.samples) {
      require(ready.completedProject.find(sample.id) && sample.companionPending &&
                  sample.completedRevision == sample.revision && ready.editorWaveform(sample.id) &&
                  audio.reader(child(root, sample.rendered))->lengthInSamples == 44100 * 8 &&
                  child(root, card::path(sample.bank, sample.slot)).existsAsFile(),
              "Primary preview, editor waveform and saved output are available during companion work");
    }
    primaryHash = hashFile(child(root, "bank1/0.0.wav"));
    // Interrupt actual stretching, not just a queued companion job.
    auto partial = child(root, ".core-manager/cache/" + ready.project.find(id)->renderKey + "/long.wav");
    for (int n = 0; n < 2000 && !partial.existsAsFile(); ++n)
      juce::Thread::sleep(1);
    require(partial.existsAsFile() && manager.snapshot().backgroundBusy,
            "Companion rendering has started in the background");
    started = juce::Time::getMillisecondCounterHiRes();
    manager.even(id, 8);
    ready = settle(manager, false);
    auto editMs = juce::Time::getMillisecondCounterHiRes() - started;
    successful(ready);
    require(ready.project.find(id)->slices.size() == 8 && ready.pendingCompanions > 0 &&
                hashFile(child(root, "bank1/0.0.wav")) == primaryHash,
            "Even slices preempts companion work and saves without waiting for it");
    manager.move({secondId}, 1);
    ready = settle(manager, false);
    successful(ready);
    require(ready.project.find(secondId)->bank == 1 &&
                child(root, "bank2/0.0.wav").existsAsFile() &&
                !child(root, "bank1/1.1.wav").exists(),
            "Move completes during background work without a stale companion at the old slot");
    manager.remove({secondId});
    ready = settle(manager, false);
    successful(ready);
    require(!ready.project.find(secondId) && !child(root, "bank2/0.0.wav").exists() &&
                !child(root, "bank2/0.1.wav").exists(),
            "Removal cancels pending companions without resurrecting deleted files");
    manager.undo();
    successful(settle(manager, false));
    manager.edit(id, "Change audio during companion render", [](Sample &s) { s.channels = 1; });
    ready = settle(manager, false);
    successful(ready);
    require(ready.project.find(id)->channels == 1 && ready.project.find(id)->companionPending &&
                card::inspect(child(root, "bank1/0.0.wav")).channels == 1,
            "A new audio setting replaces primary output before its new companion is ready");
    primaryHash = hashFile(child(root, "bank1/0.0.wav"));
    manager.duplicate(duplicate);
    successful(settle(manager, false));
    auto copied = Project::fromJson(parseJson(child(duplicate, ".core-manager/project.json")));
    require(copied.id != ready.project.id && copied.find(id)->companionPending,
            "Duplicate is usable immediately and retains resumable companions");
    manager.open(other);
    ready = settle(manager, false);
    successful(ready);
    require(ready.root == other && ready.pendingCompanions == 0,
            "Opening another project cancels old companion work");
    auto saved = Project::fromJson(parseJson(child(root, ".core-manager/project.json")));
    require(saved.find(id)->companionPending && saved.revision == saved.completedRevision,
            "Companion work is persisted separately from completed user edits");
    std::cout << "PASS background primary ready in " << primaryMs << " ms; even-slice edit in "
              << editMs << " ms while companions remain pending\n";
  }
  {
    Manager reopened;
    reopened.open(root);
    auto ready = settle(reopened);
    successful(ready);
    require(ready.pendingCompanions == 0 && !ready.backgroundBusy &&
                !ready.project.find(id)->companionPending &&
                hashFile(child(root, "bank1/0.0.wav")) == primaryHash &&
                child(root, "bank1/0.1.wav").existsAsFile() &&
                card::inspect(child(root, "bank1/0.1.wav")).channels == 1 &&
                child(root, "bank2/0.1.wav").existsAsFile() &&
                !child(root, "bank1/1.1.wav").exists(),
            "Reopening resumes companions at current slots without rewriting primary audio");
    juce::MemoryBlock info;
    require(child(root, "bank1/0.1.wav.info").loadFileAsData(info) &&
                card::decode(info).slices.size() == 8,
            "Finished companion uses the latest slice edit");
  }
  {
    // A foreign companion destination exercises background failure and retry
    // while all primary output remains usable.
    auto foreign = child(duplicate, "bank1/0.1.wav");
    durableWrite(foreign, "foreign", 7);
    Manager manager;
    manager.open(duplicate);
    auto state = settle(manager);
    require(state.error.isEmpty() && state.companionError.contains("External change") &&
                !state.busy && !state.backgroundBusy && state.pendingCompanions > 0 &&
                state.editorWaveform(id) && foreign.loadFileAsString() == "foreign",
            "Companion failure preserves the usable editor and external files");
    manager.edit(id, "Rename during companion failure", [](Sample &s) { s.name = "Still usable"; });
    state = settle(manager, false);
    require(state.error.isEmpty() && state.project.find(id)->name == "Still usable" &&
                state.project.revision == state.project.completedRevision,
            "Companion failure does not prevent subsequent primary saves");
    require(foreign.deleteFile(), "Remove companion conflict fixture");
    manager.retry();
    successful(settle(manager));
    require(manager.snapshot().pendingCompanions == 0, "Retry finishes deferred companions");
  }
}
void sampleCVMappingChecks(const File &workspace) {
  auto root = workspace.getChildFile("sample-cv-mapping");
  require(root.createDirectory().wasOk(), "Sample CV fixture folder");
  const auto mapping = child(root, card::sampleCVMappingPath);
  {
    Manager manager;
    manager.open(root);
    successful(settle(manager));
    require(mapping.loadFileAsString() == "bank\n", "Default mapping is written exactly");
    manager.settings({{"sample_cv_mapping", "1voct"}});
    successful(settle(manager));
    require(mapping.loadFileAsString() == "1voct\n", "Native save matches the web export");
    manager.undo();
    successful(settle(manager));
    require(mapping.loadFileAsString() == "bank\n", "Undo restores the firmware mapping");
    manager.redo();
    successful(settle(manager));
    require(mapping.loadFileAsString() == "1voct\n", "Redo restores the firmware mapping");

    Look look;
    for (int presentation = 0; presentation < 3; ++presentation) {
      manager.settings({{"override_with_reset", "sample"}});
      successful(settle(manager));
      look.presentation(presentation);
      SettingsView view(manager, look, presentation);
      juce::ComboBox *mappingControl = nullptr, *resetControl = nullptr;
      for (auto *child : view.getChildren())
        if (auto *combo = dynamic_cast<juce::ComboBox *>(child)) {
          if (combo->getItemText(0) == "Bank divisions")
            mappingControl = combo;
          if (combo->getItemText(0) == "none")
            resetControl = combo;
        }
      require((mappingControl != nullptr) == (presentation != 1),
              "Sample CV mapping is hidden only in Zeptocore");
      if (mappingControl) {
        require(resetControl && !mappingControl->isEnabled() &&
                    mappingControl->getText() == "1 V/oct" &&
                    mappingControl->getTooltip().contains("firmware"),
                "Reset disables mapping while preserving its display and help");
        resetControl->setSelectedItemIndex(0, juce::sendNotificationSync);
        successful(settle(manager));
        require(mappingControl->isEnabled() && mappingControl->getText() == "1 V/oct",
                "Removing Reset restores the saved mapping selection");
        mappingControl->setSelectedItemIndex(0, juce::sendNotificationSync);
        successful(settle(manager));
        require(mapping.loadFileAsString() == "bank\n", "Dropdown saves the bank wire value");
        mappingControl->setSelectedItemIndex(1, juce::sendNotificationSync);
        successful(settle(manager));
      }
      require(mapping.loadFileAsString() == "1voct\n",
              "Presentation and Reset changes retain the saved mapping");
    }
  }
  {
    Manager reopened;
    reopened.open(root);
    auto state = settle(reopened);
    successful(state);
    require(state.project.settings.at("sample_cv_mapping") == "1voct" &&
                state.project.warnings.isEmpty(),
            "Saved mapping reopens without external-change warnings");
    // A card edited by another app must be reconciled before overwriting it.
    durableWrite(mapping, "bank\n", 5);
    reopened.settings({{"sample_cv_mapping", "bank"}});
    require(settle(reopened).error.contains("External change") &&
                mapping.loadFileAsString() == "bank\n",
            "External mapping changes are protected by the normal transaction checks");
  }
  auto legacyRoot = workspace.getChildFile("legacy-sample-cv-mapping");
  require(legacyRoot.createDirectory().wasOk(), "Legacy mapping fixture folder");
  {
    Storage storage(legacyRoot);
    auto legacy = storage.open();
    legacy.settings.erase("sample_cv_mapping");
    legacy.fingerprints.erase(card::sampleCVMappingPath);
    require(child(legacyRoot, card::sampleCVMappingPath).deleteFile(), "Legacy mapping is absent");
    durableJson(child(legacyRoot, ".core-manager/project.json"), legacy.json());
  }
  {
    Manager legacy;
    legacy.open(legacyRoot);
    successful(settle(legacy));
    legacy.settings({{"sample_cv_mapping", "1voct"}});
    successful(settle(legacy));
    legacy.undo();
    successful(settle(legacy));
    require(child(legacyRoot, card::sampleCVMappingPath).loadFileAsString() == "bank\n",
            "Undo in an older project restores the default without a schema migration");
  }
}
void savedName(const File &root, int bank, int slot, const String &name,
               const String &original) {
  auto metadata = names::read(root, bank, slot,
                              hashFile(child(root, card::path(bank, slot))));
  require(metadata.status == names::Metadata::valid && metadata.name == name &&
              metadata.originalFilename == original,
          "Sidecar names follow slot and complete primary audio SHA-256");
}
void pendingImportWaveform(const File &workspace, const File &input) {
  auto root = workspace.getChildFile("pending-waveform-" + uuid());
  require(root.createDirectory().wasOk(), "Pending waveform fixture");
  String id;
  {
    Storage storage(root);
    auto pending = storage.open();
    AudioProcessing audio;
    auto sample = audio.import(storage, input, 0, 0);
    id = sample.id;
    pending.samples.push_back(sample);
    ++pending.revision;
    storage.savePending(pending);
  }
  // A foreign destination blocks the hardware save, leaving a deterministic
  // pending import. Its editor must still have a waveform after reopening.
  auto foreign = child(root, "bank1/0.0.wav");
  durableWrite(foreign, "foreign", 7);
  Manager manager;
  manager.open(root);
  auto pending = settle(manager);
  const auto *sample = pending.project.find(id);
  auto source = pending.editorWaveform(id);
  require(pending.error.contains("External change") && sample && source && !source->peaks.empty() &&
              !pending.completedProject.find(id),
          "Reopened pending import has a source waveform even when saving is blocked");
  require(foreign.loadFileAsString() == "foreign", "Preview preserves foreign card output");
  require(foreign.deleteFile(), "Remove fixture conflict");
  manager.retry();
  auto ready = settle(manager);
  successful(ready);
  require(ready.completedProject.find(id) && ready.editorWaveform(id) &&
              ready.editorWaveform(id) != source && !ready.editorWaveform(id)->spectrum.empty(),
          "Editor switches to completed peaks after a successful hardware save");
  const auto waveHash = hashFile(foreign);
  auto slices = ready.project.find(id)->slices;
  auto lanes = ready.project.find(id)->transients;
  slices[0].stop = slices[1].start = .08;
  manager.editMarkers(id, slices, lanes);
  auto moved = settle(manager);
  successful(moved);
  require(moved.project.find(id)->spliceVariable,
          "Moving a slice boundary enables variable timing in the same edit");
  lanes[0] = {.02};
  manager.editMarkers(id, slices, lanes);
  manager.edit(id, "Unrelated playback change", [](Sample &s) { s.playMode = 1; });
  moved = settle(manager);
  successful(moved);
  juce::MemoryBlock savedInfo;
  require(child(root, "bank1/0.0.wav.info").loadFileAsData(savedInfo) &&
              card::decode(savedInfo).spliceVariable && hashFile(foreign) == waveHash,
          "Transient and playback edits preserve variable timing without rewriting audio");
  const int previousInterval = moved.project.find(id)->spliceTrigger;
  manager.even(id, 1);
  auto even = settle(manager);
  successful(even);
  require(!even.project.find(id)->spliceVariable &&
              even.project.find(id)->spliceTrigger == 48 && previousInterval != 48,
          "Even slices recalculates the interval and disables variable timing");
  for (int variant : {0, 1}) {
    savedInfo.reset();
    require(child(root, card::path(0, 0, variant) + ".info").loadFileAsData(savedInfo) &&
                card::decode(savedInfo).spliceTrigger == 48 &&
                !card::decode(savedInfo).spliceVariable,
            "Even slices saves the calculated interval to both firmware companions");
  }
  require(hashFile(foreign) == waveHash, "Recalculating splice timing preserves WAV bytes");
  manager.undo();
  auto undone = settle(manager);
  successful(undone);
  require(undone.project.find(id)->spliceVariable &&
              undone.project.find(id)->spliceTrigger == previousInterval,
          "Undo restores manual slices, variable timing and the previous interval together");
  manager.redo();
  auto redone = settle(manager);
  successful(redone);
  require(!redone.project.find(id)->spliceVariable &&
              redone.project.find(id)->spliceTrigger == 48,
          "Redo restores the calculated even interval");
  manager.undo();
  successful(settle(manager));
  manager.edit(id, "Disable variable timing manually", [](Sample &s) { s.spliceVariable = false; });
  lanes[0] = {.03};
  manager.editMarkers(id, slices, lanes);
  auto manual = settle(manager);
  successful(manual);
  require(!manual.project.find(id)->spliceVariable,
          "Editing only transient markers respects a manually disabled timing setting");
  manager.remove({id});
  auto removed = settle(manager);
  successful(removed);
  require(!removed.editorWaveform(id) && removed.sourceWaveforms.empty(),
          "Removing an import releases its source preview");
  manager.undo();
  auto restored = settle(manager);
  successful(restored);
  require(restored.editorWaveform(id) != nullptr, "Undo restores the imported sample waveform");
  const auto previousImport = restored.importGeneration;
  manager.import({input.getFullPathName(), input.getFullPathName()}, 2);
  auto batch = settle(manager);
  successful(batch);
  const auto *firstImported = batch.project.find(batch.importedSampleId);
  require(batch.importGeneration == previousImport + 1 && firstImported &&
              firstImported->bank == 2 && firstImported->slot == 0 &&
              batch.editorWaveform(firstImported->id) != nullptr &&
              batch.sourceWaveforms.size() == 1,
          "Batch import selects its first sample and shares source peaks for duplicate audio");
  manager.import({input.getFullPathName(), root.getChildFile("missing.wav").getFullPathName()}, 3);
  auto failed = settle(manager);
  require(failed.error.isNotEmpty() && failed.importGeneration == batch.importGeneration &&
              failed.importedSampleId == batch.importedSampleId,
          "A failed import batch does not issue a new sample selection");
}
void transientRecoveryCase(const File &workspace) {
  auto root = workspace.getChildFile("transient-recovery-" + uuid());
  auto wave = child(root, "bank1/0.0.wav");
  auto info = child(root, "bank1/0.0.wav.info");
  require(wave.getParentDirectory().createDirectory().wasOk() &&
              child(workspace, "bank1/0.0.wav").copyFileTo(wave),
          "Copy card for zero-marker recovery");
  juce::MemoryBlock bytes;
  require(child(workspace, "bank1/0.0.wav.info").loadFileAsData(bytes),
          "Read completed card metadata");
  auto metadata = card::decode(bytes);
  for (auto &lane : metadata.transients)
    lane = {0, 16, 32};
  bytes = card::encode(metadata);
  durableWrite(info, bytes.getData(), bytes.getSize());
  auto waveHash = hashFile(wave);
  String id, oldWarning;
  {
    Storage storage(root);
    auto project = storage.adopt();
    auto &sample = project.samples.at(0);
    id = sample.id;
    require(!sample.protectedEntry && sample.transients[1].front() == 0 &&
                project.warnings.isEmpty(),
            "Adoption preserves loop-start markers without range warnings");
    oldWarning =
        sample.name + ": Transient lane 2 has a position outside the device's encoding range.";
    project.warnings.add(oldWarning);
    project.warnings.add("Unrelated recovery warning");
    durableJson(child(root, ".core-manager/project.json"), project.json());

    // Simulate the failed, pending slice edit in an older project.
    ++project.revision;
    ++sample.revision;
    sample.slices = {{0, .5, 0}, {.5, 1, 0}};
    sample.spliceVariable = true;
    storage.savePending(project);

    auto invalid = project;
    invalid.samples[0].sourceDuration = 30;
    invalid.samples[0].transients[1] = {1048576. / 44100.};
    require(Project::fromJson(invalid.json()).warnings.contains(oldWarning),
            "Real overflow warnings survive project reload");
  }
  Manager manager;
  manager.open(root);
  auto state = settle(manager);
  successful(state);
  const auto *sample = state.project.find(id);
  require(sample && sample->transients[1].front() == 0 &&
              state.project.completedRevision == state.project.revision &&
              !state.project.warnings.contains(oldWarning) &&
              state.project.warnings.contains("Unrelated recovery warning"),
          "Pending slice save resumes and clears only obsolete range warnings");
  bytes.reset();
  require(info.loadFileAsData(bytes), "Read saved slices");
  auto saved = card::decode(bytes);
  require(saved.spliceVariable && saved.slices.size() == 2 &&
              saved.transients == metadata.transients && hashFile(wave) == waveHash,
          "Slice save retains every counted transient and unchanged WAV bytes");
}
void nameRecoveryCases(const File &workspace) {
  for (const auto &contents : juce::StringArray{"", "{broken", "{\"schema\":2}"}) {
    auto root = workspace.getChildFile("name-recovery-" + uuid());
    require(root.createDirectory().wasOk(), "Name recovery fixture");
    auto wave = child(root, "bank1/0.0.wav");
    auto info = child(root, "bank1/0.0.wav.info");
    require(wave.getParentDirectory().createDirectory().wasOk() &&
                child(workspace, "bank1/0.0.wav").copyFileTo(wave) &&
                child(workspace, "bank1/0.0.wav.info").copyFileTo(info),
            "Copy completed card audio");
    auto sidecar = child(root, "bank1/0.name.json");
    if (contents.isNotEmpty())
      durableWrite(sidecar, contents.toRawUTF8(),
                   size_t(contents.getNumBytesAsUTF8()));
    String id;
    {
      Storage storage(root);
      auto legacy = storage.adopt();
      legacy.samples[0].name = "Known renamed sample";
      id = legacy.samples[0].id;
      auto json = legacy.json();
      json["samples"]
          .getArray()
          ->getReference(0)
          .getDynamicObject()
          ->removeProperty("originalFilename");
      durableJson(child(root, ".core-manager/project.json"), json);
    }
    auto waveHash = hashFile(wave), infoHash = hashFile(info);
    auto waveTime = wave.getLastModificationTime(),
         infoTime = info.getLastModificationTime();
    Manager manager;
    manager.open(root);
    auto state = settle(manager);
    successful(state);
    require(
        state.project.find(id)->name == "Known renamed sample" &&
            state.project.find(id)->originalFilename.isEmpty(),
        "Old project remains authoritative without guessing original filename");
    if (contents.isEmpty()) {
      savedName(root, 0, 0, "Known renamed sample", "");
      require(state.project.completedRevision > 0,
              "Opening old project completes transactional name backfill");
    } else {
      manager.edit(id, "Rename with foreign sidecar",
                   [](Sample &s) { s.name = "New name"; });
      state = settle(manager);
      successful(state);
      require(sidecar.loadFileAsString() == contents &&
                  state.project.warnings.joinIntoString(" ").contains(
                      "Filename persistence skipped"),
              "Saving warns and preserves malformed or unsupported sidecars");
    }
    require(hashFile(wave) == waveHash && hashFile(info) == infoHash,
            "Name persistence preserves hardware bytes");
    if (contents.isEmpty()) {
      require(wave.getLastModificationTime() == waveTime &&
                  info.getLastModificationTime() == infoTime,
              "Backfill does not rewrite audio or binary metadata");
      require(wave.appendText("replacement"), "Replace primary audio bytes");
      durableJson(sidecar, names::encode("Replacement name", "replacement.wav",
                                         hashFile(wave)));
      manager.reconcile();
      state = settle(manager);
      successful(state);
      require(state.project.find(id)->name == "Replacement name" &&
                  state.project.find(id)->originalFilename == "replacement.wav",
              "Replaced audio recovers matching sidecar instead of old project "
              "names");
    } else {
      manager.remove({id});
      successful(settle(manager));
      require(sidecar.loadFileAsString() == contents,
              "Deletion preserves unowned sidecar");
    }
  }
}
} // namespace
void managerTests() {
  auto workspace = File::getSpecialLocation(File::tempDirectory)
                  .getChildFile("core-manager-job-" + uuid());
  require(workspace.createDirectory().wasOk(), "Manager test folder");
  struct Clean {
    File root;
    ~Clean() { root.deleteRecursively(); }
  } clean{workspace};
  historyAvailability(workspace);
  backgroundCompanions(workspace);
  sampleCVMappingChecks(workspace);
  // Keep independent project fixtures outside the project being duplicated.
  auto root = workspace.getChildFile("project");
  require(root.createDirectory().wasOk(), "Main manager fixture folder");
  auto audioFile = root.getChildFile("input.wav");
  {
    auto out = audioFile.createOutputStream();
    card::writeHeader(*out, 4410, 44100, 1);
    for (int n = 0; n < 4410; ++n)
      out->writeShort(short(std::sin(n * .1) * 10000));
  }
  {
    Manager m;
    m.open(root);
    auto opened = settle(m);
    successful(opened);
    require(opened.project.samples.empty() && opened.project.settings == card::defaultSettings(),
            "Opening a new folder initializes defaults before importing audio");
    for (const auto &[path, enabled] : card::settingsFiles(opened.project.settings))
      require(child(root, path).existsAsFile() == enabled,
              "New folder has exactly one marker for every default setting: " + path);
    m.import({audioFile.getFullPathName()}, 0);
    auto state = settle(m);
    successful(state);
    require(state.project.samples.size() == 1, "Background import");
    auto id = state.project.samples.front().id;
    require(state.importGeneration > 0 && state.importedSampleId == id,
            "Single import requests selection of the imported sample");
    require(state.project.find(id)->slices.size() == 16 && !state.project.find(id)->spliceVariable,
            "Imported sample is ready with sixteen even slices");
    auto early = state;
    early.completedProject.samples.clear();
    require(early.editorWaveform(id) ==
                early.sourceWaveforms.at(early.project.find(id)->sourceHash),
            "A new import displays source peaks before it has a completed slot");
    require(state.editorWaveform(id) == state.library.samples.front().wave,
            "Completed audio takes precedence over the early import waveform");
    early.project.samples.front().bank = 3;
    early.project.samples.front().slot = 8;
    require(early.editorWaveform(id) ==
                early.sourceWaveforms.at(early.project.find(id)->sourceHash),
            "Source waveform identity follows an imported sample across slot moves");
    Device device;
    Preview preview;
    zv::Session visual(m, device, preview);
    visual.selectedId = id;
    visual.previewSource = true;
    visual.enable(true);
    visual.tick(1000);
    require(visual.wave && visual.device.display &&
                visual.device.display->state.stopped &&
                !visual.device.display->state.effects && !visual.device.press &&
                !visual.position(1000),
            "Preview source uses committed sample without fabricated hardware "
            "activity");
    visual.enable(false);
    visual.tick(1100);
    require(!visual.wave && !visual.device.display,
            "Disabled visualizer does no session work");
    visual.enable(true);
    visual.tick(1200);
    require(visual.wave != nullptr,
            "Visualizer re-enable restores completed data");
    visual.previewSource = false;
    visual.enable(true);
    visual.tick(1300);
    require(visual.wave && !visual.device.display,
            "Device source clears synthetic preview state");
    visual.enable(false);

    require(root.getChildFile("bank1/0.0.wav").existsAsFile() &&
                root.getChildFile("bank1/0.1.wav").existsAsFile(),
            "Continuous primary and companion output");
    auto before = hashFile(root.getChildFile("bank1/0.0.wav"));
    auto modified =
        root.getChildFile("bank1/0.0.wav").getLastModificationTime();
    const auto displayName = String::fromUTF8("Amen — edited 鼓");
    std::map<String, std::pair<String, juce::Time>> unchangedOutputs;
    for (auto path : {"bank1/0.0.wav", "bank1/0.1.wav", "bank1/0.0.wav.info",
                      "bank1/0.1.wav.info"}) {
      auto file = child(root, path);
      unchangedOutputs[path] = {hashFile(file), file.getLastModificationTime()};
    }
    m.edit(id, "Rename", [displayName](Sample &s) { s.name = displayName; });
    state = settle(m);
    successful(state);
    require(state.project.find(id)->originalFilename == "input.wav",
            "Rename preserves imported basename");
    savedName(root, 0, 0, displayName, "input.wav");
    for (const auto &[path, expected] : unchangedOutputs) {
      auto file = child(root, path);
      require(hashFile(file) == expected.first &&
                  file.getLastModificationTime() == expected.second,
              "Filename-only edit preserves primary/companion WAV and .info "
              "bytes and times");
    }
    {
      auto copied = workspace.getChildFile("bank-only-copy");
      require(root.getChildFile("bank1").copyDirectoryTo(
                  copied.getChildFile("bank1")),
              "Copy bank without native project");
      Storage portable(copied);
      auto recovered = portable.open();
      require(recovered.samples[0].name == displayName &&
                  recovered.samples[0].originalFilename == "input.wav",
              "Bank-only copy recovers renamed display and imported filename");
    }
    auto nameFile = child(root, "bank1/0.name.json");
    const auto nameTime = juce::Time::getCurrentTime() - juce::RelativeTime::days(1);
    require(nameFile.setLastModificationTime(nameTime), "Set sidecar fixture time");
    const auto storedNameTime = nameFile.getLastModificationTime();
    m.edit(id, "Test metadata", [](Sample &s) {
      s.sourceBpm = 172;
      s.slices = {{0, .5, 0}, {.5, 1, 0}};
      s.playMode = 2;
      s.spliceTrigger = 48;
    });
    state = settle(m);
    successful(state);
    require(hashFile(root.getChildFile("bank1/0.0.wav")) == before &&
                root.getChildFile("bank1/0.0.wav").getLastModificationTime() == modified,
            "Metadata edit does not rewrite audio");
    require(nameFile.getLastModificationTime() == storedNameTime,
            "Slice and playback edits reuse an unchanged filename sidecar");
    visual.previewSource = true;
    visual.enable(true);
    visual.tick(1400);
    require(visual.wave && visual.wave->bpm == 172 && visual.wave->slices.size() == 2,
            "Completed metadata revision refreshes the shared visualizer waveform");
    visual.enable(false);
    auto unchangedRevision = state.project.revision;
    m.edit(id, "Unchanged metadata", [](Sample &s) { s.sourceBpm = 172; });
    state = settle(m);
    successful(state);
    require(state.project.revision == unchangedRevision,
            "Unchanged field submission creates no revision or undo step");
    m.edit(id, "One shot exception", [](Sample &s) {
      s.oneShot = true;
      s.tempoMatch = false;
    });
    successful(settle(m));
    require(!root.getChildFile("bank1/0.1.wav").exists() &&
                root.getChildFile("bank1/0.0.wav").getLastModificationTime() ==
                    modified,
            "Companion exception preserves primary WAV");
    m.undo();
    successful(settle(m));
    require(root.getChildFile("bank1/0.1.wav").existsAsFile(),
            "Restore companion from cache");
    m.move({id}, 1);
    state = settle(m);
    successful(state);
    require(!root.getChildFile("bank1/0.0.wav").existsAsFile() &&
                hashFile(root.getChildFile("bank2/0.0.wav")) == before,
            "Slot move preserves rendered bytes");
    require(!child(root, "bank1/0.name.json").exists(),
            "Move removes old sidecar");
    savedName(root, 1, 0, displayName, "input.wav");
    m.undo();
    state = settle(m);
    successful(state);
    require(root.getChildFile("bank1/0.0.wav").existsAsFile() &&
                !root.getChildFile("bank2/0.0.wav").existsAsFile(),
            "Undo restores slot assignment");
    savedName(root, 0, 0, displayName, "input.wav");
    m.redo();
    successful(settle(m));
    savedName(root, 1, 0, displayName, "input.wav");
    m.undo();
    successful(settle(m));
    m.remove({id});
    successful(settle(m));
    require(!root.getChildFile("bank1/0.0.wav").existsAsFile(),
            "Remove hardware output");
    require(!child(root, "bank1/0.name.json").exists(),
            "Removal deletes owned sidecar");
    m.undo();
    state = settle(m);
    successful(state);
    require(hashFile(root.getChildFile("bank1/0.0.wav")) == before,
            "Undo removal from immutable source");
    savedName(root, 0, 0, displayName, "input.wav");
    m.settings({{"clock_stop_sync", "on"}});
    successful(settle(m));
    m.settings({{"clock_stop_sync", "off"}});
    successful(settle(m));
    require(
        !root.getChildFile("settings/clock_stop_sync-on").existsAsFile() &&
            root.getChildFile("settings/clock_stop_sync-off").existsAsFile(),
        "Exclusive settings markers committed together");
    m.edit(id, "Superseded render", [](Sample &s) { s.renderBpm = 60; });
    m.edit(id, "Final render", [](Sample &s) { s.renderBpm = 150; });
    state = settle(m);
    successful(state);
    require(state.project.find(id)->renderBpm == 150 &&
                state.project.revision == state.project.completedRevision,
            "Latest revision reaches Ready");
    savedName(root, 0, 0, displayName, "input.wav");
    m.import({audioFile.getFullPathName()}, 0);
    auto withTwo = settle(m);
    successful(withTwo);
    auto secondId = withTwo.project.samples.back().id;
    savedName(root, 0, 1, "input.wav", "input.wav");
    m.move({id}, 0, 1);
    successful(settle(m));
    savedName(root, 0, 1, displayName, "input.wav");
    savedName(root, 0, 0, "input.wav", "input.wav");
    m.undo();
    withTwo = settle(m);
    successful(withTwo);
    savedName(root, 0, 0, displayName, "input.wav");
    savedName(root, 0, 1, "input.wav", "input.wav");
    m.merge({id, secondId}, 1);
    auto merged = settle(m);
    successful(merged);
    require(merged.project.samples.back().originalFilename.isEmpty(),
            "New native merge has no invented original filename");
    savedName(root, 1, 0, "Merged samples", "");
    m.undo();
    withTwo = settle(m);
    successful(withTwo);
    {
      auto destination = root.getSiblingFile(root.getFileName() + "-duplicate");
      Clean duplicateClean{destination};
      m.duplicate(destination);
      successful(settle(m));
      Storage duplicate(destination);
      auto copied = duplicate.open();
      require(copied.find(id)->name == displayName &&
                  copied.find(secondId)->originalFilename == "input.wav",
              "Duplicate retains independent names for duplicate basenames");
      savedName(destination, 0, 0, displayName, "input.wav");
    }
    auto beforeEdgeMove = withTwo.project.revision;
    m.move({id, secondId}, 0, -1);
    auto edge = settle(m);
    successful(edge);
    require(
        edge.project.find(id)->slot == 0 &&
            edge.project.find(secondId)->slot == 1 &&
            edge.project.revision == beforeEdgeMove,
        "Moving a selected group past bank start preserves order and revision");
    m.remove({secondId});
    successful(settle(m));
    // A sidecar edit cannot rename an intact native sample with unchanged
    // audio.
    durableJson(child(root, "bank1/0.name.json"),
                names::encode("External name", "external.wav",
                              hashFile(child(root, "bank1/0.0.wav"))));
    m.reconcile();
    state = settle(m);
    successful(state);
    require(state.project.find(id)->name == displayName &&
                state.project.find(id)->originalFilename == "input.wav",
            "Reconcile preserves native names when primary audio is unchanged");
    savedName(root, 0, 0, displayName, "input.wav");
    auto external = root.getChildFile("bank1/0.0.wav");
    external.appendText("external change");
    auto altered = hashFile(external);
    m.edit(id, "External conflict", [](Sample &s) { s.spliceTrigger = 72; });
    state = settle(m);
    require(state.error.contains("External change") &&
                hashFile(external) == altered,
            "Do not overwrite externally modified audio");
    m.reconcile();
    state = settle(m);
    successful(state);
    require(hashFile(external) == altered && state.project.find(id),
            "Reconcile preserves external bytes and stable sample ID");
    require(state.project.find(id)->name == "Recovered card audio 1" &&
                state.project.find(id)->originalFilename.isEmpty(),
            "Replaced audio with stale sidecar never inherits previous sample "
            "name");
    m.undo();
    state = settle(m);
    successful(state);
    require(state.project.find(id)->spliceTrigger == 72,
            "Undo reload retains pending edits");
    auto revision = state.project.find(id)->revision;
    m.edit(id, "Invalidate late analysis",
           [](Sample &s) { s.spliceTrigger = 24; });
    m.applyAnalysis(state.project.id, id, revision, {{{.01}, {.02}, {.03}}});
    state = settle(m);
    require(state.error.contains("after the sample changed") &&
                state.project.find(id)->transients[0].empty(),
            "Late online response cannot replace markers");
    m.retry();
    successful(settle(m));
    // Destructor drains queued edits into the portable pending manifest.
    m.edit(id, "Close with pending edit", [](Sample &s) { s.sourceBpm = 166; });
  }
  {
    Manager reopened;
    reopened.open(root);
    auto state = settle(reopened);
    successful(state);
    require(state.project.samples.front().sourceBpm == 166 &&
                state.project.revision == state.project.completedRevision,
            "Reopen resumes a pending shutdown edit");
    auto id = state.project.samples.front().id;
    auto completed = state.project.completedRevision;
    reopened.edit(id, "Disconnect during debounce",
                  [](Sample &s) { s.spliceTrigger = 96; });
    for (int n = 0;
         n < 1000 && reopened.snapshot().project.revision == completed; ++n)
      juce::Thread::sleep(1);
    auto parked = root.getSiblingFile(root.getFileName() + "-unplugged");
    require(root.moveFileTo(parked), "Simulated volume disconnect");
    auto disconnected = settle(reopened);
    require(disconnected.error.isNotEmpty() &&
                !disconnected.library.samples.empty(),
            "Missing folder retains completed visualization");
    require(!root.exists(), "Disconnected paths must not be recreated");
    require(parked.moveFileTo(root), "Simulated reconnect");
    reopened.retry();
    successful(settle(reopened));
  }
  pendingImportWaveform(root, audioFile);
  transientRecoveryCase(root);
  nameRecoveryCases(root);
  std::cout << "PASS background import, metadata-only save, move, undo, "
               "remove, settings, stale jobs, conflicts\n";
}
} // namespace core
