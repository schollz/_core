#pragma once
#include "Core.h"
#include <juce_audio_devices/juce_audio_devices.h>
#include <atomic>
#include <map>

namespace zv
{
class MidiLink final : private juce::MidiInputCallback, private juce::Timer
{
  public:
    static std::shared_ptr<MidiLink> acquire(const juce::String &input, const juce::String &output);
    ~MidiLink() override;
    DeviceState state;
    bool connected() const
    {
        return input != nullptr && output != nullptr;
    }
    juce::String error;
    const juce::String inputId, outputId;
    void refresh();

  private:
    MidiLink(juce::String in, juce::String out);
    void handleIncomingMidiMessage(juce::MidiInput *, const juce::MidiMessage &) override;
    void timerCallback() override;
    void close();
    struct Packet
    {
        std::array<uint8_t, 128> data{};
        size_t size = 0;
        double at = 0;
    };
    std::array<Packet, 512> queue;
    juce::CriticalSection queueMutex;
    size_t head = 0, tail = 0;
    std::atomic<bool> overflow{false};
    std::unique_ptr<juce::MidiInput> input;
    std::unique_ptr<juce::MidiOutput> output;
    double polled = -1e9, checked = -1e9;
};
} // namespace zv
