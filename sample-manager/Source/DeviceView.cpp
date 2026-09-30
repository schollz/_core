#include "DeviceView.h"
namespace core {
DeviceView::DeviceView(Device &d, Look &l, FirmwareHardware hardware)
    : device(d), look(l), firmware(d, l, hardware) {
  setLookAndFeel(&look);
  connection.onResize = [this] { layoutConnection(); };
  addAndMakeVisible(tabs);
  tabs.addTab("Connection", juce::Colours::transparentBlack, &connection, false);
  tabs.addTab("Firmware", juce::Colours::transparentBlack, &firmware, false);
  tabs.setCurrentTabIndex(1);
  tabs.setTabBarDepth(36);
  tabs.setOutline(0);
  lookAndFeelChanged();
  for (auto *c : std::initializer_list<juce::Component *>{&input, &output, &refreshButton, &version,
                                                          &instructions, &error, &log})
    connection.addAndMakeVisible(c);
  input.setTextWhenNothingSelected("MIDI input");
  output.setTextWhenNothingSelected("MIDI output");
  input.setTitle("MIDI input");
  output.setTitle("MIDI output");
  input.setTooltip(
      "Choose the MIDI input that receives status and telemetry from your instrument.");
  output.setTooltip("Choose the MIDI output used to send commands to your instrument.");
  refreshButton.setTooltip("Scan again for MIDI ports.");
  version.setTooltip(
      "Ask the selected instrument for its firmware version. The reply appears in the log.");
  log.setTooltip("Recent MIDI connection, status and device messages. Select text to copy it.");
  instructions.setText(
      "Device connection is independent of the presentation and edited sample.\n"
      "Open Firmware to download a UF2, view the installation guide, or flash your device.",
      juce::dontSendNotification);
  log.setMultiLine(true);
  log.setReadOnly(true);
  int order = 1;
  for (auto *c :
       std::initializer_list<juce::Component *>{&input, &output, &refreshButton, &version, &log})
    c->setExplicitFocusOrder(order++);
  auto selected = [this] {
    device.choose(
        input.getSelectedId() > 0 ? inputPorts[input.getSelectedId() - 1].identifier : String(),
        output.getSelectedId() > 0 ? outputPorts[output.getSelectedId() - 1].identifier : String());
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
  refresh();
  setSize(940, 800);
  startTimer(250);
}
DeviceView::~DeviceView() {
  stopTimer();
  tabs.clearTabs();
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
}
void DeviceView::timerCallback() {
  const auto text = device.log();
  if (text != log.getText())
    log.setText(text, false);
  error.setTooltip(error.getText());
}
void DeviceView::paint(juce::Graphics &g) { g.fillAll(look.theme.background); }
void DeviceView::lookAndFeelChanged() {
  auto &bar = tabs.getTabbedButtonBar();
  bar.setColour(juce::TabbedButtonBar::tabTextColourId, juce::Colour(0xff1a1a1a));
  bar.setColour(juce::TabbedButtonBar::frontTextColourId, juce::Colour(0xff1a1a1a));
  for (int i = 0; i < tabs.getNumTabs(); ++i)
    tabs.setTabBackgroundColour(i, look.theme.sidebar);
}
void DeviceView::resized() { tabs.setBounds(getLocalBounds()); }
void DeviceView::layoutConnection() {
  const int width = connection.getWidth(), height = connection.getHeight();
  instructions.setBounds(20, 15, width - 40, 70);
  const int half = (width - 50) / 2;
  input.setBounds(20, 100, half, 30);
  output.setBounds(30 + half, 100, half, 30);
  refreshButton.setBounds(20, 145, refreshButton.preferredWidth(30), 30);
  version.setBounds(refreshButton.getRight() + 10, 145, version.preferredWidth(30), 30);
  log.setBounds(20, 190, width - 40, std::max(100, height - 270));
  error.setBounds(20, height - 65, width - 40, 50);
}
} // namespace core
