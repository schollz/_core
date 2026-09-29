#include "AudioProcessing.h"
#include <ImportData.h>
#include <iostream>
namespace core {
void importTests() {
  auto root = File::getSpecialLocation(File::tempDirectory)
                  .getChildFile("core-import-test-" + uuid());
  require(root.createDirectory().wasOk(), "Import test root");
  struct Clean {
    File f;
    ~Clean() { f.deleteRecursively(); }
  } clean{root};
  Storage storage(root);
  AudioProcessing audio;
  for (int n = 0; n < ImportData::namedResourceListSize; ++n) {
    int size = 0;
    auto *data =
        ImportData::getNamedResource(ImportData::namedResourceList[n], size);
    auto name = String(ImportData::originalFilenames[n]);
    auto file = root.getChildFile(name);
    durableWrite(file, data, size_t(size));
    auto sample = audio.import(storage, file, 0, 0);
    require(sample.sourceDuration > .9 && sample.sourceDuration < 1.1,
            "Decode " + name);
    if (name == "cue.wav" || name == "op1.aif" || name == "import.xrni" ||
        name == "import.ogg")
      require(sample.slices.size() == 2 &&
                  std::abs(sample.slices[1].start - .5) < .001,
              "Embedded boundaries " + name);
    if (name == "loop.wav")
      require(sample.slices.size() == 1 && sample.slices[0].start == .25 &&
                  sample.slices[0].stop == .75,
              "WAV loop range");
    if (name == "custom.aiff")
      require(sample.slices.size() == 2 && sample.slices[1].start == .25,
              "AIFF structured COMT custom markers");
    if (name == "import.ogg")
      require(sample.oneShot && !sample.tempoMatch && sample.playMode == 1,
              "Ogg one-shot metadata");
    if (name == "import.xrni")
      require(child(root, sample.originalArchive).existsAsFile(),
              "XRNI original archive retained");
    if (name == "import.wav") {
      sample.oneShot = true;
      sample.tempoMatch = false;
      auto first = audio.render(root, sample);
      auto before = child(root, first.padded).getLastModificationTime();
      auto key = audioKey(sample);
      sample.sourceBpm = 160;
      sample.slices = {{0, .3, 0}, {.3, 1, 0}};
      require(audioKey(sample) == key, "Metadata-only render identity");
      auto reused = audio.render(root, sample);
      require(child(root, reused.padded).getLastModificationTime() == before,
              "Reuse verified cache");
      auto good = hashFile(child(root, reused.preview));
      child(root, reused.preview).appendText("damaged cache");
      auto repaired = audio.render(root, sample);
      require(hashFile(child(root, repaired.preview)) == good,
              "Repair corrupted audio cache");
    }
  }
  auto broken = root.getChildFile("broken.wav");
  durableWrite(broken, "bad", 3);
  bool rejected = false;
  try {
    audio.import(storage, broken, 0, 0);
  } catch (...) {
    rejected = true;
  }
  require(rejected, "Reject corrupt import");
  std::cout << "PASS WAV/AIFF/FLAC/MP3/Ogg/XRNI imports, cues/loops/OP1/custom "
               "markers, immutable originals and verified render cache\n";
}
} // namespace core
