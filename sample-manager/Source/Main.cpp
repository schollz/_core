#include "../Tests/NativeDragFixture.h"
#include "AppView.h"
namespace core {
int runTests();
void crashTransaction(const juce::File &);
int diskFullTest(const juce::File &);
} // namespace core
std::shared_ptr<void> startMidiTests(std::function<void(int)>);
class CoreApplication final : public juce::JUCEApplication {
public:
  const juce::String getApplicationName() override {
    return "Core Sample Manager";
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
      setApplicationReturnValue(core::runTests());
      quit();
      return;
    }
    window =
        std::make_unique<Window>(command.contains("--drag-drop-acceptance"));
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
        : DocumentWindow("Core Sample Manager", juce::Colour(0xfff0f0f0),
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
