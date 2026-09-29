#include "WaveformDisplay.h"

#include "CustomFont.h"

WaveformDisplay::WaveformDisplay() { setOpaque(true); }

void WaveformDisplay::setTheme(const ThemePalette& theme) {
  currentTheme = theme;
  repaint();
}

void WaveformDisplay::paint(juce::Graphics& g) {
  auto bounds = getLocalBounds();

  // Background
  g.fillAll(currentTheme.background);

  // Draw border (Digital Brutalism: thick rectangular outline)
  g.setColour(currentTheme.outline);
  g.drawRect(bounds, 2);

  if (audioBuffer == nullptr || audioBuffer->getNumSamples() == 0) {
    g.setColour(currentTheme.mutedText);
    g.setFont(CustomFont::getMonaspaceKrypton(12.0f));
    g.drawText("DROP AUDIO FILE OR LOAD SAMPLE", bounds,
               juce::Justification::centred);
    return;
  }

  // Determine currently playing slice boundaries (for highlighted waveform)
  bool hasHighlight = false;
  float highlightStartX = 0.0f;
  float highlightEndX = 0.0f;
  if (isPlaying && showPrimary && playbackPosition >= 0.0f &&
      playbackPosition <= 1.0f && slicePositions.size() >= 2) {
    for (size_t i = 0; i + 1 < slicePositions.size(); ++i) {
      if (playbackPosition >= slicePositions[i] &&
          playbackPosition < slicePositions[i + 1]) {
        highlightStartX = normalizedToScreenX(slicePositions[i]);
        highlightEndX = normalizedToScreenX(slicePositions[i + 1]);
        hasHighlight = (highlightEndX > highlightStartX);
        break;
      }
    }
  }

  // Draw waveform from cache
  if (!waveformCache.empty()) {
    int height = bounds.getHeight();
    float midY = static_cast<float>(height) / 2.0f;
    const size_t cacheSize = waveformCache.size();

    // Base waveform
    g.setColour(currentTheme.waveform);
    for (size_t i = 0; i < cacheSize; ++i) {
      float amplitude = waveformCache[i] * midY;
      float x = static_cast<float>(i);
      g.drawLine(x, midY - amplitude, x, midY + amplitude, 2.0f);
    }

    // Highlight waveform within active slice by overdrawing in brighter/darker
    // tone
    if (hasHighlight) {
      int startPixel =
          juce::jlimit(0, static_cast<int>(cacheSize) - 1,
                       static_cast<int>(std::floor(highlightStartX)));
      int endPixel = juce::jlimit(0, static_cast<int>(cacheSize) - 1,
                                  static_cast<int>(std::ceil(highlightEndX)));

      if (endPixel >= startPixel) {
        juce::Colour highlightWave = currentTheme.isDark
                                         ? currentTheme.waveform.brighter(0.8f)
                                         : currentTheme.waveform.darker(0.5f);
        g.setColour(highlightWave);

        for (int i = startPixel;
             i <= endPixel && i < static_cast<int>(cacheSize); ++i) {
          float amplitude = waveformCache[static_cast<size_t>(i)] * midY;
          float x = static_cast<float>(i);
          g.drawLine(x, midY - amplitude, x, midY + amplitude, 2.0f);
        }
      }
    }
  }

  // Draw slice markers as dashed lines (Digital Brutalism style)
  for (size_t i = 0; i < slicePositions.size(); ++i) {
    // Skip the 0.0 and 1.0 edges; they're always at the boundaries.
    if (i == 0 || i + 1 == slicePositions.size()) continue;

    float x = normalizedToScreenX(slicePositions[i]);
    const bool isHovered = (static_cast<int>(i) == hoveredMarkerIndex);
    const juce::Colour markerColour =
        isHovered
            ? (currentTheme.isDark ? currentTheme.sliceLine.brighter(0.6f)
                                   : currentTheme.sliceLine.darker(0.4f))
            : currentTheme.sliceLine;
    const float markerThickness = isHovered ? 3.0f : 2.0f;

    // Only draw if visible
    if (x >= 0 && x <= static_cast<float>(bounds.getWidth())) {
      g.setColour(markerColour);

      // Draw dashed vertical line
      constexpr int ypos = 2;
      float dashLength = 4.0f;
      float gapLength = 4.0f;
      float y = static_cast<float>(ypos);
      while (y < static_cast<float>(bounds.getHeight() - ypos)) {
        float endY = juce::jmin(y + dashLength,
                                static_cast<float>(bounds.getHeight() - ypos));
        g.drawLine(x, y, x, endY, markerThickness);
        y += dashLength + gapLength;
      }

      // Draw marker handle at top (rectangular, no rounded corners)
      constexpr int markerWidth = 13;
      constexpr int markerHeight = 12;
      constexpr int markerHalfWidth = markerWidth / 2;
      g.fillRect(static_cast<int>(x) - markerHalfWidth, ypos, markerWidth,
                 markerHeight);
      if (isHovered) {
        g.drawRect(static_cast<int>(x) - markerHalfWidth - 1, ypos - 1,
                   markerWidth + 2, markerHeight + 2, 2);
      }

      // Draw slice number
      g.setColour(currentTheme.isDark ? juce::Colours::black
                                      : juce::Colours::white);
      g.setFont(CustomFont::getMonaspaceKrypton(9.0f));
      if (i < slicePositions.size() - 1) {
        g.drawText(juce::String(static_cast<int>(i)),
                   static_cast<int>(x) - markerHalfWidth, ypos, markerWidth,
                   markerHeight, juce::Justification::centred);
      }
    }
  }

  // Draw playback position lines
  if (showPrimary && playbackPosition > 0.0f && playbackPosition < 1.0f) {
    float x = normalizedToScreenX(playbackPosition);

    // Only draw if visible
    if (x >= 0 && x <= static_cast<float>(bounds.getWidth())) {
      g.setColour(currentTheme.playbackLine.withAlpha(primaryAlpha));
      g.drawLine(x, 0.0f, x, static_cast<float>(bounds.getHeight()), 2.0f);
    }
  }

  if (showSecondary && secondaryPlaybackPosition > 0.0f &&
      secondaryPlaybackPosition < 1.0f) {
    float x = normalizedToScreenX(secondaryPlaybackPosition);

    if (x >= 0 && x <= static_cast<float>(bounds.getWidth())) {
      g.setColour(currentTheme.playbackLine.withAlpha(secondaryAlpha));
      g.drawLine(x, 0.0f, x, static_cast<float>(bounds.getHeight()), 2.0f);
    }
  }

  // Virtual cursors (yellow)
  if (showVirtualPrimary && virtualPlaybackPosition > 0.0f &&
      virtualPlaybackPosition < 1.0f) {
    float x = normalizedToScreenX(virtualPlaybackPosition);
    if (x >= 0 && x <= static_cast<float>(bounds.getWidth())) {
      g.setColour(juce::Colours::yellow.withAlpha(virtualPrimaryAlpha));
      g.drawLine(x, 0.0f, x, static_cast<float>(bounds.getHeight()), 2.0f);
    }
  }

  if (showVirtualSecondary && virtualSecondaryPlaybackPosition > 0.0f &&
      virtualSecondaryPlaybackPosition < 1.0f) {
    float x = normalizedToScreenX(virtualSecondaryPlaybackPosition);
    if (x >= 0 && x <= static_cast<float>(bounds.getWidth())) {
      g.setColour(juce::Colours::yellow.withAlpha(virtualSecondaryAlpha));
      g.drawLine(x, 0.0f, x, static_cast<float>(bounds.getHeight()), 2.0f);
    }
  }
}

