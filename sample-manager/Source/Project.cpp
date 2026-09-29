#include "Project.h"
#include <cmath>
namespace core {
void Sample::updateSpliceTrigger(SpliceTimingCalculation calculation) {
  require(std::isfinite(sourceDuration) && sourceDuration > 0 &&
              std::isfinite(sourceBpm) && sourceBpm > 0 && !slices.empty(),
          "Cannot calculate splice timing without duration, BPM and slices");
  // Use the unpadded, normal-speed source timeline. Render BPM changes the
  // duration inversely, so it does not change the number of beats in the loop.
  double beats = sourceDuration / (60. / sourceBpm);
  double ticks;
  if (calculation == SpliceTimingCalculation::initialImport) {
    // Website import (zeptocore.go): whole-loop beats, 192 ticks per beat,
    // rounded to a quarter tick before conversion to the integer field.
    beats = oneShot ? 1. : std::round(beats);
    ticks = std::trunc(std::round(192. * beats / double(slices.size()) * 4.) / 4.);
  } else {
    // Website createRegionsEvenly (app.js) quantizes to 24-tick divisions.
    ticks = std::round(beats * 192. / double(slices.size()) / 24.) * 24.;
  }
  require(std::isfinite(ticks), "Splice interval exceeds supported range");
  // Very short/dense slices can round to zero on the website. Keep the native
  // firmware field valid, including its 15-bit upper bound.
  spliceTrigger = int(juce::jlimit(2., 32767., ticks));
}
var Sample::json() const {
  var o = object();
  put(o, "id", id);
  put(o, "name", name);
  put(o, "originalFilename", originalFilename);
  put(o, "source", source);
  put(o, "sourceHash", sourceHash);
  put(o, "originalArchive", originalArchive);
  put(o, "origin", origin);
  put(o, "problem", problem);
  put(o, "bank", bank);
  put(o, "slot", slot);
  put(o, "channels", channels);
  put(o, "rate", rate);
  put(o, "playMode", playMode);
  put(o, "spliceTrigger", spliceTrigger);
  put(o, "sourceBpm", sourceBpm);
  put(o, "renderBpm", renderBpm);
  put(o, "sourceDuration", sourceDuration);
  put(o, "preservePitch", preservePitch);
  put(o, "tempoMatch", tempoMatch);
  put(o, "oneShot", oneShot);
  put(o, "spliceVariable", spliceVariable);
  put(o, "protected", protectedEntry);
  put(o, "revision", juce::int64(revision));
  put(o, "completedRevision", juce::int64(completedRevision));
  put(o, "rendered", rendered);
  put(o, "companion", companion);
  put(o, "renderKey", renderKey);
  juce::Array<var> marks, lanes, paths;
  for (auto m : slices) {
    var v = object();
    put(v, "start", m.start);
    put(v, "stop", m.stop);
    put(v, "type", m.type);
    marks.add(v);
  }
  for (const auto &lane : transients) {
    juce::Array<var> a;
    for (auto x : lane)
      a.add(x);
    lanes.add(a);
  }
  for (const auto &path : ownedPaths)
    paths.add(path);
  put(o, "slices", marks);
  juce::Array<var> anchors;
  for (auto m : renderAnchors) {
    var v = object();
    put(v, "start", m.start);
    put(v, "stop", m.stop);
    anchors.add(v);
  }
  put(o, "renderAnchors", anchors);
  put(o, "transients", lanes);
  put(o, "ownedPaths", paths);
  return o;
}
Sample Sample::fromJson(const var &o) {
  require(o.isObject(), "Invalid sample");
  Sample s;
  s.id = o["id"].toString();
  s.name = o["name"].toString();
  s.originalFilename = o["originalFilename"].toString();
  s.source = o["source"].toString();
  s.sourceHash = o["sourceHash"].toString();
  s.originalArchive = o["originalArchive"].toString();
  s.origin = o["origin"].toString();
  s.problem = o["problem"].toString();
  s.bank = int(o["bank"]);
  s.slot = int(o["slot"]);
  s.channels = int(o["channels"]);
  s.rate = int(o["rate"]);
  s.playMode = int(o["playMode"]);
  s.spliceTrigger = int(o["spliceTrigger"]);
  s.sourceBpm = double(o["sourceBpm"]);
  s.renderBpm = double(o["renderBpm"]);
  s.sourceDuration = double(o["sourceDuration"]);
  s.preservePitch = bool(o["preservePitch"]);
  s.tempoMatch = bool(o["tempoMatch"]);
  s.oneShot = bool(o["oneShot"]);
  s.spliceVariable = bool(o["spliceVariable"]);
  s.protectedEntry = bool(o["protected"]);
  s.revision = juce::int64(o["revision"]);
  s.completedRevision = juce::int64(o["completedRevision"]);
  s.rendered = o["rendered"].toString();
  s.companion = o["companion"].toString();
  s.renderKey = o["renderKey"].toString();
  s.slices.clear();
  if (auto *a = o["slices"].getArray())
    for (auto &m : *a)
      s.slices.push_back(
          {double(m["start"]), double(m["stop"]), int(m["type"])});
  s.renderAnchors = s.slices;
  if (auto *a = o["renderAnchors"].getArray()) {
    s.renderAnchors.clear();
    for (auto &m : *a)
      s.renderAnchors.push_back({double(m["start"]), double(m["stop"]), 0});
  }
  if (auto *a = o["transients"].getArray()) {
    require(a->size() == 3, "Invalid transient lanes");
    for (int l = 0; l < 3; ++l)
      if (auto *lane = a->getReference(l).getArray())
        for (auto &t : *lane)
          s.transients[size_t(l)].push_back(double(t));
  }
  if (auto *a = o["ownedPaths"].getArray())
    for (auto &p : *a)
      s.ownedPaths.push_back(p.toString());
  return s;
}
var Project::json() const {
  var o = object();
  put(o, "schema", 1);
  put(o, "id", id);
  put(o, "revision", juce::int64(revision));
  put(o, "completedRevision", juce::int64(completedRevision));
  juce::Array<var> a, w;
  for (const auto &s : samples)
    a.add(s.json());
  for (const auto &x : warnings)
    w.add(x);
  put(o, "samples", a);
  put(o, "warnings", w);
  var settingsJson = object(), files = object();
  for (const auto &[k, v] : settings)
    put(settingsJson, k, v);
  for (const auto &[k, v] : fingerprints)
    put(files, k, v);
  put(o, "settings", settingsJson);
  put(o, "fingerprints", files);
  return o;
}
Project Project::fromJson(const var &o) {
  require(o.isObject() && int(o["schema"]) == 1, "Unsupported project schema");
  Project p;
  p.id = o["id"].toString();
  p.revision = juce::int64(o["revision"]);
  p.completedRevision = juce::int64(o["completedRevision"]);
  require(o["samples"].isArray(), "Missing sample catalogue");
  for (const auto &s : *o["samples"].getArray())
    p.samples.push_back(Sample::fromJson(s));
  if (auto *s = o["settings"].getDynamicObject())
    for (const auto &kv : s->getProperties())
      p.settings[kv.name.toString()] = kv.value.toString();
  if (auto *f = o["fingerprints"].getDynamicObject())
    for (const auto &kv : f->getProperties())
      p.fingerprints[kv.name.toString()] = kv.value.toString();
  if (auto *w = o["warnings"].getArray())
    for (const auto &x : *w)
      p.warnings.add(x.toString());
  p.validate();
  // Older versions stored warnings for valid zero positions. Recheck only
  // these range warnings, keeping any real incompatibility (also when names
  // repeat across banks) and all unrelated recovery warnings.
  std::set<String> rangeWarnings, stillInvalid;
  for (const auto &sample : p.samples) {
    for (size_t lane = 0; lane < 3; ++lane) {
      auto warning = sample.name + ": Transient lane " + String(int(lane + 1)) +
                     " has a position outside the device's encoding range.";
      rangeWarnings.insert(warning);
      if (sample.protectedEntry)
        stillInvalid.insert(warning);
      for (auto t : sample.transients[lane]) {
        const auto frame = std::round(t * sample.ratio() * 44100.);
        if (frame >= 65536. * 16)
          stillInvalid.insert(warning);
      }
    }
  }
  for (const auto &warning : rangeWarnings)
    if (!stillInvalid.count(warning))
      p.warnings.removeString(warning);
  return p;
}
Sample *Project::find(const String &id) {
  for (auto &s : samples)
    if (s.id == id)
      return &s;
  return nullptr;
}
const Sample *Project::find(const String &id) const {
  for (const auto &s : samples)
    if (s.id == id)
      return &s;
  return nullptr;
}
int Project::freeSlot(int bank) const {
  require(bank >= 0 && bank < 16, "Invalid bank");
  for (int slot = 0; slot < 16; ++slot) {
    bool used = false;
    for (const auto &s : samples)
      if (s.bank == bank && s.slot == slot)
        used = true;
    if (!used)
      return slot;
  }
  return -1;
}
void Project::validate() const {
  require(id.isNotEmpty() && revision >= 0 && completedRevision >= 0 &&
              completedRevision <= revision,
          "Invalid project identity or revision");
  std::set<String> ids;
  std::set<int> slots;
  for (const auto &s : samples) {
    require(s.id.isNotEmpty() && ids.insert(s.id).second,
            "Duplicate sample ID");
    require(s.bank >= 0 && s.bank < 16 && s.slot >= 0 && s.slot < 16 &&
                slots.insert(s.bank * 16 + s.slot).second,
            "Duplicate or invalid slot");
    if (s.protectedEntry)
      continue;
    require(s.source.startsWith(".core-manager/sources/") &&
                s.sourceHash.length() == 64,
            "Invalid immutable source path");
    require(s.channels >= 1 && s.channels <= 2 &&
                (s.rate == 44100 || s.rate == 88200),
            "Invalid audio format");
    require(
        std::isfinite(s.sourceBpm) && s.sourceBpm >= 30 && s.sourceBpm <= 300 &&
            std::isfinite(s.renderBpm) &&
            (s.renderBpm == 0 || (s.renderBpm >= 30 && s.renderBpm <= 300)),
        juce::String::fromUTF8("BPM must be 30–300, or unset for Render BPM"));
    require(std::isfinite(s.sourceDuration) && s.sourceDuration > 0,
            "Empty audio");
    require(!s.slices.empty(), "A sample needs at least one slice");
    for (auto m : s.slices)
      require(std::isfinite(m.start) && std::isfinite(m.stop) && m.start >= 0 &&
                  m.start < m.stop && m.stop <= 1 && m.type >= 0 &&
                  m.type <= 127,
              "Invalid slice");
    require(!s.renderAnchors.empty(), "Missing source render anchors");
    for (auto m : s.renderAnchors)
      require(std::isfinite(m.start) && std::isfinite(m.stop) && m.start >= 0 &&
                  m.start < m.stop && m.stop <= 1,
              "Invalid source render anchor");
    for (const auto &lane : s.transients)
      for (auto t : lane)
        require(std::isfinite(t) && t >= 0 && t <= s.sourceDuration,
                "Transient is outside source");
  }
}
void History::accept(const Project &p, const String &label) {
  past.push_back({p, label});
  future.clear();
}
bool History::undo(Project &p) {
  if (past.empty())
    return false;
  future.push_back({p, past.back().label});
  p = past.back().project;
  past.pop_back();
  return true;
}
bool History::redo(Project &p) {
  if (future.empty())
    return false;
  past.push_back({p, future.back().label});
  p = future.back().project;
  future.pop_back();
  return true;
}
var History::json(const String &projectId) const {
  var o = object();
  put(o, "projectId", projectId);
  juce::Array<var> a, b;
  auto add = [](const std::vector<Entry> &items, juce::Array<var> &target) {
    for (const auto &item : items) {
      var v = object();
      put(v, "project", item.project.json());
      put(v, "label", item.label);
      target.add(v);
    }
  };
  add(past, a);
  add(future, b);
  put(o, "past", a);
  put(o, "future", b);
  return o;
}
void History::restore(const var &o, const String &projectId) {
  clear();
  if (o["projectId"].toString() != projectId)
    return;
  auto read = [](const var &a, std::vector<Entry> &target) {
    if (auto *array = a.getArray())
      for (const auto &v : *array)
        target.push_back(
            {Project::fromJson(v["project"]), v["label"].toString()});
  };
  read(o["past"], past);
  read(o["future"], future);
}
String audioKey(const Sample &s) {
  // Source BPM alone and slice edits are metadata. Anchors are captured when
  // the immutable source is created, so identical keys always produce identical
  // audio.
  auto key = "render-v2:" + s.sourceHash + ":" + String(s.rate) + ":" +
             String(s.channels) + ":" + String(s.ratio(), 12) + ":" +
             String(int(s.ratio() == 1. || s.preservePitch));
  for (auto m : s.renderAnchors)
    key += ":" + String(m.start, 12) + ":" + String(m.stop, 12);
  return juce::SHA256(key.toRawUTF8(), size_t(key.getNumBytesAsUTF8()))
      .toHexString();
}
card::Info sampleInfo(const Sample &s, uint64_t frames, bool companion) {
  require(frames > 0 && frames <= uint64_t(INT32_MAX) / (s.channels * 2),
          "Sample exceeds firmware file size limit");
  card::Info i;
  i.size = uint32_t(frames * s.channels * 2);
  i.bpm = int(std::lround(s.renderBpm > 0 ? s.renderBpm : s.sourceBpm));
  i.channels = s.channels;
  i.rate = s.rate;
  i.playMode = s.playMode;
  i.oneShot = s.oneShot;
  i.tempoMatch = s.tempoMatch;
  i.spliceTrigger = s.spliceTrigger;
  i.spliceVariable = s.spliceVariable;
  for (auto m : s.slices)
    i.slices.push_back({uint32_t(std::llround(m.start * i.size)) / 4 * 4,
                        uint32_t(std::llround(m.stop * i.size)) / 4 * 4,
                        int8_t(m.type)});
  for (size_t l = 0; l < 3; ++l)
    for (auto t : s.transients[l]) {
      // Firmware transients use the normal-speed 44.1 kHz timeline in both
      // companions.
      juce::ignoreUnused(companion);
      auto value = t * s.ratio() * 44100;
      require(value <= UINT32_MAX, "Transient exceeds supported timeline");
      i.transients[l].push_back(uint32_t(std::llround(value)));
    }
  return i;
}
} // namespace core
