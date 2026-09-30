#pragma once
#include "Session.h"
#include "../IconButton.h"
namespace zv {
class MonoLook final : public juce::LookAndFeel_V4 {
public:
  MonoLook();
  juce::Font getTextButtonFont(juce::TextButton &, int) override;
  juce::Font getComboBoxFont(juce::ComboBox &) override;
  void drawButtonBackground(juce::Graphics &, juce::Button &,
                            const juce::Colour &, bool, bool) override;
  juce::Font font(float size, bool bold = false) const;
  juce::Font icon(float size) const;

private:
  juce::Typeface::Ptr regular, medium, icons;
};
class Editor final : public juce::Component, public juce::TooltipClient, private juce::Timer {
public:
  explicit Editor(Session &);
  void setActive(bool);
  void setDetached() {
    detach.setButtonText("FULL");
    detach.setIcon("maximize");
    detach.setTooltip("Enter or leave fullscreen\nfor this visualizer window.");
    resized();
  }
  juce::String getTooltip() override {
    return "Waveform, effects and spectrum\nfor the selected Device or Preview source.\n"
           "Use the sample editor to change markers.";
  }
  std::function<void()> onDetach;
  ~Editor() override;
  void paint(juce::Graphics &) override;
  void resized() override;
  void renderAt(double time) {
    fixedTime = time;
    updateControls();
    animate(time);
    repaint();
  }

private:
  void timerCallback() override;
  void updateControls();
  void animate(double now);
  void drawWave(juce::Graphics &, juce::Rectangle<float>, double now);
  void drawEffects(juce::Graphics &, juce::Rectangle<float>, double now);
  void drawSpectrum(juce::Graphics &, juce::Rectangle<float>);
  void text(juce::Graphics &, const juce::String &, juce::Rectangle<float>,
            float, juce::Colour,
            juce::Justification = juce::Justification::centredLeft,
            bool bold = false);
  Session &session;
  MonoLook look;
  core::Tooltips tooltips{*this};
  core::IconButton detach{"DETACH", "external-link"};
  juce::ComboBox source;
  juce::ToggleButton motion{"REDUCED MOTION"};
  bool setup = false, updating = false;
  bool reduceMotion = false;
  juce::String portKey;
  juce::Rectangle<float> scope;
  std::shared_ptr<const Wave> cachedWave;
  juce::Path envelope;
  juce::Rectangle<float> cachedBounds;
  std::array<float, 32> displayed{};
  std::array<double, 16> effectStarted{};
  uint16_t previousEffects = 0;
  double lastFrame = 0, lastPress = -1;
  juce::String lastTrigger;
  float pressSide = 1, pressHorizontal = 0.5f, pressVertical = 0.5f;
  std::optional<double> fixedTime;
};
} // namespace zv
