#pragma once
#include "Device.h"
#include "Firmware.h"
#include "IconButton.h"

namespace core {
class FirmwareView final : public juce::Component, private juce::Timer {
public:
  FirmwareView(Device &, Look &, FirmwareHardware);
  ~FirmwareView() override;
  void resized() override;
  void paint(juce::Graphics &g) override { g.fillAll(look.theme.background); }
  void cancelDownload() { download.cancel(); }

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
  void updateBuilds();
  void updateEntry();
  const FirmwareEntry &selectedEntry() const;
  void refreshVolumes();
  void flash();
  void selectImage(const File &, const String &product);
  bool busy() const;
  Device &device;
  Look &look;
  juce::Viewport viewport;
  juce::Component content;
  juce::Label heading, hardwareLabel, buildLabel, modelNote, release, description, downloadStatus,
      installHeading, fileLabel, error, guideHeading;
  juce::ComboBox hardware, build, volume;
  IconButton get{"Download UF2", "download"}, cancel{"Cancel", "x"},
      reveal{"Show in folder", "folder-open"}, docs{"Bootloader guide", "external-link"},
      reset{"Reset to bootloader", "power"}, rescan{"Refresh drives", "refresh-cw"},
      choose{"Choose local UF2", "file-up"}, write{"Flash selected UF2", "file-up"};
  double downloadProgress = 0;
  juce::ProgressBar progressBar{downloadProgress};
  std::array<std::unique_ptr<GuideStep>, 4> steps;
  std::vector<const FirmwareEntry *> entries;
  FirmwareDownload download;
  bool awaitingDownload = false, confirming = false, choosing = false;
  juce::Array<File> volumes;
  File image, downloadedFile;
  std::unique_ptr<juce::FileChooser> chooser;
  std::thread worker;
  std::atomic<bool> stopping{false}, working{false};
  std::atomic<double> flashProgress{0};
  std::mutex mutex;
  String result;
};
} // namespace core
