#include "Manager.h"
#include "ScreenWaveforms.h"
#include "../../test/screen/golden.h"
#include <iostream>
namespace core {
namespace {
ManagerState settled(Manager &manager) {
  for (int n = 0; n < 3000; ++n) {
    auto state = manager.snapshot();
    if (!state.busy) {
      require(state.error.isEmpty(), state.error);
      return state;
    }
    juce::Thread::sleep(10);
  }
  throw std::runtime_error("Screen waveform job timed out");
}
juce::MemoryBlock readBytes(const File &f) {
  juce::MemoryBlock b;
  require(f.loadFileAsData(b), "Screen fixture missing: " + f.getFullPathName());
  return b;
}
void validFile(const File &root, int bank, int slot) {
  auto b = readBytes(child(root, screenwaves::path(bank, slot)));
  require(b.getSize() == SW_FILE_BYTES, "Screen waveform has complete file length");
  auto info = readBytes(child(root, card::path(bank, slot) + ".info"));
  auto i = card::decode(info);
  const auto *p = static_cast<const uint8_t *>(b.getData());
  require(sw_valid(p, b.getSize(), i.size, i.rate, unsigned(i.channels),
                   sw_info_hash(static_cast<const uint8_t *>(info.getData()), info.getSize())),
          "JUCE screen file passes firmware validation: " + screenwaves::path(bank,slot) +
          " bytes=" + String(int(b.getSize())) + " pcm=" + String(sw_u32(p+8)) +
          "/" + String(i.size) + " info=" + String(sw_u32(p+20)) + "/" +
          String(sw_info_hash(static_cast<const uint8_t *>(info.getData()), info.getSize())));
  auto hash = hashFile(child(root, card::path(bank, slot)));
  for (int n = 0; n < 32; ++n)
    require(p[24 + n] == uint8_t(hash.substring(n*2, n*2+2).getHexValue32()),
            "Screen file belongs to exact committed WAV");
}
}
void screenWaveformTests() {
  juce::MemoryBlock info(screen_golden_info, sizeof screen_golden_info);
  zv::Wave wave;
  wave.channels = 2; wave.sampleRate = 44100; wave.duration = 2.0 / 44100;
  wave.peaks = {{-32768, -32768}, {32767, 32767}};
  auto bytes = screenwaves::encode(wave, info, String::repeatedString("0",64));
  const auto *p = static_cast<const uint8_t *>(bytes.getData());
  require(bytes.getSize() == 448 && !memcmp(p, screen_golden_header,64),
          "JUCE writer matches independent Python golden header");
  for (int n = 64; n < 448; ++n) require(p[n] == 255, "Opposite stereo peaks retain full amplitude");
  wave.peaks = {{0,0},{0,0}};
  bytes = screenwaves::encode(wave, info, String::repeatedString("0",64));
  p = static_cast<const uint8_t *>(bytes.getData());
  for (int n = 64; n < 448; ++n) require(p[n] == 0, "Silence has zero waveform amplitude");

  const auto root = File::getSpecialLocation(File::tempDirectory).getChildFile("screen-test-" + uuid());
  require(root.createDirectory().wasOk(), "Create screen fixture");
  struct Cleanup { File f; ~Cleanup() { f.deleteRecursively(); } } cleanup{root};
  const auto wav = child(root, card::path(0,0));
  ensureDirectory(wav.getParentDirectory());
  {
    auto out = wav.createOutputStream();
    card::writeHeader(*out, 44100 + 256, 44100, 2);
    for (int frame = 0; frame < 44100 + 256; ++frame) {
      out->writeShort(frame == 22050 + 64 ? short(-20000) : short(0));
      out->writeShort(frame == 22050 + 64 ? short(30000) : short(0));
    }
    out->flush();
  }
  card::Info meta;
  meta.channels = 2; meta.size = 256*4; meta.slices = {{0,1024,0}};
  bytes = card::encode(meta);
  const auto infoPath = child(root, card::path(0,0) + ".info");
  durableWrite(infoPath, bytes.getData(),bytes.getSize());
  const auto originalAudio = hashFile(wav), originalInfo = hashFile(infoPath);
  const auto audioTime = wav.getLastModificationTime(), infoTime = infoPath.getLastModificationTime();
  {
    Manager manager{std::chrono::milliseconds(20)};
    manager.setAutoSave(false);
    manager.open(root); auto state = settled(manager);
    validFile(root,0,0);
    require(hashFile(wav) == originalAudio && hashFile(infoPath) == originalInfo &&
                wav.getLastModificationTime() == audioTime && infoPath.getLastModificationTime() == infoTime,
            "Automatic screen backfill preserves audio and metadata bytes/timestamps");
    auto id = state.project.samples.front().id;
    manager.edit(id,"BPM",[](Sample &s) { s.sourceBpm = 130; }); settled(manager);
    manager.saveNow(); settled(manager); validFile(root,0,0);
    require(hashFile(wav) == originalAudio && wav.getLastModificationTime() == audioTime,
            "Metadata-only screen update retains WAV");
    manager.move({id},1); settled(manager); manager.saveNow(); settled(manager);
    validFile(root,1,0);
    require(!child(root,screenwaves::path(0,0)).exists(), "Moving sample removes old waveform");
    manager.remove({id}); settled(manager); manager.saveNow(); settled(manager);
    require(!child(root,screenwaves::path(1,0)).exists(), "Deleting sample removes waveform");
    manager.undo(); settled(manager); manager.saveNow(); settled(manager); validFile(root,1,0);
  }
  auto target = child(root,screenwaves::path(1,0));
  const auto waveTime = target.getLastModificationTime();
  {
    Manager manager;
    manager.setAutoSave(false); manager.open(root); settled(manager);
    require(target.getLastModificationTime() == waveTime, "Unchanged waveform is not rewritten");
  }
  durableWrite(target,"bad",3);
  {
    Manager manager;
    manager.setAutoSave(false); manager.open(root); settled(manager); validFile(root,1,0);
  }
  std::cout << "PASS: screen waveform golden format, stereo, silence, auto backfill, metadata edits, move/delete/undo, reuse and repair\n";
}
}
