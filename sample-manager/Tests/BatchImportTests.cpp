#include "Manager.h"
#include <cmath>
#include <iostream>

namespace core {
namespace {
ManagerState waitForBatch(Manager &manager) {
  const auto deadline = juce::Time::getMillisecondCounterHiRes() + 90000.;
  while (manager.snapshot().busy && juce::Time::getMillisecondCounterHiRes() < deadline)
    juce::Thread::sleep(5);
  auto result = manager.snapshot();
  require(!result.busy && result.error.isEmpty(), "Batch import failed: " + result.error);
  return result;
}
void fixture(const File &file, int index, int frames) {
  auto out = file.createOutputStream();
  require(out != nullptr, "Create batch fixture");
  card::writeHeader(*out, frames, 44100, 2);
  juce::MemoryOutputStream pcm;
  for (int n = 0; n < frames; ++n) {
    pcm.writeShort(short(12000 * std::sin(n * (.025 + index * .001))));
    pcm.writeShort(short(9000 * std::sin(n * (.043 + index * .002))));
  }
  require(out->write(pcm.getData(), pcm.getDataSize()), "Write batch fixture");
}
} // namespace

void batchImportTests(bool benchmark) {
  auto root = File::getSpecialLocation(File::tempDirectory).getChildFile("core-batch-test-" + uuid());
  require(root.createDirectory().wasOk(), "Create batch test folder");
  struct Cleanup {
    File root;
    ~Cleanup() { root.deleteRecursively(); }
  } cleanup{root};
  const int count = benchmark ? 13 : 6;
  const int frames = benchmark ? 11 * 44100 : 44100;
  juce::StringArray paths;
  for (int i = 0; i < count; ++i) {
    auto input = root.getChildFile("batch-" + String(i) + "-bpm120.wav");
    fixture(input, i, frames);
    paths.add(input.getFullPathName());
  }
  if (!benchmark) {
    paths.add(paths[0]);
    paths.add(paths[0]);
  }
  auto projectRoot = root.getChildFile("project");
  require(projectRoot.createDirectory().wasOk(), "Create batch project");
  Manager manager;
  manager.open(projectRoot);
  waitForBatch(manager);
  const auto started = juce::Time::getMillisecondCounterHiRes();
  manager.import(paths, 0);
  auto result = waitForBatch(manager);
  const auto elapsed = juce::Time::getMillisecondCounterHiRes() - started;
  require(result.project.samples.size() == size_t(paths.size()), "All batch files imported");
  String hashes;
  for (int i = 0; i < paths.size(); ++i) {
    const auto &sample = result.project.samples[size_t(i)];
    require(sample.slot == i && sample.originalFilename == File(paths[i]).getFileName(),
            "Concurrent work preserves input order and bank slots");
    require(result.completedAudio.at(sample.id).frames == uint64_t(frames) &&
                result.editorWaveform(sample.id) != nullptr,
            "Every imported sample has completed audio and a waveform");
    for (auto path : {card::path(0, i), card::path(0, i) + ".info", sample.rendered})
      hashes += hashFile(child(projectRoot, path));
    require(!child(projectRoot, card::path(0, i, 1)).exists(), "Batch creates no companions");
  }
  std::cout << "BATCH_IMPORT files=" << paths.size() << " seconds_each=" << frames / 44100.
            << " elapsed_ms=" << elapsed << " output_sha256="
            << juce::SHA256(hashes.toRawUTF8(), size_t(hashes.getNumBytesAsUTF8())).toHexString()
            << std::endl;
  if (!benchmark) {
    const auto &samples = result.project.samples;
    require(samples[0].rendered == samples[size_t(count)].rendered &&
                samples[0].rendered == samples[size_t(count + 1)].rendered,
            "Identical inputs safely share completed cache files");
    manager.undo();
    require(waitForBatch(manager).project.samples.empty(), "Batch is one undo step");
    manager.redo();
    require(waitForBatch(manager).project.samples.size() == size_t(paths.size()), "Batch redo restores all samples");

    // Failure in one render must leave every active hardware file unchanged,
    // even when other workers have already finished their new cache entries.
    auto before = manager.snapshot();
    const auto source = child(projectRoot, before.project.samples[0].source);
    const auto parked = source.getSiblingFile("parked-source.wav");
    require(source.moveFileTo(parked), "Park one immutable source");
    for (int i = 0; i < count; ++i)
      manager.edit(before.project.samples[size_t(i)].id, "Rerender batch", [](Sample &s) {
        s.renderBpm = 80.;
      });
    auto deadline = juce::Time::getMillisecondCounterHiRes() + 30000.;
    while (manager.snapshot().busy && juce::Time::getMillisecondCounterHiRes() < deadline)
      juce::Thread::sleep(5);
    auto failed = manager.snapshot();
    require(!failed.busy && failed.error.isNotEmpty() &&
                failed.completedProject.revision == before.completedProject.revision,
            "A failed parallel render does not publish partial completion");
    for (const auto &[path, hash] : before.completedProject.fingerprints)
      require(fingerprint(child(projectRoot, path)) == hash, "Failed render preserves hardware files");
    require(parked.moveFileTo(source), "Restore immutable source");
    manager.retry();
    result = waitForBatch(manager);
    require(result.completedAudio.at(result.project.samples[0].id).frames == uint64_t(frames * 3 / 2),
            "Retry reuses completed worker caches and finishes the failed batch");

    // Change the generation while multiple independent render jobs are active.
    // Workers must join before queued edits can update the same cache or files.
    for (const auto &sample : result.project.samples)
      manager.edit(sample.id, "Slow batch render", [](Sample &s) {
        s.renderBpm = 40.;
        s.preservePitch = true;
      });
    deadline = juce::Time::getMillisecondCounterHiRes() + 30000.;
    bool rendering = false;
    while (juce::Time::getMillisecondCounterHiRes() < deadline) {
      auto active = manager.snapshot();
      if (active.status.startsWith("Processing audio") &&
          active.project.samples.back().renderBpm == 40.) {
        rendering = true;
        break;
      }
      juce::Thread::sleep(1);
    }
    require(rendering, "Observe pending batch render");
    const auto changedId = result.project.samples.front().id;
    const auto removedId = result.project.samples.back().id;
    manager.edit(changedId, "Replace in-progress batch edit", [](Sample &s) {
      s.renderBpm = 0.;
      s.preservePitch = false;
    });
    manager.remove({removedId});
    result = waitForBatch(manager);
    require(result.project.find(removedId) == nullptr &&
                result.completedAudio.at(changedId).frames == uint64_t(frames) &&
                result.project.revision == result.completedProject.revision,
            "Stale parallel renders cannot overwrite newer edits or recreate removed samples");

    auto broken = root.getChildFile("broken.wav");
    durableWrite(broken, "bad", 3);
    const auto revision = manager.snapshot().project.revision;
    manager.import({paths[0], broken.getFullPathName(), paths[1]}, 0);
    deadline = juce::Time::getMillisecondCounterHiRes() + 30000.;
    while (manager.snapshot().busy && juce::Time::getMillisecondCounterHiRes() < deadline)
      juce::Thread::sleep(5);
    auto rejected = manager.snapshot();
    require(!rejected.busy && rejected.error.isNotEmpty() && rejected.project.revision == revision &&
                rejected.project.samples.size() == result.project.samples.size(),
            "A corrupt member does not publish a partial batch");
    std::cout << "PASS batch ordering, shared caches, undo/redo, render failure/retry, cancellation and corrupt import\n";
  }
}
} // namespace core
