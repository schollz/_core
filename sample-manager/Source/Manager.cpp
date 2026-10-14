#include "Manager.h"
#include "NameMetadata.h"
#include "Parallel.h"
namespace core {
namespace {
String infoSettings(const Sample &s) {
  auto sample = s.json();
  var fields = object();
  for (auto key : {"sourceBpm", "renderBpm", "rate", "channels", "playMode", "spliceTrigger",
                   "oneShot", "tempoMatch", "spliceVariable", "slices", "transients"})
    put(fields, key, sample[key]);
  return juce::JSON::toString(fields);
}
String editSettings(const Sample &sample) {
  auto value = sample.json();
  for (const auto *key :
       {"revision", "completedRevision", "rendered", "renderKey", "ownedPaths", "nameUnsafe"})
    value.getDynamicObject()->removeProperty(key);
  return juce::JSON::toString(value);
}
bool restoreRecovery(const Sample &sample, const Sample *previous) {
  return !sample.recoveryFiles.empty() &&
         (!previous || sample.recoveryFiles != previous->recoveryFiles ||
          sample.bank != previous->bank || sample.slot != previous->slot);
}
} // namespace
// A disk job owns its immutable input and private result. Only the model worker
// adopts results; progress notifications never replace the live project.
struct DiskWork {
  std::shared_ptr<Storage> storage;
  Project project, committed;
  History history, committedHistory;
  String manifestHash, importedSampleId;
  uint64_t importGeneration = 0;
  AudioProcessing audio;
  std::vector<Sample> added;
  zv::LibraryState libraryState;
  std::map<String, std::shared_ptr<const zv::Wave>> sourceWaveforms;
  std::map<String, CompletedAudio> completedAudio;
  std::function<bool()> stop;
  std::function<void(String, String)> report;
  void publish(String status, String error = {}) { report(std::move(status), std::move(error)); }
  void open(const File &);
  void save();
  void reconcile();
  void prepareImportWaveforms();
  void prepareCompletedAudio();
  void prepareVisualization(std::function<void(const zv::LibraryState &)> = {});
};
String SampleRenderStatus::text() const {
  const auto separator = String::fromUTF8(" · ");
  String result = "Original: " + String(original, 2) + " s" + separator;
  if (pending && !failed)
    return result + "Target: " + String(target, 2) +
           String::fromUTF8(rendering ? " s (rendering…)" : " s (pending save)");
  if (completed.frames > 0) {
    result +=
        String(failed ? "Last rendered: " : "Rendered: ") + String(completed.duration(), 2) + " s";
    if (failed)
      result += separator + "Update failed";
    return result + separator + String(completed.rate / 1000., 1) + " kHz output";
  }
  if (failed && completed.path.isNotEmpty())
    return result + "Last rendered: unavailable" + separator + "Update failed";
  return result + (failed
                       ? "Target: " + String(target, 2) + " s (update failed; no completed render)"
                       : "Rendered: unavailable");
}
String SampleRenderStatus::tooltip() const {
  return text() + "\nOriginal is the imported audio duration. Target uses Source BPM / Render BPM. "
                  "Rendered is the actual completed, unpadded audio heard in preview. "
                  "The saved hardware WAV has an additional half-second of circular padding "
                  "at each end (one second total).";
}
String SampleRenderStatus::previewTooltip() const {
  if (completed.path.isEmpty())
    return failed ? "The render update failed. No completed render is available for preview."
                  : "Preview will be available when the first render completes.";
  String result = failed    ? "The render update failed. "
                  : pending ? (rendering ? "Rendering update. " : "Unsaved audio settings. ")
                            : "";
  result += "Preview uses the last completed render";
  if (completed.frames > 0)
    result += " (" + String(completed.duration(), 2) + " s)";
  return result + ". Play the selected sample, or stop the current preview. Space.";
}
SampleRenderStatus ManagerState::renderStatus(const String &sampleId) const {
  SampleRenderStatus result;
  if (const auto *sample = project.find(sampleId)) {
    result.original = sample->sourceDuration;
    result.target = sample->sourceDuration * sample->ratio();
    const auto *previous = completedProject.find(sampleId);
    result.pending =
        !previous || previous->rendered.isEmpty() || audioKey(*sample) != audioKey(*previous);
    result.failed = result.pending && primaryUpdateFailed;
    result.rendering = isSaving || autoSave;
    if (previous) {
      result.completed.path = previous->rendered;
      const auto cached = completedAudio.find(sampleId);
      if (cached != completedAudio.end() && cached->second.path == previous->rendered)
        result.completed = cached->second;
    }
  }
  return result;
}
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
Manager::Manager(std::chrono::milliseconds delay) : saveDelay(delay) {
  ioWorker = std::thread([this] { runIo(); });
  recoveryWorker = std::thread([this] { runRecovery(); });
  worker = std::thread([this] { run(); });
}
Manager::~Manager() {
  stopping = true;
  wake.notify_all();
  if (worker.joinable())
    worker.join();
  {
    std::lock_guard<std::mutex> lock(ioMutex);
    ioStopping = true;
  }
  ioWake.notify_all();
  if (ioWorker.joinable())
    ioWorker.join();
  {
    std::lock_guard<std::mutex> lock(recoveryMutex);
    recoveryStopping = true;
  }
  recoveryWake.notify_all();
  if (recoveryWorker.joinable())
    recoveryWorker.join();
}
ManagerState Manager::snapshot() const {
  std::lock_guard<std::mutex> lock(mutex);
  return state;
}
bool Manager::idle() const { return !snapshot().busy; }
void Manager::post(Command command) {
  {
    std::lock_guard<std::mutex> lock(mutex);
    commands.push_back(std::move(command));
    state.busy = true;
  }
  wake.notify_one();
}
void Manager::publish(String status, String error) {
  diagnostics::log("MANAGER",
                   "Status=" + status + " error=" + error +
                       " revision=" + String(juce::int64(project.revision)) +
                       " completed_revision=" + String(juce::int64(committed.completedRevision)));
  std::lock_guard<std::mutex> lock(mutex);
  state.project = project;
  state.completedProject = committed;
  state.completedAudio = completedAudio;
  state.primaryUpdateFailed = failed && dirty;
  state.library = libraryState;
  state.sourceWaveforms = sourceWaveforms;
  state.importedSampleId = importedSampleId;
  state.importGeneration = importGeneration;
  state.root = storage ? storage->root : File();
  state.status = std::move(status);
  state.error = std::move(error);
  state.hasUnsavedChanges = dirty;
  state.isSaving = saving;
  state.autoSave = autoSave;
  state.exclusiveOperation = exclusive;
  state.recoveryPending = recoveryDone.load() < recoverySequence;
  state.markerSequences = markerSequences;
  state.busy = foregroundRunning || ioActive || !deferredIo.empty() || !commands.empty() ||
               (dirty && !failed && (autoSave || saveRequested));
  state.available = storage != nullptr;
  state.canUndo = history.canUndo();
  state.canRedo = history.canRedo();
  ++state.generation;
  sendChangeMessage();
}
void Manager::run() {
  startIo(
      [](DiskWork &job) {
        try {
          const auto cache = stateRoot().getChildFile("visualizer/last-project.json");
          if (!cache.existsAsFile())
            return;
          const auto saved = parseJson(cache);
          job.libraryState.root = saved["root"].toString();
          if (auto *samples = saved["samples"].getArray())
            for (const auto &entry : *samples) {
              auto wave = zv::waveFromJson(entry);
              job.libraryState.samples.push_back({wave->bank, wave->sample, {}, {}, wave});
            }
        } catch (...) {
        }
      },
      [this](DiskWork &job) {
        libraryState = std::move(job.libraryState);
        publish("Open a folder to begin");
      });
  while (true) {
    Command command;
    {
      std::unique_lock<std::mutex> lock(mutex);
      wake.wait_for(lock, std::chrono::milliseconds(25),
                    [&] { return stopping || !commands.empty(); });
      if (stopping && commands.empty())
        break;
      if (!commands.empty()) {
        command = std::move(commands.front());
        commands.pop_front();
      }
      foregroundRunning = bool(command);
    }
    try {
      if (command)
        command();
      if (!ioActive && !deferredIo.empty() && !stopping) {
        auto job = std::move(deferredIo.front());
        deferredIo.pop_front();
        job();
      }
      if (!stopping && !ioActive && dirty && !failed &&
          (saveRequested || (autoSave && std::chrono::steady_clock::now() - lastEdit >= saveDelay)))
        startSave();
    } catch (const std::exception &e) {
      failed = true;
      publish(dirty ? "Save failed" : "Action required", e.what());
    }
    bool notify = false;
    {
      std::lock_guard<std::mutex> lock(mutex);
      const auto wasBusy = state.busy;
      const auto wasRecovery = state.recoveryPending;
      foregroundRunning = false;
      state.busy = ioActive || !deferredIo.empty() || !commands.empty() ||
                   (dirty && !failed && (autoSave || saveRequested));
      state.recoveryPending = recoveryDone.load() < recoverySequence;
      notify = wasBusy != state.busy || wasRecovery != state.recoveryPending;
    }
    if (notify)
      sendChangeMessage();
  }
  if (storage)
    persist();
}
void Manager::runIo() {
  while (true) {
    Command job;
    {
      std::unique_lock<std::mutex> lock(ioMutex);
      ioWake.wait(lock, [&] { return ioStopping || !ioJobs.empty(); });
      if (ioJobs.empty() && ioStopping)
        return;
      job = std::move(ioJobs.front());
      ioJobs.pop_front();
    }
    job();
  }
}
void Manager::runRecovery() {
  while (true) {
    std::vector<std::pair<uint64_t, Command>> jobs;
    {
      std::unique_lock<std::mutex> lock(recoveryMutex);
      recoveryWake.wait(lock, [&] { return recoveryStopping || !recoveryJobs.empty(); });
      if (recoveryJobs.empty() && recoveryStopping)
        return;
      while (!recoveryStopping && std::chrono::steady_clock::now() <
                                      std::min(recoveryLast + std::chrono::milliseconds(250),
                                               recoveryFirst + std::chrono::seconds(1)))
        recoveryWake.wait_until(lock, std::min(recoveryLast + std::chrono::milliseconds(250),
                                               recoveryFirst + std::chrono::seconds(1)));
      for (auto &[id, job] : recoveryJobs) {
        juce::ignoreUnused(id);
        jobs.push_back(std::move(job));
      }
      recoveryJobs.clear();
    }
    std::sort(jobs.begin(), jobs.end(),
              [](const auto &a, const auto &b) { return a.first < b.first; });
    for (auto &job : jobs)
      job.second();
  }
}
void Manager::persist() {
  if (!storage)
    return;
  const auto copy = project;
  const auto undo = history;
  const auto hash = manifestHash;
  const bool discard = discarded;
  const auto sequence = ++recoverySequence;
  {
    std::lock_guard<std::mutex> lock(recoveryMutex);
    if (recoveryJobs.empty())
      recoveryFirst = std::chrono::steady_clock::now();
    recoveryLast = std::chrono::steady_clock::now();
    recoveryJobs[copy.id] = {
        sequence, [this, copy, undo, hash, discard, sequence] {
          try {
            diagnostics::Scope trace("RECOVERY", "Local checkpoint revision=" +
                                                     String(juce::int64(copy.revision)));
            auto checkpoint = object();
            put(checkpoint, "version", 1);
            put(checkpoint, "project", copy.json());
            put(checkpoint, "history", undo.json(copy.id));
            put(checkpoint, "manifestHash", hash);
            put(checkpoint, "discardPending", discard);
            durableJson(stateRoot().getChildFile("recovery/" + copy.id + ".json"), checkpoint);
            recoveryDone = sequence;
          } catch (const std::exception &e) {
            post([this, error = String(e.what())] { publish("Recovery failed", error); });
          }
        }};
  }
  recoveryWake.notify_one();
}
void Manager::startIo(std::function<void(DiskWork &)> task,
                      std::function<void(DiskWork &)> accepted, bool isExclusive) {
  if (ioActive) {
    deferredIo.push_back(
        [this, task, accepted, isExclusive] { startIo(task, accepted, isExclusive); });
    return;
  }
  auto job = std::make_shared<DiskWork>();
  job->storage = storage;
  job->project = project;
  job->committed = committed;
  job->history = history;
  job->committedHistory = committedHistory;
  job->manifestHash = manifestHash;
  job->libraryState = libraryState;
  job->sourceWaveforms = sourceWaveforms;
  job->completedAudio = completedAudio;
  job->importedSampleId = importedSampleId;
  job->importGeneration = importGeneration;
  job->stop = [this] { return stopping.load(); };
  job->report = [this](String status, String error) {
    post([this, status, error] { publish(status, error); });
  };
  ioActive = true;
  exclusive = isExclusive;
  {
    std::lock_guard<std::mutex> lock(ioMutex);
    ioJobs.push_back([this, job, task, accepted] {
      try {
        task(*job);
        post([this, job, accepted] {
          ioActive = false;
          exclusive = false;
          accepted(*job);
        });
      } catch (const std::exception &e) {
        post([this, error = String(e.what())] {
          ioActive = false;
          saving = false;
          exclusive = false;
          failed = true;
          saveRequested = false;
          publish(dirty ? "Save failed" : "Action required", error);
        });
      }
    });
  }
  ioWake.notify_one();
  publish(saving ? "Saving…" : "Processing");
}
void Manager::refreshHealth() {
  for (auto &sample : project.samples) {
    const auto *saved = committed.find(sample.id);
    if (sample.protectedEntry || !saved || saved->bank != sample.bank || saved->slot != sample.slot)
      continue;
    for (const auto &entry : libraryState.samples)
      if (entry.path == card::path(sample.bank, sample.slot) && entry.bank == sample.bank &&
          entry.sample == sample.slot && entry.error.isNotEmpty() && entry.error != "Cancelled") {
        sample.protectedEntry = true;
        sample.problem = entry.error;
        project.warnings.addIfNotAlreadyThere("Bank " + String(sample.bank + 1) + " slot " +
                                              String(sample.slot + 1) + ": " + sample.problem);
      }
  }
}
void Manager::adoptDisk(DiskWork &job) {
  storage = job.storage;
  project = job.project;
  committed = job.committed;
  history = job.history;
  committedHistory = job.committedHistory;
  manifestHash = job.manifestHash;
  sourceWaveforms = job.sourceWaveforms;
  completedAudio = job.completedAudio;
  libraryState = job.libraryState;
  importedSampleId = job.importedSampleId;
  importGeneration = job.importGeneration;
  dirty = project.revision > project.completedRevision;
  failed = false;
  discarded = false;
  markerSequences.clear();
  lastEdit = std::chrono::steady_clock::now();
  refreshHealth();
  persist();
  publish(dirty ? "Unsaved changes" : "Saved");
}
void Manager::open(const File &root) {
  post([this, root] {
    require(!dirty, "Save or discard pending edits before opening another folder");
    if (storage && storage->root == root)
      return;
    startIo([root](DiskWork &job) { job.open(root); }, [this](DiskWork &job) { adoptDisk(job); },
            true);
  });
}
void Manager::changed() {
  dirty = true;
  discarded = false;
  lastEdit = std::chrono::steady_clock::now();
  std::set<String> active;
  bool needsWave = false;
  for (const auto &sample : project.samples) {
    if (sample.protectedEntry)
      continue;
    active.insert(sample.sourceHash);
    needsWave |= !committed.find(sample.id) && !sourceWaveforms.count(sample.sourceHash);
  }
  for (auto it = sourceWaveforms.begin(); it != sourceWaveforms.end();)
    if (!active.count(it->first))
      it = sourceWaveforms.erase(it);
    else
      ++it;
  if (needsWave)
    startIo([](DiskWork &job) { job.prepareImportWaveforms(); },
            [this](DiskWork &job) {
              for (const auto &sample : project.samples)
                if (job.sourceWaveforms.count(sample.sourceHash))
                  sourceWaveforms[sample.sourceHash] = job.sourceWaveforms.at(sample.sourceHash);
              publish(dirty ? "Unsaved changes" : "Saved", snapshot().error);
            });
  persist();
  publish(saving ? "Saving…" : "Unsaved changes", snapshot().error);
}
void Manager::setAutoSave(bool enabled) {
  post([this, enabled] {
    autoSave = enabled;
    lastEdit = std::chrono::steady_clock::now();
    publish(dirty ? "Unsaved changes" : "Saved", snapshot().error);
  });
}
void Manager::saveNow() {
  post([this] {
    failed = false;
    saveRequested = dirty;
    publish(dirty ? "Saving…" : "Saved");
  });
}
void Manager::retry() { saveNow(); }
void Manager::afterEdits(std::function<void(ManagerState)> callback) {
  post([this, callback] {
    auto value = snapshot();
    value.busy = ioActive || !deferredIo.empty();
    callback(value);
  });
}
void Manager::discardPending() {
  post([this] {
    if (ioActive) {
      deferredIo.push_back([this] { discardPending(); });
      return;
    }
    project = committed;
    history = committedHistory;
    markerSequences.clear();
    dirty = false;
    failed = false;
    saveRequested = false;
    discarded = true;
    persist();
    startIo(
        [](DiskWork &job) {
          job.storage->savePending(job.committed);
          durableJson(child(job.storage->root, ".core-manager/history.json"),
                      job.history.json(job.project.id));
        },
        [this](DiskWork &) { publish("Saved"); });
  });
}
void Manager::startSave() {
  saving = true;
  saveRequested = false;
  const auto hook = beforeSave;
  startIo(
      [hook](DiskWork &job) {
        if (hook)
          hook();
        job.save();
      },
      [this](DiskWork &job) {
        saving = false;
        committed = job.committed;
        committedHistory = job.history;
        manifestHash = job.manifestHash;
        libraryState = job.libraryState;
        completedAudio = job.completedAudio;
        project.completedRevision = committed.completedRevision;
        project.fingerprints = committed.fingerprints;
        for (auto &sample : project.samples)
          if (const auto *saved = committed.find(sample.id)) {
            sample.completedRevision = saved->completedRevision;
            sample.namePending = saved->namePending;
            if (audioKey(sample) == audioKey(*saved)) {
              sample.rendered = saved->rendered;
              sample.renderKey = saved->renderKey;
              sample.ownedPaths = saved->ownedPaths;
              if (sample.bank == saved->bank && sample.slot == saved->slot)
                sample.nameUnsafe = saved->nameUnsafe;
            }
          }
        if (project.revision == committed.revision)
          project.warnings = committed.warnings;
        refreshHealth();
        dirty = project.revision > committed.completedRevision;
        if (!dirty)
          saveRequested = false;
        failed = false;
        persist();
        publish(dirty ? "Unsaved changes" : "Saved");
      });
}
void Manager::change(const String &label, const std::function<void(Project &)> &operation,
                     const std::function<void()> &onAccepted) {
  diagnostics::Scope trace("MANAGER", "Change: " + label);
  require(storage && !exclusive, "Wait for the project operation before editing");
  auto before = project;
  auto candidate = project;
  operation(candidate);
  if (juce::JSON::toString(candidate.json()) == juce::JSON::toString(project.json()))
    return;
  candidate.revision = project.revision + 1;
  candidate.completedRevision = committed.completedRevision;
  for (auto &s : candidate.samples) {
    const auto *before = project.find(s.id);
    if (!before || juce::JSON::toString(s.json()) != juce::JSON::toString(before->json()))
      s.revision = candidate.revision;
  }
  candidate.validate();
  history.accept(before, label);
  project = std::move(candidate);
  dirty = true;
  if (onAccepted)
    onAccepted();
  changed();
}
void Manager::import(const juce::StringArray &paths, int bank) {
  post([this, paths, bank] {
    require(storage != nullptr, "Open a folder first");
    startIo(
        [paths, bank](DiskWork &job) {
          int count = 0;
          for (const auto &path : paths) {
            const int slot = job.project.freeSlot(bank);
            require(slot >= 0,
                    "Bank is full (16 slots). Choose another bank; no sample was overwritten.");
            job.publish("Importing " + String(++count) + " of " + String(paths.size()) + ": " +
                        File(path).getFileName());
            auto sample = job.audio.import(*job.storage, File(path), bank, slot, job.stop);
            job.added.push_back(sample);
            job.project.samples.push_back(std::move(sample));
          }
          job.prepareImportWaveforms();
        },
        [this, bank](DiskWork &job) {
          change(
              "Import samples",
              [&](Project &current) {
                for (auto sample : job.added) {
                  sample.slot = current.freeSlot(bank);
                  require(sample.slot >= 0, "Bank filled while importing; originals are retained");
                  current.samples.push_back(sample);
                }
              },
              [&] {
                for (const auto &[hash, wave] : job.sourceWaveforms)
                  sourceWaveforms[hash] = wave;
                if (!job.added.empty())
                  importedSampleId = job.added.front().id;
                ++importGeneration;
              });
        });
  });
}
void Manager::edit(const String &id, const String &label, std::function<void(Sample &)> fn) {
  post([this, id, label, fn = std::move(fn)] {
    change(label, [&](Project &p) {
      auto *s = p.find(id);
      require(s && !s->protectedEntry, "Sample is missing or protected");
      if (!s->recoveryFiles.empty()) {
        s->recoveryFiles.clear();
        s->recoveryHashes.clear();
      }
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
void Manager::editMarkerLane(const String &id, int lane, std::vector<Marker> slices,
                             std::vector<double> transients, uint64_t sequence) {
  post([this, id, lane, slices = std::move(slices), transients = std::move(transients), sequence] {
    require(lane >= -1 && lane < 3, "Invalid marker lane");
    change("Edit markers", [&](Project &p) {
      auto *sample = p.find(id);
      require(sample && !sample->protectedEntry, "Sample is missing or protected");
      sample->recoveryFiles.clear();
      sample->recoveryHashes.clear();
      if (lane < 0) {
        if (juce::JSON::toString(sample->json()["slices"]) != [&] {
              auto copy = *sample;
              copy.slices = slices;
              return juce::JSON::toString(copy.json()["slices"]);
            }())
          sample->spliceVariable = true;
        sample->slices = slices;
      } else
        sample->transients[size_t(lane)] = transients;
    });
    markerSequences[id] = sequence;
    publish(saving ? "Saving…" : dirty ? "Unsaved changes" : "Saved", snapshot().error);
  });
}
void Manager::remove(const std::vector<String> &ids) {
  post([this, ids] {
    auto erase = [this, ids] {
      change("Remove samples", [&](Project &p) {
        for (const auto &id : ids)
          if (const auto *sample = p.find(id)) {
            const auto name = names::path(sample->bank, sample->slot);
            for (int n = p.warnings.size() - 1; n >= 0; --n)
              if (p.warnings[n].contains(name))
                p.warnings.remove(n);
            p.warnings.removeString("Bank " + String(sample->bank + 1) + " slot " +
                                    String(sample->slot + 1) + ": " + sample->problem);
          }
        p.samples.erase(std::remove_if(p.samples.begin(), p.samples.end(),
                                       [&](const Sample &sample) {
                                         return std::find(ids.begin(), ids.end(), sample.id) !=
                                                ids.end();
                                       }),
                        p.samples.end());
      });
    };
    bool damaged = false;
    for (const auto &id : ids)
      if (auto *sample = project.find(id))
        damaged |= sample->protectedEntry || sample->nameUnsafe;
    if (!damaged) {
      erase();
      return;
    }
    startIo(
        [ids](DiskWork &job) {
          for (const auto &id : ids)
            if (auto *sample = job.project.find(id);
                sample && (sample->protectedEntry || sample->nameUnsafe))
              job.storage->archiveDamaged(*sample);
        },
        [this, ids, erase](DiskWork &job) {
          for (const auto &id : ids)
            if (auto *current = project.find(id))
              if (const auto *archived = job.project.find(id);
                  archived && (archived->protectedEntry || archived->nameUnsafe)) {
                require(current->bank == archived->bank && current->slot == archived->slot,
                        "Slot changed while preparing recovery");
                current->recoveryFiles = archived->recoveryFiles;
                current->recoveryHashes = archived->recoveryHashes;
                current->ownedPaths = archived->ownedPaths;
                if (auto *baseline = committed.find(id)) {
                  baseline->recoveryFiles = archived->recoveryFiles;
                  baseline->recoveryHashes = archived->recoveryHashes;
                  baseline->ownedPaths = archived->ownedPaths;
                }
              }
          erase();
        });
  });
}
void Manager::clearBank(int bank) {
  post([this, bank] {
    change("Clear bank", [&](Project &p) {
      p.samples.erase(
          std::remove_if(p.samples.begin(), p.samples.end(),
                         [&](const Sample &s) { return s.bank == bank && !s.protectedEntry; }),
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
        std::sort(list.begin(), list.end(), [](auto *a, auto *b) { return a->slot < b->slot; });
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
              s->recoveryFiles.clear();
              s->recoveryHashes.clear();
              other->recoveryFiles.clear();
              other->recoveryHashes.clear();
              std::swap(s->slot, other->slot);
            } else {
              s->recoveryFiles.clear();
              s->recoveryHashes.clear();
              s->slot = destination;
            }
          }
      } else
        for (const auto &id : ids) {
          auto *s = p.find(id);
          require(s && !s->protectedEntry, "Cannot move protected entries");
          if (s->bank == bank)
            continue;
          int slot = p.freeSlot(bank);
          require(slot >= 0, "Destination bank is full");
          s->recoveryFiles.clear();
          s->recoveryHashes.clear();
          s->bank = bank;
          s->slot = slot;
        }
    });
  });
}
void Manager::merge(const std::vector<String> &ids, int bank) {
  post([this, ids, bank] {
    require(storage != nullptr, "Open a folder first");
    auto selected = std::make_shared<std::vector<Sample>>();
    auto merged = std::make_shared<Sample>();
    startIo(
        [ids, bank, selected, merged](DiskWork &job) {
          for (const auto &sample : job.project.samples)
            if (std::find(ids.begin(), ids.end(), sample.id) != ids.end()) {
              require(!sample.protectedEntry, "Cannot merge a damaged sample");
              selected->push_back(sample);
            }
          std::sort(selected->begin(), selected->end(),
                    [](auto &a, auto &b) { return a.bank * 16 + a.slot < b.bank * 16 + b.slot; });
          merged->bank = bank;
          merged->name = "Merged samples";
          auto file = job.audio.merge(job.storage->root, *selected, *merged, job.stop);
          merged->source = job.storage->importSource(file, merged->sourceHash);
          job.project.samples.push_back(*merged);
          job.prepareImportWaveforms();
        },
        [this, bank, selected, merged](DiskWork &job) {
          for (const auto &original : *selected) {
            const auto *current = project.find(original.id);
            require(current && current->revision == original.revision,
                    "Sample changed while merging; merge again");
          }
          change(
              "Merge samples",
              [&](Project &p) {
                merged->slot = p.freeSlot(bank);
                require(merged->slot >= 0, "Bank is full; choose another bank before merging");
                p.samples.push_back(*merged);
              },
              [&] { sourceWaveforms = job.sourceWaveforms; });
        });
  });
}
void Manager::even(const String &id, int count) {
  diagnostics::log("SLICES", "Even sample=" + id + " target=" + String(count));
  edit(id, "Even slices", [count](Sample &s) {
    require(count > 0 && count <= 1024, juce::String::fromUTF8("Choose 1–1024 slices"));
    s.slices.clear();
    for (int n = 0; n < count; ++n)
      s.slices.push_back({double(n) / count, double(n + 1) / count, 0});
    s.spliceVariable = false;
    s.updateSpliceTrigger(SpliceTimingCalculation::evenSlices);
  });
}
void Manager::detect(const String &id, int count, String method, double spacing) {
  post([this, id, count, method, spacing] {
    require(count > 0 && count <= 1024, "Choose 1–1024 slices");
    startIo(
        [id, count, method, spacing](DiskWork &job) {
          auto *sample = job.project.find(id);
          require(sample && !sample->protectedEntry, "Cannot analyse this entry");
          sample->slices = job.audio.detect(child(job.storage->root, sample->source), method,
                                            spacing, job.stop, count);
        },
        [this, id](DiskWork &job) {
          const auto *analysed = job.project.find(id);
          const auto *current = project.find(id);
          require(current && current->revision == analysed->revision,
                  "Sample changed during analysis; analyse again");
          change("Automatic slicing", [&](Project &p) {
            p.find(id)->slices = analysed->slices;
            p.find(id)->spliceVariable = true;
          });
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
    require(!exclusive, "Wait for the project operation before undoing");
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
      changed();
    }
  });
}
void Manager::redo() {
  post([this] {
    require(!exclusive, "Wait for the project operation before redoing");
    auto revision = project.revision;
    if (history.redo(project)) {
      project.revision = revision + 1;
      for (auto &s : project.samples)
        if (!s.protectedEntry)
          s.revision = project.revision;
      project.completedRevision = committed.completedRevision;
      project.fingerprints = committed.fingerprints;
      dirty = true;
      changed();
    }
  });
}
void Manager::duplicate(const File &destination) {
  post([this, destination] {
    require(storage && !dirty, "Finish saving before duplicating");
    startIo([destination](DiskWork &job) { job.storage->duplicateTo(destination); },
            [this](DiskWork &) { publish("Saved"); }, true);
  });
}
void Manager::cleanup() {
  post([this] {
    require(storage && !dirty, "Finish saving before cleanup");
    startIo(
        [](DiskWork &job) {
          job.history.clear();
          durableJson(child(job.storage->root, ".core-manager/history.json"),
                      job.history.json(job.project.id));
          job.storage->cleanup(job.project, true);
        },
        [this](DiskWork &) {
          history.clear();
          committedHistory.clear();
          persist();
          publish("Saved");
        },
        true);
  });
}
void Manager::reconcile() {
  post([this] {
    require(storage != nullptr, "Open a folder first");
    startIo([](DiskWork &job) { job.reconcile(); }, [this](DiskWork &job) { adoptDisk(job); },
            true);
  });
}
void Manager::applyAnalysis(const String &projectId, const String &sampleId, int64_t revision,
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
void DiskWork::open(const File &root) {
  diagnostics::Scope trace("MANAGER", "Open folder " + root.getFullPathName());

  int lastProgress = -1;
  auto report = [&](int percent) {
    percent = juce::jlimit(0, 100, percent);
    if (percent > lastProgress) {
      lastProgress = percent;
      publish("Opening folder..." + String(percent) + "%");
    }
  };
  report(0);
  auto candidate = std::make_shared<Storage>(root);
  auto loaded = candidate->open([&](double fraction) { report(int(fraction * 70.)); });
  storage = std::move(candidate);
  project = std::move(loaded);
  sourceWaveforms.clear();
  completedAudio.clear();
  importedSampleId.clear();
  committed = Project::fromJson(parseJson(child(root, ".core-manager/project.json")));
  // Adopt newly recognized card settings into the committed baseline too.
  // Pending/history state can contain edits, so read the actual card value.
  if (committed.settings.find("start_tempo") == committed.settings.end()) {
    const auto settings = card::readSettings(root, project.warnings);
    const auto found = settings.find("start_tempo");
    committed.settings["start_tempo"] = found == settings.end() ? String("default") : found->second;
    committed.fingerprints[card::startTempoPath] = fingerprint(child(root, card::startTempoPath));
    if (child(root, "start_tempo").exists())
      committed.fingerprints["start_tempo"] = fingerprint(child(root, "start_tempo"));
  }
  if (committed.settings.find("startup_playback") == committed.settings.end()) {
    const auto settings = card::readSettings(root, project.warnings);
    const auto found = settings.find("startup_playback");
    committed.settings["startup_playback"] =
        found == settings.end() ? String("play") : found->second;
    committed.fingerprints[card::startupPlaybackPath] =
        fingerprint(child(root, card::startupPlaybackPath));
    if (child(root, "startup_playback").exists())
      committed.fingerprints["startup_playback"] = fingerprint(child(root, "startup_playback"));
  }
  manifestHash = fingerprint(child(root, ".core-manager/project.json"));
  history.clear();
  auto undoFile = child(root, ".core-manager/history.json");
  if (undoFile.existsAsFile())
    history.restore(parseJson(undoFile), project.id,
                    {{"startup_playback", committed.settings.at("startup_playback")}});

  committedHistory = history;
  committedHistory.retainSaved(committed.revision);
  const auto local = stateRoot().getChildFile("recovery/" + project.id + ".json");
  if (local.existsAsFile()) {
    auto checkpoint = parseJson(local);
    if (checkpoint.hasProperty("history") && checkpoint["project"]["id"].toString() == project.id &&
        (int64_t(checkpoint["project"]["revision"]) >= project.revision ||
         (bool(checkpoint["discardPending"]) &&
          checkpoint["manifestHash"].toString() == manifestHash)))
      history.restore(checkpoint["history"], project.id,
                      {{"startup_playback", committed.settings.at("startup_playback")}});
  }
  prepareImportWaveforms();

  report(75);
  prepareVisualization([&](const zv::LibraryState &progress) {
    report(75 + 24 * progress.completed / std::max(1, progress.total));
  });
  report(100);
  publish("Ready");
}
void DiskWork::prepareImportWaveforms() {
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
      sourceWaveforms[sample.sourceHash] =
          audio.sourceWaveform(child(storage->root, sample.source), [this] { return stop(); });
      diagnostics::log("WAVEFORM", "Imported waveform ready sample=" + sample.id +
                                       " name=" + sample.name + " before hardware save");
    } catch (const std::exception &e) {
      if (stop())
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
void DiskWork::prepareCompletedAudio() {
  std::map<String, CompletedAudio> next;
  for (const auto &sample : committed.samples) {
    if (sample.protectedEntry || sample.rendered.isEmpty())
      continue;
    const auto cached = completedAudio.find(sample.id);
    if (cached != completedAudio.end() && cached->second.path == sample.rendered) {
      next.emplace(sample.id, cached->second);
      continue;
    }
    try {
      const auto header = card::inspect(child(storage->root, sample.rendered));
      next.emplace(sample.id, CompletedAudio{sample.rendered, header.frames, double(header.rate)});
    } catch (const std::exception &e) {
      diagnostics::log("AUDIO", "Completed preview metadata unavailable: " + String(e.what()));
    }
  }
  completedAudio = std::move(next);
}
void DiskWork::prepareVisualization(std::function<void(const zv::LibraryState &)> progress) {
  diagnostics::Scope trace("VISUALIZER", "Prepare project waveforms");
  require(storage != nullptr, "Open a folder");
  prepareCompletedAudio();
  publish("Updating waveforms");
  String currentFile;
  auto report = [&](const zv::LibraryState &update) {
    if (update.current != currentFile) {
      currentFile = update.current;
      diagnostics::log("VISUALIZER", "Analysing " + currentFile +
                                         " completed=" + String(update.completed) +
                                         " total=" + String(update.total));
    }
    if (progress)
      progress(update);
  };
  libraryState = zv::Library::prepare(
      storage->root, zv::Library::defaultCache(), [this] { return stop(); }, report, nullptr,
      &libraryState, &committed.fingerprints);
  diagnostics::log("VISUALIZER", "Total=" + String(libraryState.total) +
                                     " prepared=" + String(libraryState.prepared) + " reused=" +
                                     String(libraryState.reused) + " error=" + libraryState.error +
                                     " warning=" + libraryState.warning);
  for (const auto &entry : libraryState.samples)
    diagnostics::log("VISUALIZER",
                     entry.path + " ready=" + String(entry.wave ? 1 : 0) + " error=" + entry.error);
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
void DiskWork::save() {
  diagnostics::Scope trace("MANAGER", "Save revision=" + String(juce::int64(project.revision)));
  require(storage && storage->root.isDirectory(),
          "Project volume is unavailable. Reconnect it and Retry; pending "
          "edits are retained.");
  publish("Checking card files");
  auto root = storage->root;
  require(fingerprint(child(root, ".core-manager/project.json")) == manifestHash,
          "Project manifest is missing or changed externally. Reconnect or "
          "reconcile before saving.");
  auto cancel = [&] { return stop(); };
  cancelled(cancel);
  std::set<String> changedIds, rawPaths;
  std::map<String, String> authorised;
  for (const auto &old : committed.samples) {
    const auto *now = project.find(old.id);
    if (now && now->protectedEntry)
      continue;
    if (!now || now->bank != old.bank || now->slot != old.slot ||
        editSettings(*now) != editSettings(old)) {
      if (!old.protectedEntry || !old.recoveryFiles.empty())
        changedIds.insert(old.id);
      if (!now)
        for (const auto &[path, hash] : old.recoveryHashes) {
          authorised[path] = hash;
          rawPaths.insert(path);
        }
    }
  }
  for (const auto &sample : project.samples) {
    const auto *old = committed.find(sample.id);
    if ((!sample.protectedEntry &&
         (!old || editSettings(sample) != editSettings(*old) ||
          std::find(old->ownedPaths.begin(), old->ownedPaths.end(),
                    names::path(old->bank, old->slot)) == old->ownedPaths.end())) ||
        (sample.protectedEntry && !old && !sample.recoveryFiles.empty()))
      changedIds.insert(sample.id);
    if (!sample.recoveryFiles.empty() && changedIds.count(sample.id))
      for (const auto &[path, hash] : sample.recoveryHashes) {
        juce::ignoreUnused(hash);
        rawPaths.insert(path);
      }
  }
  for (const auto &old : committed.samples)
    if (changedIds.count(old.id))
      for (const auto &[path, hash] : old.recoveryHashes) {
        juce::ignoreUnused(hash);
        rawPaths.insert(path);
      }
  std::set<String> affectedPaths;
  for (const auto &old : committed.samples)
    if (changedIds.count(old.id))
      affectedPaths.insert(old.ownedPaths.begin(), old.ownedPaths.end());
  for (const auto &sample : project.samples)
    if (changedIds.count(sample.id)) {
      for (int variant = 0; variant <= 8; variant += 2)
        for (const auto &suffix : juce::StringArray{"", ".info"})
          affectedPaths.insert(card::path(sample.bank, sample.slot, variant) + suffix);
      affectedPaths.insert(names::path(sample.bank, sample.slot));
    }
  const auto previousMarkers = card::settingsFiles(committed.settings);
  const auto nextMarkers = card::settingsFiles(project.settings);
  const auto previousText = card::textSettingsContents(committed.settings);
  const auto nextText = card::textSettingsContents(project.settings);
  for (const auto &[path, value] : nextMarkers)
    if (!previousMarkers.count(path) || previousMarkers.at(path) != value)
      affectedPaths.insert(path);
  for (const auto &[path, value] : nextText)
    if (!previousText.count(path) || previousText.at(path) != value)
      affectedPaths.insert(path);
  auto verified = committed.fingerprints;
  for (const auto &[path, hash] : authorised)
    verified[path] = hash;
  std::vector<std::pair<String, String>> checks;
  for (const auto &path : affectedPaths)
    if (verified.count(path))
      checks.push_back({path, verified.at(path)});
  {
    diagnostics::Scope traceChecks("STORAGE", "Verify affected files before save count=" +
                                                  String(int(checks.size())));
    parallelFor(checks.size(), [&](size_t index) {
      const auto &[path, expected] = checks[index];
      const auto file = child(root, path);
      const auto actual = rawPaths.count(path) && file.existsAsFile() ? hashFile(file, cancel)
                                                                      : fingerprint(file, cancel);
      require(actual == expected, "External change: " + path + ". Reload/reconcile before saving.");
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
    if (sample.protectedEntry || restoreRecovery(sample, committed.find(sample.id)))
      return false;
    const auto *previous = committed.find(sample.id);
    return !previous || audioKey(sample) != audioKey(*previous);
  };
  // A cache key owns its converted/preview/padded files. Different keys can
  // render concurrently; identical imports must never write that cache at the
  // same time. Each worker has its own JUCE decoder/format manager.
  std::map<String, size_t> renderKeys;
  std::vector<std::vector<size_t>> renderGroups;
  std::vector<Rendered> renders(completed.samples.size());
  size_t renderCount = 0;
  for (size_t index = 0; index < completed.samples.size(); ++index) {
    const auto &sample = completed.samples[index];
    if (!needsRender(sample))
      continue;
    auto inserted = renderKeys.emplace(audioKey(sample), renderGroups.size());
    if (inserted.second)
      renderGroups.emplace_back();
    renderGroups[inserted.first->second].push_back(index);
    ++renderCount;
  }
  if (renderCount) {
    diagnostics::Scope traceRenders("AUDIO",
                                    "Render batch files=" + String(int(renderCount)) +
                                        " independent_caches=" + String(int(renderGroups.size())));
    std::atomic<size_t> renderedCount{0};
    auto report = [&](size_t) {
      auto status = "Processing audio " + String(int(renderedCount.load())) + " of " +
                    String(int(renderCount));
      if (renderCount == 1)
        status += ": " + completed.samples[renderGroups[0][0]].name;
      publish(status);
    };
    report(0);
    parallelFor(
        renderGroups.size(),
        [&](size_t group) {
          AudioProcessing renderer;
          for (const auto index : renderGroups[group]) {
            cancelled(cancel);
            // Rendering also verifies each immutable source, including duplicate
            // imports. Later members reuse the first member's verified cache.
            renders[index] = renderer.render(root, completed.samples[index], cancel);
            ++renderedCount;
          }
        },
        report);
    cancelled(cancel);
    publish("Preparing card files");
  }
  std::map<String, File> desired;
  std::map<String, String> nameExpected;
  std::set<String> unmanagedNames;
  std::set<String> oldOwned;
  for (const auto &s : committed.samples)
    if (changedIds.count(s.id) && (!s.protectedEntry || !s.recoveryFiles.empty()))
      for (const auto &path : s.ownedPaths)
        oldOwned.insert(path);
  for (size_t index = 0; index < completed.samples.size(); ++index) {
    auto &s = completed.samples[index];
    cancelled(cancel);
    auto *previous = committed.find(s.id);
    if (!changedIds.count(s.id)) {
      s.completedRevision = s.revision;
      continue;
    }
    if (s.protectedEntry || restoreRecovery(s, previous)) {
      if (restoreRecovery(s, previous))
        for (const auto &[path, backup] : s.recoveryFiles) {
          require(hashFile(child(root, backup), cancel) == s.recoveryHashes.at(path),
                  "Damaged sample recovery is unavailable");
          desired[path] = child(root, backup);
        }
      else
        for (const auto &path : s.ownedPaths)
          oldOwned.erase(path);
      s.completedRevision = s.revision;
      continue;
    }
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
      const auto &rendered = renders[index];
      s.rendered = rendered.preview;
      s.renderKey = rendered.key;
      desired[card::path(s.bank, s.slot)] = child(root, rendered.padded);
      auto bytes = encodeInfo(sampleInfo(s, rendered.frames));
      auto path = child(root, ".core-manager/cache/metadata-" + s.id + "-0.info");
      durableWrite(path, bytes.getData(), bytes.getSize());
      desired[card::path(s.bank, s.slot) + ".info"] = path;
    } else {
      s.rendered = previous->rendered;
      s.renderKey = previous->renderKey;
      for (const auto &path : previous->ownedPaths) {
        if (path.endsWith(".name.json"))
          continue; // Regenerate from this sample and its completed primary WAV.
        auto filename = path.fromLastOccurrenceOf(
            "/", false,
            false); // only use the basename; the project path remains relative
        auto suffix = filename.fromFirstOccurrenceOf(".", true, false);
        auto target = "bank" + String(s.bank + 1) + "/" + String(s.slot) + suffix;
        desired[target] = child(root, path);
      }
      if (metadataChanged)
        for (int variant = 0; variant <= 8; variant += 2) {
          auto wavePath = card::path(s.bank, s.slot, variant);
          auto found = desired.find(wavePath);
          if (found == desired.end())
            continue;
          auto w = card::inspect(found->second);
          auto info = sampleInfo(s, w.frames - w.rate);
          auto bytes = encodeInfo(info);
          auto path =
              child(root, ".core-manager/cache/metadata-" + s.id + "-" + String(variant) + ".info");
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
    s.nameUnsafe = existingName.status == names::Metadata::invalid;
    if (existingName.status == names::Metadata::invalid) {
      completed.warnings.addIfNotAlreadyThere(existingName.warning);
      oldOwned.erase(namePath);
      unmanagedNames.insert(namePath);
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
    s.namePending = false;
    s.ownedPaths.clear();
    auto prefix = "bank" + String(s.bank + 1) + "/" + String(s.slot) + ".";
    for (const auto &[path, file] : desired)
      if (path.startsWith(prefix))
        s.ownedPaths.push_back(path);
    s.completedRevision = s.revision;
  }
  for (const auto &[path, enabled] : card::settingsFiles(completed.settings)) {
    if (!affectedPaths.count(path))
      continue;
    if (enabled) {
      auto marker = child(root, ".core-manager/cache/empty-marker");
      if (!marker.existsAsFile())
        durableWrite(marker, nullptr, 0);
      desired[path] = marker;
    } else
      oldOwned.insert(path);
  }
  for (const auto &[path, content] : card::textSettingsContents(completed.settings)) {
    if (!affectedPaths.count(path))
      continue;
    auto textFile =
        child(root, ".core-manager/cache/" + path.fromLastOccurrenceOf("/", false, false));
    durableWrite(textFile, content.toRawUTF8(), size_t(content.getNumBytesAsUTF8()));
    desired[path] = textFile;
  }
  std::vector<Replacement> replacements;
  auto expected = [&](const String &path) {
    if (authorised.count(path))
      return authorised.at(path);
    auto found = committed.fingerprints.find(path);
    if (found != committed.fingerprints.end())
      return found->second;
    auto name = nameExpected.find(path);
    return name == nameExpected.end() ? String("missing") : name->second;
  };
  completed.fingerprints = verified;
  for (const auto &path : unmanagedNames)
    completed.fingerprints.erase(path);
  for (const auto &[path, file] : desired) {
    cancelled(cancel);
    auto before = expected(path);
    auto known = verified.find(path);
    require((known == verified.end() ? fingerprint(child(root, path), cancel) : known->second) ==
                before,
            "External change: " + path + ". Reload/reconcile before saving.");
    auto after = checkedHash(file);
    if (after != before)
      replacements.push_back({path, file, before, after, rawPaths.count(path) != 0});
    completed.fingerprints[path] = after;
    oldOwned.erase(path);
  }
  for (const auto &path : oldOwned) {
    auto before = expected(path);
    cancelled(cancel);
    if (before != "missing")
      replacements.push_back({path, {}, before, {}, rawPaths.count(path) != 0});
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
  committed = completed;
  project = completed;
  manifestHash = fingerprint(child(root, ".core-manager/project.json"));
  storage->savePending(project);
  durableJson(child(root, ".core-manager/history.json"), history.json(project.id));
  prepareVisualization();
  publish("Ready");
}
void DiskWork::reconcile() {
  require(storage && storage->root.isDirectory(), "Reconnect the project folder first");
  auto root = storage->root;
  auto manifest = child(root, ".core-manager/project.json");
  auto archive = child(root, ".core-manager/recovery/reconcile-" + uuid());
  durableJson(archive.getChildFile("pending-project.json"), project.json());
  durableJson(archive.getChildFile("completed-project.json"), committed.json());
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
        if (old == committed.fingerprints.end() || now == adopted.fingerprints.end() ||
            old->second != now->second)
          unchanged = false;
      }
      if (unchanged) {
        const auto protectedEntry = sample.protectedEntry;
        const auto problem = sample.problem;
        sample = *previous;
        sample.protectedEntry = protectedEntry;
        sample.problem = problem;
      } else {
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
  manifestHash = fingerprint(manifest);
  storage->queueNameBackfill(project);
  committedHistory = history;
  storage->savePending(project);
  durableJson(child(root, ".core-manager/history.json"), history.json(project.id));
  prepareVisualization();
  publish("Ready");
}
} // namespace core
