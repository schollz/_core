#pragma once
#include "Library.h"
#include "Midi.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace zv
{
struct Settings
{
    juce::String root, input, output;
    int width = 1000, height = 650;
    bool reduceMotion = false;
    juce::var toJson() const;
    static Settings fromJson(const juce::var &);
    static Settings defaults();
    void saveDefaults() const;
};
class Session : private juce::Timer
{
  public:
    Session();
    ~Session() override;
    Settings settings() const;
    // Safe from host state callbacks; all side effects happen on the message thread.
    void setSettings(Settings, bool persist = false);
    void rescan();
    void refreshPorts();
    void tick(double now);
    LibraryState libraryState;
    DeviceState device;
    std::shared_ptr<const Wave> wave;
    Playhead playhead;
    juce::Array<juce::MidiDeviceInfo> inputs, outputs;
    juce::String connection = "Waiting for zeptocore", connectionError;
    bool connected = false, choosePorts = false;
    int bank = -1, sample = -1;
    bool displayFresh(double now) const;
    // Deterministic diagnostics never open hardware or write preferences.
    void useFixture(std::shared_ptr<const Wave>, DeviceState, double now);

  private:
    void timerCallback() override;
    mutable juce::CriticalSection settingsMutex;
    Settings config;
    bool dirty = true, persistRequested = false, fixture = false;
    std::optional<Settings> applied;
    Library library;
    std::shared_ptr<MidiLink> midi;
    double lastPorts = -1e9;
    std::optional<double> lastDisplay;
};
} // namespace zv
