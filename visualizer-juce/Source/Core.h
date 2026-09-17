#pragma once
#include <juce_core/juce_core.h>
#include <array>
#include <optional>
#include <vector>
#include <functional>
#include <memory>

namespace zv
{
constexpr double staleMs = 1500.0;
inline double nowMs()
{
    return juce::Time::getMillisecondCounterHiRes();
}
inline bool fresh(std::optional<double> at, double now)
{
    return at && now - *at < staleMs;
}
struct Playback
{
    int bank = 0, sample = 0, slice = 0;
    uint32_t trigger = 0;
    int bpm = 0;
    bool forward = true, stopped = false, muted = false, valid = false, estimated = false;
    std::optional<uint16_t> effects;
};
struct Legacy
{
    int bank = 0, sample = 0, bpm = 0;
    bool stopped = false, muted = false;
};
struct Message
{
    enum Kind
    {
        view,
        info,
        button
    } kind;
    Playback playback;
    Legacy legacy;
    int pad = 0;
};
std::optional<Message> decode(const uint8_t *, size_t);
struct Slice
{
    double start = 0, stop = 0;
};
struct Wave
{
    int bank = 0, sample = 0, sampleRate = 44100, channels = 1, bpm = 0, playMode = 0;
    bool tempoMatch = false;
    double duration = 0;
    std::vector<Slice> slices;
    std::vector<std::vector<int16_t>> peaks;
    std::vector<uint8_t> spectrum; // frame-major: 32 bands, 20 Hz
};
Wave analyse(const juce::MemoryBlock &wav, const juce::MemoryBlock &info, int bank, int sample,
             std::function<bool()> cancelled = {});
juce::var waveToJson(const Wave &);
std::shared_ptr<const Wave> waveFromJson(const juce::var &);
std::array<float, 32> spectrumAt(const Wave *, std::optional<double> position);
class Playhead
{
  public:
    void update(const Playback &, std::shared_ptr<const Wave>, double at);
    std::optional<double> value(double now) const;
    void reset()
    {
        state.reset();
        wave.reset();
    }

  private:
    std::optional<Playback> state;
    std::shared_ptr<const Wave> wave;
    double at = 0, position = 0;
};
struct Display
{
    Playback state;
    double at;
};
class Transition
{
  public:
    Display update(const Playback &, double at);

  private:
    std::optional<Playback> previous;
    std::optional<Display> pending;
};
struct Press
{
    int button, bank, sample, slice;
    double at;
};
struct DeviceState
{
    std::optional<Playback> playback;
    std::optional<Display> display;
    std::optional<Legacy> legacy;
    std::optional<Press> press;
    std::optional<double> receivedAt, legacyAt;
    Transition transition;
    void receive(const Message &, double at);
};
} // namespace zv
