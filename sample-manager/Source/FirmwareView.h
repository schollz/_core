#pragma once
#include "Firmware.h"
#include "IconButton.h"
#include "Theme.h"

namespace core {
class FirmwareView final : public juce::Component, private juce::Timer {
public:
  FirmwareView(Look &, FirmwareHardware);
  ~FirmwareView() override;
  void resized() override;
  void paint(juce::Graphics &g) override { g.fillAll(look.theme.background); }
  void cancelDownload() {
    download.cancel();
    releases.cancel();
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
  const FirmwareEntry &selectedEntry() const;
  bool busy() const;
  Look &look;
  juce::Viewport viewport;
  juce::Component content;
  juce::Label heading, hardwareLabel, versionLabel, buildLabel, modelNote, release, description,
      catalogStatus, downloadStatus, guideHeading;
  juce::ComboBox hardware, version, build;
  IconButton get{"Download UF2", "download"}, cancel{"Cancel", "x"},
      reveal{"Show in folder", "folder-open"}, docs{"Bootloader guide", "external-link"},
      refresh{"Refresh versions", "refresh-cw"};
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
};
} // namespace core
