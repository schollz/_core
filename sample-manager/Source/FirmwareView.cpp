#include "FirmwareView.h"
#include <LucideData.h>

namespace core {
namespace {
void label(juce::Label &target, const String &text) {
  target.setText(text, juce::dontSendNotification);
  target.setTooltip(text);
  target.setJustificationType(juce::Justification::topLeft);
}
} // namespace
FirmwareView::GuideStep::GuideStep(Look &l, String n, String t, String b, String name)
    : look(l), number(n) {
  addAndMakeVisible(title);
  addAndMakeVisible(body);
  label(title, n + ". " + t);
  label(body, b);
  int size = 0;
  const auto *data =
      LucideData::getNamedResource((name.removeCharacters("-") + "_svg").toRawUTF8(), size);
  if (data)
    icon = juce::Drawable::createFromSVGString(
        String::fromUTF8(data, size).replace("currentColor", "#555555"));
}
void FirmwareView::GuideStep::resized() {
  title.setBounds(12, 12, getWidth() - 60, 37);
  body.setBounds(12, 53, getWidth() - 24, getHeight() - 61);
}
void FirmwareView::GuideStep::paint(juce::Graphics &g) {
  g.setColour(look.theme.sidebar.withAlpha(.4f));
  g.fillRoundedRectangle(getLocalBounds().toFloat(), 8.f);
  g.setColour(look.theme.accent.withAlpha(.5f));
  g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(.5f), 8.f, 1.f);
  if (icon)
    icon->drawWithin(g, {float(getWidth() - 42), 15.f, 26.f, 26.f},
                     juce::RectanglePlacement::centred, 1.f);
}
FirmwareView::FirmwareView(Device &d, Look &l, FirmwareHardware initial) : device(d), look(l) {
  setLookAndFeel(&look);
  addAndMakeVisible(viewport);
  viewport.setViewedComponent(&content, false);
  viewport.setScrollBarsShown(true, false);
  viewport.setScrollOnDragMode(juce::Viewport::ScrollOnDragMode::never);
  for (auto *c : std::initializer_list<juce::Component *>{
           &heading,        &hardwareLabel,  &buildLabel, &modelNote, &release,      &description,
           &downloadStatus, &installHeading, &fileLabel,  &error,     &guideHeading, &hardware,
           &build,          &volume,         &get,        &cancel,    &reveal,       &docs,
           &reset,          &rescan,         &choose,     &write,     &progressBar})
    content.addAndMakeVisible(c);
  label(heading, "Firmware downloads");
  label(hardwareLabel, "Hardware model");
  label(buildLabel, "Firmware build");
  label(modelNote, "Choose your physical hardware. Ectocore may appear as ezeptocore over MIDI.");
  label(installHeading, "Install a UF2");
  label(fileLabel, "Download firmware above or choose a local UF2.");
  label(guideHeading, "Download and install");
  for (auto *h : {&heading, &installHeading, &guideHeading})
    h->setFont(look.font(17));
  hardware.addItemList({"Ezeptocore", "Zeptocore", "Ectocore"}, 1);
  hardware.setSelectedId(int(initial) + 1, juce::dontSendNotification);
  hardware.setTitle("Hardware model");
  build.setTitle("Firmware build");
  volume.setTitle("Bootloader volume");
  hardware.setTooltip("Select the physical hardware to download its dedicated firmware. MIDI names "
                      "do not distinguish Ectocore and Ezeptocore. Changing the app's theme also "
                      "selects the matching hardware here.");
  build.setTooltip("Normal suits most uses. Choose Visualizer for full device visualization.");
  volume.setTextWhenNothingSelected("Select detected RPI-RP2 bootloader volume");
  volume.setTooltip("Select the connected RP2040 bootloader volume to receive the selected UF2.");
  get.setTooltip("Download this README-listed build to Downloads and check it. Flashing is a "
                 "separate action.");
  cancel.setTooltip("Cancel the current firmware download and remove its partial file.");
  reveal.setTooltip("Show the last successfully downloaded UF2 in your Downloads folder.");
  reset.setTooltip("Stop playback and reset the MIDI-connected device to its USB bootloader after "
                   "confirmation.");
  rescan.setTooltip("Scan for bootloader drives after resetting or connecting your instrument.");
  choose.setTooltip("Choose and check a UF2 from this computer. Selecting it does not flash it.");
  write.setTooltip("Copy the selected UF2 to the selected bootloader volume after confirmation. "
                   "The device then restarts.");
  progressBar.setPercentageDisplay(true);
  progressBar.setVisible(false);
  int order = 1;
  for (auto *c :
       std::initializer_list<juce::Component *>{&hardware, &build, &get, &cancel, &reveal, &choose,
                                                &reset, &rescan, &volume, &write, &docs})
    c->setExplicitFocusOrder(order++);
  steps[0] = std::make_unique<GuideStep>(look, "1", "Choose hardware", "", "settings-2");
  steps[1] = std::make_unique<GuideStep>(
      look, "2", "Download the UF2",
      "Click Download UF2. The checked file is saved in Downloads and selected for installation.",
      "download");
  steps[2] =
      std::make_unique<GuideStep>(look, "3", "Enter bootloader",
                                  "Connect USB. Use Reset to bootloader with MIDI, or follow your "
                                  "hardware guide. Refresh drives and select RPI-RP2.",
                                  "usb");
  steps[3] = std::make_unique<GuideStep>(look, "4", "Flash and reconnect",
                                         "Check the filename and drive. Click Flash selected UF2, "
                                         "confirm, and wait for the device to restart.",
                                         "file-up");
  for (auto &step : steps)
    content.addAndMakeVisible(*step);
  hardware.onChange = [this] { updateBuilds(); };
  build.onChange = [this] { updateEntry(); };
  get.onClick = [this] {
    if (busy())
      return;
    try {
      download.start(selectedEntry(), firmwareDownloadsDirectory());
      awaitingDownload = true;
    } catch (const std::exception &e) {
      label(downloadStatus, e.what());
    }
    timerCallback();
  };
  cancel.onClick = [this] { download.cancel(); };
  reveal.onClick = [this] {
    if (downloadedFile.existsAsFile())
      downloadedFile.revealToUser();
  };
  docs.onClick = [this] {
    if (!juce::URL(firmwareGuideUrl(selectedEntry().hardware)).launchInDefaultBrowser())
      label(error, "Could not open your browser. " + firmwareGuideUrl(selectedEntry().hardware));
  };
  reset.onClick = [this] {
    if (busy())
      return;
    confirming = true;
    timerCallback();
    auto safe = juce::Component::SafePointer<FirmwareView>(this);
    juce::AlertWindow::showAsync(
        juce::MessageBoxOptions()
            .withTitle("Reset connected device")
            .withMessage(
                "Stop playback and reset the selected MIDI device into its USB bootloader?")
            .withButton("Reset")
            .withButton("Cancel"),
        [safe](int n) {
          if (!safe)
            return;
          safe->confirming = false;
          if (n == 1)
            try {
              safe->device.command(0);
            } catch (const std::exception &e) {
              label(safe->error, e.what());
            }
          safe->timerCallback();
        });
  };
  rescan.onClick = [this] { refreshVolumes(); };
  choose.onClick = [this] {
    if (busy())
      return;
    choosing = true;
    timerCallback();
    chooser = std::make_unique<juce::FileChooser>("Choose Core firmware from this computer", File(),
                                                  "*.uf2");
    auto safe = juce::Component::SafePointer<FirmwareView>(this);
    chooser->launchAsync(juce::FileBrowserComponent::openMode |
                             juce::FileBrowserComponent::canSelectFiles,
                         [safe](const juce::FileChooser &f) {
                           if (!safe)
                             return;
                           safe->choosing = false;
                           if (f.getResult().existsAsFile())
                             try {
                               safe->selectImage(f.getResult(), inspectUf2(f.getResult()).product);
                             } catch (const std::exception &e) {
                               label(safe->error, e.what());
                             }
                           safe->timerCallback();
                         });
  };
  write.onClick = [this] { flash(); };
  updateBuilds();
  refreshVolumes();
  timerCallback();
  startTimer(100);
}
FirmwareView::~FirmwareView() {
  stopTimer();
  download.cancel();
  stopping = true;
  if (worker.joinable())
    worker.join();
  viewport.setViewedComponent(nullptr, false);
  setLookAndFeel(nullptr);
}
bool FirmwareView::busy() const {
  return awaitingDownload || working || confirming || choosing || download.snapshot().active();
}
void FirmwareView::setHardware(FirmwareHardware model) {
  // Keep the latest theme choice until an active operation has completed.
  pendingHardwareId = int(model) + 1;
  timerCallback();
}
void FirmwareView::updateBuilds() {
  entries.clear();
  build.clear(juce::dontSendNotification);
  for (const auto &entry : firmwareCatalog())
    if (entry.hardware == FirmwareHardware(hardware.getSelectedId() - 1)) {
      entries.push_back(&entry);
      build.addItem(firmwareBuildName(entry.build), int(entries.size()));
    }
  build.setSelectedId(1, juce::dontSendNotification);
  updateEntry();
}
const FirmwareEntry &FirmwareView::selectedEntry() const {
  return *entries.at(size_t(build.getSelectedId() - 1));
}
void FirmwareView::updateEntry() {
  const auto &entry = selectedEntry();
  label(release, entry.version + "  /  " + entry.filename);
  release.setTooltip(entry.url);
  label(description, firmwareDescription(entry));
  label(steps[0]->body,
        "Selected: " + firmwareHardwareName(entry.hardware) +
            ". Choose Normal for everyday use, or Visualizer for full device visualization.");
  docs.setButtonText(firmwareHardwareName(entry.hardware) + " bootloader guide");
  docs.setTooltip("Open your hardware documentation for manual bootloader entry.\n" +
                  firmwareGuideUrl(entry.hardware));
}
void FirmwareView::selectImage(const File &file, const String &product) {
  image = file;
  label(fileLabel, product + ": " + file.getFileName());
  fileLabel.setTooltip(file.getFullPathName());
  label(error, "");
}
void FirmwareView::refreshVolumes() {
  const auto previous = volume.getSelectedId() > 0 ? volumes[volume.getSelectedId() - 1] : File();
  volumes = bootloaderVolumes();
  volume.clear(juce::dontSendNotification);
  for (int i = 0; i < volumes.size(); ++i) {
    volume.addItem(volumes[i].getFullPathName(), i + 1);
    if (volumes[i] == previous || volumes.size() == 1)
      volume.setSelectedId(i + 1, juce::dontSendNotification);
  }
}
void FirmwareView::flash() {
  if (busy() || !image.existsAsFile() || volume.getSelectedId() == 0)
    return;
  const auto target = volumes[volume.getSelectedId() - 1];
  const auto selected = image;
  confirming = true;
  timerCallback();
  auto safe = juce::Component::SafePointer<FirmwareView>(this);
  juce::AlertWindow::showAsync(
      juce::MessageBoxOptions()
          .withTitle("Flash local firmware")
          .withMessage("Write " + selected.getFileName() + " to " + target.getFullPathName() +
                       "? Check that the UF2 matches your hardware. The device will restart.")
          .withButton("Flash")
          .withButton("Cancel"),
      [safe, target, selected](int n) {
        if (!safe)
          return;
        safe->confirming = false;
        if (n != 1) {
          safe->timerCallback();
          return;
        }
        if (safe->worker.joinable())
          safe->worker.join();
        safe->working = true;
        safe->flashProgress = 0;
        safe->timerCallback();
        auto *self = safe.getComponent();
        self->worker = std::thread([self, target, selected] {
          String message;
          try {
            flashLocalUf2(
                selected, target, [self](double p) { self->flashProgress = p; },
                [self] { return self->stopping.load(); });
            message = "Firmware copy completed. Wait for the device to reconnect.";
          } catch (const std::exception &e) {
            message = e.what();
          }
          {
            std::lock_guard<std::mutex> lock(self->mutex);
            self->result = message;
          }
          self->working = false;
        });
      });
}
void FirmwareView::timerCallback() {
  const auto state = download.snapshot();
  if (awaitingDownload) {
    label(downloadStatus, state.message);
    downloadProgress = state.total > 0 ? double(state.received) / double(state.total) : -1.;
    if (!state.active()) {
      awaitingDownload = false;
      if (state.status == FirmwareDownload::Status::succeeded) {
        downloadedFile = state.file;
        selectImage(state.file, firmwareHardwareName(selectedEntry().hardware));
      }
    }
  }
  if (pendingHardwareId != 0 && !awaitingDownload && !busy()) {
    const auto id = pendingHardwareId;
    pendingHardwareId = 0;
    hardware.setSelectedId(id, juce::sendNotificationSync);
  }
  const bool active = busy();
  for (auto *c :
       std::initializer_list<juce::Component *>{&hardware, &build, &get, &choose, &volume, &rescan})
    c->setEnabled(!active);
  reset.setEnabled(!active && device.connected());
  write.setEnabled(!active && image.existsAsFile() && volume.getSelectedId() > 0);
  cancel.setEnabled(state.active());
  reveal.setEnabled(downloadedFile.existsAsFile());
  progressBar.setVisible(state.active());
  if (working)
    label(error, "Copying firmware: " + String(int(flashProgress * 100)) + "%");
  else {
    std::lock_guard<std::mutex> lock(mutex);
    if (result.isNotEmpty()) {
      label(error, result);
      result.clear();
    }
  }
}
void FirmwareView::resized() {
  viewport.setBounds(getLocalBounds());
  const int width = std::max(360, getWidth() - viewport.getScrollBarThickness());
  const bool sideBySide = width >= 830;
  const int column = sideBySide ? width - 350 : width - 40;
  int y = 20;
  auto line = [&](juce::Component &c, int height, int gap = 8) {
    c.setBounds(20, y, column, height);
    y += height + gap;
  };
  line(heading, 26);
  if (column >= 540) {
    hardwareLabel.setBounds(20, y, column / 2 - 10, 22);
    buildLabel.setBounds(20 + column / 2, y, column / 2, 22);
    y += 24;
    hardware.setBounds(20, y, column / 2 - 10, 32);
    build.setBounds(20 + column / 2, y, column / 2, 32);
    y += 42;
  } else {
    line(hardwareLabel, 22, 2);
    line(hardware, 32);
    line(buildLabel, 22, 2);
    line(build, 32, 10);
  }
  line(modelNote, 43);
  line(release, 44, 4);
  line(description, 76);
  int x = 20;
  for (auto *button : {&get, &cancel, &reveal}) {
    const auto bw = button->preferredWidth(32);
    if (x + bw > 20 + column) {
      x = 20;
      y += 40;
    }
    button->setBounds(x, y, bw, 32);
    x += bw + 8;
  }
  y += 40;
  line(progressBar, 14, 4);
  line(downloadStatus, 78, 16);
  line(installHeading, 26);
  choose.setBounds(20, y, choose.preferredWidth(32), 32);
  y += 40;
  line(fileLabel, 48);
  x = 20;
  for (auto *button : {&reset, &rescan}) {
    const auto bw = button->preferredWidth(32);
    if (x + bw > 20 + column) {
      x = 20;
      y += 40;
    }
    button->setBounds(x, y, bw, 32);
    x += bw + 8;
  }
  y += 40;
  line(volume, 32);
  write.setBounds(20, y, write.preferredWidth(32), 32);
  y += 40;
  line(error, 72);
  const int leftHeight = y;
  const int guideX = sideBySide ? width - 310 : 20;
  const int guideWidth = sideBySide ? 290 : column;
  y = sideBySide ? 20 : y + 12;
  guideHeading.setBounds(guideX, y, guideWidth, 26);
  y += 34;
  for (auto &step : steps) {
    const int h = sideBySide ? 154 : 128;
    step->setBounds(guideX, y, guideWidth, h);
    y += h + 12;
  }
  docs.setBounds(guideX, y, guideWidth, 34);
  content.setSize(width, std::max(leftHeight, y + 54));
}
} // namespace core
