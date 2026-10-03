#pragma once
#include "AudioProcessing.h"
#include "Visualizer/Library.h"
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>
namespace core {
struct CompletedAudio {
  String path;
  uint64_t frames = 0;
  double rate = 0;
  double duration() const { return rate > 0 ? double(frames) / rate : 0.; }
};
struct SampleRenderStatus {
  bool pending = false, failed = false;
  double original = 0, target = 0;
  CompletedAudio completed;
  String text() const;
  String tooltip() const;
  String previewTooltip() const;
};
struct ManagerState {
  Project project, completedProject;
  // Unpadded preview headers are cached by the worker, never read by the UI.
  std::map<String, CompletedAudio> completedAudio;
  SampleRenderStatus renderStatus(const String &sampleId) const;
  bool primaryUpdateFailed = false;
  zv::LibraryState library;
  // Editor-only source peaks are available before hardware rendering/commit.
  std::map<String, std::shared_ptr<const zv::Wave>> sourceWaveforms;
  std::shared_ptr<const zv::Wave> editorWaveform(const String &sampleId) const;
  String importedSampleId;
  uint64_t importGeneration = 0;
  File root;
  String status = "Open a folder to begin", error;
  bool busy = false, available = false;
  bool canUndo = false, canRedo = false;
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
  void editMarkers(const String &id, std::vector<Marker>,
                    std::array<std::vector<double>, 3>);
  void remove(const std::vector<String> &ids);
  void clearBank(int bank);
  void move(const std::vector<String> &ids, int bank, int delta = 0);
  void merge(const std::vector<String> &ids, int bank);
  void even(const String &id, int slices);
  void detect(const String &id, int count, String method, double spacing);
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
  void change(const String &label, const std::function<void(Project &)> &,
              const std::function<void()> &onAccepted = {});
  void persist();
  void save(uint64_t generation);
  void prepareImportWaveforms();
  void prepareCompletedAudio();
  void prepareVisualization(
      std::function<void(const zv::LibraryState &)> progress = {});
  zv::LibraryState libraryState;
  std::map<String, std::shared_ptr<const zv::Wave>> sourceWaveforms;
  std::map<String, CompletedAudio> completedAudio;
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
  bool foregroundRunning = false;
  String manifestHash;
  String importedSampleId;
  uint64_t importGeneration = 0;
  std::thread worker;
};
} // namespace core
