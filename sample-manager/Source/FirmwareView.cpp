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
           &heading, &hardwareLabel, &versionLabel, &buildLabel, &modelNote, &release,
           &description, &catalogStatus, &downloadStatus, &guideHeading, &hardware, &version,
           &build, &get, &cancel, &reveal, &docs, &refresh, &progressBar, &installHeading,
           &installStatus, &reset, &rescan, &volume, &write})
    content.addAndMakeVisible(c);
  label(heading, "Choose, download, install");
  label(hardwareLabel, "Hardware model");
  label(versionLabel, "Firmware version");
  label(buildLabel, "Firmware build");
  label(modelNote, "Choose your physical hardware. Ectocore may appear as ezeptocore over MIDI.");
  label(guideHeading, "How to install");
  label(installHeading, "Install downloaded firmware");
  label(installStatus, "Download the selected version to enable installation.");
  for (auto *h : {&heading, &guideHeading, &installHeading})
    h->setFont(look.font(17));
  hardware.addItemList({"Ezeptocore", "Zeptocore", "Ectocore"}, 1);
  hardware.setSelectedId(int(initial) + 1, juce::dontSendNotification);
  hardware.setTitle("Hardware model");
  version.setTitle("Firmware version");
  build.setTitle("Firmware build");
  volume.setTitle("Bootloader volume");
  volume.setTextWhenNothingSelected("Connect your device in bootloader mode (RPI-RP2)");
  volume.setTooltip("A single detected RPI-RP2 drive is selected automatically. Choose your device if several are connected.");
  hardware.setTooltip("Select your physical hardware. Changing the app's theme also selects the "
                      "matching hardware here.");
  version.setTooltip("Choose an available release. The latest version is selected by default.");
  build.setTooltip("Only builds published for this hardware and version are shown. "
                   "Choose Visualizer for full device visualization.");
  get.setTooltip("Download the selected UF2 to Downloads and check the completed file.");
  cancel.setTooltip("Cancel the download or version lookup.");
  reveal.setTooltip("Show the last successfully downloaded UF2 in your Downloads folder.");
  refresh.setTooltip("Check GitHub for available firmware versions and builds.");
  reset.setTooltip("After downloading, stop playback and reset the MIDI-connected device to its USB bootloader after confirmation.");
  rescan.setTooltip("Look for RPI-RP2 bootloader drives. Drives are also detected automatically after downloading.");
  write.setTooltip("Install the validated download for the selected hardware, version, and build after confirmation.");
  progressBar.setPercentageDisplay(true);
  progressBar.setVisible(false);
  int order = 1;
  for (auto *c : std::initializer_list<juce::Component *>{&hardware, &version, &build, &refresh,
                                                         &get, &cancel, &reveal, &reset, &rescan,
                                                         &volume, &write, &docs})
    c->setExplicitFocusOrder(order++);
  steps[0] = std::make_unique<GuideStep>(look, "1", "Choose firmware", "", "settings-2");
  steps[1] = std::make_unique<GuideStep>(
      look, "2", "Download the UF2",
      "Click Download UF2. The checked file is saved in Downloads and becomes ready to install.", "download");
  steps[2] = std::make_unique<GuideStep>(
      look, "3", "Enter bootloader",
      "Connect USB. Use Reset to bootloader with MIDI, or follow your hardware guide. Select the detected RPI-RP2 drive.", "usb");
  steps[3] = std::make_unique<GuideStep>(
      look, "4", "Install and reconnect",
      "Click Install firmware and confirm the version and drive. Wait for installation to finish and the device to restart.", "file-up");
  for (auto &step : steps)
    content.addAndMakeVisible(*step);
  hardware.onChange = [this] { updateVersions(); };
  version.onChange = [this] {
    versionChosen = true;
    updateBuilds();
  };
  build.onChange = [this] { updateEntry(); };
  refresh.onClick = [this] { refreshVersions(); };
  get.onClick = [this] {
    if (busy() || entries.empty())
      return;
    try {
      download.start(selectedEntry(), firmwareDownloadsDirectory());
      awaitingDownload = true;
    } catch (const std::exception &e) {
      label(downloadStatus, e.what());
    }
    timerCallback();
  };
  cancel.onClick = [this] { cancelDownload(); };
  reveal.onClick = [this] {
    if (downloadedFile.existsAsFile())
      downloadedFile.revealToUser();
  };
  docs.onClick = [this] {
    const auto url = firmwareGuideUrl(FirmwareHardware(hardware.getSelectedId() - 1));
    if (!juce::URL(url).launchInDefaultBrowser())
      label(downloadStatus, "Could not open your browser. " + url);
  };
  reset.onClick = [this] {
    if (busy() || !downloadMatchesSelection() || !device.connected())
      return;
    confirming = true;
    const auto epoch = ++confirmationEpoch;
    timerCallback();
    auto safe = juce::Component::SafePointer<FirmwareView>(this);
    confirm(juce::MessageBoxOptions().withTitle("Reset connected device")
                .withMessage("Stop playback and reset the selected MIDI device into its USB bootloader?")
                .withButton("Reset").withButton("Cancel"),
            [safe, epoch](int n) {
              if (!safe || safe->confirmationEpoch != epoch)
                return;
              safe->confirming = false;
              if (n == 1)
                try {
                  safe->device.command(0);
                } catch (const std::exception &e) {
                  label(safe->installStatus, e.what());
                }
              safe->refreshVolumes();
              safe->timerCallback();
            });
  };
  rescan.onClick = [this] { if (!busy()) refreshVolumes(); };
  write.onClick = [this] { install(); };
  updateVersions();
  timerCallback();
  startTimer(100);
}
FirmwareView::~FirmwareView() {
  stopTimer();
  cancelDownload();
  stopping = true;
  if (installWorker.joinable())
    installWorker.join();
  viewport.setViewedComponent(nullptr, false);
  setLookAndFeel(nullptr);
}
bool FirmwareView::busy() const {
  return awaitingDownload || awaitingVersions || download.snapshot().active() || confirming || awaitingInstall;
}
void FirmwareView::loadVersions() {
  if (!versionsLoaded && !busy())
    refreshVersions();
}
void FirmwareView::refreshVersions() {
  if (busy())
    return;
  preferredVersion = onlineCatalog && versionChosen && version.getSelectedId() > 0
                         ? versions[version.getSelectedId() - 1] : String();
  releases.start(releaseEndpoint);
  awaitingVersions = true;
  timerCallback();
}
void FirmwareView::setHardware(FirmwareHardware model) {
  pendingHardwareId = int(model) + 1;
  timerCallback();
}
void FirmwareView::updateVersions(const String &preferred) {
  versions.clear();
  version.clear(juce::dontSendNotification);
  for (const auto &entry : catalog)
    if (entry.hardware == FirmwareHardware(hardware.getSelectedId() - 1))
      versions.addIfNotAlreadyThere(entry.version);
  for (int i = 0; i < versions.size(); ++i)
    version.addItem(versions[i] + (i == 0 ? (onlineCatalog ? " (latest)" : " (bundled)") : ""), i + 1);
  const auto preferredIndex = versions.indexOf(preferred);
  versionChosen = preferredIndex >= 0;
  version.setSelectedId(versions.isEmpty() ? 0 : std::max(0, preferredIndex) + 1,
                        juce::dontSendNotification);
  updateBuilds();
}
void FirmwareView::updateBuilds() {
  entries.clear();
  build.clear(juce::dontSendNotification);
  for (const auto &entry : catalog)
    if (entry.hardware == FirmwareHardware(hardware.getSelectedId() - 1) &&
        version.getSelectedId() > 0 && entry.version == versions[version.getSelectedId() - 1]) {
      entries.push_back(&entry);
      build.addItem(firmwareBuildName(entry.build), int(entries.size()));
    }
  build.setSelectedId(entries.empty() ? 0 : 1, juce::dontSendNotification);
  updateEntry();
}
const FirmwareEntry &FirmwareView::selectedEntry() const {
  return *entries.at(size_t(build.getSelectedId() - 1));
}
void FirmwareView::updateEntry() {
  label(installStatus, downloadMatchesSelection() ? "Ready to install: " + downloadedFile.getFileName()
                                                : "Download the selected version to enable installation.");
  const auto model = FirmwareHardware(hardware.getSelectedId() - 1);
  label(steps[0]->body, "Selected: " + firmwareHardwareName(model) +
        ". Choose a version and build. Visualizer enables full device visualization when available.");
  docs.setButtonText(firmwareHardwareName(model) + " bootloader guide");
  docs.setTooltip("Open your hardware documentation for manual bootloader entry.\n" + firmwareGuideUrl(model));
  if (entries.empty()) {
    label(release, "No published UF2s for this hardware.");
    label(description, "Try Refresh versions or check the hardware selection.");
    return;
  }
  const auto &entry = selectedEntry();
  label(release, entry.filename);
  release.setTooltip(entry.url);
  label(description, firmwareDescription(entry));
}
bool FirmwareView::downloadMatchesSelection() const {
  if (entries.empty() || build.getSelectedId() <= 0 || downloadedChecksum.isEmpty() ||
      !downloadedFile.existsAsFile())
    return false;
  const auto &entry = selectedEntry();
  return downloadedEntry.hardware == entry.hardware && downloadedEntry.version == entry.version &&
         downloadedEntry.build == entry.build && downloadedEntry.filename == entry.filename;
}
void FirmwareView::refreshVolumes() {
  const auto previous = volume.getSelectedId() > 0 ? volumes[volume.getSelectedId() - 1] : File();
  volumes = findVolumes();
  volume.clear(juce::dontSendNotification);
  for (int i = 0; i < volumes.size(); ++i) {
    volume.addItem(volumes[i].getFullPathName(), i + 1);
    if (volumes[i] == previous || volumes.size() == 1)
      volume.setSelectedId(i + 1, juce::dontSendNotification);
  }
}
void FirmwareView::install() {
  if (busy() || !downloadMatchesSelection() || volume.getSelectedId() == 0)
    return;
  const auto target = volumes[volume.getSelectedId() - 1];
  confirming = true;
  const auto epoch = ++confirmationEpoch;
  timerCallback();
  auto safe = juce::Component::SafePointer<FirmwareView>(this);
  confirm(juce::MessageBoxOptions().withTitle("Install downloaded firmware")
              .withMessage("Install " + downloadedFile.getFileName() + " for " +
                           firmwareHardwareName(downloadedEntry.hardware) + " on " + target.getFullPathName() +
                           "? Check that this is the correct device. It will restart after installation.")
              .withButton("Install").withButton("Cancel"),
          [safe, target, epoch](int n) {
            if (!safe || safe->confirmationEpoch != epoch)
              return;
            safe->confirming = false;
            if (n == 1)
              safe->startInstall(target);
            safe->timerCallback();
          });
}
void FirmwareView::startInstall(const File &target) {
  if (busy() || !downloadMatchesSelection())
    return;
  if (installWorker.joinable())
    installWorker.join();
  const auto file = downloadedFile;
  const auto checksum = downloadedChecksum;
  awaitingInstall = true;
  installing = true;
  installProgress = 0;
  label(installStatus, "Installing " + file.getFileName() + "...");
  installWorker = std::thread([this, file, target, checksum] {
    String result;
    try {
      require(hashFile(file, [this] { return stopping.load(); }) == checksum,
              "The downloaded file changed. Download it again before installing.");
      copyFirmware(file, target, [this](double p) { installProgress = p; },
                   [this] { return stopping.load(); });
      result = "Installation completed. Wait for the device to restart and reconnect.";
    } catch (const std::exception &error) {
      result = error.what();
    }
    diagnostics::log("FIRMWARE", "Install " + file.getFileName() + ": " + result);
    {
      std::lock_guard<std::mutex> lock(installMutex);
      installResult = result;
    }
    installing = false;
  });
}
void FirmwareView::timerCallback() {
  if (awaitingVersions) {
    auto state = releases.snapshot();
    label(catalogStatus, state.message);
    if (!state.active()) {
      awaitingVersions = false;
      versionsLoaded = state.status != FirmwareReleases::Status::cancelled;
      if (state.status == FirmwareReleases::Status::succeeded) {
        catalog = std::move(state.entries);
        onlineCatalog = true;
        updateVersions(preferredVersion);
      } else {
        label(catalogStatus, state.message + (onlineCatalog ? " Keeping the previous version list."
                                                           : " Showing bundled firmware; try Refresh versions."));
      }
    }
  }
  const auto state = download.snapshot();
  if (awaitingDownload) {
    label(downloadStatus, state.message);
    downloadProgress = state.total > 0 ? double(state.received) / double(state.total) : -1.;
    if (!state.active()) {
      awaitingDownload = false;
      if (state.status == FirmwareDownload::Status::succeeded) {
        downloadedFile = state.file;
        downloadedEntry = state.entry;
        downloadedChecksum = state.checksum;
        label(installStatus, "Ready to install: " + downloadedFile.getFileName());
        refreshVolumes();
      }
    }
  }
  if (awaitingInstall) {
    downloadProgress = installProgress.load();
    if (!installing) {
      awaitingInstall = false;
      std::lock_guard<std::mutex> lock(installMutex);
      label(installStatus, installResult);
    }
  }
  if (pendingHardwareId != 0 && !busy()) {
    const auto id = pendingHardwareId;
    pendingHardwareId = 0;
    hardware.setSelectedId(id, juce::sendNotificationSync);
  }
  const bool active = busy();
  if (!active && downloadMatchesSelection() && ++volumeTicks >= 10) {
    volumeTicks = 0;
    refreshVolumes();
  }
  hardware.setEnabled(!active);
  version.setEnabled(!active && !versions.isEmpty());
  build.setEnabled(!active && !entries.empty());
  get.setEnabled(!active && !entries.empty());
  refresh.setEnabled(!active);
  reset.setEnabled(!active && downloadMatchesSelection() && device.connected());
  rescan.setEnabled(!active);
  volume.setEnabled(!active);
  write.setEnabled(!active && downloadMatchesSelection() && volume.getSelectedId() > 0);
  cancel.setEnabled(state.active() || awaitingVersions);
  reveal.setEnabled(downloadedFile.existsAsFile());
  progressBar.setVisible(state.active() || awaitingVersions || awaitingInstall);
  if (awaitingVersions)
    downloadProgress = -1.;
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
  line(hardwareLabel, 22, 2);
  line(hardware, 32);
  if (column >= 540) {
    const int half = (column - 10) / 2;
    versionLabel.setBounds(20, y, half, 22);
    buildLabel.setBounds(30 + half, y, half, 22);
    y += 24;
    version.setBounds(20, y, half, 32);
    build.setBounds(30 + half, y, half, 32);
    y += 42;
  } else {
    line(versionLabel, 22, 2);
    line(version, 32);
    line(buildLabel, 22, 2);
    line(build, 32, 10);
  }
  refresh.setBounds(20, y, refresh.preferredWidth(32), 32);
  y += 40;
  line(catalogStatus, 36, 4);
  line(modelNote, 32, 4);
  line(release, 30, 4);
  line(description, 56);
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
  line(downloadStatus, 48);
  line(installHeading, 26, 4);
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
  line(installStatus, 48);
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
