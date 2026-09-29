#include "NameMetadata.h"
namespace core::names {
String path(int bank, int slot) {
  return "bank" + String(bank + 1) + "/" + String(slot) + ".name.json";
}
namespace {
bool basename(const String &s) {
  return !s.containsAnyOf("/\\") && s != "." && s != "..";
}
bool sha256(const String &s) {
  return s.length() == 64 && s.containsOnly("0123456789abcdefABCDEF");
}
bool completeObject(const char *text, size_t length) {
  // JUCE's parser accepts trailing text and has no recursion limit. Reject
  // both here before parsing an optional file from an external card.
  int depth = 0;
  bool started = false, quoted = false, escaped = false;
  for (size_t i = 0; i < length; ++i) {
    const auto c = text[i];
    if (quoted) {
      if (static_cast<unsigned char>(c) < 0x20)
        return false;
      if (escaped)
        escaped = false;
      else if (c == '\\')
        escaped = true;
      else if (c == '"')
        quoted = false;
    } else {
      if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
        continue;
      if (depth == 0 && (started || c != '{'))
        return false;
      started = true;
      if (c == '"')
        quoted = true;
      else if (c == '\'')
        return false;
      else if (c == '{' || c == '[') {
        if (++depth > 16)
          return false;
      } else if (c == '}' || c == ']') {
        if (--depth < 0)
          return false;
      }
    }
  }
  return started && depth == 0 && !quoted;
}
} // namespace
Metadata read(const File &root, int bank, int slot, const String &audioSha256) {
  Metadata result;
  try {
    auto file = child(root, path(bank, slot));
    if (!file.exists())
      return result;
    require(file.existsAsFile() && file.getSize() <= maxBytes,
            "not a file or exceeds 64 KiB");
    auto in = file.createInputStream();
    require(in != nullptr, "cannot read metadata");
    juce::MemoryBlock bytes;
    in->readIntoMemoryBlock(bytes, maxBytes);
    require(in->getStatus().wasOk() && in->isExhausted() &&
                bytes.getSize() == size_t(file.getSize()) &&
                bytes.getSize() > 0,
            "incomplete or oversized metadata");
    auto *utf8 = static_cast<const char *>(bytes.getData());
    require(
        std::memchr(utf8, 0, bytes.getSize()) == nullptr &&
            juce::CharPointer_UTF8::isValidString(utf8, int(bytes.getSize())),
        "invalid UTF-8");
    require(completeObject(utf8, bytes.getSize()),
            "malformed or overly nested JSON");
    var value;
    require(
        juce::JSON::parse(String::fromUTF8(utf8, int(bytes.getSize())), value)
                .wasOk() &&
            value.isObject(),
        "malformed JSON");
    require((value["schema"].isInt() || value["schema"].isInt64()) &&
                juce::int64(value["schema"]) == 1,
            "unsupported filename schema");
    require(value["name"].isString() && value["originalFilename"].isString() &&
                value["audioSha256"].isString() &&
                basename(value["originalFilename"].toString()) &&
                sha256(value["audioSha256"].toString()),
            "invalid filename metadata fields");
    require(sha256(audioSha256) &&
                value["audioSha256"].toString().equalsIgnoreCase(audioSha256),
            "primary audio SHA-256 mismatch");
    result.name = value["name"].toString();
    result.originalFilename = value["originalFilename"].toString();
    result.fingerprint = juce::SHA256(bytes).toHexString();
    result.status = Metadata::valid;
  } catch (const std::exception &e) {
    result.status = Metadata::invalid;
    result.warning =
        path(bank, slot) + ": " + String(e.what()) +
        ". Filename persistence skipped; existing metadata preserved.";
  }
  return result;
}
var encode(const String &name, const String &originalFilename,
           const String &audioSha256) {
  require(basename(originalFilename) && sha256(audioSha256),
          "Invalid filename metadata");
  var value = object();
  put(value, "schema", 1);
  put(value, "name", name);
  put(value, "originalFilename", originalFilename);
  put(value, "audioSha256", audioSha256);
  require(juce::JSON::toString(value).getNumBytesAsUTF8() <= maxBytes,
          "Filename metadata exceeds 64 KiB");
  return value;
}
} // namespace core::names
