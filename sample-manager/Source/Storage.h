#pragma once
#include "Project.h"
namespace core {
// All disk operations run on the project worker. UI only submits immutable
// snapshots.
void durableWrite(const File &, const void *, size_t);
void durableJson(const File &, const var &);
class ProjectLock {
public:
  explicit ProjectLock(const File &root);
  ~ProjectLock();
  ProjectLock(const ProjectLock &) = delete;

private:
#if JUCE_WINDOWS
  void *handle = nullptr;
#else
  int descriptor = -1;
#endif
};
struct Replacement {
  String path;
  File staged;
  String expected;
  String contentHash; // optional planned hash of staged bytes
}; // empty staged = removal
class Transaction {
public:
  explicit Transaction(File projectRoot) : root(std::move(projectRoot)) {}
  // Test seam: inject interruption after a journaled filesystem mutation.
  std::function<void(int)> afterMutation;
  std::function<bool()> shouldCancel;
  void commit(const std::vector<Replacement> &, const Project &completed);
  static void recover(const File &);

private:
  File root;
};
class Storage {
public:
  explicit Storage(File projectRoot);
  File root;
  Project open(std::function<void(double)> progress = {});
  String importSource(const File &, String &hash);
  void savePending(const Project &);
  void duplicateTo(const File &destination);
  void cleanup(const Project &, bool recoveryHistory);
  Project adopt(bool writeManifest = true,
                std::function<void(double)> progress = {});
  void queueNameBackfill(Project &);

private:
  ProjectLock lock;
};
} // namespace core
