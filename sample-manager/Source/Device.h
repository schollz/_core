#pragma once
#include "Theme.h"
#include "Visualizer/Midi.h"
namespace core {
class Device final : private juce::Timer {
public:
  Device();
  ~Device() override;
  juce::Array<juce::MidiDeviceInfo> inputs, outputs;
  String inputId, outputId, error;
  bool reduceMotion = false;
  void refresh();
  void choose(String input, String output);
  void telemetry(bool enabled);
  void command(int c) {
    require(link != nullptr, "Select a device first");
    link->command(c);
  }
  bool connected() const { return link && link->connected(); }
  zv::DeviceState state() const {
    return link ? link->state : zv::DeviceState();
  }
  String log() const {
    return link ? link->log.joinIntoString("\n") : String();
  }
  void save();

private:
  void timerCallback() override { refresh(); }
  std::shared_ptr<zv::MidiLink> link;
  bool telemetryRequested = false;
};
} // namespace core
