#include "Manager.h"
#include "OnlineAnalysis.h"
#include "Storage.h"
#include "Uf2.h"
#include <iostream>
namespace core {
void crashTransaction(const File &root) {
  require(root.isAChildOf(File::getSpecialLocation(File::tempDirectory)) &&
              root.getFileName().startsWith("core-recovery-test-"),
          "Crash fixture requires its isolated temporary folder");
  Storage storage(root);
  auto project = storage.open();
  auto replacement = child(root, ".core-manager/cache/crash-stage");
  durableWrite(replacement, "after", 5);
  auto nameStage = child(root, ".core-manager/cache/crash-name");
  durableWrite(nameStage, "new name", 8);
  project.revision = project.completedRevision = 1;
  Transaction tx(root);
  tx.afterMutation = [](int mutation) {
    if (mutation == 1)
      std::_Exit(77);
  };
  tx.commit(
      {{"bank1/probe", replacement, fingerprint(child(root, "bank1/probe"))},
       {"bank1/0.name.json", nameStage,
        fingerprint(child(root, "bank1/0.name.json"))}},
      project);
}
int diskFullTest(const File &root) {
  try {
    require(root.isAChildOf(File::getSpecialLocation(File::tempDirectory)) &&
                root.getParentDirectory().getFileName().startsWith(
                    "core-disk-full-test-") &&
                root.getChildFile("disposable-test-volume").existsAsFile() &&
                root.getVolumeTotalSize() <= 32 * 1024 * 1024,
            "Disk-full acceptance requires the dedicated disposable <=32 MiB "
            "test image");
    auto local = root.getParentDirectory().getChildFile("local-state");
    stateRootOverride = local;
    auto settle = [](Manager &m) {
      for (int n = 0; n < 3000; ++n) {
        auto state = m.snapshot();
        if (!state.busy)
          return state;
        juce::Thread::sleep(10);
      }
      throw std::runtime_error("Disk-full job timed out");
    };
    auto input = root.getChildFile("test.wav");
    {
      auto out = input.createOutputStream();
      require(out != nullptr, "Disk-full fixture output");
      card::writeHeader(*out, 4410, 44100, 1);
      for (int n = 0; n < 4410; ++n)
        out->writeShort(short(std::sin(n * .1) * 10000));
    }
    Manager manager;
    manager.open(root);
    require(settle(manager).error.isEmpty(), "Open disk-full fixture");
    manager.import({input.getFullPathName()}, 0);
    auto before = settle(manager);
    require(before.error.isEmpty() && before.project.samples.size() == 1,
            "Complete baseline before disk-full test");
    auto id = before.project.samples.front().id;
    auto wav = child(root, "bank1/0.0.wav");
    auto info = child(root, "bank1/0.0.wav.info");
    auto wavHash = hashFile(wav), infoHash = hashFile(info);
    auto filler = root.getChildFile("space-filler");
    {
      auto out = filler.createOutputStream(0);
      require(out != nullptr, "Create bounded test filler");
      std::array<uint8_t, 4096> bytes{};
      for (int n = 0; n < 8192 && out->write(bytes.data(), bytes.size()); ++n) {
      }
      out->flush();
      require(out->getStatus().failed(), "Reach real filesystem ENOSPC");
    }
    manager.edit(id, "Pending edit on full disk",
                 [](Sample &s) { s.spliceTrigger = 120; });
    auto failed = settle(manager);
    require(failed.error.isNotEmpty() && hashFile(wav) == wavHash &&
                hashFile(info) == infoHash &&
                failed.project.completedRevision ==
                    before.project.completedRevision &&
                failed.project.find(id)->spliceTrigger == 120,
            "Disk-full retains completed hardware and pending edit");
    auto pending = parseJson(
        local.getChildFile("recovery/" + before.project.id + ".json"));
    require(
        Project::fromJson(pending).find(id)->spliceTrigger == 120,
        "Pending edit survives in local recovery while project disk is full");
    require(filler.deleteFile(), "Release disposable test volume space");
    manager.retry();
    auto recovered = settle(manager);
    require(recovered.error.isEmpty() &&
                recovered.project.revision ==
                    recovered.project.completedRevision &&
                recovered.project.find(id)->spliceTrigger == 120 &&
                hashFile(wav) == wavHash && hashFile(info) != infoHash,
            "Retry completes pending metadata without rewriting audio");
    std::cout << "PASS real ENOSPC, unchanged completed output, local pending "
                 "recovery, retry to Ready\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "FAIL disk-full: " << e.what() << std::endl;
    return 1;
  }
}
void recoveryTests() {
  auto root = File::getSpecialLocation(File::tempDirectory)
                  .getChildFile("core-recovery-test-" + uuid());
  require(root.createDirectory().wasOk(), "Recovery fixture");
  struct Clean {
    File f;
    ~Clean() {
      f.setReadOnly(false, true);
      f.deleteRecursively();
    }
  } clean{root};
  {
    Storage storage(root);
    storage.open();
    durableWrite(child(root, "bank1/probe"), "before", 6);
    durableWrite(child(root, "bank1/0.name.json"), "old name", 8);
  }
  juce::ChildProcess childProcess;
  require(childProcess.start(juce::StringArray{
              File::getSpecialLocation(File::currentExecutableFile)
                  .getFullPathName(),
              "--crash-transaction", root.getFullPathName()}),
          "Launch isolated crash fixture");
  require(childProcess.waitForProcessToFinish(10000) &&
              childProcess.getExitCode() == 77,
          "Abrupt process exit during journaled replacement");
  require(child(root, "bank1/probe").loadFileAsString() == "after",
          "Crash leaves a partially applied transaction");
  require(child(root, "bank1/0.name.json").loadFileAsString() == "new name",
          "Crash occurs after both audio and name mutations");
  {
    Storage storage(root);
    auto project = storage.open();
    require(child(root, "bank1/probe").loadFileAsString() == "before" &&
                child(root, "bank1/0.name.json").loadFileAsString() ==
                    "old name" &&
                project.completedRevision == 0,
            "Reopen rolls back process crash before resuming");
  }
#if !JUCE_WINDOWS
  auto readOnly = child(root, "read-only");
  readOnly.createDirectory();
  readOnly.setReadOnly(true);
  bool rejected = false;
  try {
    durableWrite(readOnly.getChildFile("output"), "x", 1);
  } catch (...) {
    rejected = true;
  }
  readOnly.setReadOnly(false);
  require(rejected, "Read-only destination reports write failure");
#endif
  auto response =
      juce::JSON::parse("{\"a\":[4410,0,4410],\"b\":[8820],\"c\":[]}");
  auto lanes = OnlineAnalysis::parse(response, 1.);
  require(lanes[0].size() == 1 && lanes[0][0] == .1 && lanes[1][0] == .2,
          "Online response parsing and sentinel handling");
  bool rejectedResponse = false;
  try {
    OnlineAnalysis::parse(
        juce::JSON::parse("{\"a\":[44101],\"b\":[],\"c\":[]}"), 1.);
  } catch (...) {
    rejectedResponse = true;
  }
  require(rejectedResponse, "Out-of-range online markers rejected");
  auto uf2 = root.getChildFile("test.uf2");
  std::array<uint8_t, 512> block{};
  auto word = [&](int offset, uint32_t value) {
    for (int n = 0; n < 4; ++n)
      block[size_t(offset + n)] = uint8_t(value >> (8 * n));
  };
  word(0, 0x0a324655);
  word(4, 0x9e5d5157);
  word(8, 0x2000);
  word(12, 0x10000000);
  word(16, 256);
  word(20, 0);
  word(24, 1);
  word(28, 0xe48bff56);
  word(508, 0x0ab16f30);
  std::memcpy(block.data() + 32, "ezeptocore", 10);
  durableWrite(uf2, block.data(), block.size());
  require(inspectUf2(uf2).product == "Ezeptocore / Ectocore",
          "Local UF2 processor and product validation");
  word(28, 123);
  durableWrite(uf2, block.data(), block.size());
  bool wrongFamily = false;
  try {
    inspectUf2(uf2);
  } catch (...) {
    wrongFamily = true;
  }
  require(wrongFamily,
          "Reject foreign UF2 processor without writing any volume");
  std::cout << "PASS process-crash rollback, permission failures, online "
               "response parser and local UF2 validation\n";
}
} // namespace core
