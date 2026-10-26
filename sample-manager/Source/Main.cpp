#include "../Tests/NativeDragFixture.h"
#include "AppView.h"
#include "ScreenWaveforms.h"
#include <iostream>
namespace core {
int runTests(const String &suite);
void crashTransaction(const juce::File &);
int diskFullTest(const juce::File &);
} // namespace core
std::shared_ptr<void> startMidiTests(std::function<void(int)>);
class CoreApplication final : public juce::JUCEApplication {
public:
  const juce::String getApplicationName() override {
    return JUCE_APPLICATION_NAME_STRING;
  }
  const juce::String getApplicationVersion() override {
    return JUCE_APPLICATION_VERSION_STRING;
  }
  void initialise(const juce::String &command) override {
    if (core::diagnostics::enabled()) {
      logger = std::make_unique<core::diagnostics::JuceLogger>();
      juce::Logger::setCurrentLogger(logger.get());
      core::diagnostics::log("APP", "Starting version=" + getApplicationVersion() +
                                       " JUCE=" + juce::SystemStats::getJUCEVersion() +
                                       " OS=" + juce::SystemStats::getOperatingSystemName() +
                                       " CPUs=" + juce::String(juce::SystemStats::getNumCpus()) +
                                       " memory_mb=" + juce::String(juce::SystemStats::getMemorySizeInMegabytes()));
      core::diagnostics::log("APP", "Executable=" + juce::File::getSpecialLocation(
          juce::File::currentExecutableFile).getFullPathName() +
          " state_root=" + core::stateRoot().getFullPathName() + " arguments=" + command);
    }
    if (command.startsWith("--prepare-screen-waveforms ")) {
      try {
        auto path = command.fromFirstOccurrenceOf(" ", false, false).unquoted();
        core::Manager manager;
        manager.setAutoSave(false);
        manager.open(juce::File(path));
        juce::String previous;
        const auto deadline = juce::Time::getMillisecondCounterHiRes() + 1800000.;
        do {
          auto state = manager.snapshot();
          if (state.status != previous) {
            std::cout << state.status << std::endl;
            previous = state.status;
          }
          if (juce::Time::getMillisecondCounterHiRes() >= deadline)
            throw std::runtime_error("Screen preparation timed out");
          juce::Thread::sleep(20);
        } while (!manager.idle());
        const auto state = manager.snapshot();
        core::require(state.error.isEmpty() && state.available, state.error);
        unsigned ready = 0, expected = 0;
        for (const auto &sample : state.completedProject.samples) {
          if (sample.protectedEntry) continue;
          ++expected;
          const auto file = core::child(state.root, core::screenwaves::path(sample.bank,sample.slot));
          juce::MemoryBlock wave, info;
          const auto audioPath = core::card::path(sample.bank,sample.slot);
          const auto audioHash = state.completedProject.fingerprints.find(audioPath);
          if (file.loadFileAsData(wave) &&
              core::child(state.root,audioPath+".info").loadFileAsData(info) &&
              audioHash != state.completedProject.fingerprints.end()) {
            const auto meta = core::card::decode(info);
            const auto *p = static_cast<const uint8_t *>(wave.getData());
            bool valid = sw_valid(p,wave.getSize(),meta.size,meta.rate,meta.channels,
                sw_info_hash(static_cast<const uint8_t *>(info.getData()),info.getSize()));
            for (int n=0; valid && n<32; ++n)
              valid = p[24+n] == uint8_t(audioHash->second.substring(n*2,n*2+2).getHexValue32());
            if (valid) ++ready;
          }
        }
        std::cout << "Screen waveforms ready: " << ready << "/" << expected << std::endl;
        core::require(ready == expected, "Some screen waveforms could not be prepared");
      } catch (const std::exception &e) {
        std::cerr << e.what() << std::endl;
        setApplicationReturnValue(1);
      }
      quit();
      return;
    }
    if (command.startsWith("--crash-transaction ")) {
      auto path = command.fromFirstOccurrenceOf(" ", false, false).unquoted();
      core::crashTransaction(juce::File(path));
      quit();
      return;
    }
    if (command.startsWith("--disk-full-test ")) {
      auto path = command.fromFirstOccurrenceOf(" ", false, false).unquoted();
      setApplicationReturnValue(core::diskFullTest(juce::File(path)));
      quit();
      return;
    }
    if (command.contains("--midi-test")) {
      midiTest = startMidiTests([this](int result) {
        setApplicationReturnValue(result);
        quit();
      });
      return;
    }
    if (command.contains("--self-test")) {
      setApplicationReturnValue(
          core::runTests(command.fromFirstOccurrenceOf("--self-test", false, false).trim()));
      quit();
      return;
    }
    window =
        std::make_unique<Window>(command.contains("--drag-drop-acceptance"));
  }
  void systemRequestedQuit() override {
    if (window)
      if (auto *view = dynamic_cast<core::AppView *>(window->getContentComponent())) {
        view->requestClose([this] { quit(); });
        return;
      }
    quit();
  }
  void shutdown() override {
    core::diagnostics::log("APP", "Shutdown requested");
    midiTest.reset();
    window.reset();
    core::diagnostics::log("APP", "Shutdown complete");
    if (logger) {
      juce::Logger::setCurrentLogger(nullptr);
      logger.reset();
    }
  }

private:
  std::unique_ptr<core::diagnostics::JuceLogger> logger;
  class Window : public juce::DocumentWindow {
  public:
    explicit Window(bool dragAcceptance)
        : DocumentWindow(JUCE_APPLICATION_NAME_STRING, juce::Colour(0xfff0f0f0),
                         allButtons) {
      setUsingNativeTitleBar(true);
      setContentOwned(new core::AppView, true);
      if (dragAcceptance) {
        fixture = std::make_unique<NativeDragFixture>();
        getContentComponent()->addAndMakeVisible(*fixture);
        fixture->setBounds(360, 12, 330, 38);
      }
      setResizable(true, true);
      setResizeLimits(1120, 840, 2400, 1600);
      centreWithSize(1320, 920);
      setVisible(true);
    }
    std::unique_ptr<NativeDragFixture> fixture;
    void closeButtonPressed() override {
      juce::JUCEApplication::getInstance()->systemRequestedQuit();
    }
  };
  std::shared_ptr<void> midiTest;
  std::unique_ptr<Window> window;
};
START_JUCE_APPLICATION(CoreApplication)
