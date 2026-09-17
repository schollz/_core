#pragma once
#include "Session.h"
#include <juce_audio_processors/juce_audio_processors.h>
namespace zv
{
class Processor final : public juce::AudioProcessor
{
  public:
    Processor();
    const juce::String getName() const override
    {
        return JucePlugin_Name;
    }
    void prepareToPlay(double, int) override {}
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout &) const override;
    void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override {}
    void processBlock(juce::AudioBuffer<double> &, juce::MidiBuffer &) override {}
    void processBlockBypassed(juce::AudioBuffer<float> &, juce::MidiBuffer &) override {}
    void processBlockBypassed(juce::AudioBuffer<double> &, juce::MidiBuffer &) override {}
    bool supportsDoublePrecisionProcessing() const override
    {
        return true;
    }
    double getTailLengthSeconds() const override
    {
        return 0;
    }
    bool acceptsMidi() const override
    {
        return false;
    }
    bool producesMidi() const override
    {
        return false;
    }
    bool hasEditor() const override
    {
        return true;
    }
    juce::AudioProcessorEditor *createEditor() override;
    int getNumPrograms() override
    {
        return 1;
    }
    int getCurrentProgram() override
    {
        return 0;
    }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override
    {
        return {};
    }
    void changeProgramName(int, const juce::String &) override {}
    void getStateInformation(juce::MemoryBlock &) override;
    void setStateInformation(const void *, int) override;
    Session session;
};
} // namespace zv
