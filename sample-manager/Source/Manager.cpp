#include "Manager.h"
namespace core {
Manager::Manager() : worker([this] { run(); }) {}
Manager::~Manager() {
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
void Manager::post(Command c) {
  {
    std::lock_guard<std::mutex> lock(mutex);
    commands.push_back(std::move(c));
    ++serial;
    state.busy = true;
  }
  wake.notify_one();
}
void Manager::publish(String status, String error) {
  {
    std::lock_guard<std::mutex> lock(mutex);
    state.project = project;
    state.completedProject = committed;
    state.library = libraryState;
    state.root = storage ? storage->root : File();
    state.status = std::move(status);
    state.error = std::move(error);
    state.busy = commandRunning || !commands.empty() || (dirty && !failed);
    state.available = storage && storage->root.isDirectory();
    ++state.generation;
  }
  sendChangeMessage();
}
void Manager::run() {
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
    {
      std::unique_lock<std::mutex> lock(mutex);
      wake.wait(lock, [&] {
        return stopping || !commands.empty() || (dirty && !failed);
      });
      if (stopping && commands.empty())
        break;
      if (commands.empty()) {
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
    } catch (const std::exception &e) {
      if (String(e.what()) != "Cancelled") {
        failed = true;
        publish("Action required", e.what());
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
    if (storage && dirty)
      throw std::runtime_error(
          "Finish or reconcile pending changes before opening another folder");
    publish("Opening folder");
    auto candidate = std::make_unique<Storage>(root);
    auto loaded = candidate->open();
    storage = std::move(candidate);
    project = std::move(loaded);
    committed =
        Project::fromJson(parseJson(child(root, ".core-manager/project.json")));
    manifestHash = fingerprint(child(root, ".core-manager/project.json"));
    history.clear();
    auto undoFile = child(root, ".core-manager/history.json");
    if (undoFile.existsAsFile())
      history.restore(parseJson(undoFile), project.id);
    dirty = project.revision > project.completedRevision;
    failed = false;
    prepareVisualization();
    publish(dirty ? "Processing" : "Ready");
  });
}
void Manager::persist() {
  require(storage != nullptr, "Open a project folder first");
  // A second recovery copy survives an unplugged/remounted project volume.
  auto local = stateRoot().getChildFile("recovery/" + project.id + ".json");
  durableJson(local, project.json());
  storage->savePending(project);
  durableJson(child(storage->root, ".core-manager/history.json"),
              history.json(project.id));
}
void Manager::change(const String &label,
                     const std::function<void(Project &)> &operation) {
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
  publish("Saving changes");
  persist();
  publish("Processing");
}
void Manager::import(const juce::StringArray &paths, int bank) {
  post([this, paths, bank] {
    require(storage != nullptr, "Open a folder first");
    change("Import samples", [&](Project &p) {
      for (const auto &path : paths) {
        int slot = p.freeSlot(bank);
        require(slot >= 0, "Bank is full (16 slots). Choose another bank; no "
                           "sample was overwritten.");
        p.samples.push_back(audio.import(*storage, File(path), bank, slot));
      }
    });
  });
}
void Manager::edit(const String &id, const String &label,
                   std::function<void(Sample &)> fn) {
  post([this, id, label, fn = std::move(fn)] {
    change(label, [&](Project &p) {
      auto *s = p.find(id);
      require(s && !s->protectedEntry, "Sample is missing or protected");
      fn(*s);
    });
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
  edit(id, "Even slices", [count](Sample &s) {
    require(count > 0 && count <= 1024,
            juce::String::fromUTF8("Choose 1–1024 slices"));
    s.slices.clear();
    for (int n = 0; n < count; ++n)
      s.slices.push_back({double(n) / count, double(n + 1) / count, 0});
  });
}
void Manager::detect(const String &id, String method, double spacing) {
  post([this, id, method, spacing] {
    require(storage != nullptr, "Open a folder");
    auto *s = project.find(id);
    require(s && !s->protectedEntry, "Cannot analyse this entry");
    publish("Processing local onsets");
    auto generation = serial.load();
    auto markers =
        audio.detect(child(storage->root, s->source), method, spacing,
                     [&] { return stopping || serial.load() != generation; });
    change("Automatic slicing", [&](Project &p) {
      auto *target = p.find(id);
      target->slices = markers;
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
      persist();
      publish("Processing redo");
    }
  });
}
void Manager::retry() {
  post([this] {
    failed = false;
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
          sample.name = previous->name;
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
    dirty = false;
    failed = false;
    persist();
    prepareVisualization();
    publish("Ready");
  });
}
void Manager::prepareVisualization() {
  require(storage != nullptr, "Open a folder");
  libraryState =
      zv::Library::prepare(storage->root, zv::Library::defaultCache(),
                           [this] { return stopping.load(); });
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
  require(storage && storage->root.isDirectory(),
          "Project volume is unavailable. Reconnect it and Retry; pending "
          "edits are retained.");
  auto root = storage->root;
  require(fingerprint(child(root, ".core-manager/project.json")) ==
              manifestHash,
          "Project manifest is missing or changed externally. Reconnect or "
          "reconcile before saving.");
  auto cancel = [&] { return stopping || serial.load() != generation; };
  cancelled(cancel);
  for (const auto &[path, expected] : committed.fingerprints)
    require(fingerprint(child(root, path)) == expected,
            "External change: " + path + ". Reload/reconcile before saving.");
  auto completed = project;
  std::map<String, File> desired;
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
    bool changed = !previous || audioKey(s) != audioKey(*previous) ||
                   (s.oneShot && !s.tempoMatch) !=
                       (previous->oneShot && !previous->tempoMatch);
    bool metadataChanged = !previous || s.revision != previous->revision;
    if (changed) {
      publish("Processing " + s.name);
      auto rendered = audio.render(root, s, cancel);
      s.rendered = rendered.preview;
      s.companion = rendered.companionPreview;
      s.renderKey = rendered.key;
      desired[card::path(s.bank, s.slot)] = child(root, rendered.padded);
      if (rendered.companionPadded.isNotEmpty())
        desired[card::path(s.bank, s.slot, 1)] =
            child(root, rendered.companionPadded);
      for (int variant = 0;
           variant < (rendered.companionPadded.isNotEmpty() ? 2 : 1);
           ++variant) {
        auto info =
            sampleInfo(s, variant ? rendered.companionFrames : rendered.frames,
                       variant != 0);
        auto bytes = card::encode(info);
        auto path = child(root, ".core-manager/cache/metadata-" + s.id + "-" +
                                    String(variant) + ".info");
        durableWrite(path, bytes.getData(), bytes.getSize());
        desired[card::path(s.bank, s.slot, variant) + ".info"] = path;
      }
    } else {
      s.rendered = previous->rendered;
      s.companion = previous->companion;
      s.renderKey = previous->renderKey;
      for (const auto &path : previous->ownedPaths) {
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
          auto bytes = card::encode(info);
          auto path = child(root, ".core-manager/cache/metadata-" + s.id + "-" +
                                      String(variant) + ".info");
          durableWrite(path, bytes.getData(), bytes.getSize());
          desired[wavePath + ".info"] = path;
        }
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
  std::vector<Replacement> replacements;
  auto expected = [&](const String &path) {
    auto found = committed.fingerprints.find(path);
    return found == committed.fingerprints.end() ? String("missing")
                                                 : found->second;
  };
  completed.fingerprints = committed.fingerprints;
  for (const auto &[path, file] : desired) {
    cancelled(cancel);
    auto before = expected(path);
    require(fingerprint(child(root, path)) == before,
            "External change: " + path + ". Reload/reconcile before saving.");
    auto after = hashFile(file);
    if (after != before)
      replacements.push_back({path, file, before});
    completed.fingerprints[path] = after;
    oldOwned.erase(path);
  }
  for (const auto &path : oldOwned) {
    auto before = expected(path);
    require(fingerprint(child(root, path)) == before,
            "External change: " + path + ". Reload/reconcile before saving.");
    if (before != "missing")
      replacements.push_back({path, {}, before});
    completed.fingerprints[path] = "missing";
  }
  completed.completedRevision = completed.revision;
  cancelled(cancel);
  require(fingerprint(child(root, ".core-manager/project.json")) ==
              manifestHash,
          "Project manifest changed externally. Reconcile before saving.");
  publish("Saving");
  Transaction tx(root);
  tx.shouldCancel = cancel;
  tx.commit(replacements, completed);
  committed = completed;
  project = completed;
  manifestHash = fingerprint(child(root, ".core-manager/project.json"));
  dirty = false;
  failed = false;
  prepareVisualization();
  publish("Ready");
}
} // namespace core
