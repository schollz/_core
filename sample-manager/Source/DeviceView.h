#pragma once
#include "Device.h"
#include "Uf2.h"
#include <mutex>
#include <thread>
namespace core {
class DeviceView final : public juce::Component, private juce::Timer {
public:
  DeviceView(Device &, Look &);
  ~DeviceView() override;
  void resized() override;
  void paint(juce::Graphics &) override;

private:
  void timerCallback() override;
  void refresh();
  void flash();
  Device &device;
  Look &look;
  juce::ComboBox input, output, volume;
  juce::TextButton refreshButton{"Refresh ports"}, version{"Query version"},
      reset{"Reset to bootloader"}, choose{"Choose local UF2"},
      write{"Flash selected UF2"};
  juce::Label instructions, fileLabel, error;
  juce::TextEditor log;
  juce::Array<juce::MidiDeviceInfo> inputPorts, outputPorts;
  juce::Array<File> volumes;
  File image;
  std::unique_ptr<juce::FileChooser> chooser;
  std::thread worker;
  std::atomic<bool> stopping{false}, working{false};
  std::atomic<double> progress{0};
  std::mutex mutex;
  String result;
};
} // namespace core
