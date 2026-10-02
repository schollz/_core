#include "Storage.h"
#include "NameMetadata.h"
#include "Parallel.h"
#if JUCE_WINDOWS
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#endif
namespace core {
namespace {
void syncDirectory(const File &f) {
#if !JUCE_WINDOWS
  int fd = ::open(f.getFullPathName().toRawUTF8(), O_RDONLY);
  require(fd >= 0, "Cannot open directory for sync");
  int result = ::fsync(fd);
  ::close(fd);
  require(result == 0, "Cannot sync directory: " + f.getFullPathName());
#else
  juce::ignoreUnused(f);
#endif
}
void renameFile(const File &a, const File &b) {
  ensureDirectory(b.getParentDirectory());
#if JUCE_WINDOWS
  require(MoveFileExW(a.getFullPathName().toWideCharPointer(),
                      b.getFullPathName().toWideCharPointer(),
                      MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0,
          "Cannot replace destination");
#else
  std::error_code ec;
  std::filesystem::rename(
      std::filesystem::u8path(a.getFullPathName().toStdString()),
      std::filesystem::u8path(b.getFullPathName().toStdString()), ec);
  require(!ec, "Cannot replace " + b.getFullPathName() + ": " +
                   String(ec.message()));
#endif
  syncDirectory(b.getParentDirectory());
  if (a.getParentDirectory() != b.getParentDirectory())
    syncDirectory(a.getParentDirectory());
}
void removeFile(const File &f) {
  require(!f.exists() || f.deleteFile(),
          "Cannot remove " + f.getFullPathName());
  syncDirectory(f.getParentDirectory());
}
void copyChecked(const File &from, const File &to,
                 const std::function<bool()> &cancel = {}) {
  ensureDirectory(to.getParentDirectory());
  auto in = from.createInputStream();
  auto out = to.createOutputStream();
  require(in && out, "Cannot copy " + from.getFileName());
  out->setPosition(0);
  out->truncate();
  std::array<char, 256 * 1024> buffer;
  juce::int64 copied = 0;
  while (!in->isExhausted()) {
    cancelled(cancel);
    auto count = in->read(buffer.data(), int(buffer.size()));
    require(count > 0 && out->write(buffer.data(), size_t(count)), "Incomplete copy");
    copied += count;
  }
  require(in->getStatus().wasOk() && copied == from.getSize(), "Incomplete copy");
  out->flush();
  require(out->getStatus().wasOk(),
          "Copy failed: " + out->getStatus().getErrorMessage());
  require(hashFile(from, cancel) == hashFile(to, cancel), "Copy verification failed");
  syncDirectory(to.getParentDirectory());
}
void rollback(const File &root, const File &dir, const var &j) {
  auto *entries = j["entries"].getArray();
  require(entries, "Invalid recovery journal");
  for (int n = entries->size() - 1; n >= 0; --n) {
    const auto &e = entries->getReference(n);
    auto dest = child(root, e["path"].toString());
    auto backup = dir.getChildFile("backup/" + String(n));
    // Only restore files that are still our replacement. Preserve external
    // edits.
    auto current = fingerprint(dest);
    if (current == e["before"].toString())
      continue;
    require(current == e["after"].toString(),
            "Recovery conflict: " + e["path"].toString() +
                " changed outside the app. Backups are retained; reconcile "
                "before retrying.");
    // Backup copies are complete before the first replacement. Restoring is
    // repeatable.
    if (bool(e["existed"])) {
      require(backup.existsAsFile() &&
                  hashFile(backup) == e["before"].toString(),
              "Recovery backup missing or damaged");
      auto temp = dest.getSiblingFile(dest.getFileName() + ".core-restore");
      copyChecked(backup, temp);
      renameFile(temp, dest);
    } else if (dest.existsAsFile())
      removeFile(dest);
  }
  var done = j.clone();
  put(done, "state", "rolled-back");
  durableJson(dir.getChildFile("journal.json"), done);
}
} // namespace
void durableWrite(const File &target, const void *bytes, size_t size) {
  diagnostics::log("STORAGE", "Write " + target.getFullPathName() +
                                  " bytes=" + String(juce::int64(size)));
  ensureDirectory(target.getParentDirectory());
  auto temp = target.getSiblingFile(target.getFileName() + ".tmp-" + uuid());
  {
    auto out = temp.createOutputStream();
    require(out != nullptr, "Cannot write " + target.getFullPathName());
    require(out->write(bytes, size), "Write failed (check free disk space)");
    out->flush();
    require(out->getStatus().wasOk(), out->getStatus().getErrorMessage());
  }
#if JUCE_WINDOWS
  require(MoveFileExW(temp.getFullPathName().toWideCharPointer(),
                      target.getFullPathName().toWideCharPointer(),
                      MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0,
          "Cannot replace saved file");
#else
  renameFile(temp, target);
#endif
}
void durableJson(const File &f, const var &v) {
  auto s = juce::JSON::toString(v);
  durableWrite(f, s.toRawUTF8(), size_t(s.getNumBytesAsUTF8()));
}
ProjectLock::ProjectLock(const File &root) {
  require(root.isDirectory(), "Project folder is unavailable");
  auto folder = child(root, ".core-manager");
  require(folder.createDirectory().wasOk(), "Project folder is read-only");
  auto file = child(root, ".core-manager/writer.lock");
#if JUCE_WINDOWS
  handle = CreateFileW(file.getFullPathName().toWideCharPointer(),
                       GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_ALWAYS,
                       FILE_ATTRIBUTE_NORMAL, nullptr);
  require(handle != INVALID_HANDLE_VALUE,
          "This project already has a writer, or cannot be written");
#else
  descriptor =
      ::open(file.getFullPathName().toRawUTF8(), O_CREAT | O_RDWR, 0600);
  require(descriptor >= 0, "Cannot open project lock");
  if (::flock(descriptor, LOCK_EX | LOCK_NB) != 0) {
    ::close(descriptor);
    descriptor = -1;
    throw std::runtime_error("This project is already open for writing");
  }
#endif
}
ProjectLock::~ProjectLock() {
#if JUCE_WINDOWS
  if (handle && handle != INVALID_HANDLE_VALUE)
    CloseHandle(handle);
#else
  if (descriptor >= 0) {
    ::flock(descriptor, LOCK_UN);
    ::close(descriptor);
  }
#endif
}
void Transaction::commit(const std::vector<Replacement> &requested,
                         const Project &completed) {
  diagnostics::Scope trace("STORAGE", "Commit revision=" +
      String(juce::int64(completed.revision)) + " replacements=" +
      String(int(requested.size())));
  completed.validate();
  auto dir = child(root, ".core-manager/transactions/" + uuid());
  ensureDirectory(dir);
  auto items = requested;
  auto stagedManifest = dir.getChildFile("manifest.json");
  durableJson(stagedManifest, completed.json());
  items.push_back({".core-manager/project.json", stagedManifest,
                   fingerprint(child(root, ".core-manager/project.json"))});
  std::set<String> unique;
  var journal = object();
  juce::Array<var> entries;
  // Prepare all replacements and durable backups before touching active
  // hardware files.
  for (size_t n = 0; n < items.size(); ++n) {
    cancelled(shouldCancel);
    auto &r = items[n];
    require(unique.insert(r.path).second, "Duplicate transaction destination");
    auto dest = child(root, r.path);
    require(fingerprint(dest, shouldCancel) == r.expected,
            "External change detected: " + r.path + ". Reload/reconcile before saving.");
    var e = object();
    put(e, "path", r.path);
    put(e, "existed", dest.existsAsFile());
    put(e, "before", r.expected);
    put(e, "remove", r.staged == File());
    auto after = r.staged == File() ? String("missing") : hashFile(r.staged, shouldCancel);
    require(r.contentHash.isEmpty() || after == r.contentHash,
            "Staged file changed while preparing save: " + r.path);
    put(e, "after", after);
    if (dest.existsAsFile()) {
      auto backup = dir.getChildFile("backup/" + String(int(n)));
      copyChecked(dest, backup, shouldCancel);
      require(hashFile(backup, shouldCancel) == r.expected, "File changed while preparing backup");
    }
    if (r.staged != File()) {
      auto staged = dir.getChildFile("new/" + String(int(n)));
      copyChecked(r.staged, staged, shouldCancel);
      require(hashFile(staged, shouldCancel) == after,
              "Staged file changed during preparation: " + r.path);
    }
    entries.add(e);
  }
  put(journal, "state", "prepared");
  put(journal, "entries", entries);
  put(journal, "revision", juce::int64(completed.revision));
  durableJson(dir.getChildFile("journal.json"), journal);
  try {
    for (size_t n = 0; n < items.size(); ++n) {
      cancelled(shouldCancel);
      auto dest = child(root, items[n].path);
      diagnostics::log("STORAGE", "Commit file " + items[n].path);
      require(fingerprint(dest, shouldCancel) == items[n].expected,
              "External change detected during save: " + items[n].path);
      auto staged = dir.getChildFile("new/" + String(int(n)));
      if (staged.existsAsFile()) {
        renameFile(staged, dest);
      } else if (dest.existsAsFile())
        removeFile(dest);
      if (afterMutation)
        afterMutation(int(n));
    }
    cancelled(shouldCancel);
    put(journal, "state", "complete");
    durableJson(dir.getChildFile("journal.json"), journal);
  } catch (...) {
    diagnostics::log("STORAGE", "Commit interrupted; rolling back " + dir.getFullPathName());
    rollback(root, dir, journal);
    throw;
  }
  auto history = child(root, ".core-manager/recovery/" + dir.getFileName());
  require(history.getParentDirectory().createDirectory().wasOk(),
          "Cannot create recovery folder");
  renameFile(dir, history);
}
void Transaction::recover(const File &root) {
  diagnostics::Scope trace("STORAGE", "Recover " + root.getFullPathName());
  auto transactions = child(root, ".core-manager/transactions");
  for (auto dir : transactions.findChildFiles(File::findDirectories, false)) {
    require(!dir.isSymbolicLink(), "Unsafe transaction path");
    auto file = dir.getChildFile("journal.json");
    if (!file.existsAsFile())
      continue;
    auto j = parseJson(file);
    auto state = j["state"].toString();
    require(state == "prepared" || state == "complete" ||
                state == "rolled-back",
            "Unknown transaction state");
    if (state == "prepared")
      rollback(root, dir, j);
    auto history = child(root, ".core-manager/recovery/" + dir.getFileName());
    require(history.getParentDirectory().createDirectory().wasOk(),
            "Cannot create recovery folder");
    renameFile(dir, history);
  }
}
Storage::Storage(File f) : root(std::move(f)), lock(root) {
  for (const auto *name : {"sources", "cache", "transactions", "recovery"})
    require(
        child(root, String(".core-manager/") + name).createDirectory().wasOk(),
        "Cannot prepare project storage");
}
String Storage::importSource(const File &input, String &hash) {
  hash = hashFile(input);
  auto relative =
      ".core-manager/sources/" + hash + input.getFileExtension().toLowerCase();
  auto dest = child(root, relative);
  if (!dest.existsAsFile()) {
    auto temp = dest.getSiblingFile(dest.getFileName() + ".import-" + uuid());
    copyChecked(input, temp);
    require(hashFile(temp) == hash, "Source changed during import");
    renameFile(temp, dest);
  } else
    require(hashFile(dest) == hash, "Immutable source has been modified");
  return relative;
}
void Storage::savePending(const Project &p) {
  p.validate();
  auto manifest = child(root, ".core-manager/project.json");
  require(root.isDirectory() && manifest.existsAsFile(),
          "Project volume is unavailable; pending edits are retained locally");
  require(parseJson(manifest)["id"].toString() == p.id,
          "A different project is mounted at this path; reconnect the original "
          "folder");
  durableJson(child(root, ".core-manager/pending.json"), p.json());
}
Project Storage::open(std::function<void(double)> progress) {
  diagnostics::Scope trace("STORAGE", "Open " + root.getFullPathName());
  auto report = [&](double fraction) {
    if (progress)
      progress(fraction);
  };
  report(0.);
  Transaction::recover(root);
  report(0.05);
  auto file = child(root, ".core-manager/project.json");
  Project p = file.existsAsFile()
                  ? Project::fromJson(parseJson(file))
                  : adopt(true, [&](double fraction) {
                      report(0.05 + 0.45 * fraction);
                    });
  report(0.50);
  auto pending = child(root, ".core-manager/pending.json");
  if (pending.existsAsFile()) {
    auto next = Project::fromJson(parseJson(pending));
    if (next.id == p.id && next.revision > p.completedRevision)
      p = std::move(next);
  }
  auto local = stateRoot().getChildFile("recovery/" + p.id + ".json");
  if (local.existsAsFile()) {
    auto next = Project::fromJson(parseJson(local));
    if (next.id == p.id && next.revision > p.revision) {
      p = std::move(next);
      p.warnings.add("Recovered pending edits from the local recovery copy.");
    }
  }
  if (p.settings.find("midi_channel") == p.settings.end()) {
    auto cardSettings = card::readSettings(root, p.warnings);
    auto found = cardSettings.find("midi_channel");
    p.settings["midi_channel"] = found == cardSettings.end() ? String("1") : found->second;
    p.fingerprints[card::midiChannelPath] = fingerprint(child(root, card::midiChannelPath));
    if (child(root, "midi_channel").exists())
      p.fingerprints["midi_channel"] = fingerprint(child(root, "midi_channel"));
  }
  report(0.55);
  std::vector<std::pair<String, String>> checks(p.fingerprints.begin(),
                                                p.fingerprints.end());
  std::vector<String> hashes(checks.size());
  {
    diagnostics::Scope checksTrace("STORAGE", "Verify file fingerprints");
    parallelFor(
        checks.size(),
        [&](size_t index) {
          const auto &path = checks[index].first;
          hashes[index] = fingerprint(child(root, path));
        },
        [&](size_t completed) {
          report(0.55 + 0.40 * double(completed) / double(checks.size()));
        });
  }
  for (size_t index = 0; index < checks.size(); ++index) {
    const auto &[path, expected] = checks[index];
    if (hashes[index] != expected)
      p.warnings.addIfNotAlreadyThere("External change: " + path +
                                      ". Reload/reconcile before saving.");
  }
  report(0.95);
  // Older manifests protected the entire sample for companion-only failures.
  // Retry only when the owned files still match their recorded fingerprints;
  // external changes continue through the normal reconciliation workflow.
  for (auto &s : p.samples) {
    if (!s.protectedEntry ||
        s.problem != "WAV and metadata disagree about format or circular padding" ||
        s.source.isEmpty() || !child(root, s.source).existsAsFile())
      continue;
    bool unchanged = true;
    for (const auto &path : s.ownedPaths) {
      auto expected = p.fingerprints.find(path);
      if (expected == p.fingerprints.end() ||
          fingerprint(child(root, path)) != expected->second)
        unchanged = false;
    }
    if (!unchanged)
      continue;
    try {
      const auto primary = card::path(s.bank, s.slot);
      juce::MemoryBlock bytes;
      require(child(root, primary + ".info").loadFileAsData(bytes), "Missing .info");
      card::validatePair(card::inspect(child(root, primary)), card::decode(bytes));
      s.protectedEntry = false;
      p.warnings.removeString("Bank " + String(s.bank + 1) + " slot " +
                              String(s.slot + 1) + ": " + s.problem);
      s.problem.clear();
      s.companion.clear();
      s.companionPending = !(s.oneShot && !s.tempoMatch);
    } catch (const std::exception &) {
      // A damaged primary still needs explicit recovery.
    }
  }
  // Re-evaluate persisted transient warnings after releasing protected entries.
  p = Project::fromJson(p.json());
  size_t checked = 0;
  for (const auto &s : p.samples) {
    if (!s.protectedEntry)
      require(child(root, s.source).existsAsFile(),
              "Missing original: " + s.name);
    report(0.95 + 0.05 * double(++checked) / double(p.samples.size()));
  }
  report(1.);
  queueNameBackfill(p);
  diagnostics::log("STORAGE",
                   "Loaded samples=" + String(int(p.samples.size())) +
                       " warnings=" + p.warnings.joinIntoString(" | "));
  return p;
}
void Storage::queueNameBackfill(Project &p) {
  bool needsSave = false;
  for (const auto &s : p.samples) {
    auto primary = p.fingerprints.find(card::path(s.bank, s.slot));
    auto metadata = names::read(
        root, s.bank, s.slot,
        primary == p.fingerprints.end() ? String() : primary->second);
    if (metadata.status == names::Metadata::invalid)
      p.warnings.addIfNotAlreadyThere(metadata.warning);
    if (!s.protectedEntry &&
        (metadata.status == names::Metadata::missing ||
         (metadata.status == names::Metadata::valid &&
          (metadata.name != s.name ||
           metadata.originalFilename != s.originalFilename ||
           std::find(s.ownedPaths.begin(), s.ownedPaths.end(),
                     names::path(s.bank, s.slot)) == s.ownedPaths.end()))))
      needsSave = true;
  }
  // A normal pending save writes the names with the completed audio. Keep the
  // sample revisions unchanged so backfill alone cannot rewrite WAV or .info.
  if (needsSave && p.revision == p.completedRevision)
    ++p.revision;
}
Project Storage::adopt(bool writeManifest,
                       std::function<void(double)> progress) {
  diagnostics::Scope trace("STORAGE", "Adopt card " + root.getFullPathName());
  Project p;
  p.settings = card::readSettings(root, p.warnings);
  // Enumerate each bank once instead of stat-ing every possible variant of
  // every empty slot. Unknown files stay outside the ownership set.
  std::set<String> present;
  for (int b = 0; b < 16; ++b) {
    const auto bank = "bank" + String(b + 1);
    const auto directory = child(root, bank);
    if (directory.isDirectory())
      for (const auto &file : directory.findChildFiles(File::findFiles, false))
        present.insert(bank + "/" + file.getFileName());
  }
  std::vector<String> paths;
  for (int b = 0; b < 16; ++b)
    for (int slot = 0; slot < 16; ++slot)
      for (int variant = 0; variant <= 9; ++variant)
        for (const auto &suffix : juce::StringArray{"", ".info"}) {
          auto path = card::path(b, slot, variant) + suffix;
          if (present.count(path))
            paths.push_back(path);
        }
  std::vector<String> hashes(paths.size());
  {
    diagnostics::Scope checksTrace("STORAGE", "Hash adopted card files");
    parallelFor(
        paths.size(),
        [&](size_t index) {
          hashes[index] = hashFile(child(root, paths[index]));
        },
        [&](size_t completed) {
          if (progress)
            progress(0.45 * double(completed) / double(paths.size()));
        });
  }
  for (size_t index = 0; index < paths.size(); ++index)
    p.fingerprints[paths[index]] = hashes[index];
  for (int b = 0; b < 16; ++b)
    for (int slot = 0; slot < 16; ++slot) {
      if (progress)
        progress(0.45 + 0.55 * double(b * 16 + slot) / 256.);
      auto relative = card::path(b, slot);
      bool occupied =
          present.count(relative) || present.count(relative + ".info");
      for (int variant = 1; variant <= 9 && !occupied; ++variant)
        occupied = present.count(card::path(b, slot, variant)) ||
                   present.count(card::path(b, slot, variant) + ".info");
      if (!occupied)
        continue;
      auto wav = child(root, relative);
      auto info = child(root, relative + ".info");
      diagnostics::log("STORAGE", "Adopt bank=" + String(b + 1) +
                                      " slot=" + String(slot + 1));
      Sample s;
      s.bank = b;
      s.slot = slot;
      s.name = "Recovered card audio " + String(slot + 1);
      s.origin = "recovered card audio";
      for (int v = 0; v <= 9; ++v)
        for (const auto &suffix : juce::StringArray{"", ".info"}) {
          auto path = card::path(b, slot, v) + suffix;
          if (present.count(path))
            s.ownedPaths.push_back(path);
        }
      auto namePath = names::path(b, slot);
      auto primaryHash = p.fingerprints.find(relative);
      auto metadata = names::read(
          root, b, slot,
          primaryHash == p.fingerprints.end() ? String() : primaryHash->second);
      if (metadata.status == names::Metadata::valid) {
        s.originalFilename = metadata.originalFilename;
        if (metadata.name.isNotEmpty())
          s.name = metadata.name;
        else if (metadata.originalFilename.isNotEmpty())
          s.name = metadata.originalFilename;
        s.ownedPaths.push_back(namePath);
        p.fingerprints[namePath] = metadata.fingerprint;
      } else if (metadata.status == names::Metadata::invalid)
        p.warnings.add(metadata.warning);
      try {
        juce::MemoryBlock bytes;
        require(info.loadFileAsData(bytes), "Missing .info");
        auto i = card::decode(bytes);
        auto w = card::inspect(wav);
        card::validatePair(w, i);
        s.channels = i.channels;
        s.rate = i.rate;
        s.sourceBpm = juce::jlimit(30, 300, i.bpm);
        s.playMode = i.playMode;
        s.oneShot = i.oneShot;
        s.tempoMatch = i.tempoMatch;
        s.spliceTrigger = i.spliceTrigger;
        s.spliceVariable = i.spliceVariable;
        s.sourceDuration = double(i.size) / (i.rate * i.channels * 2);
        s.slices.clear();
        for (auto m : i.slices)
          s.slices.push_back(
              {double(m.start) / i.size, double(m.stop) / i.size, m.type});
        s.renderAnchors = s.slices;
        for (size_t lane = 0; lane < 3; ++lane)
          for (auto t : i.transients[lane])
            s.transients[lane].push_back(double(t) / 44100.);
        auto recovered =
            child(root, ".core-manager/cache/recovered-" + s.id + ".wav");
        {
          auto in = wav.createInputStream();
          auto out = recovered.createOutputStream();
          require(in && out, "Cannot recover card audio");
          card::writeHeader(*out, i.size / (i.channels * 2), i.rate,
                            i.channels);
          in->setPosition(juce::int64(w.dataOffset +
                                      uint64_t(i.rate / 2) * i.channels * 2));
          require(out->writeFromInputStream(*in, i.size) == i.size,
                  "Truncated recovered audio");
          out->flush();
          require(out->getStatus().wasOk(), "Cannot write recovered audio");
        }
        s.source = importSource(recovered, s.sourceHash);
        s.rendered = s.source;
        s.renderKey = audioKey(s);
        s.completedRevision = s.revision;
        for (int v = 1; v < 2; ++v) {
          auto other = child(root, card::path(b, slot, v));
          if (other.existsAsFile()) {
            juce::MemoryBlock meta;
            try {
              require(child(root, card::path(b, slot, v) + ".info")
                          .loadFileAsData(meta),
                      "Companion metadata missing");
              card::validatePair(card::inspect(other), card::decode(meta));
            } catch (const std::exception &) {
              // The primary has already been validated and recovered. Rebuild
              // a legacy/broken companion from it instead of protecting audio
              // that is otherwise usable. The worker replaces it transactionally.
              s.companionPending = !(s.oneShot && !s.tempoMatch);
            }
          }
        }
        for (const auto &warning : card::compatibility(i))
          p.warnings.add(s.name + ": " + warning);
      } catch (const std::exception &e) {
        s.protectedEntry = true;
        s.problem = e.what();
        p.warnings.add("Bank " + String(b + 1) + " slot " + String(slot + 1) +
                       ": " + s.problem);
      }
      p.samples.push_back(std::move(s));
    }
  for (const auto &[path, enabled] : card::settingsFiles(p.settings))
    p.fingerprints[path] = fingerprint(child(root, path));
  for (const auto &[path, contents] : card::textSettingsContents(p.settings))
    p.fingerprints[path] = fingerprint(child(root, path));
  if (child(root, "midi_channel").exists())
    p.fingerprints["midi_channel"] = fingerprint(child(root, "midi_channel"));
  if (child(root, "sample_cv_mapping").exists())
    p.fingerprints["sample_cv_mapping"] = fingerprint(child(root, "sample_cv_mapping"));
  p.validate();
  if (writeManifest) {
    const auto settingsFolder = child(root, "settings");
    if (p.samples.empty() && present.empty() && !settingsFolder.existsAsFile() &&
        !child(root, "sample_cv_mapping").exists() &&
        !child(root, "midi_channel").exists() &&
        settingsFolder.findChildFiles(File::findFiles, true).isEmpty()) {
      diagnostics::Scope defaultsTrace("STORAGE", "Initialize default settings");
      p.settings = card::defaultSettings();
      auto marker = child(root, ".core-manager/cache/empty-marker");
      durableWrite(marker, "", 0);
      const auto emptyHash = hashFile(marker);
      std::vector<Replacement> replacements;
      for (const auto &[path, enabled] : card::settingsFiles(p.settings)) {
        p.fingerprints[path] = enabled ? emptyHash : String("missing");
        if (enabled)
          replacements.push_back({path, marker, "missing", emptyHash});
      }
      for (const auto &[path, content] : card::textSettingsContents(p.settings)) {
        auto textFile = child(root, ".core-manager/cache/" + path.fromLastOccurrenceOf("/", false, false));
        durableWrite(textFile, content.toRawUTF8(), size_t(content.getNumBytesAsUTF8()));
        const auto textHash = hashFile(textFile);
        p.fingerprints[path] = textHash;
        replacements.push_back({path, textFile, "missing", textHash});
      }
      // Settings and the first manifest become durable together. Recovery can
      // retry initialization even if rollback left empty settings directories.
      Transaction tx(root);
      tx.commit(replacements, p);
    } else
      durableJson(child(root, ".core-manager/project.json"), p.json());
  }
  if (progress)
    progress(1.);
  return p;
}
void Storage::duplicateTo(const File &destination) {
  require(destination != root && !destination.isAChildOf(root) && !root.isAChildOf(destination),
          "Choose a separate project folder");
  require(!destination.exists() ||
              destination.findChildFiles(File::findFilesAndDirectories, false).isEmpty(),
          "Duplicate destination must be empty");
  require(destination.createDirectory().wasOk(),
          "Cannot create duplicate folder");
  require(destination.getChildFile(".core-manager").createDirectory().wasOk(),
          "Cannot initialize duplicate project storage");
  for (const auto &f : root.findChildFiles(File::findFiles, true)) {
    auto relative = f.getRelativePathFrom(root).replaceCharacter('\\', '/');
    if (relative == ".core-manager/writer.lock")
      continue;
    require(!f.isSymbolicLink(), "Cannot duplicate symbolic links");
    copyChecked(f, child(destination, relative));
  }
  // A duplicate is a separate project. Local recovery and job identities must
  // not collide.
  auto manifest = child(destination, ".core-manager/project.json");
  auto copy = Project::fromJson(parseJson(manifest));
  copy.id = uuid();
  durableJson(manifest, copy.json());
  auto pending = child(destination, ".core-manager/pending.json");
  if (pending.existsAsFile()) {
    auto p = Project::fromJson(parseJson(pending));
    p.id = copy.id;
    durableJson(pending, p.json());
  }
  auto historyFile = child(destination, ".core-manager/history.json");
  if (historyFile.existsAsFile()) {
    auto history = parseJson(historyFile);
    put(history, "projectId", copy.id);
    for (const auto *name : {"past", "future"})
      if (auto *entries = history[name].getArray())
        for (auto &entry : *entries) {
          auto project = entry["project"];
          put(project, "id", copy.id);
          put(entry, "project", project);
        }
    durableJson(historyFile, history);
  }
}
void Storage::cleanup(const Project &p, bool history) {
  require(p.revision == p.completedRevision, "Finish saving before cleanup");
  if (!history)
    return; // Undo and recovery may reference older originals: retain them
            // together.
  require(child(root, ".core-manager/recovery").deleteRecursively(),
          "Cannot clear recovery history");
  std::set<String> used;
  for (const auto &s : p.samples) {
    used.insert(s.source);
    used.insert(s.originalArchive);
  }
  for (const auto &f : child(root, ".core-manager/sources")
                           .findChildFiles(File::findFiles, false))
    if (!used.count(f.getRelativePathFrom(root).replaceCharacter('\\', '/')))
      removeFile(f);
}
} // namespace core
