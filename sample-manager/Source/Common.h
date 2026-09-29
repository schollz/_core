#pragma once
#include "Diagnostics.h"
#include <array>
#include <atomic>
#include <filesystem>
#include <functional>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_cryptography/juce_cryptography.h>
#include <map>
#include <set>
#include <stdexcept>
#include <vector>
namespace core {
using juce::File;
using juce::String;
using juce::var;
// Tests use a private state root and never replace the user's saved
// preferences/cache.
inline File stateRootOverride;
inline File stateRoot() {
  return stateRootOverride != File()
             ? stateRootOverride
             : File::getSpecialLocation(File::userApplicationDataDirectory)
                   .getChildFile("com.infinitedigits.coresamplemanager");
}
inline void require(bool condition, const String &message) {
  if (!condition) {
    diagnostics::log("ERROR", message);
    throw std::runtime_error(message.toStdString());
  }
}
inline void ensureDirectory(const File &directory) {
  // Never let a recursive mkdir recreate a disconnected project's mount path.
  File anchor;
  for (auto p = directory; p != p.getParentDirectory();
       p = p.getParentDirectory())
    if (p.getFileName() == ".core-manager") {
      anchor = p;
      break;
    }
  if (anchor == File()) {
    require(directory.createDirectory().wasOk(),
            "Cannot create directory: " + directory.getFullPathName());
    return;
  }
  require(anchor.isDirectory(),
          "Project volume is unavailable; pending edits are retained locally");
  std::vector<File> missing;
  for (auto p = directory; p != anchor; p = p.getParentDirectory())
    missing.push_back(p);
  for (auto it = missing.rbegin(); it != missing.rend(); ++it) {
    std::error_code error;
    std::filesystem::create_directory(
        std::filesystem::u8path(it->getFullPathName().toStdString()), error);
    require(!error && it->isDirectory(),
            "Cannot create project directory: " + String(error.message()));
  }
}
inline var object() { return new juce::DynamicObject; }
inline void put(var &o, const juce::Identifier &key, const var &value) {
  o.getDynamicObject()->setProperty(key, value);
}
inline String uuid() { return juce::Uuid().toString(); }
inline String hashFile(const File &f, const std::function<bool()> &cancel = {}) {
  struct Input final : juce::FileInputStream {
    Input(const File &file, const std::function<bool()> &stop)
        : juce::FileInputStream(file), stop(stop) {}
    int read(void *buffer, int bytes) override {
      if (stop && stop())
        throw std::runtime_error("Cancelled");
      return juce::FileInputStream::read(buffer, bytes);
    }
    const std::function<bool()> &stop;
  } stream(f, cancel);
  require(stream.openedOk(), "Cannot read " + f.getFullPathName());
  // JUCE SHA256 requests 64 bytes per read. Buffer those requests instead of
  // issuing a filesystem read for every SHA block (millions on a sample card).
  juce::BufferedInputStream buffered(stream, 256 * 1024);
  auto hash = juce::SHA256(buffered).toHexString();
  require(stream.getStatus().wasOk() && buffered.isExhausted(),
          "Cannot finish reading " + f.getFullPathName());
  return hash;
}
inline constexpr int filenameMetadataMaxBytes = 64 * 1024;
inline String fingerprint(const File &f, const std::function<bool()> &cancel = {}) {
  if (cancel && cancel())
    throw std::runtime_error("Cancelled");
  if (!f.existsAsFile())
    return "missing";
  if (!f.getFileName().endsWith(".name.json"))
    return hashFile(f, cancel);
  // External edits to an owned sidecar must not turn the project-open
  // fingerprint pass into an unbounded read, or prevent its audio from opening.
  auto stream = f.createInputStream();
  if (!stream)
    return "unreadable filename metadata";
  if (stream->getTotalLength() > filenameMetadataMaxBytes)
    return "oversized filename metadata";
  juce::BufferedInputStream buffered(*stream, filenameMetadataMaxBytes);
  auto hash = juce::SHA256(buffered, filenameMetadataMaxBytes).toHexString();
  return stream->getStatus().wasOk() && stream->isExhausted()
             ? hash
             : String("unreadable filename metadata");
}
inline void cancelled(const std::function<bool()> &fn) {
  if (fn && fn())
    throw std::runtime_error("Cancelled");
}
inline var parseJson(const File &f) {
  var value;
  auto result = juce::JSON::parse(f.loadFileAsString(), value);
  require(result.wasOk() && value.isObject(),
          "Invalid JSON: " + f.getFileName());
  return value;
}
inline File child(const File &root, const String &relative) {
  require(relative.isNotEmpty() && !File::isAbsolutePath(relative) &&
              !relative.containsChar('\\'),
          "Invalid relative path");
  auto parts = juce::StringArray::fromTokens(relative, "/", "");
  require(!parts.contains("..") && !parts.contains("."),
          "Path escapes project");
  auto f = root.getChildFile(relative);
  require(f.isAChildOf(root), "Path escapes project");
  for (auto p = f; p != root; p = p.getParentDirectory())
    require(!p.isSymbolicLink(),
            "Project paths may not traverse symbolic links: " + relative);
  return f;
}
} // namespace core
