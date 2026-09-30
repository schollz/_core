#include "Manager.h"
#include "NameMetadata.h"
#include "Parallel.h"
namespace core {
namespace {
String infoSettings(const Sample &s) {
  auto sample = s.json();
  var fields = object();
  for (auto key : {"sourceBpm", "renderBpm", "rate", "channels", "playMode",
                   "spliceTrigger", "oneShot", "tempoMatch", "spliceVariable",
                   "slices", "transients"})
    put(fields, key, sample[key]);
  return juce::JSON::toString(fields);
}
} // namespace
std::shared_ptr<const zv::Wave> ManagerState::editorWaveform(const String &sampleId) const {
  const auto *sample = project.find(sampleId);
  if (!sample)
    return {};
  if (const auto *completed = completedProject.find(sampleId))
    for (const auto &entry : library.samples)
      if (entry.bank == completed->bank && entry.sample == completed->slot && entry.wave)
        return entry.wave;
  auto source = sourceWaveforms.find(sample->sourceHash);
  return source != sourceWaveforms.end() ? source->second : nullptr;
}
Manager::Manager() : worker([this] { run(); }) {}
Manager::~Manager() {
  diagnostics::Scope trace("MANAGER", "Stop worker");
  stopping = true;
  ++serial;
  wake.notify_all();
  if (worker.joinable())
    worker.join();
}
ManagerState Manager::snapshot() const {
  std::lock_guard<std::mutex> lock(mutex);
  return state;
}
bool Manager::idle() const {
  auto s = snapshot();
  return !s.busy;
}
int Manager::pendingCompanions() const {
  return int(std::count_if(project.samples.begin(), project.samples.end(), [](const Sample &s) {
    return !s.protectedEntry && s.companionPending && !(s.oneShot && !s.tempoMatch);
  }));
}
void Manager::post(Command c) {
  {
    std::lock_guard<std::mutex> lock(mutex);
    commands.push_back(std::move(c));
    ++serial;
    state.busy = true;
    diagnostics::log("MANAGER", "Queued command generation=" +
        String(juce::int64(serial.load())) + " queue_size=" + String(int(commands.size())));
  }
  wake.notify_one();
}
void Manager::publish(String status, String error) {
  const int pending = pendingCompanions();
  if (pending == 0)
    companionError.clear();
  if (status == "Ready" && pending > 0)
    status += companionError.isEmpty()
                  ? " (background companions: " + String(pending) + " remaining)"
                  : " (companions paused; use Retry pending save)";
  diagnostics::log("MANAGER", "Status=" + status + " error=" + error +
      " samples=" + String(int(project.samples.size())) +
      " revision=" + String(juce::int64(project.revision)) +
      " completed_revision=" + String(juce::int64(project.completedRevision)));
  {
    std::lock_guard<std::mutex> lock(mutex);
    state.project = project;
    state.completedProject = committed;
    state.library = libraryState;
    state.sourceWaveforms = sourceWaveforms;
    state.importedSampleId = importedSampleId;
    state.importGeneration = importGeneration;
    state.root = storage ? storage->root : File();
    state.status = std::move(status);
    state.error = std::move(error);
    state.companionError = companionError;
    state.pendingCompanions = pending;
    state.backgroundBusy = pending > 0 && companionError.isEmpty() && !failed;
    state.busy = commandRunning || !commands.empty() || (dirty && !failed);
    state.available = storage && storage->root.isDirectory();
    state.canUndo = history.canUndo();
    state.canRedo = history.canRedo();
    ++state.generation;
  }
  sendChangeMessage();
}
void Manager::run() {
  diagnostics::log("MANAGER", "Worker started");
  try {
    auto cache = stateRoot().getChildFile("visualizer/last-project.json");
    if (cache.existsAsFile()) {
      auto saved = parseJson(cache);
      libraryState.root = saved["root"].toString();
      if (auto *a = saved["samples"].getArray())
        for (const auto &v : *a) {
          auto wave = zv::waveFromJson(v);
          libraryState.samples.push_back(
              {wave->bank, wave->sample, {}, {}, wave});
        }
      publish("Open a folder to edit; last completed visualization is cached");
    }
  } catch (...) {
  }

  while (true) {
    Command command;
    uint64_t generation = 0;
    bool backgroundJob = false;
    {
      std::unique_lock<std::mutex> lock(mutex);
      wake.wait(lock, [&] {
        return stopping || !commands.empty() || (dirty && !failed) ||
               (!dirty && !failed && pendingCompanions() > 0 && companionError.isEmpty());
      });
      if (stopping && commands.empty())
        break;
      if (commands.empty() && dirty) {
        wake.wait_for(lock, std::chrono::milliseconds(250),
                      [&] { return stopping || !commands.empty(); });
        if (stopping && commands.empty())
          break;
      }
      if (!commands.empty()) {
        command = std::move(commands.front());
        commands.pop_front();
        commandRunning = true;
      }
      generation = serial.load();
    }
    try {
      if (command)
        command();
      else if (dirty && !failed)
        save(generation);
      else if (!dirty && !failed && pendingCompanions() > 0 && companionError.isEmpty()) {
        // New commands cancel this job at the next audio/file block. Primary
        // saves and user actions always run before optional companion work.
        backgroundJob = true;
        saveCompanion(generation);
      }
    } catch (const std::exception &e) {
      diagnostics::log("MANAGER", "Worker exception: " + String(e.what()));
      if (String(e.what()) != "Cancelled") {
        if (backgroundJob) {
          companionError = e.what();
          publish("Ready");
        } else {
          failed = true;
          publish("Action required", e.what());
        }
      }
    }
    {
      std::lock_guard<std::mutex> lock(mutex);
      commandRunning = false;
      state.busy = !commands.empty() || (dirty && !failed);
    }
    sendChangeMessage();
  }
  if (dirty && storage)
    try {
      persist();
    } catch (...) {
    } // local recovery is written before volume persistence
}
void Manager::open(const File &root) {
  post([this, root] {
    diagnostics::Scope trace("MANAGER", "Open folder " + root.getFullPathName());
    if (storage && dirty)
      throw std::runtime_error(
          "Finish or reconcile pending changes before opening another folder");
    int lastProgress = -1;
    auto report = [&](int percent) {
      percent = juce::jlimit(0, 100, percent);
      if (percent > lastProgress) {
        lastProgress = percent;
        publish("Opening folder..." + String(percent) + "%");
      }
    };
    report(0);
    auto candidate = std::make_unique<Storage>(root);
    auto loaded = candidate->open([&](double fraction) {
      report(int(fraction * 70.));
    });
    storage = std::move(candidate);
    project = std::move(loaded);
    companionError.clear();
    sourceWaveforms.clear();
    importedSampleId.clear();
    committed =
        Project::fromJson(parseJson(child(root, ".core-manager/project.json")));
    manifestHash = fingerprint(child(root, ".core-manager/project.json"));
    history.clear();
    auto undoFile = child(root, ".core-manager/history.json");
    if (undoFile.existsAsFile())
      history.restore(parseJson(undoFile), project.id);
    dirty = project.revision > project.completedRevision;
    failed = false;
    prepareImportWaveforms();
    if (dirty)
      persist();
    report(75);
    prepareVisualization([&](const zv::LibraryState &progress) {
      report(75 + 24 * progress.completed / std::max(1, progress.total));
    });
    report(100);
    publish(dirty ? "Processing" : "Ready");
  });
}
void Manager::persist() {
  diagnostics::Scope trace("MANAGER", "Persist pending changes");
  require(storage != nullptr, "Open a project folder first");
  // A second recovery copy survives an unplugged/remounted project volume.
  auto local = stateRoot().getChildFile("recovery/" + project.id + ".json");
  durableJson(local, project.json());
  storage->savePending(project);
  durableJson(child(storage->root, ".core-manager/history.json"), history.json(project.id));
}
void Manager::change(const String &label, const std::function<void(Project &)> &operation,
                     const std::function<void()> &onAccepted) {
  diagnostics::Scope trace("MANAGER", "Change: " + label);
  require(storage && storage->root.isDirectory(),
          "Project volume is unavailable; reconnect it before editing");
  auto before = project;
  auto candidate = project;
  operation(candidate);
  if (juce::JSON::toString(candidate.json()) ==
      juce::JSON::toString(project.json()))
    return;
  candidate.revision = project.revision + 1;
  candidate.completedRevision = committed.completedRevision;
  for (auto &s : candidate.samples) {
    const auto *before = project.find(s.id);
    if (!before ||
        juce::JSON::toString(s.json()) != juce::JSON::toString(before->json()))
      s.revision = candidate.revision;
  }
  candidate.validate();
  history.accept(before, label);
  project = std::move(candidate);
  dirty = true;
  failed = false;
  if (onAccepted)
    onAccepted();
  prepareImportWaveforms();
  publish("Saving changes");
  persist();
  publish("Processing");
}
void Manager::prepareImportWaveforms() {
  std::set<String> activeSources;
  std::vector<const Sample *> pending;
  for (const auto &sample : project.samples) {
    if (sample.protectedEntry)
      continue;
    activeSources.insert(sample.sourceHash);
    if (committed.find(sample.id) || sourceWaveforms.count(sample.sourceHash))
      continue;
    pending.push_back(&sample);
  }
  for (size_t index = 0; index < pending.size(); ++index) {
    const auto &sample = *pending[index];
    if (sourceWaveforms.count(sample.sourceHash))
      continue;
    publish("Preparing waveform " + String(int(index) + 1) + " of " + String(int(pending.size())) +
            ": " + sample.name);
    try {
      sourceWaveforms[sample.sourceHash] = audio.sourceWaveform(child(storage->root, sample.source),
                                                                [this] { return stopping.load(); });
      diagnostics::log("WAVEFORM", "Imported waveform ready sample=" + sample.id +
                                       " name=" + sample.name + " before hardware save");
    } catch (const std::exception &e) {
      if (stopping)
        throw;
      // A preview failure must not discard an otherwise usable import.
      diagnostics::log("WAVEFORM", "Source preview unavailable sample=" + sample.id +
                                       " error=" + String(e.what()));
    }
  }
  for (auto it = sourceWaveforms.begin(); it != sourceWaveforms.end();)
    if (!activeSources.count(it->first))
      it = sourceWaveforms.erase(it);
    else
      ++it;
}
void Manager::import(const juce::StringArray &paths, int bank) {
  post([this, paths, bank] {
    require(storage != nullptr, "Open a folder first");
    String firstImported;
    change(
        "Import samples",
        [&](Project &p) {
          int imported = 0;
          for (const auto &path : paths) {
            int slot = p.freeSlot(bank);
            require(slot >= 0, "Bank is full (16 slots). Choose another bank; no "
                               "sample was overwritten.");
            publish("Importing " + String(++imported) + " of " + String(paths.size()) + ": " +
                    File(path).getFileName());
            auto sample = audio.import(*storage, File(path), bank, slot);
            if (firstImported.isEmpty())
              firstImported = sample.id;
            p.samples.push_back(std::move(sample));
          }
        },
        [&] {
          importedSampleId = firstImported;
          ++importGeneration;
        });
  });
}
void Manager::edit(const String &id, const String &label, std::function<void(Sample &)> fn) {
  post([this, id, label, fn = std::move(fn)] {
    change(label, [&](Project &p) {
      auto *s = p.find(id);
      require(s && !s->protectedEntry, "Sample is missing or protected");
      fn(*s);
    });
  });
}
void Manager::editMarkers(const String &id, std::vector<Marker> slices,
                          std::array<std::vector<double>, 3> transients) {
  edit(id, "Edit markers",
       [slices = std::move(slices), transients = std::move(transients)](Sample &s) {
         const bool slicesChanged =
             s.slices.size() != slices.size() ||
             !std::equal(s.slices.begin(), s.slices.end(), slices.begin(),
                         [](const Marker &a, const Marker &b) {
                           return a.start == b.start && a.stop == b.stop && a.type == b.type;
                         });
         if (slicesChanged)
           s.spliceVariable = true;
         s.slices = slices;
         s.transients = transients;
       });
}
void Manager::remove(const std::vector<String> &ids) {
  post([this, ids] {
    change("Remove samples", [&](Project &p) {
      for (const auto &id : ids) {
        auto *s = p.find(id);
        require(!s || !s->protectedEntry,
                "Cannot delete a protected entry. Repair it outside the app, "
                "then reconcile.");
      }
      p.samples.erase(std::remove_if(p.samples.begin(), p.samples.end(),
                                     [&](const Sample &s) {
                                       return std::find(ids.begin(), ids.end(),
                                                        s.id) != ids.end();
                                     }),
                      p.samples.end());
    });
  });
}
void Manager::clearBank(int bank) {
  post([this, bank] {
    change("Clear bank", [&](Project &p) {
      p.samples.erase(std::remove_if(p.samples.begin(), p.samples.end(),
                                     [&](const Sample &s) {
                                       return s.bank == bank &&
                                              !s.protectedEntry;
                                     }),
                      p.samples.end());
    });
  });
}
void Manager::move(const std::vector<String> &ids, int bank, int delta) {
  post([this, ids, bank, delta] {
    change("Reorder samples", [&](Project &p) {
      if (delta != 0) {
        std::vector<Sample *> list;
        for (auto &s : p.samples)
          if (s.bank == bank)
            list.push_back(&s);
        std::sort(list.begin(), list.end(),
                  [](auto *a, auto *b) { return a->slot < b->slot; });
        if (delta > 0)
          std::reverse(list.begin(), list.end());
        for (auto *s : list)
          if (std::find(ids.begin(), ids.end(), s->id) != ids.end()) {
            require(!s->protectedEntry, "Cannot move protected entries");
            int destination = s->slot + (delta > 0 ? 1 : -1);
            if (destination < 0 || destination >= 16)
              continue;
            Sample *other = nullptr;
            for (auto &candidate : p.samples)
              if (candidate.bank == bank && candidate.slot == destination)
                other = &candidate;
            if (other) {
              if (std::find(ids.begin(), ids.end(), other->id) != ids.end())
                continue;
              require(!other->protectedEntry, "Destination is protected");
              std::swap(s->slot, other->slot);
            } else
              s->slot = destination;
          }
      } else
        for (const auto &id : ids) {
          auto *s = p.find(id);
          require(s && !s->protectedEntry, "Cannot move protected entries");
          if (s->bank == bank)
            continue;
          int slot = p.freeSlot(bank);
          require(slot >= 0, "Destination bank is full");
          s->bank = bank;
          s->slot = slot;
        }
    });
  });
}
void Manager::merge(const std::vector<String> &ids, int bank) {
  post([this, ids, bank] {
    change("Merge samples", [&](Project &p) {
      int slot = p.freeSlot(bank);
      require(slot >= 0, "Bank is full; choose another bank before merging");
      std::vector<Sample> selected;
      for (const auto &s : p.samples)
        if (std::find(ids.begin(), ids.end(), s.id) != ids.end())
          selected.push_back(s);
      std::sort(selected.begin(), selected.end(), [](auto &a, auto &b) {
        return a.bank * 16 + a.slot < b.bank * 16 + b.slot;
      });
      Sample s;
      s.bank = bank;
      s.slot = slot;
      s.name = "Merged samples";
      auto file = audio.merge(storage->root, selected, s);
      s.source = storage->importSource(file, s.sourceHash);
      p.samples.push_back(std::move(s));
    });
  });
}
void Manager::even(const String &id, int count) {
  diagnostics::log("SLICES", "Even sample=" + id + " target=" + String(count));
  edit(id, "Even slices", [count](Sample &s) {
    require(count > 0 && count <= 1024,
            juce::String::fromUTF8("Choose 1–1024 slices"));
    s.slices.clear();
    for (int n = 0; n < count; ++n)
      s.slices.push_back({double(n) / count, double(n + 1) / count, 0});
    s.spliceVariable = false;
    s.updateSpliceTrigger(SpliceTimingCalculation::evenSlices);
  });
}
void Manager::detect(const String &id, int count, String method, double spacing) {
  post([this, id, count, method, spacing] {
    diagnostics::Scope trace("SLICES", "Auto sample=" + id + " target=" +
        String(count) + " method=" + method + " spacing_ms=" + String(spacing));
    require(count > 0 && count <= 1024,
            juce::String::fromUTF8("Choose 1–1024 slices"));
    require(storage != nullptr, "Open a folder");
    auto *s = project.find(id);
    require(s && !s->protectedEntry, "Cannot analyse this entry");
    publish("Processing local onsets");
    auto generation = serial.load();
    auto markers =
        audio.detect(child(storage->root, s->source), method, spacing,
                     [&] { return stopping || serial.load() != generation; },
                     count);
    change("Automatic slicing", [&](Project &p) {
      auto *target = p.find(id);
      target->slices = markers;
      target->spliceVariable = true;
    });
  });
}
void Manager::settings(const card::Settings &settings) {
  post([this, settings] {
    change("Device settings", [&](Project &p) {
      for (const auto &[k, v] : settings)
        p.settings[k] = v;
    });
  });
}
void Manager::undo() {
  post([this] {
    auto revision = project.revision;
    auto fingerprints = committed.fingerprints;
    if (history.undo(project)) {
      project.revision = revision + 1;
      for (auto &s : project.samples)
        if (!s.protectedEntry)
          s.revision = project.revision;
      project.completedRevision = committed.completedRevision;
      project.fingerprints = fingerprints;
      dirty = true;
      failed = false;
      prepareImportWaveforms();
      persist();
      publish("Processing undo");
    }
  });
}
void Manager::redo() {
  post([this] {
    auto revision = project.revision;
    if (history.redo(project)) {
      project.revision = revision + 1;
      for (auto &s : project.samples)
        if (!s.protectedEntry)
          s.revision = project.revision;
      project.completedRevision = committed.completedRevision;
      project.fingerprints = committed.fingerprints;
      dirty = true;
      failed = false;
      prepareImportWaveforms();
      persist();
      publish("Processing redo");
    }
  });
}
void Manager::retry() {
  post([this] {
    failed = false;
    companionError.clear();
    dirty = storage && project.revision > project.completedRevision;
    publish(dirty ? "Processing" : "Ready");
  });
}
void Manager::duplicate(const File &f) {
  post([this, f] {
    require(storage && !dirty, "Finish saving before duplicating");
    publish("Duplicating project");
    storage->duplicateTo(f);
    publish("Ready");
  });
}
void Manager::cleanup() {
  post([this] {
    require(storage && !dirty, "Finish saving before cleanup");
    history.clear();
    durableJson(child(storage->root, ".core-manager/history.json"),
                history.json(project.id));
    storage->cleanup(project, true);
    publish("Ready");
  });
}
void Manager::applyAnalysis(const String &projectId, const String &sampleId,
                            int64_t revision,
                            std::array<std::vector<double>, 3> lanes) {
  post([this, projectId, sampleId, revision, lanes = std::move(lanes)] {
    auto *s = project.find(sampleId);
    require(project.id == projectId && s && s->revision == revision,
            "Analysis finished after the sample changed. Existing markers were "
            "retained; run analysis again if needed.");
    change("Online drum analysis", [&](Project &p) {
      auto *target = p.find(sampleId);
      target->transients = lanes;
      ++target->revision;
    });
  });
}
void Manager::reconcile() {
  post([this] {
    require(storage && storage->root.isDirectory(),
            "Reconnect the project folder first");
    auto root = storage->root;
    auto manifest = child(root, ".core-manager/project.json");
    auto archive = child(root, ".core-manager/recovery/reconcile-" + uuid());
    durableJson(archive.getChildFile("pending-project.json"), project.json());
    durableJson(archive.getChildFile("completed-project.json"),
                committed.json());
    auto adopted = storage->adopt(false);
    adopted.id = project.id;
    adopted.revision = project.revision + 1;
    adopted.completedRevision = adopted.revision;
    for (auto &sample : adopted.samples) {
      const Sample *previous = nullptr;
      for (const auto &old : committed.samples)
        if (old.bank == sample.bank && old.slot == sample.slot) {
          previous = &old;
          break;
        }
      if (previous) {
        auto primary = card::path(sample.bank, sample.slot);
        auto oldPrimary = committed.fingerprints.find(primary);
        auto newPrimary = adopted.fingerprints.find(primary);
        bool sameAudio = oldPrimary != committed.fingerprints.end() &&
                         newPrimary != adopted.fingerprints.end() &&
                         oldPrimary->second == newPrimary->second;
        bool unchanged = sample.ownedPaths == previous->ownedPaths;
        for (const auto &path : sample.ownedPaths) {
          auto old = committed.fingerprints.find(path);
          auto now = adopted.fingerprints.find(path);
          if (old == committed.fingerprints.end() ||
              now == adopted.fingerprints.end() || old->second != now->second)
            unchanged = false;
        }
        if (unchanged)
          sample = *previous;
        else {
          sample.id = previous->id;
          if (sameAudio) {
            sample.name = previous->name;
            sample.originalFilename = previous->originalFilename;
          }
        }
      }
      sample.revision = sample.completedRevision = adopted.revision;
    }
    adopted.validate();
    Transaction tx(root);
    tx.commit({}, adopted);
    history.accept(project, "Before external reload");
    project = adopted;
    committed = adopted;
    companionError.clear();
    manifestHash = fingerprint(manifest);
    storage->queueNameBackfill(project);
    dirty = project.revision > project.completedRevision;
    failed = false;
    persist();
    prepareVisualization();
    publish(dirty ? "Processing" : "Ready");
  });
}
void Manager::prepareVisualization(
    std::function<void(const zv::LibraryState &)> progress) {
  diagnostics::Scope trace("VISUALIZER", "Prepare project waveforms");
  require(storage != nullptr, "Open a folder");
  String currentFile;
  auto report = [&](const zv::LibraryState &update) {
    if (update.current != currentFile) {
      currentFile = update.current;
      diagnostics::log("VISUALIZER", "Analysing " + currentFile +
          " completed=" + String(update.completed) + " total=" + String(update.total));
    }
    if (progress)
      progress(update);
  };
  libraryState =
      zv::Library::prepare(storage->root, zv::Library::defaultCache(),
                           [this] { return stopping.load(); }, report);
  diagnostics::log("VISUALIZER", "Total=" + String(libraryState.total) +
      " prepared=" + String(libraryState.prepared) + " reused=" +
      String(libraryState.reused) + " error=" + libraryState.error +
      " warning=" + libraryState.warning);
  for (const auto &entry : libraryState.samples)
    diagnostics::log("VISUALIZER", entry.path + " ready=" +
        String(entry.wave ? 1 : 0) + " error=" + entry.error);
  var saved = object();
  put(saved, "root", storage->root.getFullPathName());
  put(saved, "projectId", committed.id);
  put(saved, "revision", juce::int64(committed.completedRevision));
  juce::Array<var> samples;
  for (const auto &s : libraryState.samples)
    if (s.wave)
      samples.add(zv::waveToJson(*s.wave));
  put(saved, "samples", samples);
  auto cache = stateRoot().getChildFile("visualizer/last-project.json");
  durableJson(cache, saved);
}
void Manager::save(uint64_t generation) {
  diagnostics::Scope trace("MANAGER", "Save generation=" + String(juce::int64(generation)));
  require(storage && storage->root.isDirectory(),
          "Project volume is unavailable. Reconnect it and Retry; pending "
          "edits are retained.");
  publish("Checking card files");
  auto root = storage->root;
  require(fingerprint(child(root, ".core-manager/project.json")) == manifestHash,
          "Project manifest is missing or changed externally. Reconnect or "
          "reconcile before saving.");
  auto cancel = [&] { return stopping || serial.load() != generation; };
  cancelled(cancel);
  // Check every owned file once. Later planning reuses these hashes for
  // unchanged files; the transaction rechecks every actual replacement.
  const auto verified = committed.fingerprints;
  std::vector<std::pair<String, String>> checks(verified.begin(), verified.end());
  {
    diagnostics::Scope traceChecks("STORAGE", "Verify files before save");
    parallelFor(checks.size(), [&](size_t index) {
      const auto &[path, expected] = checks[index];
      require(fingerprint(child(root, path), cancel) == expected,
              "External change: " + path + ". Reload/reconcile before saving.");
    });
  }
  cancelled(cancel);
  auto checkedHash = [&](const File &file) {
    const auto relative = file.getRelativePathFrom(root).replaceCharacter('\\', '/');
    auto known = verified.find(relative);
    return known != verified.end() && known->second != "missing" ? known->second
                                                                 : hashFile(file, cancel);
  };
  auto completed = project;
  auto needsRender = [&](const Sample &sample) {
    if (sample.protectedEntry)
      return false;
    const auto *previous = committed.find(sample.id);
    return !previous || audioKey(sample) != audioKey(*previous) ||
           (sample.oneShot && !sample.tempoMatch) != (previous->oneShot && !previous->tempoMatch);
  };
  const int renderCount =
      int(std::count_if(completed.samples.begin(), completed.samples.end(), needsRender));
  int renderedCount = 0;
  std::map<String, File> desired;
  std::map<String, String> nameExpected;
  std::set<String> oldOwned;
  for (const auto &s : committed.samples)
    if (!s.protectedEntry)
      for (const auto &path : s.ownedPaths)
        oldOwned.insert(path);
  for (auto &s : completed.samples) {
    cancelled(cancel);
    if (s.protectedEntry)
      continue;
    auto *previous = committed.find(s.id);
    auto encodeInfo = [&](const card::Info &info) {
      try {
        return card::encode(info);
      } catch (const std::exception &e) {
        throw std::runtime_error(("Bank " + String(s.bank + 1) + " slot " + String(s.slot + 1) +
                                  " (" + s.name + "): " + String(e.what()))
                                     .toStdString());
      }
    };
    bool changed = needsRender(s);
    bool metadataChanged = !previous || infoSettings(s) != infoSettings(*previous);
    if (changed) {
      publish("Processing audio " + String(++renderedCount) + " of " + String(renderCount) + ": " +
              s.name);
      auto rendered = audio.render(root, s, cancel, false);
      s.rendered = rendered.preview;
      s.companion.clear();
      s.companionPending = !(s.oneShot && !s.tempoMatch);
      s.renderKey = rendered.key;
      desired[card::path(s.bank, s.slot)] = child(root, rendered.padded);
      auto bytes = encodeInfo(sampleInfo(s, rendered.frames));
      auto path = child(root, ".core-manager/cache/metadata-" + s.id + "-0.info");
      durableWrite(path, bytes.getData(), bytes.getSize());
      desired[card::path(s.bank, s.slot) + ".info"] = path;
    } else {
      s.rendered = previous->rendered;
      s.companion = previous->companion;
      s.companionPending = previous->companionPending;
      s.renderKey = previous->renderKey;
      for (const auto &path : previous->ownedPaths) {
        if (path.endsWith(".name.json"))
          continue; // Regenerate from this sample and its completed primary WAV.
        auto filename = path.fromLastOccurrenceOf(
            "/", false,
            false); // only use the basename; the project path remains relative
        auto suffix = filename.fromFirstOccurrenceOf(".", true, false);
        auto target =
            "bank" + String(s.bank + 1) + "/" + String(s.slot) + suffix;
        desired[target] = child(root, path);
      }
      if (metadataChanged)
        for (int variant = 0; variant < 2; ++variant) {
          auto wavePath = card::path(s.bank, s.slot, variant);
          auto found = desired.find(wavePath);
          if (found == desired.end())
            continue;
          auto w = card::inspect(found->second);
          auto info = sampleInfo(s, w.frames - w.rate, variant != 0);
          auto bytes = encodeInfo(info);
          auto path = child(root, ".core-manager/cache/metadata-" + s.id + "-" +
                                      String(variant) + ".info");
          durableWrite(path, bytes.getData(), bytes.getSize());
          desired[wavePath + ".info"] = path;
        }
    }
    auto namePath = names::path(s.bank, s.slot);
    auto primaryPath = card::path(s.bank, s.slot);
    auto primaryHash = committed.fingerprints.find(primaryPath);
    auto existingName =
        names::read(root, s.bank, s.slot,
                    primaryHash == committed.fingerprints.end() ? String() : primaryHash->second);
    if (existingName.status == names::Metadata::invalid) {
      completed.warnings.addIfNotAlreadyThere(existingName.warning);
      oldOwned.erase(namePath);
    } else {
      auto audioHash = checkedHash(desired.at(primaryPath));
      if (existingName.status == names::Metadata::valid && existingName.name == s.name &&
          existingName.originalFilename == s.originalFilename &&
          primaryHash != committed.fingerprints.end() && primaryHash->second == audioHash) {
        desired[namePath] = child(root, namePath);
      } else {
        auto staged = child(root, ".core-manager/cache/name-" + s.id + ".json");
        durableJson(staged, names::encode(s.name, s.originalFilename, audioHash));
        desired[namePath] = staged;
      }
      nameExpected[namePath] = existingName.status == names::Metadata::valid
                                   ? existingName.fingerprint
                                   : String("missing");
    }
    s.ownedPaths.clear();
    auto prefix = "bank" + String(s.bank + 1) + "/" + String(s.slot) + ".";
    for (const auto &[path, file] : desired)
      if (path.startsWith(prefix))
        s.ownedPaths.push_back(path);
    s.completedRevision = s.revision;
  }
  for (const auto &[path, enabled] : card::settingsFiles(completed.settings)) {
    if (enabled) {
      auto marker = child(root, ".core-manager/cache/empty-marker");
      if (!marker.existsAsFile())
        durableWrite(marker, nullptr, 0);
      desired[path] = marker;
    } else
      oldOwned.insert(path);
  }
  auto mapping = child(root, ".core-manager/cache/sample-cv-mapping");
  const auto mappingContent = card::sampleCVMappingContents(completed.settings);
  durableWrite(mapping, mappingContent.toRawUTF8(), size_t(mappingContent.getNumBytesAsUTF8()));
  desired[card::sampleCVMappingPath] = mapping;
  std::vector<Replacement> replacements;
  auto expected = [&](const String &path) {
    auto found = committed.fingerprints.find(path);
    if (found != committed.fingerprints.end())
      return found->second;
    auto name = nameExpected.find(path);
    return name == nameExpected.end() ? String("missing") : name->second;
  };
  completed.fingerprints = committed.fingerprints;
  for (const auto &[path, file] : desired) {
    cancelled(cancel);
    auto before = expected(path);
    auto known = verified.find(path);
    require((known == verified.end() ? fingerprint(child(root, path), cancel) : known->second) ==
                before,
            "External change: " + path + ". Reload/reconcile before saving.");
    auto after = checkedHash(file);
    if (after != before)
      replacements.push_back({path, file, before, after});
    completed.fingerprints[path] = after;
    oldOwned.erase(path);
  }
  for (const auto &path : oldOwned) {
    auto before = expected(path);
    cancelled(cancel);
    if (before != "missing")
      replacements.push_back({path, {}, before});
    completed.fingerprints[path] = "missing";
  }
  completed.completedRevision = completed.revision;
  cancelled(cancel);
  require(fingerprint(child(root, ".core-manager/project.json")) == manifestHash,
          "Project manifest changed externally. Reconcile before saving.");
  publish("Saving");
  Transaction tx(root);
  tx.shouldCancel = cancel;
  tx.commit(replacements, completed);
  publish("Updating waveforms");
  committed = completed;
  project = completed;
  manifestHash = fingerprint(child(root, ".core-manager/project.json"));
  dirty = false;
  failed = false;
  prepareVisualization();
  publish("Ready");
}
void Manager::saveCompanion(uint64_t generation) {
  auto found = std::find_if(project.samples.begin(), project.samples.end(), [](const Sample &s) {
    return !s.protectedEntry && s.companionPending && !(s.oneShot && !s.tempoMatch);
  });
  if (found == project.samples.end())
    return;
  const auto sample = *found;
  diagnostics::Scope trace("COMPANION", "Background sample=" + sample.id + " name=" + sample.name);
  auto cancel = [&] { return stopping || serial.load() != generation; };
  cancelled(cancel);
  require(storage && storage->root.isDirectory(), "Reconnect the project folder and retry companions");
  auto root = storage->root;
  auto manifest = child(root, ".core-manager/project.json");
  require(fingerprint(manifest, cancel) == manifestHash,
          "Project manifest changed externally. Reconcile before saving companions.");
  std::vector<std::pair<String, String>> checks(committed.fingerprints.begin(),
                                              committed.fingerprints.end());
  parallelFor(checks.size(), [&](size_t index) {
    const auto &[path, expected] = checks[index];
    require(fingerprint(child(root, path), cancel) == expected,
            "External change: " + path + ". Reconcile before saving companions.");
  });
  auto rendered = audio.render(root, sample, cancel);
  require(rendered.companionPadded.isNotEmpty(), "Companion output is missing");
  auto completed = project;
  auto *target = completed.find(sample.id);
  target->companion = rendered.companionPreview;
  target->companionPending = false;
  auto wavePath = card::path(sample.bank, sample.slot, 1);
  auto infoPath = wavePath + ".info";
  auto metadata = child(root, ".core-manager/cache/metadata-" + sample.id + "-1.info");
  auto bytes = card::encode(sampleInfo(sample, rendered.companionFrames, true));
  durableWrite(metadata, bytes.getData(), bytes.getSize());
  std::vector<Replacement> replacements;
  for (const auto &[path, staged] :
       std::vector<std::pair<String, File>>{{wavePath, child(root, rendered.companionPadded)},
                                            {infoPath, metadata}}) {
    auto known = committed.fingerprints.find(path);
    auto expected = known == committed.fingerprints.end() ? String("missing") : known->second;
    auto hash = hashFile(staged, cancel);
    replacements.push_back({path, staged, expected, hash});
    completed.fingerprints[path] = hash;
    target->ownedPaths.push_back(path);
  }
  cancelled(cancel);
  require(fingerprint(manifest, cancel) == manifestHash,
          "Project manifest changed externally. Reconcile before saving companions.");
  Transaction tx(root);
  tx.shouldCancel = cancel;
  tx.commit(replacements, completed);
  committed = completed;
  project = completed;
  manifestHash = fingerprint(manifest);
  // Primary audio and the visualizer are unchanged. Publishing the completed
  // companion updates progress without interrupting playback or rebuilding peaks.
  publish("Ready");
}
} // namespace core
