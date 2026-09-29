#pragma once
#include "Common.h"
namespace core {
struct Theme {
  String name, effectHeading;
  juce::Colour background, header, sidebar, accent, wave;
  static Theme get(int presentation);
};
class Look final : public juce::LookAndFeel_V4 {
public:
  Look();
  void presentation(int);
  Theme theme = Theme::get(0);
  juce::Font font(float height) const;
  juce::Font icon(float height) const;
  juce::Font getTextButtonFont(juce::TextButton &, int height) override {
    return font(std::min(13.f, height * .48f));
  }
  juce::Font getComboBoxFont(juce::ComboBox &) override { return font(13); }
  juce::Font getLabelFont(juce::Label &) override { return font(13); }

private:
  juce::Typeface::Ptr mono, icons;
};
File preferencesFile();
bool migrateVisualizerPreferences(const File &previous,
                                  const File &destination);
} // namespace core
