#pragma once
#include "Common.h"
namespace core::names {
constexpr int maxBytes = filenameMetadataMaxBytes;
String path(int bank, int slot);
struct Metadata {
  enum Status { missing, valid, invalid } status = missing;
  String name, originalFilename, fingerprint, warning;
};
// Never throws for unreadable, unsafe, malformed, or foreign metadata. Only a
// matching sibling primary WAV can establish ownership of a sidecar.
Metadata read(const File &root, int bank, int slot, const String &audioSha256);
var encode(const String &name, const String &originalFilename,
           const String &audioSha256);
} // namespace core::names
