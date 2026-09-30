#include "IconButton.h"
#include <LucideData.h>
namespace core {
namespace {
constexpr int horizontalPadding = 10, iconGap = 6;
}
IconButton::IconButton(const String &text, const String &iconName, bool onlyIcon)
    : TextButton(text), iconOnly(onlyIcon) {
  setIcon(iconName);
}
void IconButton::setIcon(const String &iconName) {
  if (currentIcon == iconName)
    return;
  currentIcon = iconName;
  const auto resource = iconName.removeCharacters("-") + "_svg";
  int size = 0;
  const auto *data = LucideData::getNamedResource(resource.toRawUTF8(), size);
  drawing.reset();
  // Resolve the SVG's inherited currentColor without altering vendored assets.
  if (data != nullptr)
    drawing = juce::Drawable::createFromSVGString(
        String::fromUTF8(data, size).replace("currentColor", "#000000"));
  jassert(drawing != nullptr);
  iconInk = juce::Colours::black;
  repaint();
}
float IconButton::iconSize(int height) {
  return juce::jlimit(14.f, 20.f, float(height - 12));
}
int IconButton::preferredWidth(int height) {
  const auto font = getLookAndFeel().getTextButtonFont(*this, height);
  const bool hasText = !iconOnly && getButtonText().isNotEmpty();
  return 2 * horizontalPadding + int(iconSize(height)) +
         (hasText ? iconGap + juce::GlyphArrangement::getStringWidthInt(font, getButtonText()) : 0);
}
void IconButton::paintButton(juce::Graphics &g, bool highlighted, bool down) {
  auto &look = getLookAndFeel();
  look.drawButtonBackground(g, *this,
                            findColour(getToggleState() ? buttonOnColourId : buttonColourId),
                            highlighted, down);
  const auto ink = findColour(getToggleState() ? textColourOnId : textColourOffId)
                       .withMultipliedAlpha(isEnabled() ? 1.f : .5f);
  const auto font = look.getTextButtonFont(*this, getHeight());
  const bool hasText = !iconOnly && getButtonText().isNotEmpty();
  const float available = float(std::max(0, getWidth() - 2 * horizontalPadding));
  const float side = drawing ? std::min(iconSize(getHeight()), available) : 0.f;
  const float gap = hasText && drawing ? float(iconGap) : 0.f;
  const float textWidth = hasText ? float(juce::GlyphArrangement::getStringWidthInt(
                                        font, getButtonText()))
                                 : 0.f;
  auto content = getLocalBounds().toFloat().withSizeKeepingCentre(
      std::min(available, side + gap + textWidth), float(getHeight()));
  if (drawing) {
    if (iconInk != ink) {
      drawing->replaceColour(iconInk, ink);
      iconInk = ink;
    }
    const auto bounds = content.removeFromLeft(side).withSizeKeepingCentre(side, side);
    // Scale the common viewBox, not each icon's ink bounds, for consistent strokes.
    drawing->draw(g, 1.f, juce::AffineTransform::scale(side / 24.f)
                              .translated(bounds.getX(), bounds.getY()));
    content.removeFromLeft(gap);
  }
  if (hasText && content.getWidth() > 0) {
    g.setColour(ink);
    g.setFont(font);
    g.drawText(getButtonText(), content, juce::Justification::centredLeft, true);
  }
}
} // namespace core
