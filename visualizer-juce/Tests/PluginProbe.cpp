#include <juce_audio_utils/juce_audio_utils.h>
#include <iostream>
#include <cmath>
int main(int argc, char **argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    if (argc != 2)
    {
        std::cerr << "Usage: visualizer_plugin_probe /path/to/plugin.vst3|component\n";
        return 2;
    }
    juce::AudioPluginFormatManager manager;
    manager.addFormat(std::make_unique<juce::VST3PluginFormat>());
#if JUCE_MAC
    manager.addFormat(std::make_unique<juce::AudioUnitPluginFormat>());
#endif
    juce::OwnedArray<juce::PluginDescription> descriptions;
    std::cout << "Scanning plugin" << std::endl;
    for (auto *format : manager.getFormats())
        format->findAllTypesForFile(descriptions, juce::String::fromUTF8(argv[1]));
    if (descriptions.isEmpty())
    {
        std::cerr << "No plugin found\n";
        return 1;
    }
    for (auto *desc : descriptions)
    {
        std::cout << "Creating instance" << std::endl;
        juce::String error;
        auto instance = manager.createPluginInstance(*desc, 48000, 256, error);
        if (!instance)
        {
            std::cerr << error << std::endl;
            return 1;
        }
        instance->setPlayConfigDetails(2, 2, 48000, 256);
        std::cout << "Checking audio" << std::endl;
        instance->prepareToPlay(48000, 256);
        juce::AudioBuffer<float> buffer(2, 256), original(2, 256);
        juce::MidiBuffer midi;
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < 256; ++i)
                buffer.setSample(c, i, (float)std::sin(i * .03 + c));
        original.makeCopyOf(buffer);
        instance->processBlock(buffer, midi);
        for (int c = 0; c < 2; ++c)
            if (std::memcmp(buffer.getReadPointer(c), original.getReadPointer(c), 256 * sizeof(float)) != 0)
            {
                std::cerr << "Audio changed\n";
                return 1;
            }
        juce::MemoryBlock saved;
        std::cout << "Checking state" << std::endl;
        instance->getStateInformation(saved);
        instance->setStateInformation(saved.getData(), (int)saved.getSize());
        for (int i = 0; i < 2; ++i)
        {
            std::cout << "Checking editor " << i << std::endl;
            std::unique_ptr<juce::AudioProcessorEditor> editor(instance->createEditorAndMakeActive());
            if (!editor)
            {
                std::cerr << "No editor\n";
                return 1;
            }
            editor->setSize(800, 500);
            auto image = editor->createComponentSnapshot(editor->getLocalBounds());
            if (image.isNull())
                return 1;
            instance->editorBeingDeleted(editor.get());
        }
        instance->releaseResources();
        std::cout << "PASS " << desc->pluginFormatName << " / " << desc->name
                  << " / audio, state, editor reopen\n";
    }
    return 0;
}
