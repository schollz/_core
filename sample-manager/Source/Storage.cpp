#include "Storage.h"
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
void copyChecked(const File &from, const File &to) {
  ensureDirectory(to.getParentDirectory());
  auto in = from.createInputStream();
  auto out = to.createOutputStream();
  require(in && out, "Cannot copy " + from.getFileName());
  out->setPosition(0);
  out->truncate();
  require(out->writeFromInputStream(*in, -1) == from.getSize(),
          "Incomplete copy");
  out->flush();
  require(out->getStatus().wasOk(),
          "Copy failed: " + out->getStatus().getErrorMessage());
  require(hashFile(from) == hashFile(to), "Copy verification failed");
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
    require(fingerprint(dest) == r.expected,
            "External change detected: " + r.path +
                ". Reload/reconcile before saving.");
    var e = object();
    put(e, "path", r.path);
    put(e, "existed", dest.existsAsFile());
    put(e, "before", r.expected);
    put(e, "remove", r.staged == File());
    put(e, "after",
        r.staged == File() ? String("missing") : hashFile(r.staged));
    if (dest.existsAsFile()) {
      auto backup = dir.getChildFile("backup/" + String(int(n)));
      copyChecked(dest, backup);
      require(hashFile(backup) == r.expected,
              "File changed while preparing backup");
    }
    if (r.staged != File())
      copyChecked(r.staged, dir.getChildFile("new/" + String(int(n))));
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
      require(fingerprint(dest) == items[n].expected,
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
    rollback(root, dir, journal);
    throw;
  }
  auto history = child(root, ".core-manager/recovery/" + dir.getFileName());
  require(history.getParentDirectory().createDirectory().wasOk(),
          "Cannot create recovery folder");
  renameFile(dir, history);
}
void Transaction::recover(const File &root) {
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
Project Storage::open() {
  Transaction::recover(root);
  auto file = child(root, ".core-manager/project.json");
  Project p =
      file.existsAsFile() ? Project::fromJson(parseJson(file)) : adopt();
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
  for (const auto &[path, hash] : p.fingerprints)
    if (fingerprint(child(root, path)) != hash)
      p.warnings.addIfNotAlreadyThere("External change: " + path +
                                      ". Reload/reconcile before saving.");
  for (const auto &s : p.samples)
    if (!s.protectedEntry)
      require(child(root, s.source).existsAsFile(),
              "Missing original: " + s.name);
  return p;
}
Project Storage::adopt(bool writeManifest) {
  Project p;
  p.settings = card::readSettings(root, p.warnings);
  for (int b = 0; b < 16; ++b)
    for (int slot = 0; slot < 16; ++slot) {
      auto relative = card::path(b, slot);
      auto wav = child(root, relative);
      auto info = child(root, relative + ".info");
      bool occupied = wav.existsAsFile() || info.existsAsFile();
      for (int variant = 1; variant <= 9 && !occupied; ++variant)
        occupied =
            child(root, card::path(b, slot, variant)).existsAsFile() ||
            child(root, card::path(b, slot, variant) + ".info").existsAsFile();
      if (!occupied)
        continue;
      Sample s;
      s.bank = b;
      s.slot = slot;
      s.name = "Recovered card audio " + String(slot + 1);
      s.origin = "recovered card audio";
      for (int v = 0; v <= 9; ++v)
        for (const auto &suffix : juce::StringArray{"", ".info"}) {
          auto path = card::path(b, slot, v) + suffix;
          auto f = child(root, path);
          if (f.existsAsFile()) {
            s.ownedPaths.push_back(path);
            p.fingerprints[path] = hashFile(f);
          }
        }
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
            require(child(root, card::path(b, slot, v) + ".info")
                        .loadFileAsData(meta),
                    "Companion metadata missing");
            card::validatePair(card::inspect(other), card::decode(meta));
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
  p.validate();
  if (writeManifest)
    durableJson(child(root, ".core-manager/project.json"), p.json());
  return p;
}
void Storage::duplicateTo(const File &destination) {
  require(destination != root && !destination.isAChildOf(root) &&
              !root.isAChildOf(destination),
          "Choose a separate project folder");
  require(!destination.exists() ||
              destination.findChildFiles(File::findFilesAndDirectories, false)
                  .isEmpty(),
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
