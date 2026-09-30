#pragma once
#include "Firmware.h"
#include "Device.h"
#include "IconButton.h"
#include "Theme.h"

namespace core {
class FirmwareView final : public juce::Component, private juce::Timer {
public:
  FirmwareView(Device &, Look &, FirmwareHardware);
  ~FirmwareView() override;
  void resized() override;
  void paint(juce::Graphics &g) override { g.fillAll(look.theme.background); }
  void cancelDownload() {
    download.cancel();
    releases.cancel();
    ++confirmationEpoch;
    confirming = false;
  }
  void setHardware(FirmwareHardware);
  void loadVersions();

private:
  friend void firmwareViewTests();
  struct GuideStep final : juce::Component {
    GuideStep(Look &, String number, String title, String body, String icon);
    void resized() override;
    void paint(juce::Graphics &) override;
    Look &look;
    String number;
    juce::Label title, body;
    std::unique_ptr<juce::Drawable> icon;
  };
  void timerCallback() override;
  void refreshVersions();
  void updateVersions(const String &preferred = {});
  void updateBuilds();
  void updateEntry();
  void refreshVolumes();
  void install();
  void startInstall(const File &target);
  bool downloadMatchesSelection() const;
  const FirmwareEntry &selectedEntry() const;
  bool busy() const;
  Device &device;
  Look &look;
  juce::Viewport viewport;
  juce::Component content;
  juce::Label heading, hardwareLabel, versionLabel, buildLabel, modelNote, release, description,
      catalogStatus, downloadStatus, guideHeading, installHeading, installStatus;
  juce::ComboBox hardware, version, build, volume;
  IconButton get{"Download UF2", "download"}, cancel{"Cancel", "x"},
      reveal{"Show in folder", "folder-open"}, docs{"Bootloader guide", "external-link"},
      refresh{"Refresh versions", "refresh-cw"}, reset{"Reset to bootloader", "power"},
      rescan{"Refresh drives", "refresh-cw"}, write{"Install firmware", "file-up"};
  double downloadProgress = 0;
  juce::ProgressBar progressBar{downloadProgress};
  std::array<std::unique_ptr<GuideStep>, 4> steps;
  std::vector<FirmwareEntry> catalog = firmwareCatalog();
  juce::StringArray versions;
  std::vector<const FirmwareEntry *> entries;
  FirmwareDownload download;
  FirmwareReleases releases;
  String releaseEndpoint = firmwareReleasesUrl(), preferredVersion;
  bool awaitingDownload = false, awaitingVersions = false, versionsLoaded = false, onlineCatalog = false;
  bool versionChosen = false;
  int pendingHardwareId = 0;
  File downloadedFile;
  FirmwareEntry downloadedEntry{};
  String downloadedChecksum;
  juce::Array<File> volumes;
  std::function<juce::Array<File>()> findVolumes = bootloaderVolumes;
  std::function<void(const File &, const File &, std::function<void(double)>,
                     std::function<bool()>)> copyFirmware = flashLocalUf2;
  std::function<void(juce::MessageBoxOptions, std::function<void(int)>)> confirm =
      [](auto options, auto callback) { juce::AlertWindow::showAsync(options, std::move(callback)); };
  bool confirming = false, awaitingInstall = false;
  unsigned confirmationEpoch = 0;
  int volumeTicks = 0;
  std::thread installWorker;
  std::atomic<bool> stopping{false}, installing{false};
  std::atomic<double> installProgress{0};
  std::mutex installMutex;
  String installResult;
};
} // namespace core
