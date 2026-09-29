#include "DeviceView.h"
#include <thread>
namespace core {
DeviceView::DeviceView(Device &d, Look &l) : device(d), look(l) {
  setSize(780, 560);
  setLookAndFeel(&look);
  for (auto *c : std::initializer_list<juce::Component *>{
           &input, &output, &volume, &refreshButton, &version, &reset, &choose,
           &write, &instructions, &fileLabel, &error, &log})
    addAndMakeVisible(c);
  input.setTextWhenNothingSelected("MIDI input");
  output.setTextWhenNothingSelected("MIDI output");
  volume.setTextWhenNothingSelected("Detected RP2040 bootloader volume");
  instructions.setText(
      "Device connection is independent of the presentation and edited "
      "sample.\nVisualization needs opt-in telemetry firmware; normal firmware "
      "supports legacy status.",
      juce::dontSendNotification);
  log.setMultiLine(true);
  log.setReadOnly(true);
  auto selected = [this] {
    device.choose(input.getSelectedId() > 0
                      ? inputPorts[input.getSelectedId() - 1].identifier
                      : String(),
                  output.getSelectedId() > 0
                      ? outputPorts[output.getSelectedId() - 1].identifier
                      : String());
  };
  input.onChange = selected;
  output.onChange = selected;
  refreshButton.onClick = [this] { refresh(); };
  version.onClick = [this] {
    try {
      device.command(1);
    } catch (const std::exception &e) {
      error.setText(e.what(), juce::dontSendNotification);
    }
  };
  reset.onClick = [this] {
    auto safe = juce::Component::SafePointer<DeviceView>(this);
    juce::AlertWindow::showAsync(
        juce::MessageBoxOptions()
            .withTitle("Reset connected device")
            .withMessage("Stop playback and reset the selected device into its "
                         "USB bootloader?")
            .withButton("Reset")
            .withButton("Cancel"),
        [safe](int n) {
          if (safe && n == 1)
            try {
              safe->device.command(0);
            } catch (const std::exception &e) {
              safe->error.setText(e.what(), juce::dontSendNotification);
            }
        });
  };
  choose.onClick = [this] {
    chooser = std::make_unique<juce::FileChooser>(
        "Choose Core firmware from this computer", File(), "*.uf2");
    auto safe = juce::Component::SafePointer<DeviceView>(this);
    chooser->launchAsync(
        juce::FileBrowserComponent::openMode |
            juce::FileBrowserComponent::canSelectFiles,
        [safe](const juce::FileChooser &f) {
          if (!safe || !f.getResult().existsAsFile())
            return;
          try {
            auto info = inspectUf2(f.getResult());
            safe->image = f.getResult();
            safe->fileLabel.setText(info.product + ": " +
                                        safe->image.getFileName(),
                                    juce::dontSendNotification);
          } catch (const std::exception &e) {
            safe->image = File();
            safe->fileLabel.setText(e.what(), juce::dontSendNotification);
          }
        });
  };
  write.onClick = [this] { flash(); };
  refresh();
  startTimer(250);
}
DeviceView::~DeviceView() {
  stopTimer();
  stopping = true;
  if (worker.joinable())
    worker.join();
  setLookAndFeel(nullptr);
}
void DeviceView::refresh() {
  device.refresh();
  inputPorts = device.inputs;
  outputPorts = device.outputs;
  input.clear(juce::dontSendNotification);
  output.clear(juce::dontSendNotification);
  for (int i = 0; i < inputPorts.size(); ++i) {
    input.addItem(inputPorts[i].name, i + 1);
    if (inputPorts[i].identifier == device.inputId)
      input.setSelectedId(i + 1, juce::dontSendNotification);
  }
  for (int i = 0; i < outputPorts.size(); ++i) {
    output.addItem(outputPorts[i].name, i + 1);
    if (outputPorts[i].identifier == device.outputId)
      output.setSelectedId(i + 1, juce::dontSendNotification);
  }
  volumes = bootloaderVolumes();
  volume.clear(juce::dontSendNotification);
  for (int i = 0; i < volumes.size(); ++i)
    volume.addItem(volumes[i].getFullPathName(), i + 1);
  if (volumes.size() == 1)
    volume.setSelectedId(1, juce::dontSendNotification);
}
void DeviceView::flash() {
  if (working || image == File() || volume.getSelectedId() == 0)
    return;
  auto target = volumes[volume.getSelectedId() - 1];
  auto safe = juce::Component::SafePointer<DeviceView>(this);
  juce::AlertWindow::showAsync(
      juce::MessageBoxOptions()
          .withTitle("Flash local firmware")
          .withMessage("Write " + image.getFileName() + " to " +
                       target.getFullPathName() + "? The device will restart.")
          .withButton("Flash")
          .withButton("Cancel"),
      [safe, target](int n) {
        if (!safe || n != 1)
          return;
        if (safe->worker.joinable())
          safe->worker.join();
        safe->working = true;
        safe->progress = 0;
        auto *self = safe.getComponent();
        self->worker = std::thread([self, target, image = self->image] {
          String result;
          try {
            flashLocalUf2(
                image, target, [self](double p) { self->progress = p; },
                [self] { return self->stopping.load(); });
            result =
                "Firmware copy completed. Wait for the device to reconnect.";
          } catch (const std::exception &e) {
            result = e.what();
          }
          {
            std::lock_guard<std::mutex> lock(self->mutex);
            self->result = result;
          }
          self->working = false;
        });
      });
}
void DeviceView::timerCallback() {
  log.setText(device.log(), false);
  write.setEnabled(!working && image.existsAsFile() &&
                   volume.getSelectedId() > 0);
  if (working)
    error.setText("Copying firmware: " + String(int(progress * 100)) + "%",
                  juce::dontSendNotification);
  else {
    std::lock_guard<std::mutex> lock(mutex);
    if (result.isNotEmpty())
      error.setText(result, juce::dontSendNotification);
  }
}
void DeviceView::paint(juce::Graphics &g) { g.fillAll(look.theme.background); }
void DeviceView::resized() {
  instructions.setBounds(20, 15, getWidth() - 40, 52);
  int half = (getWidth() - 50) / 2;
  input.setBounds(20, 80, half, 30);
  output.setBounds(30 + half, 80, half, 30);
  refreshButton.setBounds(20, 125, 150, 30);
  version.setBounds(180, 125, 150, 30);
  reset.setBounds(340, 125, 200, 30);
  log.setBounds(20, 170, getWidth() - 40, getHeight() - 355);
  choose.setBounds(20, getHeight() - 170, 185, 30);
  fileLabel.setBounds(215, getHeight() - 170, getWidth() - 235, 30);
  volume.setBounds(20, getHeight() - 125, getWidth() - 260, 30);
  write.setBounds(getWidth() - 225, getHeight() - 125, 205, 30);
  error.setBounds(20, getHeight() - 75, getWidth() - 40, 55);
}
} // namespace core
