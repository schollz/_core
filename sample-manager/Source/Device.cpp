#include "Device.h"
#include "Storage.h"
namespace core {
Device::Device() {
  try {
    auto p = parseJson(preferencesFile());
    inputId = p["input"].toString();
    outputId = p["output"].toString();
    reduceMotion = bool(p["reduceMotion"]);
  } catch (...) {
  }
  refresh();
  startTimer(2000);
}
Device::~Device() {
  stopTimer();
  link.reset();
}
void Device::refresh() {
  inputs = juce::MidiInput::getAvailableDevices();
  outputs = juce::MidiOutput::getAvailableDevices();
  auto select = [](const auto &ports, const String &previous) {
    for (const auto &p : ports)
      if (p.identifier == previous)
        return previous;
    String found;
    int count = 0;
    for (const auto &p : ports)
      if (p.name.containsIgnoreCase("zeptocore") ||
          p.name.containsIgnoreCase("ectocore")) {
        found = p.identifier;
        ++count;
      }
    return count == 1 ? found : String();
  };
  auto in = select(inputs, inputId), out = select(outputs, outputId);
  if (in.isEmpty() || out.isEmpty()) {
    link.reset();
    error = "Select MIDI input and output for your Core device";
    return;
  }
  if (!link || link->inputId != in || link->outputId != out) {
    link = zv::MidiLink::acquire(in, out);
    link->setTelemetry(telemetryRequested);
  }
  link->refresh();
  error = link->error;
}
void Device::choose(String input, String output) {
  inputId = std::move(input);
  outputId = std::move(output);
  refresh();
  save();
}
void Device::telemetry(bool enabled) {
  telemetryRequested = enabled;
  if (link)
    link->setTelemetry(enabled);
}
void Device::save() {
  var p = object();
  try {
    p = parseJson(preferencesFile());
  } catch (...) {
  }
  put(p, "input", inputId);
  put(p, "output", outputId);
  put(p, "reduceMotion", reduceMotion);
  durableJson(preferencesFile(), p);
}
} // namespace core
