#pragma once
#include "Device.h"
#include "FirmwareView.h"
namespace core {
class DeviceView final : public juce::Component, private juce::Timer {
public:
  DeviceView(Device &, Look &, FirmwareHardware);
  ~DeviceView() override;
  void resized() override;
  void paint(juce::Graphics &) override;
  void lookAndFeelChanged() override;
  void cancelDownload() { firmware.cancelDownload(); }

private:
  void timerCallback() override;
  void refresh();
  void layoutConnection();
  Device &device;
  Look &look;
  juce::TabbedComponent tabs{juce::TabbedButtonBar::TabsAtTop};
  struct ConnectionPanel : juce::Component {
    std::function<void()> onResize;
    void resized() override {
      if (onResize)
        onResize();
    }
  } connection;
  FirmwareView firmware;
  juce::ComboBox input, output;
  IconButton refreshButton{"Refresh ports", "refresh-cw"}, version{"Query version", "info"};
  juce::Label instructions, error;
  juce::TextEditor log;
  juce::Array<juce::MidiDeviceInfo> inputPorts, outputPorts;
  Tooltips tooltips{*this};
};
} // namespace core
