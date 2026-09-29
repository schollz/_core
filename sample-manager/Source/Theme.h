#pragma once
#include "Common.h"
namespace core {
struct Theme {
  String name, effectHeading;
  juce::Colour background, header, sidebar, accent, wave, bankRail;
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
// JUCE scopes tooltip windows to a native peer. The dock shares a peer with
// the manager, so also restrict each tooltip to its own view and look-and-feel.
class Tooltips final : public juce::TooltipWindow {
public:
  explicit Tooltips(juce::Component &view) : TooltipWindow(&view, 600), owner(view) {
    setOpaque(false);
  }
  String getTipFor(juce::Component &component) override {
    if ((&component != &owner && !owner.isParentOf(&component)) ||
        &component.getLookAndFeel() != &owner.getLookAndFeel())
      return {};
    // Text editors and other controls contain internal viewports/labels that
    // receive hover events. Let those wrappers use their control's help text.
    for (auto *target = &component; target != nullptr; target = target->getParentComponent()) {
      auto tip = TooltipWindow::getTipFor(*target);
      if (tip.isNotEmpty())
        return tip;
      if (target == &owner)
        break;
    }
    return {};
  }

private:
  juce::Component &owner;
};
File preferencesFile();
bool migrateVisualizerPreferences(const File &previous, const File &destination);
} // namespace core
