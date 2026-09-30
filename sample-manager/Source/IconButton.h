#pragma once
#include "Common.h"
namespace core {
// A normal JUCE text button with a bundled, theme-colored Lucide SVG.
// Icon-only buttons retain their text for accessibility and action logging.
class IconButton final : public juce::TextButton {
public:
  IconButton(const String &text, const String &iconName, bool iconOnly = false);
  void setIcon(const String &iconName);
  int preferredWidth(int height);

private:
  void paintButton(juce::Graphics &, bool highlighted, bool down) override;
  static float iconSize(int height);
  String currentIcon;
  bool iconOnly;
  juce::Colour iconInk{juce::Colours::black};
  std::unique_ptr<juce::Drawable> drawing;
};
} // namespace core
