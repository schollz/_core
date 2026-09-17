#include "PluginProcessor.h"
#include "VisualizerView.h"
namespace zv
{
Processor::Processor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
}
bool Processor::isBusesLayoutSupported(const BusesLayout &b) const
{
    auto out = b.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo()) &&
           b.getMainInputChannelSet() == out;
}
juce::AudioProcessorEditor *Processor::createEditor()
{
    return new Editor(*this);
}
void Processor::getStateInformation(juce::MemoryBlock &dest)
{
    auto text = juce::JSON::toString(session.settings().toJson());
    dest.replaceAll(text.toRawUTF8(), text.getNumBytesAsUTF8());
}
void Processor::setStateInformation(const void *bytes, int length)
{
    if (!bytes || length <= 0 || length > 1024 * 1024)
        return;
    auto v = juce::JSON::parse(juce::String::fromUTF8((const char *)bytes, length));
    if (v.isObject() && (int)v["version"] == 1)
        session.setSettings(Settings::fromJson(v));
}
} // namespace zv
juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter()
{
    return new zv::Processor();
}
