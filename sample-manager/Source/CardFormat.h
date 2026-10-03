#pragma once
#include "Common.h"
namespace core::card {
struct Slice {
  uint32_t start = 0, stop = 0;
  int8_t type = 0;
};
struct Info {
  uint32_t size = 0;
  int bpm = 120, playMode = 0, rate = 44100, channels = 1, version = 1,
      spliceTrigger = 96;
  bool oneShot = false, tempoMatch = true, spliceVariable = false;
  std::vector<Slice> slices;
  std::array<std::vector<uint32_t>, 3>
      transients; // frames at 44.1 kHz; wire uses units of 16
};
struct Wav {
  uint32_t rate = 0, channels = 0;
  uint64_t frames = 0, dataOffset = 0, dataBytes = 0;
};
Info decode(const juce::MemoryBlock &);
juce::MemoryBlock encode(const Info &);
Wav inspect(const File &);
Wav inspect(juce::InputStream &, juce::int64 fileSize);
void validatePair(const Wav &, const Info &);
juce::StringArray compatibility(const Info &);
String path(int bank, int slot, int variant = 0);
// Recognize only canonical, obsolete odd-numbered card audio/metadata paths.
bool isCompanionPath(const String &);
void writeHeader(juce::OutputStream &, uint64_t frames, int rate, int channels);
// A complete model of recognized settings. Unrecognized files are never
// owned.
struct Setting {
  String key, label;
  juce::StringArray values;
  String initial;
  bool zeptoHidden = false;
};
const std::vector<Setting> &settingDefinitions();
using Settings = std::map<String, String>;
Settings defaultSettings();
Settings readSettings(const File &, juce::StringArray &warnings);
std::map<String, bool> settingsFiles(const Settings &);
// This setting uses one replaceable text file instead of exclusive markers.
inline constexpr auto sampleCVMappingPath = "settings/sample_cv_mapping";
String sampleCVMappingContents(const Settings &);
inline constexpr auto midiChannelPath = "settings/midi_channel";
String midiChannelContents(const Settings &);
inline constexpr auto startTempoPath = "settings/start_tempo";
String startTempoContents(const Settings &);
std::map<String, String> textSettingsContents(const Settings &);
} // namespace core::card
