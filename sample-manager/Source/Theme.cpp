#include "Theme.h"
#include "Storage.h"
#include <BinaryData.h>
namespace core {
Theme Theme::get(int p) {
  if (p == 1)
    return {"Zeptocore",
            "Random effect banks",
            juce::Colour(0xfff4f0ff),
            juce::Colour(0xffbdafeb),
            juce::Colour(0xffd2cde9),
            juce::Colour(0xff6d51c9),
            juce::Colour(0xff9b82db)};
  if (p == 2)
    return {"Ectocore",
            "Grimoire of Breaks:",
            juce::Colour(0xffe6eeff),
            juce::Colour(0xff5282d9),
            juce::Colour(0xff94acda),
            juce::Colour(0xff3478ff),
            juce::Colour(0xff5282d9)};
  return {"Ezeptocore",
          "Effect banks:",
          juce::Colour(0xfff0f0f0),
          juce::Colour(0xff1a1a1a),
          juce::Colour(0xffd0d0d0),
          juce::Colour(0xffb39124),
          juce::Colour(0xff77777b)};
}
Look::Look() {
  mono = juce::Typeface::createSystemTypefaceFor(
      BinaryData::MonaspaceKryptonRegular_otf,
      BinaryData::MonaspaceKryptonRegular_otfSize);
  icons = juce::Typeface::createSystemTypefaceFor(
      BinaryData::fasolid900_ttf, BinaryData::fasolid900_ttfSize);
  presentation(0);
}
juce::Font Look::font(float height) const {
  return juce::Font(juce::FontOptions(mono).withHeight(height));
}
juce::Font Look::icon(float height) const {
  return juce::Font(juce::FontOptions(icons).withHeight(height));
}
void Look::presentation(int p) {
  theme = Theme::get(p);
  setColour(juce::ResizableWindow::backgroundColourId, theme.background);
  setColour(juce::ListBox::backgroundColourId, theme.background);
  setColour(juce::ListBox::textColourId, juce::Colour(0xff1a1a1a));
  setColour(juce::TextButton::buttonColourId, theme.sidebar);
  setColour(juce::TextButton::buttonOnColourId, theme.accent);
  setColour(juce::TextButton::textColourOffId, juce::Colour(0xff1a1a1a));
  setColour(juce::TextButton::textColourOnId, juce::Colours::white);
  setColour(juce::ComboBox::backgroundColourId, juce::Colours::white);
  setColour(juce::ComboBox::textColourId, juce::Colour(0xff1a1a1a));
  setColour(juce::TextEditor::backgroundColourId, juce::Colours::white);
  setColour(juce::TextEditor::textColourId, juce::Colour(0xff1a1a1a));
  setColour(juce::Label::textColourId, juce::Colour(0xff1a1a1a));
  setColour(juce::ToggleButton::textColourId, juce::Colour(0xff1a1a1a));
  setColour(juce::ToggleButton::tickColourId, theme.accent);
  setColour(juce::ToggleButton::tickDisabledColourId, juce::Colour(0xff777777));
}
bool migrateVisualizerPreferences(const File &previous,
                                  const File &destination) {
  if (destination.existsAsFile() || !previous.existsAsFile())
    return false;
  auto old = parseJson(previous);
  require(old.isObject(), "Former visualizer preferences must be an object");
  var next = object();
  for (auto key : {"input", "output"})
    if (old[key].isString())
      put(next, key, old[key]);
  put(next, "reduceMotion", bool(old["reduceMotion"]));
  auto folder = old["referenceRoot"].toString();
  if (File::isAbsolutePath(folder))
    put(next, "lastProject", folder);
  put(next, "migratedVisualizerPreferences", true);
  durableJson(destination, next);
  return true;
}
File preferencesFile() { return stateRoot().getChildFile("settings.json"); }
} // namespace core
