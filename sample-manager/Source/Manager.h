#pragma once
#include "AudioProcessing.h"
#include "Visualizer/Library.h"
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>
namespace core {
struct ManagerState {
  Project project, completedProject;
  zv::LibraryState library;
  File root;
  String status = "Open a folder to begin", error;
  bool busy = false, available = false;
  uint64_t generation = 0;
};
class Manager final : public juce::ChangeBroadcaster {
public:
  Manager();
  ~Manager() override;
  ManagerState snapshot() const;
  void open(const File &);
  void import(const juce::StringArray &, int bank);
  void edit(const String &id, const String &label,
            std::function<void(Sample &)>);
  void remove(const std::vector<String> &ids);
  void clearBank(int bank);
  void move(const std::vector<String> &ids, int bank, int delta = 0);
  void merge(const std::vector<String> &ids, int bank);
  void even(const String &id, int slices);
  void detect(const String &id, String method, double spacing);
  void settings(const card::Settings &);
  void undo();
  void redo();
  void retry();
  void duplicate(const File &);
  void cleanup();
  void reconcile();
  void applyAnalysis(const String &projectId, const String &sampleId,
                     int64_t revision, std::array<std::vector<double>, 3>);
  bool idle() const;

private:
  using Command = std::function<void()>;
  void post(Command);
  void run();
  void publish(String status, String error = {});
  void change(const String &label, const std::function<void(Project &)> &);
  void persist();
  void save(uint64_t generation);
  void prepareVisualization();
  zv::LibraryState libraryState;
  mutable std::mutex mutex;
  std::condition_variable wake;
  std::deque<Command> commands;
  ManagerState state;
  std::atomic<uint64_t> serial{0};
  std::atomic<bool> stopping{false};
  std::unique_ptr<Storage> storage;
  Project project, committed;
  History history;
  AudioProcessing audio;
  bool dirty = false, failed = false;
  bool commandRunning = false;
  String manifestHash;
  std::thread worker;
};
} // namespace core
