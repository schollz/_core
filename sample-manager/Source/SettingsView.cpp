#include "SettingsView.h"
#include <BinaryData.h>
namespace core {
namespace {
const char *effectNames[]{
    "Distortion",   "Loss",      "Bitcrush",  "Filter",
    "Time stretch", "Delay",     "Comb",      "Beat repeat",
    "Reverb",       "Auto pan",  "Slow down", "Speed up",
    "Reverse",      "Retrigger", "Repitch",   "Tape stop"};
const char *effectHelp[]{"Add saturation and grit.",
                         "Degrade the sound with digital loss.",
                         "Reduce digital resolution for a crunchy sound.",
                         "Change the tone with filtering.",
                         "Stretch playback in time.",
                         "Add repeating echoes.",
                         "Add resonant comb filtering.",
                         "Repeat short sections of the beat.",
                         "Add reverberation.",
                         "Move the sound between left and right.",
                         "Slow playback down.",
                         "Speed playback up.",
                         "Play audio backwards.",
                         "Restart audio for repeated attacks.",
                         "Change the pitch.",
                         "Slow playback to a stop like a tape machine."};
String settingHelp(const card::Setting &setting) {
  static const std::map<String, String> help{
      {"brightness", "Set the hardware LED brightness from 0 to 100."},
      {"clock_stop_sync",
       "Stop playback when the external clock stops, and resume when it returns."},
      {"clock_output_trig", "Send short trigger pulses from the clock output instead of gates."},
      {"clock_behavior_sync_slice", "Sync the clock output to slice changes instead of the tempo."},
      {"amen_cv",
       "Choose whether Amen CV accepts positive-only or positive and negative control voltages."},
      {"amen_behavior", "Choose how Amen CV changes playback: jump, repeat or split."},
      {"break_cv",
       "Choose whether Break CV accepts positive-only or positive and negative control voltages."},
      {"sample_cv",
       "Bank divisions spreads samples across 0 to +5 V (unipolar) or -5 to +5 V (bipolar). "
       "In 1 V/oct mode, negative voltages wrap backward with bipolar polarity, "
       "or select sample 1 with unipolar polarity."},
      {"sample_cv_mapping",
       "Bank divisions divides the CV range by the bank's sample count. "
       "1 V/oct selects sample 1 at 0 V and advances one sample per semitone (1/12 V), "
       "wrapping around the bank. Requires firmware with 1 V/oct sample CV support. "
       "Inactive when Sample CV is assigned to Reset; the saved choice is retained."},
      {"override_with_reset",
       "Choose which CV input acts as reset. None keeps the normal input functions."},
      {"knobx_select_sample", "Let Zeptocore's X knob select samples."},
      {"mash_mode_momentary",
       "Keep MASH active only while held, instead of toggling it on and off."}};
  const auto found = help.find(setting.key);
  return found != help.end() ? found->second : "Choose " + setting.label + " for the hardware.";
}
const juce::juce_wchar glyphs[]{0xf6be, 0xf8d7, 0xf83e, 0xf0b0, 0xf252, 0xf1da,
                                0xf55d, 0xf2f9, 0xf773, 0xf025, 0xf554, 0xf70c,
                                0xf2ea, 0xf2a1, 0xf001, 0xf4db};
class RuneButton : public juce::TextButton {
public:
  RuneButton(Look &l, int rune) : look(l) {
    int size = 0;
    auto name = "rune" + String(rune) + "_svg";
    auto data = BinaryData::getNamedResource(name.toRawUTF8(), size);
    drawable = juce::Drawable::createFromImageData(data, size);
    setTitle("Rune " + String(rune));
    setTooltip("Rune " + String(rune));
  }
  void paintButton(juce::Graphics &g, bool over, bool down) override {
    g.setColour(getToggleState() ? look.theme.accent : look.theme.sidebar);
    g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(2), 6);
    if (drawable)
      drawable->drawWithin(g, getLocalBounds().toFloat().reduced(12),
                           juce::RectanglePlacement::centred,
                           over || down ? 1.f : .85f);
  }

private:
  Look &look;
  std::unique_ptr<juce::Drawable> drawable;
};
class EffectButton : public juce::TextButton {
public:
  EffectButton(Look &l, int i) : look(l), index(i) {
    setTooltip(effectNames[i]);
    setButtonText(effectNames[i]);
  }
  void paintButton(juce::Graphics &g, bool hover, bool down) override {
    g.setColour(getToggleState()
                    ? look.theme.accent
                    : (hover || down ? look.theme.sidebar.darker(.08f)
                                     : look.theme.sidebar));
    g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(2), 7);
    g.setColour(getToggleState() ? juce::Colours::white
                                 : juce::Colour(0xff1a1a1a));
    g.setFont(look.icon(23));
    g.drawText(String::charToString(glyphs[index]),
               getLocalBounds().withTrimmedBottom(24),
               juce::Justification::centred);
    g.setFont(look.font(10));
    g.drawText(effectNames[index], getLocalBounds().removeFromBottom(25),
               juce::Justification::centred);
  }

private:
  Look &look;
  int index;
};
} // namespace
SettingsView::SettingsView(Manager &m, Look &l, int p)
    : manager(m), look(l), presentation(p),
      values(m.snapshot().project.settings) {
  setLookAndFeel(&look);
  for (const auto &d : card::settingDefinitions()) {
    bool knob =
        d.key == "knobx_select_sample" || d.key == "mash_mode_momentary";
    if ((p == 1) != knob)
      continue;
    definitions.push_back(&d);
    auto *label = labels.add(new juce::Label({}, d.label));
    label->setTooltip(settingHelp(d));
    addAndMakeVisible(label);
    auto *combo = controls.add(new juce::ComboBox);
    combo->setTooltip(settingHelp(d));
    int n = 1;
    for (const auto &v : d.values)
      combo->addItem(d.key == "sample_cv_mapping"
                         ? (v == "1voct" ? "1 V/oct" : "Bank divisions")
                         : v,
                     n++);
    auto found = values.find(d.key);
    combo->setSelectedItemIndex(
        d.values.indexOf(found == values.end() ? d.initial : found->second),
        juce::dontSendNotification);
    combo->onChange = [this, combo, key = d.key, choices = d.values] {
      const auto index = combo->getSelectedItemIndex();
      if (index < 0)
        return;
      values[key] = choices[index];
      manager.settings({{key, values[key]}});
      refreshSampleCVMapping();
    };
    addAndMakeVisible(combo);
  }
  refreshSampleCVMapping();
  heading.setText(look.theme.effectHeading, juce::dontSendNotification);
  heading.setTooltip(
      "Choose a bank below, then enable the effects available in that bank on the hardware.");
  addAndMakeVisible(heading);
  const char *roman[]{"I", "II", "III", "IV", "V", "VI", "VII"};
  for (int b = 0; b < 7; ++b) {
    auto *button = banks.add(
        p == 2 ? static_cast<juce::TextButton *>(new RuneButton(look, b + 1))
               : new juce::TextButton(roman[b]));
    button->setClickingTogglesState(false);
    button->setTooltip("Edit " + String(p == 2 ? "rune " : "effect bank ") + String(b + 1) +
                       ". Choose it, then toggle the effects below.");
    button->onClick = [this, b] {
      bank = b;
      refreshEffects();
      repaint();
    };
    addAndMakeVisible(button);
    if (p == 2) {
      int size = 0;
      auto name = "rune" + String(b + 1) + "_svg";
      auto data = BinaryData::getNamedResource(name.toRawUTF8(), size);
      runes[size_t(b)] = juce::Drawable::createFromImageData(data, size);
    }
  }
  for (int fx = 0; fx < 16; ++fx) {
    auto *button = effects.add(new EffectButton(look, fx));
    button->onClick = [this, fx] {
      auto key =
          "grimoire/rune" + String(bank + 1) + "/effect" + String(fx + 1);
      values[key] = values[key] == "on" ? "off" : "on";
      manager.settings({{key, values[key]}});
      refreshEffects();
    };
    addAndMakeVisible(button);
  }
  refreshEffects();
  setSize(780, p == 1 ? 460 : 690);
}
void SettingsView::refreshSampleCVMapping() {
  const auto reset = values.find("override_with_reset");
  const bool enabled = reset == values.end() || reset->second != "sample";
  for (int n = 0; n < controls.size(); ++n)
    if (definitions[size_t(n)]->key == "sample_cv_mapping") {
      controls[n]->setEnabled(enabled);
      labels[n]->setEnabled(enabled);
    }
}
void SettingsView::refreshEffects() {
  for (int b = 0; b < 7; ++b)
    banks[b]->setToggleState(b == bank, juce::dontSendNotification);
  for (int fx = 0; fx < 16; ++fx) {
    auto key = "grimoire/rune" + String(bank + 1) + "/effect" + String(fx + 1);
    auto it = values.find(key);
    effects[fx]->setToggleState(it != values.end() && it->second == "on",
                                juce::dontSendNotification);
    effects[fx]->setTooltip(String(effectNames[fx]) + ": " + effectHelp[fx] + "\n" +
                            (effects[fx]->getToggleState() ? "Disable" : "Enable") + " in " +
                            (presentation == 2 ? "rune " : "effect bank ") + String(bank + 1) +
                            ".");
  }
}
void SettingsView::resized() {
  int y = 24;
  int width = (getWidth() - 72) / 2;
  for (int n = 0; n < labels.size(); ++n) {
    int column = n % 2, row = n / 2;
    labels[n]->setBounds(24 + column * (width + 24), y + row * 62, width, 22);
    controls[n]->setBounds(24 + column * (width + 24), y + row * 62 + 24, width,
                           28);
  }
  y += ((labels.size() + 1) / 2) * 62 + 10;
  heading.setBounds(24, y, getWidth() - 48, 26);
  y += 34;
  for (int b = 0; b < 7; ++b)
    banks[b]->setBounds(24 + b * 102, y, 92, 44);
  y += 55;
  for (int f = 0; f < 16; ++f)
    effects[f]->setBounds(24 + (f % 8) * 91, y + (f / 8) * 84, 88, 78);
}
void SettingsView::paint(juce::Graphics &g) {
  g.fillAll(look.theme.background);
}
} // namespace core
