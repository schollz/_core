#include "CardFormat.h"
#include <cstring>
#include <limits>
namespace core::card {
namespace {
uint16_t u16(const uint8_t *p) { return uint16_t(p[0] | uint16_t(p[1]) << 8); }
uint32_t u32(const uint8_t *p) {
  return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 |
         uint32_t(p[3]) << 24;
}
void basic(const Info &i) {
  require(i.size > 0 && i.size <= INT32_MAX &&
              (i.channels == 1 || i.channels == 2) &&
              i.size % (i.channels * 2) == 0,
          "Invalid sample byte size");
  require(i.channels == 1 || i.channels == 2, "Unsupported channel count");
  require(i.rate == 44100 || i.rate == 88200, "Unsupported card sample rate");
  require(i.version == 0 || i.version == 1, "Unsupported metadata version");
  require(i.bpm >= 0 && i.bpm <= 510 && i.playMode >= 0 && i.playMode <= 4,
          "Invalid BPM or playback mode");
  require(i.spliceTrigger >= 2 && i.spliceTrigger <= 32767,
          "Invalid splice trigger");
  require(!i.slices.empty() && i.slices.size() <= 255,
          juce::String::fromUTF8("Hardware supports 1–255 slices"));
  for (const auto &s : i.slices)
    require(
        s.start < s.stop && s.stop <= i.size &&
            s.start % (i.channels * 2) == 0 && s.stop % (i.channels * 2) == 0,
        "Slice boundary is outside the sample or not aligned to four bytes");
}
} // namespace
Info decode(const juce::MemoryBlock &b) {
  require(b.getSize() >= 11, "Truncated metadata");
  auto p = static_cast<const uint8_t *>(b.getData());
  Info i;
  i.size = u32(p);
  auto flags = u32(p + 4);
  auto splice = u16(p + 8);
  require((flags >> 23) == 0, "Unsupported metadata flags");
  i.bpm = int(flags & 511);
  i.playMode = int((flags >> 9) & 7);
  i.oneShot = (flags & 4096) != 0;
  i.tempoMatch = (flags & 8192) != 0;
  i.rate = (flags & 16384) ? 88200 : 44100;
  i.channels = (flags & 32768) ? 2 : 1;
  i.version = int((flags >> 16) & 127);
  i.spliceTrigger = splice & 32767;
  i.spliceVariable = (splice & 32768) != 0;
  auto n = size_t(p[10]);
  size_t end = 11 + 9 * n;
  require(b.getSize() >= end, "Truncated slices");
  for (size_t j = 0; j < n; ++j)
    i.slices.push_back({u32(p + 11 + j * 4), u32(p + 11 + n * 4 + j * 4),
                        int8_t(p[11 + n * 8 + j])});
  basic(i);
  if (i.version == 1) {
    require(b.getSize() >= end + 6, "Missing transient lane counts");
    std::array<uint16_t, 3> counts{u16(p + end), u16(p + end + 2),
                                   u16(p + end + 4)};
    end += 6;
    for (size_t lane = 0; lane < 3; ++lane) {
      require(b.getSize() >= end + size_t(counts[lane]) * 2,
              "Truncated transients");
      for (unsigned j = 0; j < counts[lane]; ++j) {
        i.transients[lane].push_back(uint32_t(u16(p + end)) * 16);
        end += 2;
      }
    }
  }
  require(end == b.getSize(),
          "Unrecognized metadata extension; preserve this entry");
  return i;
}
juce::StringArray compatibility(const Info &i) {
  juce::StringArray warnings;
  if (i.slices.size() > 255)
    warnings.add("The device supports at most 255 slices.");
  for (size_t l = 0; l < 3; ++l) {
    if (i.transients[l].size() > 16)
      warnings.add("Transient lane " + String(int(l + 1)) +
                   " has more than 16 markers.");
    for (auto frame : i.transients[l])
      // The counted wire value zero is valid, including onsets in the first
      // 16 frames. Preserve it: firmware can use it for a loop-start trigger.
      if (frame / 16 > 65535) {
        warnings.add("Transient lane " + String(int(l + 1)) +
                     " has a position outside the device's encoding range.");
        break;
      }
  }
  return warnings;
}
juce::MemoryBlock encode(const Info &i) {
  basic(i);
  for (auto s : i.slices)
    require(s.start % 4 == 0 && s.stop % 4 == 0,
            "Export slice offsets must be aligned to four bytes");
  require(compatibility(i).isEmpty(), compatibility(i).joinIntoString(" "));
  juce::MemoryOutputStream out;
  uint32_t f = uint32_t(i.bpm) | uint32_t(i.playMode) << 9 |
               uint32_t(i.oneShot) << 12 | uint32_t(i.tempoMatch) << 13 |
               uint32_t(i.rate == 88200) << 14 |
               uint32_t(i.channels == 2) << 15 | uint32_t(i.version) << 16;
  out.writeInt(int(i.size));
  out.writeInt(int(f));
  out.writeShort(short(i.spliceTrigger | (i.spliceVariable ? 32768 : 0)));
  out.writeByte(char(i.slices.size()));
  for (auto s : i.slices)
    out.writeInt(int(s.start));
  for (auto s : i.slices)
    out.writeInt(int(s.stop));
  for (auto s : i.slices)
    out.writeByte(char(s.type));
  if (i.version == 1) {
    for (const auto &lane : i.transients)
      out.writeShort(short(lane.size()));
    for (const auto &lane : i.transients)
      for (auto frame : lane)
        out.writeShort(short(frame / 16));
  }
  return out.getMemoryBlock();
}
Wav inspect(const File &f) {
  auto in = f.createInputStream();
  require(in != nullptr, "Cannot read WAV");
  return inspect(*in, f.getSize());
}
Wav inspect(juce::InputStream &stream, juce::int64 fileSize) {
  auto *in = &stream;
  uint8_t h[12];
  require(in->read(h, 12) == 12 && std::memcmp(h, "RIFF", 4) == 0 &&
              std::memcmp(h + 8, "WAVE", 4) == 0,
          "Expected RIFF WAV");
  auto end = uint64_t(u32(h + 4)) + 8;
  require(end == uint64_t(fileSize), "Truncated WAV or trailing data");
  Wav w;
  bool fmt = false, data = false;
  while (uint64_t(in->getPosition()) + 8 <= end) {
    uint8_t c[8];
    require(in->read(c, 8) == 8, "Truncated WAV chunk");
    auto len = uint64_t(u32(c + 4)), pos = uint64_t(in->getPosition());
    require(len <= end - pos, "WAV chunk exceeds file");
    if (std::memcmp(c, "fmt ", 4) == 0) {
      uint8_t v[16];
      require(!fmt && len >= 16 && in->read(v, 16) == 16, "Invalid WAV format");
      fmt = true;
      w.channels = u16(v + 2);
      w.rate = u32(v + 4);
      require(u16(v) == 1 && u16(v + 14) == 16 &&
                  (w.channels == 1 || w.channels == 2) &&
                  (w.rate == 44100 || w.rate == 88200) &&
                  u16(v + 12) == w.channels * 2 &&
                  u32(v + 8) == w.rate * w.channels * 2,
              "Unsupported card WAV format");
    } else if (std::memcmp(c, "data", 4) == 0) {
      require(!data, "Multiple WAV data chunks");
      data = true;
      w.dataOffset = pos;
      w.dataBytes = len;
    }
    require(in->setPosition(juce::int64(pos + len + (len & 1))),
            "Cannot seek WAV");
  }
  require(fmt && data && w.dataBytes % (w.channels * 2) == 0,
          "Missing or unaligned WAV audio");
  w.frames = w.dataBytes / (w.channels * 2);
  return w;
}
void validatePair(const Wav &w, const Info &i) {
  basic(i);
  require(int(w.rate) == i.rate && int(w.channels) == i.channels &&
              w.dataBytes ==
                  uint64_t(i.size) + uint64_t(w.rate) * w.channels * 2,
          "WAV and metadata disagree about format or circular padding");
}
String path(int bank, int slot, int variant) {
  require(bank >= 0 && bank < 16 && slot >= 0 && slot < 16 && variant >= 0 &&
              variant <= 9,
          "Bank or slot is full or invalid");
  return "bank" + String(bank + 1) + "/" + String(slot) + "." +
         String(variant) + ".wav";
}
void writeHeader(juce::OutputStream &out, uint64_t frames, int rate,
                 int channels) {
  require((channels == 1 || channels == 2) && (rate == 44100 || rate == 88200),
          "Unsupported output format");
  uint64_t bytes = frames * uint64_t(channels * 2);
  require(bytes <= UINT32_MAX - 36, "WAV exceeds RIFF limit");
  out.write("RIFF", 4);
  out.writeInt(int(bytes + 36));
  out.write("WAVEfmt ", 8);
  out.writeInt(16);
  out.writeShort(1);
  out.writeShort(short(channels));
  out.writeInt(rate);
  out.writeInt(rate * channels * 2);
  out.writeShort(short(channels * 2));
  out.writeShort(16);
  out.write("data", 4);
  out.writeInt(int(bytes));
}
const std::vector<Setting> &settingDefinitions() {
  static const std::vector<Setting> defs = [] {
    std::vector<Setting> defs{
        {"brightness",
         "Brightness",
         {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12",
          "13", "14", "15"},
         "8"},
        {"clock_stop_sync", "Clock stop", {"on", "off"}, "on"},
        {"clock_output_trig", "Clock output trigger", {"on", "off"}, "off"},
        {"clock_behavior_sync_slice",
         "Clock syncs slices",
         {"on", "off"},
         "off"},
        {"amen_cv",
         "Amen CV polarity",
         {"unipolar", "bipolar"},
         "unipolar",
         true},
        {"amen_behavior",
         "Amen CV behavior",
         {"jump", "repeat", "split"},
         "jump",
         true},
        {"break_cv",
         "Break CV polarity",
         {"unipolar", "bipolar"},
         "unipolar",
         true},
        {"sample_cv",
         "Sample CV polarity",
         {"unipolar", "bipolar"},
         "unipolar",
         true},
        {"sample_cv_mapping",
         "Sample CV mapping",
         {"bank", "1voct"},
         "bank",
         true},
        {"override_with_reset",
         "Reset override",
         {"none", "sample", "break", "amen", "clk"},
         "none",
         true},
        {"knobx_select_sample", "Knob selects sample", {"on", "off"}, "off"},
        {"mash_mode_momentary", "Momentary MASH", {"on", "off"}, "off"}};
    defs.front().values.clear();
    for (int n = 0; n <= 100; ++n)
      defs.front().values.add(String(n));
    defs.front().initial = "50";
    return defs;
  }();
  return defs;
}
Settings defaultSettings() {
  Settings settings;
  for (const auto &definition : settingDefinitions())
    settings[definition.key] = definition.initial;
  // Website presets, with the first bank dedicated to Time Stretch.
  const std::array<std::vector<int>, 7> effects{{{5},
                                                 {6, 13},
                                                 {5, 6, 9, 10},
                                                 {6, 7, 9, 13, 14},
                                                 {11, 12, 13, 15, 16},
                                                 {5, 6, 10},
                                                 {1, 3, 5, 6, 7, 8, 10, 13, 14, 15}}};
  for (int bank = 1; bank <= 7; ++bank)
    for (int effect = 1; effect <= 16; ++effect) {
      const auto &enabled = effects[size_t(bank - 1)];
      settings["grimoire/rune" + String(bank) + "/effect" + String(effect)] =
          std::find(enabled.begin(), enabled.end(), effect) != enabled.end() ? "on" : "off";
    }
  return settings;
}
std::map<String, bool> settingsFiles(const Settings &settings) {
  std::map<String, bool> files;
  for (const auto &d : settingDefinitions()) {
    if (d.key == "sample_cv_mapping")
      continue;
    auto found = settings.find(d.key);
    if (found == settings.end())
      continue;
    require(d.values.contains(found->second), "Invalid setting " + d.key);
    for (const auto &v : d.values)
      files["settings/" + d.key + "-" + v] = v == found->second;
  }
  for (int bank = 1; bank <= 7; ++bank)
    for (int fx = 1; fx <= 16; ++fx) {
      auto key = "grimoire/rune" + String(bank) + "/effect" + String(fx);
      auto it = settings.find(key);
      if (it == settings.end())
        continue;
      require(it->second == "on" || it->second == "off",
              "Invalid effect setting");
      files["settings/" + key + "-on"] = it->second == "on";
      files["settings/" + key + "-off"] = it->second == "off";
    }
  return files;
}
String sampleCVMappingContents(const Settings &settings) {
  const auto found = settings.find("sample_cv_mapping");
  const auto value = found == settings.end() ? String("bank") : found->second;
  require(value == "bank" || value == "1voct", "Invalid Sample CV mapping");
  return value + "\n";
}
Settings readSettings(const File &root, juce::StringArray &warnings) {
  Settings values;
  auto read = [&](const String &key, const juce::StringArray &choices) {
    String chosen;
    for (const auto &v : choices)
      if (child(root, "settings/" + key + "-" + v).existsAsFile()) {
        if (chosen.isNotEmpty()) {
          warnings.add("Conflicting settings markers: " + key);
          return;
        }
        chosen = v;
      }
    if (chosen.isNotEmpty())
      values[key] = chosen;
  };
  for (const auto &d : settingDefinitions())
    if (d.key != "sample_cv_mapping")
      read(d.key, d.values);
  // Match firmware precedence: settings/ overrides the root-directory file.
  auto mapping = child(root, sampleCVMappingPath);
  if (!mapping.exists())
    mapping = child(root, "sample_cv_mapping");
  if (mapping.exists()) {
    auto input = mapping.createInputStream();
    char bytes[16]{};
    const auto size = input ? input->getTotalLength() : -1;
    const bool readable = size >= 0 && size <= 16 &&
                          input->read(bytes, int(size)) == size && input->getStatus().wasOk();
    int length = readable ? int(size) : 0;
    while (length > 0 && (bytes[length - 1] == ' ' || bytes[length - 1] == '\t' ||
                          bytes[length - 1] == '\r' || bytes[length - 1] == '\n' ||
                          bytes[length - 1] == '\v' || bytes[length - 1] == '\f'))
      --length;
    const bool octave = length == 5 && std::memcmp(bytes, "1voct", 5) == 0;
    const bool bank = length == 4 && std::memcmp(bytes, "bank", 4) == 0;
    values["sample_cv_mapping"] = octave ? "1voct" : "bank";
    if (!readable || (!octave && !bank))
      warnings.add("Invalid Sample CV mapping; using Bank divisions.");
  }
  for (int b = 1; b <= 7; ++b)
    for (int e = 1; e <= 16; ++e)
      read("grimoire/rune" + String(b) + "/effect" + String(e), {"on", "off"});
  return values;
}
} // namespace core::card
