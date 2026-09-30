#include "Uf2.h"
namespace core {
namespace {
uint32_t word(const uint8_t *p) {
  return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 |
         uint32_t(p[3]) << 24;
}
bool bootloader(const File &volume) {
  auto info = volume.getChildFile("INFO_UF2.TXT");
  if (!info.existsAsFile())
    info = volume.getChildFile("INFO_UF2.txt");
  return info.existsAsFile() && info.getSize() < 65536 &&
         info.loadFileAsString().containsIgnoreCase("RPI-RP2");
}
} // namespace
Uf2Image inspectUf2(const File &file) {
  require(file.hasFileExtension("uf2") && file.getSize() > 0 &&
              file.getSize() % 512 == 0 && file.getSize() <= 64 * 1024 * 1024,
          "Choose a complete local UF2 file");
  auto input = file.createInputStream();
  require(input != nullptr, "Cannot read UF2");
  Uf2Image image;
  std::set<uint32_t> blocks;
  juce::MemoryOutputStream payload;
  std::array<uint8_t, 512> block{};
  while (!input->isExhausted()) {
    require(input->read(block.data(), 512) == 512, "Truncated UF2 block");
    auto *p = block.data();
    require(word(p) == 0x0a324655 && word(p + 4) == 0x9e5d5157 &&
                word(p + 508) == 0x0ab16f30,
            "Invalid UF2 magic");
    auto flags = word(p + 8), address = word(p + 12), size = word(p + 16),
         number = word(p + 20), total = word(p + 24), family = word(p + 28);
    require((flags & 0x2000) != 0 && (flags & 1) == 0 && size > 0 &&
                size <= 476 && size % 4 == 0 && address >= 0x10000000 &&
                uint64_t(address) + size <= 0x11000000,
            "UF2 is not a supported flash image");
    require(family == 0xe48bff56, "UF2 targets a different processor");
    require(total == uint32_t(file.getSize() / 512) && number < total &&
                blocks.insert(number).second,
            "Incomplete or duplicate UF2 blocks");
    if (image.blocks)
      require(image.blocks == total && image.family == family,
              "Mixed UF2 images");
    image.blocks = total;
    image.family = family;
    payload.write(p + 32, size);
  }
  auto bytes = payload.getMemoryBlock();
  auto *raw = static_cast<const char *>(bytes.getData());
  std::string content(raw, bytes.getSize());
  if (content.find("ezeptocore") != std::string::npos)
    image.product = "Ezeptocore / Ectocore";
  else if (content.find("zeptocore") != std::string::npos)
    image.product = "Zeptocore";
  else if (content.find("ectocore") != std::string::npos)
    image.product = "Ectocore";
  else {
    // Non-MIDI Ectocore/Ezeptocore builds use the Pico SDK's USB CDC
    // descriptor, with no model-name string. Their USBD_PID is 0x1837
    // (lib/cmake/*ctocore*_compile_definitions*.cmake), under VID 0x2e8a.
    // Match the complete device descriptor, not an arbitrary PID byte pair
    // or a filename, so ordinary Pico CDC images are still rejected.
    constexpr uint8_t ectocoreSerialDescriptor[] = {
        0x12, 0x01, 0x10, 0x02, 0xef, 0x02, 0x01, 0x40, 0x8a,
        0x2e, 0x37, 0x18, 0x00, 0x01, 0x01, 0x02, 0x03, 0x01};
    const std::string descriptor(reinterpret_cast<const char *>(ectocoreSerialDescriptor),
                                 sizeof(ectocoreSerialDescriptor));
    if (content.find(descriptor) != std::string::npos)
      image.product = "Ezeptocore / Ectocore";
  }
  require(image.product.isNotEmpty(), "UF2 does not identify Core firmware");
  return image;
}
juce::Array<File> bootloaderVolumes() {
  juce::Array<File> roots, found;
  File::findFileSystemRoots(roots);
#if JUCE_MAC
  roots.addArray(File("/Volumes").findChildFiles(File::findDirectories, false));
#elif JUCE_LINUX
  for (const auto &parent : {File("/media"), File("/run/media")})
    for (const auto &user : parent.findChildFiles(File::findDirectories, false))
      roots.addArray(user.findChildFiles(File::findDirectories, false));
#endif
  for (const auto &root : roots)
    if (bootloader(root) && !found.contains(root))
      found.add(root);
  return found;
}
void flashLocalUf2(const File &image, const File &volume,
                   std::function<void(double)> progress,
                   std::function<bool()> cancel) {
  inspectUf2(image);
  require(bootloaderVolumes().contains(volume),
          "Selected bootloader volume is no longer connected");
  require(image.getParentDirectory() != volume,
          "Choose a firmware file outside the bootloader volume");
  auto input = image.createInputStream();
  auto out = volume.getChildFile("CORE.UF2").createOutputStream();
  require(input && out, "Cannot open bootloader volume for writing");
  std::array<uint8_t, 16384> bytes{};
  juce::int64 written = 0;
  while (!input->isExhausted()) {
    cancelled(cancel);
    auto n = input->read(bytes.data(), int(bytes.size()));
    require(n > 0 && out->write(bytes.data(), size_t(n)),
            "Firmware copy interrupted. Reconnect in bootloader mode before "
            "retrying.");
    written += n;
    if (progress)
      progress(double(written) / double(image.getSize()));
  }
  out->flush();
  require(out->getStatus().wasOk() && written == image.getSize(),
          "Could not confirm completed firmware copy");
}
} // namespace core
