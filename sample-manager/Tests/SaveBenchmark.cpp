#include "Manager.h"
#include <iostream>
namespace core {
void saveBenchmark() {
  auto parent = File::getSpecialLocation(File::tempDirectory);
  const auto location = juce::SystemStats::getEnvironmentVariable("CORE_SAVE_BENCHMARK_DIR", "");
  if (location.isNotEmpty()) {
    require(File::isAbsolutePath(location) && File(location).isDirectory(),
            "Benchmark location must be an existing absolute directory");
    parent = File(location);
  }
  const auto workspace = parent.getChildFile("core-save-benchmark-" + uuid());
  std::error_code error;
  require(std::filesystem::create_directory(
              std::filesystem::u8path(workspace.getFullPathName().toStdString()), error) &&
              !error,
          "Create save benchmark fixture without recreating missing parents");
  struct Cleanup {
    File file;
    ~Cleanup() { file.deleteRecursively(); }
  } cleanup{workspace};
  const auto input = workspace.getChildFile("benchmark-bpm120.wav");
  {
    auto output = input.createOutputStream();
    card::writeHeader(*output, 20 * 44100, 44100, 2);
    for (int n = 0; n < 20 * 44100; ++n) {
      output->writeShort(short(std::sin(n * .047) * 14000));
      output->writeShort(short(std::sin(n * .059) * 12000));
    }
  }
  const auto folder = workspace.getChildFile("project");
  require(folder.createDirectory().wasOk(), "Create populated benchmark project");
#ifdef CORE_MANAGER_BASELINE
  Manager manager;
#else
  Manager manager{std::chrono::milliseconds(20)};
#endif
  auto settle = [&] {
    for (int n = 0; n < 30000; ++n) {
      auto state = manager.snapshot();
      if (!state.busy) {
        require(state.error.isEmpty(), state.error);
        return state;
      }
      juce::Thread::sleep(5);
    }
    throw std::runtime_error("Save benchmark timed out");
  };
  manager.open(folder);
  settle();
  juce::StringArray paths;
  for (int n = 0; n < 16; ++n)
    paths.add(input.getFullPathName());
  for (int bank = 0; bank < 3; ++bank) {
    manager.import(paths, bank);
    settle();
  }
  auto ready = settle();
  require(ready.project.samples.size() == 48, "Benchmark has 48 populated slots");
  const auto id = ready.project.samples.front().id;
  const auto wav = child(folder, card::path(0, 0));
  const auto before = hashFile(wav);
  const auto modified = wav.getLastModificationTime();
  for (int trial = 0; trial < 5; ++trial) {
    diagnostics::log("BENCHMARK", "Trial=" + String(trial + 1));
    const auto started = std::chrono::steady_clock::now();
    manager.even(id, 17 + trial);
    const auto saved = settle();
    const auto elapsed =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started)
            .count();
    require(hashFile(wav) == before && wav.getLastModificationTime() == modified,
            "Benchmark metadata preserves audio");
    std::cout << "SAVE_BENCHMARK trial=" << trial + 1 << " edit_to_idle_ms=" << elapsed
              << " analysed=" << saved.library.prepared << " reused=" << saved.library.reused
              << std::endl;
  }
}
} // namespace core
