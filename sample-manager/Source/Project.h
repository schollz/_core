#pragma once
#include "CardFormat.h"
namespace core {
struct Marker {
  double start = 0, stop = 1;
  int type = 0;
};
enum class SpliceTimingCalculation { initialImport, evenSlices };
struct Sample {
  String id = uuid(), name, originalFilename, source, sourceHash, originalArchive,
         origin = "imported", problem;
  int bank = 0, slot = 0, channels = 1, rate = 44100, playMode = 0,
      spliceTrigger = 96;
  double sourceBpm = 120, renderBpm = 0, sourceDuration = 0;
  bool preservePitch = false, tempoMatch = true, oneShot = false,
       spliceVariable = false, protectedEntry = false;
  int64_t revision = 1, completedRevision = 0;
  String rendered, companion, renderKey;
  bool companionPending = false; // Primary is saved; background companion still needed.
  std::vector<Marker> slices{{0, 1, 0}};         // original normalized timeline
  std::vector<Marker> renderAnchors{{0, 1, 0}};  // immutable source key frames
  std::array<std::vector<double>, 3> transients; // original source seconds
  std::vector<String> ownedPaths;
  double ratio() const { return renderBpm > 0 ? sourceBpm / renderBpm : 1.; }
  void updateSpliceTrigger(SpliceTimingCalculation);
  var json() const;
  static Sample fromJson(const var &);
};
struct Project {
  String id = uuid();
  int64_t revision = 0, completedRevision = 0;
  std::vector<Sample> samples;
  card::Settings settings;
  std::map<String, String> fingerprints;
  juce::StringArray warnings;
  var json() const;
  static Project fromJson(const var &);
  Sample *find(const String &);
  const Sample *find(const String &) const;
  int freeSlot(int bank) const;
  void validate() const;
};
class History {
public:
  void accept(const Project &before, const String &label);
  bool undo(Project &);
  bool redo(Project &);
  void clear() {
    past.clear();
    future.clear();
  }
  var json(const String &projectId) const;
  void restore(const var &, const String &projectId);

private:
  struct Entry {
    Project project;
    String label;
  };
  std::vector<Entry> past, future;
};
card::Info sampleInfo(const Sample &, uint64_t renderedFrames,
                      bool companion = false);
String audioKey(const Sample &);
} // namespace core