void WaveformDisplay::resized() { updateWaveformCache(); }

void WaveformDisplay::setBuffer(const juce::AudioBuffer<float>* buffer) {
  audioBuffer = buffer;
  updateWaveformCache();
  repaint();
}

void WaveformDisplay::setSlicePositions(const std::vector<float>& positions) {
  slicePositions = positions;
  draggedMarkerIndex = -1;
  hoveredMarkerIndex = -1;
  repaint();
}

void WaveformDisplay::setPlaybackPosition(float position) {
  playbackPosition = position;
  showPrimary = true;
  repaint();
}

void WaveformDisplay::setPlayheadPositions(float activePosition, bool hasActive,
                                           float secondaryPosition,
                                           bool hasSecondary, float activeAlpha,
                                           float secondaryAlphaValue) {
  playbackPosition = activePosition;
  showPrimary = hasActive;
  secondaryPlaybackPosition = secondaryPosition;
  showSecondary = hasSecondary;
  primaryAlpha = activeAlpha;
  secondaryAlpha = secondaryAlphaValue;
  repaint();
}

void WaveformDisplay::setVirtualPlayheadPositions(
    float activePosition, bool hasActive, float secondaryPosition,
    bool hasSecondary, float activeAlpha, float secondaryAlphaValue) {
  virtualPlaybackPosition = activePosition;
  showVirtualPrimary = hasActive;
  virtualSecondaryPlaybackPosition = secondaryPosition;
  showVirtualSecondary = hasSecondary;
  virtualPrimaryAlpha = activeAlpha;
  virtualSecondaryAlpha = secondaryAlphaValue;
  repaint();
}

