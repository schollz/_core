#include "SettingsView.h"
#include <BinaryData.h>
namespace core {
namespace {
const char *effectNames[]{
    "Distortion",   "Loss",      "Bitcrush",  "Filter",
    "Time stretch", "Delay",     "Comb",      "Beat repeat",
    "Reverb",       "Auto pan",  "Slow down", "Speed up",
    "Reverse",      "Retrigger", "Repitch",   "Tape stop"};
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
    addAndMakeVisible(label);
    auto *combo = controls.add(new juce::ComboBox);
    int n = 1;
    for (const auto &v : d.values)
      combo->addItem(v, n++);
    auto found = values.find(d.key);
    combo->setSelectedItemIndex(
        d.values.indexOf(found == values.end() ? d.initial : found->second),
        juce::dontSendNotification);
    combo->onChange = [this, combo, key = d.key] {
      values[key] = combo->getText();
      manager.settings({{key, combo->getText()}});
    };
    addAndMakeVisible(combo);
  }
  heading.setText(look.theme.effectHeading, juce::dontSendNotification);
  addAndMakeVisible(heading);
  const char *roman[]{"I", "II", "III", "IV", "V", "VI", "VII"};
  for (int b = 0; b < 7; ++b) {
    auto *button = banks.add(
        p == 2 ? static_cast<juce::TextButton *>(new RuneButton(look, b + 1))
               : new juce::TextButton(roman[b]));
    button->setClickingTogglesState(false);
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
void SettingsView::refreshEffects() {
  for (int b = 0; b < 7; ++b)
    banks[b]->setToggleState(b == bank, juce::dontSendNotification);
  for (int fx = 0; fx < 16; ++fx) {
    auto key = "grimoire/rune" + String(bank + 1) + "/effect" + String(fx + 1);
    auto it = values.find(key);
    effects[fx]->setToggleState(it != values.end() && it->second == "on",
                                juce::dontSendNotification);
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
