#pragma once
#include "Manager.h"
#include "Theme.h"
namespace core {
class SettingsView final : public juce::Component {
public:
  SettingsView(Manager &, Look &, int presentation);
  void paint(juce::Graphics &) override;
  void resized() override;

private:
  void refreshEffects();
  void refreshSampleCVMapping();
  void commitStartTempo();
  Manager &manager;
  Look &look;
  int presentation, bank = 0;
  card::Settings values;
  juce::OwnedArray<juce::Label> labels;
  juce::OwnedArray<juce::ComboBox> controls;
  std::vector<const card::Setting *> definitions;
  juce::OwnedArray<juce::TextButton> banks, effects;
  std::array<std::unique_ptr<juce::Drawable>, 7> runes;
  juce::Label heading;
  juce::Label startTempoLabel, startTempoBpmLabel;
  juce::ComboBox startTempoMode;
  juce::TextEditor startTempoBpm;
  Tooltips tooltips{*this};
};
} // namespace core