void WaveformDisplay::setPlaybackState(bool playing) {
  isPlaying = playing;
  if (!isPlaying) {
    playbackPosition = 0.0f;
    showPrimary = false;
    showSecondary = false;
    showVirtualPrimary = false;
    showVirtualSecondary = false;
  }
  repaint();
}

void WaveformDisplay::updateWaveformCache() {
  int width = getWidth();

  if (width <= 0 || audioBuffer == nullptr ||
      audioBuffer->getNumSamples() == 0) {
    waveformCache.clear();
    cachedWidth = 0;
    return;
  }

  cachedWidth = width;
  waveformCache.resize(static_cast<size_t>(width));

  int numSamples = audioBuffer->getNumSamples();
  int numChannels = audioBuffer->getNumChannels();

  // Calculate the visible range in normalized coordinates
  float visibleRange = 1.0f / zoomFactor;
  float startNormalized = viewOffsetX;
  float endNormalized = juce::jmin(viewOffsetX + visibleRange, 1.0f);

  // Convert to sample positions
  int startSampleGlobal =
      static_cast<int>(startNormalized * static_cast<float>(numSamples));
  int endSampleGlobal =
      static_cast<int>(endNormalized * static_cast<float>(numSamples));
  int visibleSamples = endSampleGlobal - startSampleGlobal;

  float samplesPerPixel =
      static_cast<float>(visibleSamples) / static_cast<float>(width);

  for (int x = 0; x < width; ++x) {
    int startSample = startSampleGlobal +
                      static_cast<int>(static_cast<float>(x) * samplesPerPixel);
    int endSample =
        startSampleGlobal +
        static_cast<int>(static_cast<float>(x + 1) * samplesPerPixel);

    endSample = juce::jmin(endSample, numSamples);
    startSample = juce::jmin(startSample, numSamples);

    float maxVal = 0.0f;

    for (int ch = 0; ch < numChannels; ++ch) {
      const float* data = audioBuffer->getReadPointer(ch);
      for (int s = startSample; s < endSample; ++s) {
        maxVal = juce::jmax(maxVal, std::abs(data[s]));
      }
    }

    waveformCache[static_cast<size_t>(x)] = maxVal;
  }
}

int WaveformDisplay::getMarkerAtPosition(int x) const {
  for (size_t i = 0; i < slicePositions.size(); ++i) {
    float markerX = normalizedToScreenX(slicePositions[i]);
    if (std::abs(static_cast<float>(x) - markerX) <
        static_cast<float>(markerHitWidth)) {
      return static_cast<int>(i);
    }
  }

  return -1;
}

int WaveformDisplay::getSliceAtPosition(int x) const {
  if (slicePositions.size() < 2) return -1;

  float normalizedX = screenXToNormalized(static_cast<float>(x));

  for (size_t i = 0; i + 1 < slicePositions.size(); ++i) {
    if (normalizedX >= slicePositions[i] &&
        normalizedX < slicePositions[i + 1]) {
      return static_cast<int>(i);
    }
  }

  return -1;
}

void WaveformDisplay::mouseDown(const juce::MouseEvent& event) {
  int x = event.getPosition().x;

  // Secondary click adds a new slice marker
  if (event.mods.isRightButtonDown()) {
    if (getMarkerAtPosition(x) < 0 && onSliceMarkerAdded) {
      float normalizedX = screenXToNormalized(static_cast<float>(x));
      if (normalizedX > 0.0f && normalizedX < 1.0f) {
        onSliceMarkerAdded(normalizedX);
      }
    }
    return;
  }

  // Handle middle button for panning (only when zoomed in)
  if (event.mods.isMiddleButtonDown() && zoomFactor > 1.0f) {
    isMiddleButtonDragging = true;
    middleDragStartX = static_cast<float>(x);
    middleDragStartOffset = viewOffsetX;
    return;
  }

  // Check if clicking on a marker
  draggedMarkerIndex = getMarkerAtPosition(x);

  if (draggedMarkerIndex >= 0) {
    // Don't allow dragging first or last marker
    if (draggedMarkerIndex == 0 ||
        draggedMarkerIndex == static_cast<int>(slicePositions.size()) - 1) {
      draggedMarkerIndex = -1;
    }
    return;
  }

  // Check if clicking on a slice for preview
  int sliceIndex = getSliceAtPosition(x);
  if (sliceIndex >= 0 && onSliceClicked) {
    onSliceClicked(sliceIndex);
  }
}

void WaveformDisplay::mouseDoubleClick(const juce::MouseEvent& event) {
  int x = event.getPosition().x;

  if (draggedMarkerIndex >= 0) return;

  int markerIndex = getMarkerAtPosition(x);
  if (markerIndex >= 0 && onSliceMarkerDoubleClicked) {
    onSliceMarkerDoubleClicked(markerIndex);
  }
}

