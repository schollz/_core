#include "PluginProcessor.h"
#include "VisualizerView.h"
#include <iostream>
int runTests();
int renderFixtures(const juce::File &);
int analyseFile(const juce::StringArray &);
std::shared_ptr<void> startMidiTests(std::function<void(int)>);
class VisualizerApplication final : public juce::JUCEApplication
{
  public:
    const juce::String getApplicationName() override
    {
        return JucePlugin_Name;
    }
    const juce::String getApplicationVersion() override
    {
        return JucePlugin_VersionString;
    }
    bool moreThanOneInstanceAllowed() override
    {
        return true;
    }
    void initialise(const juce::String &arguments) override
    {
        auto args = juce::StringArray::fromTokens(arguments, true);
        args.removeEmptyStrings();
        for (auto &arg : args)
            arg = arg.unquoted();
        if (args.size() == 1 && args[0] == "--self-test")
        {
            juce::MessageManager::callAsync(
                [this]
                {
                    setApplicationReturnValue(runTests());
                    quit();
                });
            return;
        }
        if (args.size() == 1 && args[0] == "--midi-test")
        {
            midiTest = startMidiTests(
                [this](int result)
                {
                    setApplicationReturnValue(result);
                    quit();
                });
            return;
        }
        if (args.size() == 2 && args[0] == "--render-fixtures")
        {
            juce::MessageManager::callAsync(
                [this, args]
                {
                    setApplicationReturnValue(renderFixtures(juce::File(args[1])));
                    quit();
                });
            return;
        }
        if (args.size() == 4 && args[0] == "--analyse")
        {
            juce::MessageManager::callAsync(
                [this, args]
                {
                    setApplicationReturnValue(analyseFile(args));
                    quit();
                });
            return;
        }
        if (args.size() > 0 && !(args.size() == 2 && args[0] == "--reference"))
        {
            std::cout << "Usage: zeptocore visualizer [--reference /path/to/sd-copy]\n"
                         "  --self-test\n  --midi-test\n  --render-fixtures /output/folder\n  --analyse "
                         "file.wav file.wav.info output.json\n";
            setApplicationReturnValue(args[0] == "--help" ? 0 : 2);
            quit();
            return;
        }
        processor = std::make_unique<zv::Processor>();
        if (args.size() == 2)
        {
            auto s = processor->session.settings();
            s.root = juce::File::getCurrentWorkingDirectory().getChildFile(args[1]).getFullPathName();
            processor->session.setSettings(s, true);
        }
        window = std::make_unique<Window>(*processor);
    }
    void shutdown() override
    {
        midiTest.reset();
        window.reset();
        if (processor)
            processor->session.settings().saveDefaults();
        processor.reset();
    }
    void systemRequestedQuit() override
    {
        quit();
    }

  private:
    class Window final : public juce::DocumentWindow
    {
      public:
        explicit Window(zv::Processor &p) : DocumentWindow(JucePlugin_Name, juce::Colours::black, allButtons)
        {
            setUsingNativeTitleBar(true);
            setContentOwned(p.createEditor(), true);
            setResizable(true, false);
            setResizeLimits(480, 320, 4000, 2400);
            centreWithSize(getWidth(), getHeight());
            setVisible(true);
        }
        void closeButtonPressed() override
        {
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        }
    };
    std::unique_ptr<zv::Processor> processor;
    std::shared_ptr<void> midiTest;
    std::unique_ptr<Window> window;
};
START_JUCE_APPLICATION(VisualizerApplication)