void WaveformDisplay::mouseMove(const juce::MouseEvent& event) {
  int x = event.getPosition().x;
  int markerIndex = getMarkerAtPosition(x);
  const int lastMarkerIndex = static_cast<int>(slicePositions.size()) - 1;
  if (markerIndex <= 0 || markerIndex >= lastMarkerIndex) markerIndex = -1;

  if (markerIndex != hoveredMarkerIndex) {
    hoveredMarkerIndex = markerIndex;
    repaint();
  }
}

void WaveformDisplay::mouseExit(const juce::MouseEvent& /*event*/) {
  if (hoveredMarkerIndex != -1) {
    hoveredMarkerIndex = -1;
    repaint();
  }
}

void WaveformDisplay::mouseDrag(const juce::MouseEvent& event) {
  int x = event.getPosition().x;

  // Handle middle button panning
  if (isMiddleButtonDragging) {
    float dragDelta = static_cast<float>(x) - middleDragStartX;
    float visibleRange = 1.0f / zoomFactor;
    // Convert pixel delta to normalized offset delta
    float offsetDelta =
        -dragDelta / static_cast<float>(getWidth()) * visibleRange;
    float newOffset = middleDragStartOffset + offsetDelta;
    // Constrain offset to valid range
    viewOffsetX = juce::jlimit(0.0f, 1.0f - visibleRange, newOffset);
    updateWaveformCache();
    repaint();
    return;
  }

  if (draggedMarkerIndex < 0) return;

  float newPosition = screenXToNormalized(static_cast<float>(x));

  // Constrain to be between neighboring markers
  float minPos =
      (draggedMarkerIndex > 0)
          ? slicePositions[static_cast<size_t>(draggedMarkerIndex - 1)] +
                minimumMarkerSpacing
          : minimumMarkerSpacing;
  float maxPos =
      (static_cast<size_t>(draggedMarkerIndex) < slicePositions.size() - 1)
          ? slicePositions[static_cast<size_t>(draggedMarkerIndex + 1)] -
                minimumMarkerSpacing
          : 1.0f - minimumMarkerSpacing;

  newPosition = juce::jlimit(minPos, maxPos, newPosition);
  slicePositions[static_cast<size_t>(draggedMarkerIndex)] = newPosition;

  if (onSliceMarkerMoved) {
    onSliceMarkerMoved(draggedMarkerIndex, newPosition);
  }

  repaint();
}

void WaveformDisplay::mouseUp(const juce::MouseEvent& /*event*/) {
  draggedMarkerIndex = -1;
  isMiddleButtonDragging = false;
}

void WaveformDisplay::mouseWheelMove(const juce::MouseEvent& event,
                                     const juce::MouseWheelDetails& wheel) {
  // Get mouse position before zoom
  float mouseX = static_cast<float>(event.getPosition().x);
  float mouseNormalizedPos = screenXToNormalized(mouseX);

  // Calculate zoom delta (positive = zoom in, negative = zoom out)
  float zoomDelta = wheel.deltaY;

  // Adjust zoom factor (limit between 1x and 100x)
  float zoomMultiplier = 1.0f + (zoomDelta * 0.5f);
  float newZoomFactor = juce::jlimit(1.0f, 100.0f, zoomFactor * zoomMultiplier);

  // Calculate new visible range
  float oldVisibleRange = 1.0f / zoomFactor;
  float newVisibleRange = 1.0f / newZoomFactor;

  // Adjust offset to keep mouse position centered
  // The point under the mouse should remain at the same normalized position
  float mouseRelativePos = (mouseNormalizedPos - viewOffsetX) / oldVisibleRange;
  float newOffsetX = mouseNormalizedPos - (mouseRelativePos * newVisibleRange);

  // Constrain offset to valid range
  newOffsetX = juce::jlimit(0.0f, 1.0f - newVisibleRange, newOffsetX);

  // Apply new zoom and offset
  zoomFactor = newZoomFactor;
  viewOffsetX = newOffsetX;

  // Update display
  updateWaveformCache();
  repaint();
}

float WaveformDisplay::screenXToNormalized(float screenX) const {
  float normalizedScreenX = screenX / static_cast<float>(getWidth());
  float visibleRange = 1.0f / zoomFactor;
  return viewOffsetX + normalizedScreenX * visibleRange;
}

float WaveformDisplay::normalizedToScreenX(float normalized) const {
  float visibleRange = 1.0f / zoomFactor;
  float relativePos = (normalized - viewOffsetX) / visibleRange;
  return relativePos * static_cast<float>(getWidth());
}
